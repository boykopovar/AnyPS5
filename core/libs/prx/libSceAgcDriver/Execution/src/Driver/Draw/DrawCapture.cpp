#include <mutex>
#include <cstdio>
#include <chrono>
#include <atomic>
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BindlessHeap.hpp"
#include <algorithm>
#include <optional>
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <span>
#include <sstream>
#include <stdexcept>
#include <exception>
#include <typeinfo>

namespace AgcDriver::DriverDetail {

namespace {

void TraceMaterializationFailure(std::uint64_t token, const Submission& submission, std::size_t packetOffset, std::size_t stage, const char* phase, const std::exception_ptr& failure) {
    if (token == 0) return;
    const char* type = "unknown";
    std::string reason = "unknown exception";
    try {
        if (failure != nullptr) std::rethrow_exception(failure);
    } catch (const std::exception& error) {
        type = typeid(error).name();
        reason = error.what();
    } catch (...) {
    }
    try {
        AgcDriver::CaptureTrace::Log("draw-materialization-failed token=%llu submission=%llu queue=%x offset=%zu stage=%zu phase=%s exception=%s reason=%.256s",
                static_cast<unsigned long long>(token), static_cast<unsigned long long>(submission.serial), submission.queue, packetOffset, stage, phase, type, reason.c_str());
    } catch (...) {
    }
}

std::uint64_t UserDataMask(const DrawProgram& program, const std::optional<ShaderRecompiler::ShaderVertexStageInfo>& vertexInfo, std::uint64_t walked, const ShaderRecompiler::RecompileResult& compiled) {
    std::uint64_t mask = walked | compiled.userDataRegisterReads;
    const auto& tail = program.snapshot->userDataTailRegister;
    if (tail && *tail >= program.firstUserSgpr) {
        const auto index = *tail - program.firstUserSgpr;
        mask |= index + 1u < 64u ? std::uint64_t{3} << index : ~std::uint64_t{0};
    }
    if (vertexInfo) {
        for (const auto reg : {static_cast<std::uint32_t>(vertexInfo->fetchAttribReg), static_cast<std::uint32_t>(vertexInfo->fetchBufferReg)}) mask |= reg + 1u < 64u ? std::uint64_t{3} << reg : ~std::uint64_t{0};
    }
    return mask;
}

void TraceShaderRequestFailure(std::uint64_t token, const Submission& submission, std::size_t packetOffset, const DrawProgram& program, const std::exception_ptr& failure) noexcept {
    if (token == 0) return;
    try {
        std::string reason = "unknown exception";
        const char* type = "unknown";
        try {
            if (failure != nullptr) std::rethrow_exception(failure);
        } catch (const std::exception& error) {
            type = typeid(error).name();
            reason = error.what();
        } catch (...) {
        }
        AgcDriver::CaptureTrace::LogBestEffort("draw-shader-request-failed token=%llu submission=%llu queue=%x offset=%zu snapshot=0x%llx code=%llx codeOffset=%zu stage=%u type=%u reason=%.256s",
                static_cast<unsigned long long>(token), static_cast<unsigned long long>(submission.serial), submission.queue, packetOffset,
                static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(program.snapshot.get())),
                static_cast<unsigned long long>(program.snapshot->codeAddress), program.codeOffset,
                static_cast<unsigned>(program.binary.stage), static_cast<unsigned>(program.snapshot->type), reason.c_str());
    } catch (...) {
    }
}

}

Driver::HeapDirect Driver::materializeHeapDirect(const DrawStageContext& context, std::size_t i, const ShaderRecompiler::PreparedShaderInvocation& invocation, const ShaderRecompiler::RecompileRequest& request, PerformanceTimer& timing) {
    const auto& program = context.programs[i];
    auto& stageCapture = context.stageCaptures[i];
    HeapStageMaterialized materialized;
    Graphics::HeapCostScope callCost(Graphics::HeapCost::HeapStageCall);
    try {
        materialized = MaterializeHeapStage(invocation, program.userData, program.binary.codeAddress);
    } catch (const std::exception& error) {
        throw stageFailure(program, request, error, request.context.memory);
    }
    callCost.Stop();
    Graphics::HeapCostScope finishCost(Graphics::HeapCost::StageFinish);
    if (materialized.result == nullptr) return HeapDirect::Declined;
    if (!HeapDrawStageMappable(*materialized.result, program.binary.stage)) return HeapDirect::Unmapped;
    NoteHeapDirect();
    stageCapture.compiled = std::move(materialized.result);
    stageCapture.heapWords = stageCapture.compiled->heapTemplateId != 0 ? std::const_pointer_cast<ShaderRecompiler::HeapStageWords>(std::move(materialized.words)) : nullptr;
    if (SampleHeapStageVerify()) {
        const auto fullCapture = context.shaderMemory.Capture(invocation);
        const auto expected = invocation.Materialize(*fullCapture);
        static_cast<void>(context.shaderMemory.TakeRecentRegions());
        NoteHeapDirectVerify(stageCapture.heapWords != nullptr ? *PatchedHeapResult(*stageCapture.compiled, *stageCapture.heapWords) : *stageCapture.compiled, *expected, fullCapture.get());
    }
    stageCapture.regions.clear();
    stageCapture.direct = true;
    auto& reads = materialized.reads;
    if (reads.complete) {
        auto owned = std::make_shared<const std::vector<std::uint32_t>>(std::move(reads.values));
        std::size_t cursor = 0;
        for (const auto& [begin, end] : reads.ranges) {
            const auto count = static_cast<std::size_t>((end - begin) / sizeof(std::uint32_t));
            stageCapture.regions.push_back({begin, std::as_bytes(std::span(owned->data() + cursor, count))});
            cursor += count;
        }
        stageCapture.heapReadWords = std::move(owned);
        stageCapture.userDataUse = reads.userDataUse;
        stageCapture.capture = std::move(reads.capture);
        stageCapture.userDataMask = UserDataMask(program, context.vertexInfos[i], reads.userDataUse.asValue | reads.userDataUse.asBase, *stageCapture.compiled);
        stageCapture.ring = true;
    }
    context.recompiled[i] = true;
    timing.Mark("heap_direct");
    context.phaseTiming.Phase(DrawRowRecompile);
    return HeapDirect::Taken;
}

std::runtime_error Driver::stageFailure(const DrawProgram& program, const ShaderRecompiler::RecompileRequest& request, const std::exception& error, std::span<const ShaderRecompiler::MemoryRegion> regions) {
    static const bool dumpShaders = std::getenv("APS5_DUMP_SHADERS") != nullptr;
    std::string dump;
    if (dumpShaders) {
        try {
            auto failed = request;
            failed.context.memory = regions;
            dump = dumpRequest(program.binary.codeAddress, failed, "_failed");
        } catch (...) {}
    }
    std::ostringstream where;
    where << "shader 0x" << std::hex << program.binary.codeAddress << " stage " << std::dec << static_cast<unsigned>(program.binary.stage);
    if (!dump.empty()) where << " (" << dump << ")";
    where << ": ";
    return std::runtime_error(where.str() + error.what());
}

std::shared_ptr<const ShaderRecompiler::RecompileResult> Driver::materializeDrawStage(const DrawStageContext& context, std::size_t i, std::uint32_t pushOffset, const Pm4::DrawParameters& drawParameters, bool heapPartners) try {
    PerformanceTimer timing("Shader.DrawStage");
    using Stage = ShaderRecompiler::ShaderStage;
    context.phaseTiming.Phase(DrawRowVectors);
    const auto& program = context.programs[i];
    Graphics::HeapCostScope prepareCost(Graphics::HeapCost::StagePrepare);
    const auto waveSize = Graphics::StageWaveSize(context.graphics.stages, program.binary.stage);
    ShaderRecompiler::RecompileRequest request{
        program.binary,
        {waveSize, program.firstUserSgpr, program.userData, std::nullopt, program.binary.stage == Stage::Fragment ? std::optional(context.pixel) : std::nullopt, context.vertexInfos[i], context.memory, RegisteredFloatMode(*program.snapshot)},
        context.localDevice->Target(),
        {0, 0, pushOffset, Graphics::StagePushLimit(context.graphics.stages, pushOffset, context.graphics.depthRemap.mode != 0u) - pushOffset % Graphics::PipelinePushSlotBytes},
        ShaderRecompiler::GraphicsCompileContext{program.firstUserSgpr, context.linked, Graphics::StageMeshConfiguration(context.graphics.stages, program.binary.stage), context.graphics.stages.tessellation, {drawParameters.indexAddress, drawParameters.indexCount, drawParameters.indexSize, drawParameters.instanceCount}, program.binary.stage == Stage::Mesh ? context.meshBarycentrics : std::nullopt}
    };
    if (context.graphics.depthRemap.mode != 0u && pushOffset < Graphics::PipelinePushSlotBytes) {
        request.layout.pushConstantSizeBytes = std::min(request.layout.pushConstantSizeBytes, ShaderRecompiler::DepthRemapPushOffsetBytes - pushOffset);
    }
    const auto waitedBefore = traceCapSync() || context.profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
    const bool heapCandidate = heapPartners && (program.binary.stage == Stage::Fragment || HeapPreRasterStage(program.binary.stage)) && !HeapStageExcluded(program.binary.stage) && HeapFragmentsEnabled();
    const auto fragmentOnHeap = [&] {
        const auto fragment = std::find_if(context.programs.begin(), context.programs.end(), [](const DrawProgram& candidate) { return candidate.binary.stage == Stage::Fragment; });
        if (fragment == context.programs.end()) return true;
        const auto known = HeapFragmentKnown(fragment->snapshot.get(), fragment->codeOffset);
        return known.has_value() ? *known : PreparedHeapStage(*fragment->snapshot, fragment->codeOffset);
    };
    const bool heapVertex = request.target.descriptorMemory && HeapPreRasterStage(program.binary.stage) && fragmentOnHeap();
    const auto heapKey = HeapFragmentKey(program.codeOffset, heapVertex ? HeapKeyClass::HeapVertex : HeapKeyClass::Draw);
    const auto heapKnown = heapCandidate ? HeapFragmentKnown(program.snapshot.get(), heapKey) : std::nullopt;
    if (heapKnown.value_or(false)) request.target.gpuResolvedImages = true;
    const auto invocationFor = [&] {
        try {
            return InvocationFor(*program.snapshot, program.codeOffset, request);
        } catch (...) {
            TraceShaderRequestFailure(context.traceToken, context.submission, context.packetOffset, program, std::current_exception());
            throw;
        }
    };
    auto invocation = invocationFor();
    const auto heapEligible = [&](const std::shared_ptr<const ShaderRecompiler::IrResourcePlan>& plan) { return HeapStageEligible(program.binary.stage, plan.get(), heapVertex); };
    const auto plan = heapCandidate && invocation.Plan() == nullptr ? invocation.LoadPlan() : invocation.Plan();
    using StageReason = Graphics::HeapStageReason;
    StageReason stageReason = StageReason::Heap;
    if (!heapCandidate) stageReason = !HeapFragmentsEnabled() ? StageReason::Disabled : !heapPartners ? StageReason::PairRefused : StageReason::StageKind;
    else if (heapKnown == std::optional(false)) stageReason = StageReason::KnownIneligible;
    if (heapKnown.value_or(false) && !heapEligible(plan)) {
        stageReason = HeapStageIneligibility(program.binary.stage, plan.get(), heapVertex);
        NoteHeapFragment(program.snapshot.get(), heapKey, false);
        request.target.gpuResolvedImages = false;
        invocation = invocationFor();
    } else if (heapCandidate && !heapKnown.has_value()) {
        stageReason = HeapStageIneligibility(program.binary.stage, plan.get(), heapVertex);
        const bool eligible = stageReason == StageReason::Heap;
        NoteHeapFragment(program.snapshot.get(), heapKey, eligible);
        if (eligible) {
            request.target.gpuResolvedImages = true;
            invocation = invocationFor();
        }
    }
    Graphics::NoteHeapStageReason(stageReason);
    prepareCost.Stop();
    timing.Mark("prepared_invocation");
    auto& stageCapture = context.stageCaptures[i];
    stageCapture.direct = false;
    stageCapture.ring = false;
    stageCapture.heapReadWords.reset();
    stageCapture.capture.reset();
    stageCapture.forgetSerial = GuestMemory::ForgetSerial();
    stageCapture.pushOffset = pushOffset;
    const auto unmapped = [&] {
        Graphics::NoteHeapFallback(Graphics::HeapFallback::DrawUnmapped);
        NoteHeapFragment(program.snapshot.get(), heapKey, false);
        return materializeDrawStage(context, i, pushOffset, drawParameters, heapPartners);
    };
    if (request.target.gpuResolvedImages) {
        switch (materializeHeapDirect(context, i, invocation, request, timing)) {
        case HeapDirect::Taken: return stageCapture.compiled;
        case HeapDirect::Unmapped: return unmapped();
        case HeapDirect::Declined: break;
        }
    }
    const auto capture = [&] {
        const SampledReadScope sampling(evidenceReads);
        try {
            return CaptureInvocation(context.shaderMemory, *program.snapshot, program.codeOffset, request, invocation);
        } catch (const std::exception& error) {
            throw stageFailure(program, request, error, context.shaderMemory.Regions());
        }
    }();

    stageCapture.regions = context.shaderMemory.TakeRecentRegions();
    stageCapture.supplied = context.shaderMemory.SuppliedBlocks();
    stageCapture.userDataUse = capture->userDataUse;
    context.recompiled[i] = true;
    context.memory = context.shaderMemory.Regions();

    for (std::size_t j = 0; j < context.programs.size(); ++j) {
        if (context.matched[j] != nullptr && !context.recompiled[j] && (context.drawHit || j < i)) context.memory.insert(context.memory.end(), context.matchedRegions[j].begin(), context.matchedRegions[j].end());
    }
    request.context.memory = context.memory;
    if (traceCapSync()) traceCapture("draw-capture", program.binary.codeAddress, context.submission.queue, context.memory, Graphics::Recorder::ThreadWaitedMs() - waitedBefore);
    if (context.profile) {
        ++context.captures;
        context.phaseTiming.Phase(DrawRowCapture);

            const auto waited = std::min(Graphics::Recorder::ThreadWaitedMs() - waitedBefore, context.phaseMs[DrawRowCapture]);
            context.phaseMs[DrawRowCapture] -= waited;
            context.phaseMs[DrawRowCaptureHookWaits] += waited;
        }
        if (context.dumpTarget != 0) {

        const auto slot0 = (static_cast<std::uint64_t>(readRegister(context.queue.context, 0x390)) << 40u) | (static_cast<std::uint64_t>(readRegister(context.queue.context, 0x318)) << 8u);
        if ((context.graphics.hasColorTarget && context.graphics.color.address == context.dumpTarget) || slot0 == context.dumpTarget || program.binary.codeAddress == context.dumpTarget) static_cast<void>(dumpRequest(program.binary.codeAddress, request));
        }
        if (context.dumpSlot1 != 0) {
        const auto value = [&](std::uint32_t offset) -> std::uint64_t { const auto it = context.queue.context.find(offset); return it == context.queue.context.end() ? 0u : it->second; };
        const auto slot1 = (value(0x391) << 40u) | (value(0x327) << 8u);
        if (slot1 == context.dumpSlot1) {
            static_cast<void>(dumpRequest(program.binary.codeAddress, request));
            if (std::FILE* file = std::fopen("draw_slot1.regs", "w")) {
                for (const auto& [offset, value] : context.queue.context) std::fprintf(file, "context %x %08x\n", offset, value);
                for (const auto& [offset, value] : context.queue.userConfig) std::fprintf(file, "uconfig %x %08x\n", offset, value);
                for (const auto& [offset, value] : context.queue.shader) std::fprintf(file, "shader %x %08x\n", offset, value);
                std::fclose(file);
            }
        }
        }

        context.phaseTiming.Phase(DrawRowCapture);

    timing.Mark("capture_resources");
    stageCapture.heapWords.reset();
    if (request.target.gpuResolvedImages) {
        auto words = std::make_shared<ShaderRecompiler::HeapStageWords>();
        if (auto shared = invocation.MaterializeHeap(*capture, *words)) {
            stageCapture.compiled = std::move(shared);
            stageCapture.heapWords = std::move(words);
            stageCapture.direct = true;
            stageCapture.ring = true;
            if (SampleHeapStageVerify()) NoteHeapDirectVerify(*PatchedHeapResult(*stageCapture.compiled, *stageCapture.heapWords), *invocation.Materialize(*capture));
        }
    }
    if (stageCapture.heapWords == nullptr) {
        try {
            stageCapture.compiled = invocation.Materialize(*capture);
        } catch (const std::exception& error) {
            TraceMaterializationFailure(context.traceToken, context.submission, context.packetOffset, i, "capture-or-materialize", std::current_exception());
            throw stageFailure(program, request, error, request.context.memory);
        }
    }
    if (stageCapture.compiled->gpuResolvedImages && !HeapDrawStageMappable(*stageCapture.compiled, program.binary.stage)) return unmapped();
    stageCapture.capture = capture;
    timing.Mark("materialize");
    const auto walked = capture->userDataUse.asValue | capture->userDataUse.asBase;
    stageCapture.userDataMask = walked == 0 ? ~std::uint64_t{0} : UserDataMask(program, context.vertexInfos[i], walked, *stageCapture.compiled);
    context.phaseTiming.Phase(DrawRowRecompile);
    return stageCapture.compiled;
} catch (const std::exception& error) {
    if (!SkipRefusedDraws()) throw;
    throw RefusedDraw(context.programs[i].binary.codeAddress, error.what());
}



void Driver::recordStageDataPositions(DispatchVariant& variant, const ShaderRecompiler::ResourceCapture& capture, const std::vector<Graphics::DecodeRead>& decodeReads) {
    const auto& compiled = *variant.compiled;
    for (std::size_t b = 0; b < compiled.bindings.size(); ++b) {
        if (compiled.bindings[b].role != ShaderRecompiler::DescriptorRole::FlattenedSrt) continue;
        variant.flatBinding = static_cast<std::uint32_t>(b);
        static_cast<void>(DataWordPositions(variant.runs, capture.readTrace.leaves, capture.readTrace.otherReads, variant.words, variant.ringTemplate != nullptr && !variant.ringTemplate->flattenedSrt.empty() ? variant.ringTemplate->flattenedSrt : compiled.bindings[b].guestDescriptor, variant.dataPositions, variant.dataSlots));
        break;
    }
    if (variant.dataPositions.empty() || decodeReads.empty()) return;
    std::vector<bool> decoded(variant.words.size(), false);
    std::size_t offset = 0;
    for (const auto& [begin, end] : variant.runs) {
        for (const auto& read : decodeReads) {
            const auto from = std::max(begin, read.address);
            const auto to = std::min(end, read.address + read.size);
            if (from >= to) continue;
            for (auto word = (from - begin) / sizeof(std::uint32_t); word < (to - begin + sizeof(std::uint32_t) - 1) / sizeof(std::uint32_t); ++word) decoded[offset + static_cast<std::size_t>(word)] = true;
        }
        offset += static_cast<std::size_t>((end - begin) / sizeof(std::uint32_t));
    }
    std::size_t kept = 0;
    for (std::size_t k = 0; k < variant.dataPositions.size(); ++k) {
        if (decoded[variant.dataPositions[k]]) continue;
        variant.dataPositions[kept] = variant.dataPositions[k];
        variant.dataSlots[kept] = variant.dataSlots[k];
        ++kept;
    }
    variant.dataPositions.resize(kept);
    variant.dataSlots.resize(kept);
}

void Driver::cacheDrawStages(bool useDrawEntries, bool drawHit, const Pm4::DrawParameters& drawParameters, const std::optional<Graphics::IndirectDrawPath>& indirectCpu, const std::vector<DrawProgram>& programs, const std::vector<StageCapture>& stageCaptures, const std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>>& vertexInfos, const std::vector<std::vector<Graphics::DecodeRead>>& decodeReads, bool verifyHit, const std::vector<std::shared_ptr<DispatchVariant>>& matched, const std::vector<StageDataHit>& stageData, std::vector<std::shared_ptr<DispatchVariant>>& fresh, std::uint64_t drawKey, bool registerKey, const std::shared_ptr<const DrawDecode>& decode, DrawPhaseTiming& phaseTiming, const std::vector<bool>& reused, const std::shared_ptr<DrawEntry>& entry, std::uint64_t shapeKey, bool shapeNeedsEntry) {
    const bool cacheable = useDrawEntries && !drawHit && (drawCacheIndirect() || !(drawParameters.indirect && indirectCpu));
    const bool admitted = cacheable && (verifyHit || admitDrawKey(drawKey, registerKey && drawDataHits() ? shapeKey : 0));
    const bool replaceShape = cacheable && !admitted && shapeNeedsEntry;
    if (admitted || replaceShape) {
        phaseTiming.Phase(DrawRowVectors);
        std::uint64_t unstable = 0, mismatches = 0, differingSameRuns = 0, differingWords = 0, differingRunsChanged = 0;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            const auto& stageCapture = stageCaptures[i];
            if (reused[i]) {
                fresh[i] = matched[i];
                continue;
            }
            if (stageCapture.compiled == nullptr || !stageCapture.Cacheable() || !CacheableResult(*stageCapture.compiled)) continue;
            auto variant = std::make_shared<DispatchVariant>();
            variant->compiled = stageCapture.compiled;
            if (programs[i].binary.stage == ShaderRecompiler::ShaderStage::Mesh) {
                std::copy_n(programs[i].userData.begin() + ShaderRecompiler::MeshIndexBufferUserWord, variant->meshIndexDescriptor.size(), variant->meshIndexDescriptor.begin());
            }
            variant->shader = programs[i].snapshot;
            variant->forgetSerial = stageCapture.forgetSerial;
            variant->pushOffset = stageCapture.pushOffset;
            variant->userDataMask = stageCapture.userDataMask;
            if (stageCapture.ring && stageCapture.heapWords != nullptr) {
                auto ringTemplate = std::make_shared<ShaderRecompiler::HeapStageWords>();
                ringTemplate->vertexAttributes = stageCapture.heapWords->vertexAttributes;
                ringTemplate->flattenedSrt = stageCapture.heapWords->flattenedSrt;
                ringTemplate->bindings = stageCapture.heapWords->bindings;
                variant->ringTemplate = std::move(ringTemplate);
            }
            if (vertexInfos[i]) variant->vertexInfo = std::make_shared<const ShaderRecompiler::ShaderVertexStageInfo>(*vertexInfos[i]);

            std::vector<ShaderRecompiler::MemoryRegion> regions(stageCapture.regions.begin(), stageCapture.regions.end());
            for (const auto& read : decodeReads[i]) regions.push_back({read.address, read.Bytes()});
            std::stable_sort(regions.begin(), regions.end(), [](const ShaderRecompiler::MemoryRegion& a, const ShaderRecompiler::MemoryRegion& b) { return a.guestAddress < b.guestAddress; });
            const bool relocatable = decodeReads[i].empty() && (!vertexInfos[i] || vertexInfos[i]->resourcesNum == 0);
            tagVariantRuns(*variant, regions, stageCapture.userDataUse, relocatable, programs[i].hiddenUserWords, stageCapture.supplied);
            if (relocatable) tagUserValueWords(*variant, programs[i]);
            if (drawStageDataHits() && stageCapture.capture != nullptr && !stageCapture.capture->readTrace.leaves.empty()) recordStageDataPositions(*variant, *stageCapture.capture, decodeReads[i]);
            if (traceDrawCache() && entry != nullptr && matched[i] == nullptr && i < entry->stages.size()) {
                const auto& variants = entry->stages[i];
                const auto front = std::find_if(variants.begin(), variants.end(), [&](const std::shared_ptr<DispatchVariant>& kept) { return kept->pushOffset == variant->pushOffset; });
                if (front != variants.end() && (*front)->runs == variant->runs && (*front)->words.size() == variant->words.size()) {
                    ++differingSameRuns;
                    for (std::size_t w = 0; w < variant->words.size(); ++w) differingWords += (*front)->words[w] != variant->words[w] ? 1u : 0u;
                } else if (front != variants.end()) {
                    ++differingRunsChanged;
                }
            }
            const auto sameContent = [](const DispatchVariant& a, const std::vector<std::uint32_t>& words, const DispatchVariant& b) {
                const auto flatten = [](const DispatchVariant& source, const std::vector<std::uint32_t>& values) {
                    std::vector<std::pair<std::uint64_t, std::uint32_t>> flat;
                    std::size_t word = 0;
                    for (const auto& [begin, end] : source.runs) {
                        for (auto address = begin; address < end && word < values.size(); address += sizeof(std::uint32_t)) flat.emplace_back(address, values[word++]);
                    }
                    std::sort(flat.begin(), flat.end());
                    return flat;
                };
                return (a.runs == b.runs && words == b.words) || flatten(a, words) == flatten(b, b.words);
            };
            const auto sameForwardedWords = [](const ShaderRecompiler::RecompileResult& a, const ShaderRecompiler::RecompileResult& b, std::uint32_t hidden) {
                if (a.pushConstants.size() != b.pushConstants.size() || a.shaderDataUserWords != b.shaderDataUserWords) return false;
                for (std::size_t slot = 0; slot < a.shaderDataUserWords.size() && (slot + 1) * sizeof(std::uint32_t) <= a.pushConstants.size(); ++slot) {
                    if (a.shaderDataUserWords[slot] >= hidden && std::memcmp(a.pushConstants.data() + slot * sizeof(std::uint32_t), b.pushConstants.data() + slot * sizeof(std::uint32_t), sizeof(std::uint32_t)) != 0) return false;
                }
                return true;
            };
            if (verifyHit && matched[i] != nullptr) {
                const auto& words = stageData[i].compiled != nullptr ? stageData[i].words : matched[i]->words;
                const auto& compiled = stageData[i].compiled != nullptr ? *stageData[i].compiled : *matched[i]->compiled;
                if (!sameContent(*matched[i], words, *variant) || !sameForwardedWords(compiled, *variant->compiled, programs[i].hiddenUserWords)) {
                    ++mismatches;
                    static std::atomic<std::uint64_t> reports{0};
                    if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: stage %zu (program 0x%llx) of a hit captured differently: %zu runs / %zu words matched, %zu / %zu fresh\n", i, static_cast<unsigned long long>(programs[i].binary.codeAddress), matched[i]->runs.size(), words.size(), variant->runs.size(), variant->words.size());
                }
            }
            if (insertCompare()) {
                const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DrawCache);
                if (!captureStable(stageCapture.regions)) {
                    ++unstable;
                    continue;
                }
            }
            fresh[i] = std::move(variant);
        }
        insertDrawEntry(drawKey, fresh, registerKey ? decode : nullptr, registerKey && drawDataHits() ? shapeKey : 0, replaceShape);
        std::uint64_t dataInserts = 0, dataPositions = 0;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (fresh[i] == nullptr || reused[i] || fresh[i]->dataPositions.empty()) continue;
            ++dataInserts;
            dataPositions += fresh[i]->dataPositions.size();
        }
        if (unstable != 0 || mismatches != 0 || differingSameRuns != 0 || differingRunsChanged != 0 || dataInserts != 0) {
            std::lock_guard cacheLock(drawCacheMutex);
            drawEntryCounters.stageDataInserts += dataInserts;
            drawEntryCounters.stageDataPositions += dataPositions;
            drawEntryCounters.unstable += unstable;
            drawEntryCounters.verifyMismatches += mismatches;
            drawEntryCounters.differingSameRuns += differingSameRuns;
            drawEntryCounters.differingWords += differingWords;
            drawEntryCounters.differingRunsChanged += differingRunsChanged;
        }
        phaseTiming.Phase(DrawRowKeyLookupValidate);
    }
}

}
