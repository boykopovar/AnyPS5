#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace AgcDriver::DriverDetail {

bool SameCompilation(const ShaderRecompiler::RecompileResult& a, const ShaderRecompiler::RecompileResult& b) {
    return static_cast<const ShaderRecompiler::CompiledShaderArtifact&>(a) == static_cast<const ShaderRecompiler::CompiledShaderArtifact&>(b) && static_cast<const ShaderRecompiler::ShaderInvocation&>(a) == static_cast<const ShaderRecompiler::ShaderInvocation&>(b) && a.workgroupMemoryDwords == b.workgroupMemoryDwords;
}

ShaderRecompiler::RecompileRequest DrawStageRequest(const DrawProgram& program, const Graphics::State& graphics, const ShaderRecompiler::ShaderPixelStageInfo& pixel, const std::optional<ShaderRecompiler::ShaderVertexStageInfo>& vertexInfo, std::span<const ShaderRecompiler::MemoryRegion> memory, std::span<const ShaderRecompiler::LinkedProgram> linked, const Pm4::DrawParameters& drawParameters, const ShaderRecompiler::SpirvTarget& target, std::uint32_t pushOffset) {
    using Stage = ShaderRecompiler::ShaderStage;
    const auto waveSize = program.binary.stage == Stage::Fragment ? graphics.stages.fragmentWaveSize : graphics.stages.vertexWaveSize;
    return {
        program.binary,
        {waveSize, program.firstUserSgpr, program.userData, std::nullopt, program.binary.stage == Stage::Fragment ? std::optional(pixel) : std::nullopt, vertexInfo, memory, RegisteredFloatMode(*program.snapshot)},
        target,
        {0, 0, pushOffset, (graphics.stages.mesh ? ShaderRecompiler::MeshDrawPushOffsetBytes : graphics.stages.tessellation && pushOffset < Graphics::PipelinePushSlotBytes ? ShaderRecompiler::TessellationLevelPushOffsetBytes : Graphics::PipelinePushSlotBytes) - pushOffset % Graphics::PipelinePushSlotBytes},
        ShaderRecompiler::GraphicsCompileContext{program.firstUserSgpr, linked, graphics.stages.mesh, graphics.stages.tessellation, {drawParameters.indexAddress, drawParameters.indexCount, drawParameters.indexSize, drawParameters.instanceCount}}
    };
}

std::shared_ptr<DispatchVariant> DrawStageVariant(const DrawProgram& program, const StageCapture& capture, const std::optional<ShaderRecompiler::ShaderVertexStageInfo>& vertexInfo, std::span<const Graphics::DecodeRead> decodeReads) {
    auto variant = std::make_shared<DispatchVariant>();
    variant->compiled = capture.compiled;
    if (program.binary.stage == ShaderRecompiler::ShaderStage::Mesh) {
        std::copy_n(program.userData.begin() + ShaderRecompiler::MeshIndexBufferUserWord, variant->meshIndexDescriptor.size(), variant->meshIndexDescriptor.begin());
    }
    variant->shader = program.snapshot;
    variant->forgetSerial = capture.forgetSerial;
    variant->pushOffset = capture.pushOffset;
    if (vertexInfo) variant->vertexInfo = std::make_shared<const ShaderRecompiler::ShaderVertexStageInfo>(*vertexInfo);
    std::vector<ShaderRecompiler::MemoryRegion> regions(capture.regions.begin(), capture.regions.end());
    for (const auto& read : decodeReads) regions.push_back({read.address, std::as_bytes(std::span(read.bytes))});
    std::stable_sort(regions.begin(), regions.end(), [](const ShaderRecompiler::MemoryRegion& a, const ShaderRecompiler::MemoryRegion& b) { return a.guestAddress < b.guestAddress; });
    for (const auto& region : regions) {
        variant->runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
        const auto count = region.bytes.size() / sizeof(std::uint32_t);
        const auto offset = variant->words.size();
        variant->words.resize(offset + count);
        std::memcpy(variant->words.data() + offset, region.bytes.data(), count * sizeof(std::uint32_t));
    }
    return variant;
}

std::shared_ptr<const ShaderRecompiler::RecompileResult> Driver::materializeDrawStage(std::size_t i, std::uint32_t pushOffset, const QueueState& queue, const Submission& submission, const std::vector<DrawProgram>& programs, const Graphics::State& graphics, const ShaderRecompiler::ShaderPixelStageInfo& pixel, const std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>>& vertexInfos, std::vector<ShaderRecompiler::MemoryRegion>& memory, const std::vector<ShaderRecompiler::LinkedProgram>& linked, const Pm4::DrawParameters& drawParameters, const std::shared_ptr<VulkanDevice>& localDevice, ShaderMemory& shaderMemory, std::vector<StageCapture>& stageCaptures, std::vector<bool>& recompiled, bool drawHit, const std::vector<std::shared_ptr<DispatchVariant>>& matched, const std::vector<std::vector<ShaderRecompiler::MemoryRegion>>& matchedRegions, bool profile, std::uint64_t dumpTarget, std::uint64_t dumpSlot1, std::uint64_t& captures, DrawPhaseTiming& phaseTiming, std::array<double, DrawDriverPhaseCount>& phaseMs, std::string& rejected) {
    PerformanceTimer timing("Shader.DrawStage");
    phaseTiming.Phase(DrawRowVectors);
    const auto& program = programs[i];
    auto request = DrawStageRequest(program, graphics, pixel, vertexInfos[i], memory, linked, drawParameters, localDevice->Target(), pushOffset);
    const auto waitedBefore = traceCapSync() || profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
    const auto invocation = InvocationFor(*program.snapshot, program.codeOffset, request);
    timing.Mark("prepared_invocation");
    auto& stageCapture = stageCaptures[i];
    stageCapture.forgetSerial = GuestMemory::ForgetSerial();
    stageCapture.pushOffset = pushOffset;
    const auto named = [&](const std::exception& error) {
        static const bool dumpShaders = std::getenv("APS5_DUMP_SHADERS") != nullptr;
        std::string dump;
        if (dumpShaders) {
            try {
                dump = dumpRequest(program.binary.codeAddress, request);
            } catch (...) {}
        }
        std::ostringstream where;
        where << "shader 0x" << std::hex << program.binary.codeAddress << " stage " << std::dec << static_cast<unsigned>(program.binary.stage);
        if (!dump.empty()) where << " (" << dump << ")";
        where << ": ";
        return std::runtime_error(where.str() + error.what());
    };
    const auto capture = [&] {
        const SampledReadScope sampling(evidenceReads);
        try {
            return shaderMemory.Capture(invocation);
        } catch (const std::exception& error) {
            throw named(error);
        }
    }();

    stageCapture.regions = shaderMemory.TakeRecentRegions();
    recompiled[i] = true;
    memory = shaderMemory.Regions();

    for (std::size_t j = 0; j < programs.size(); ++j) {
        if (matched[j] != nullptr && !recompiled[j] && (drawHit || j < i)) memory.insert(memory.end(), matchedRegions[j].begin(), matchedRegions[j].end());
    }
    request.context.memory = memory;
    if (traceCapSync()) traceCapture("draw-capture", program.binary.codeAddress, submission.queue, memory, Graphics::Recorder::ThreadWaitedMs() - waitedBefore);
    if (profile) {
        ++captures;
        phaseTiming.Phase(DrawRowCapture);

        const auto waited = std::min(Graphics::Recorder::ThreadWaitedMs() - waitedBefore, phaseMs[DrawRowCapture]);
        phaseMs[DrawRowCapture] -= waited;
        phaseMs[DrawRowCaptureHookWaits] += waited;
    }
    if (dumpTarget != 0) {

        const auto slot0 = (static_cast<std::uint64_t>(readRegister(queue.context, 0x390)) << 40u) | (static_cast<std::uint64_t>(readRegister(queue.context, 0x318)) << 8u);
        if ((graphics.hasColorTarget && graphics.color.address == dumpTarget) || slot0 == dumpTarget) static_cast<void>(dumpRequest(program.binary.codeAddress, request));
    }
    if (dumpSlot1 != 0) {
        const auto value = [&](std::uint32_t offset) -> std::uint64_t { const auto it = queue.context.find(offset); return it == queue.context.end() ? 0u : it->second; };
        const auto slot1 = (value(0x391) << 40u) | (value(0x327) << 8u);
        if (slot1 == dumpSlot1) {
            static_cast<void>(dumpRequest(program.binary.codeAddress, request));
            if (std::FILE* file = std::fopen("draw_slot1.regs", "w")) {
                for (const auto& [offset, value] : queue.context) std::fprintf(file, "context %x %08x\n", offset, value);
                for (const auto& [offset, value] : queue.userConfig) std::fprintf(file, "uconfig %x %08x\n", offset, value);
                for (const auto& [offset, value] : queue.shader) std::fprintf(file, "shader %x %08x\n", offset, value);
                std::fclose(file);
            }
        }
    }

    phaseTiming.Phase(DrawRowCapture);

    timing.Mark("capture_resources");
    try {
        stageCapture.compiled = invocation.Materialize(*capture);
    } catch (const std::exception& error) {
        throw named(error);
    }
    stageCapture.capture = capture;
    timing.Mark("materialize");
    phaseTiming.Phase(DrawRowRecompile);
    return stageCapture.compiled;
}

void Driver::recordStageDataPositions(DispatchVariant& variant, const ShaderRecompiler::ResourceCapture& capture, const std::vector<Graphics::DecodeRead>& decodeReads) {
    const auto& compiled = *variant.compiled;
    for (std::size_t b = 0; b < compiled.bindings.size(); ++b) {
        if (compiled.bindings[b].role != ShaderRecompiler::DescriptorRole::FlattenedSrt) continue;
        variant.flatBinding = static_cast<std::uint32_t>(b);
        static_cast<void>(DataWordPositions(variant.runs, capture.readTrace.leaves, capture.readTrace.otherReads, variant.words, compiled.bindings[b].guestDescriptor, variant.dataPositions, variant.dataSlots));
        break;
    }
    if (variant.dataPositions.empty() || decodeReads.empty()) return;
    std::vector<bool> decoded(variant.words.size(), false);
    std::size_t offset = 0;
    for (const auto& [begin, end] : variant.runs) {
        for (const auto& read : decodeReads) {
            const auto from = std::max(begin, read.address);
            const auto to = std::min(end, read.address + read.bytes.size());
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

bool Driver::compileDrawAhead(std::uint64_t key, std::uint64_t shape, const std::shared_ptr<const DrawDecode>& decode, const Pm4::DrawParameters& parameters, const std::shared_ptr<VulkanDevice>& localDevice, DrawAhead::Speculation& speculation) {
    using Stage = ShaderRecompiler::ShaderStage;
    using Role = ShaderRecompiler::ProgramRole;
    if (ShaderRecompiler::DebugProbeActive()) return false;
    const GuestMemory::SpeculativeReadScope speculative(speculation);
    const auto& graphics = decode->state;
    if (!DrawAhead::CompilesAhead(graphics, parameters)) return false;
    const auto& roles = decode->roles;
    const auto& programs = decode->programs;
    std::vector<ShaderRecompiler::MemoryRegion> memory;
    std::vector<ShaderRecompiler::LinkedProgram> linked;
    std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>> vertexInfos(programs.size());
    std::vector<std::vector<Graphics::DecodeRead>> decodeReads(programs.size());
    for (std::size_t i = 0; i < programs.size(); ++i) {
        const auto& program = programs[i];
        for (const auto& region : program.memory) {
            if (!region.bytes.empty()) memory.push_back(region);
        }
        linked.push_back({roles[i], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
        if (program.binary.stage != Stage::Fragment && roles[i] != Role::GeometryBack) vertexInfos[i] = Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, &decodeReads[i], false, 8u - program.firstUserSgpr);
        if (vertexInfos[i]) vertexInfos[i]->paClVsOutCntl = PositionExportLayout(program.snapshot.get(), program.codeOffset, program.binary.code, graphics.paClVsOutCntl, program.tailContinuation.get()) | graphics.layerExports;
    }
    ShaderMemory shaderMemory(memory);
    std::vector<StageCapture> stageCaptures(programs.size());
    std::vector<Graphics::CompiledShader> stages;
    auto pixel = decode->pixel;
    bool preparable = !graphics.rectList;
    std::uint32_t pushCursorBytes = 0;
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (roles[i] == Role::GeometryBack) continue;
        if (speculation.Passed()) return false;
        const auto& program = programs[i];
        pushCursorBytes = Graphics::StagePushOffset(pushCursorBytes, program.binary.stage, localDevice->GraphicsPipelineLibraries());
        if (program.binary.stage == Stage::Fragment) {
            for (auto previous = i; previous-- > 0;) {
                if (stageCaptures[previous].compiled == nullptr) continue;
                pixel = Graphics::DefaultUnexportedInputs(decode->pixel, stageCaptures[previous].compiled->parameterExports);
                break;
            }
        }
        const auto request = DrawStageRequest(program, graphics, pixel, vertexInfos[i], memory, linked, parameters, localDevice->Target(), pushCursorBytes);
        const auto invocation = InvocationFor(*program.snapshot, program.codeOffset, request);
        auto& stageCapture = stageCaptures[i];
        stageCapture.forgetSerial = GuestMemory::ForgetSerial();
        stageCapture.pushOffset = pushCursorBytes;
        const auto capture = shaderMemory.Capture(invocation);
        stageCapture.regions = shaderMemory.TakeRecentRegions();
        memory = shaderMemory.Regions();
        stageCapture.compiled = invocation.Materialize(*capture);
        stageCapture.capture = capture;
        const auto& result = *stageCapture.compiled;
        const auto slotEnd = (pushCursorBytes / Graphics::PipelinePushSlotBytes + 1u) * Graphics::PipelinePushSlotBytes;
        require(result.pushConstants.size() <= slotEnd - pushCursorBytes, "stage push constants exceed the pipeline push constant block");
        if (result.barycentricEmulation.active) preparable = false;
        stages.push_back({program.binary.stage, &result, result.pushConstants.empty() ? 0u : pushCursorBytes});
        pushCursorBytes += static_cast<std::uint32_t>(result.pushConstants.size());
    }
    std::vector<std::shared_ptr<DispatchVariant>> fresh(programs.size());
    for (std::size_t i = 0; i < programs.size(); ++i) {
        const auto& stageCapture = stageCaptures[i];
        if (stageCapture.compiled == nullptr) continue;
        if (!CacheableResult(*stageCapture.compiled)) {
            preparable = false;
            continue;
        }
        fresh[i] = DrawStageVariant(programs[i], stageCapture, vertexInfos[i], decodeReads[i]);
        if (drawStageDataHits() && stageCapture.capture != nullptr && !stageCapture.capture->readTrace.leaves.empty()) recordStageDataPositions(*fresh[i], *stageCapture.capture, decodeReads[i]);
    }
    if (speculation.Passed() || ShaderRecompiler::DebugProbeActive()) return false;
    std::shared_ptr<const Graphics::PreparedDraw> prepared;
    if (preparable) {
        try {
            prepared = localDevice->PrepareDraw(graphics, stages);
        } catch (...) {
            prepared = nullptr;
        }
    }
    auto candidate = BuildAheadEntry(key, fresh, decode, drawDataHits() ? shape : 0, std::move(prepared), stages.size());
    std::vector<std::shared_ptr<DrawEntry>> released;
    bool inserted = false;
    return speculation.Commit([&] {
        inserted = insertAheadEntry(candidate, released);
        if (inserted) drawAheadCounts.compiled.fetch_add(1, std::memory_order_relaxed);
    }) && inserted;
}

void Driver::cacheDrawStages(bool useDrawEntries, bool drawHit, const Pm4::DrawParameters& drawParameters, const std::optional<Graphics::IndirectDrawPath>& indirectCpu, const std::vector<DrawProgram>& programs, const std::vector<StageCapture>& stageCaptures, const std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>>& vertexInfos, const std::vector<std::vector<Graphics::DecodeRead>>& decodeReads, bool verifyHit, const std::vector<std::shared_ptr<DispatchVariant>>& matched, std::vector<std::shared_ptr<DispatchVariant>>& fresh, std::uint64_t drawKey, bool registerKey, const std::shared_ptr<const DrawDecode>& decode, DrawPhaseTiming& phaseTiming, const std::vector<bool>& reused, const std::shared_ptr<DrawEntry>& entry, std::uint64_t shapeKey) {
    const bool cached = useDrawEntries && !(drawParameters.indirect && indirectCpu);
    const bool admitted = cached && ((!drawHit && !verifyHit) || (entry != nullptr && entry->probation)) && admitDrawKey(drawKey, registerKey && drawDataHits() ? shapeKey : 0, drawHit || verifyHit);
    if (cached && !drawHit && (verifyHit || admitted)) {
        phaseTiming.Phase(DrawRowVectors);
        std::uint64_t unstable = 0, mismatches = 0, differingSameRuns = 0, differingWords = 0, differingRunsChanged = 0;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            const auto& stageCapture = stageCaptures[i];
            if (reused[i]) {
                fresh[i] = matched[i];
                continue;
            }
            if (stageCapture.compiled == nullptr || !CacheableResult(*stageCapture.compiled)) continue;
            auto variant = DrawStageVariant(programs[i], stageCapture, vertexInfos[i], decodeReads[i]);
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
            const bool capturedDifferently = verifyHit && matched[i] != nullptr && (matched[i]->runs != variant->runs || matched[i]->words != variant->words);
            if (verifyHit && matched[i] != nullptr && (capturedDifferently || !SameCompilation(*matched[i]->compiled, *variant->compiled))) {
                ++mismatches;
                static std::atomic<std::uint64_t> reports{0};
                if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: stage %zu (program 0x%llx) of a hit %s differently: %zu runs / %zu words matched, %zu / %zu fresh\n", i, static_cast<unsigned long long>(programs[i].binary.codeAddress), capturedDifferently ? "captured" : "compiled", matched[i]->runs.size(), matched[i]->words.size(), variant->runs.size(), variant->words.size());
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
        insertDrawEntry(drawKey, fresh, registerKey ? decode : nullptr, registerKey && drawDataHits() ? shapeKey : 0);
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
