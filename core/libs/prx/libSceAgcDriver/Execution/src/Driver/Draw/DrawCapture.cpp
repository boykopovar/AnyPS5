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
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <span>
#include <sstream>
#include <stdexcept>
#include <exception>
#include <typeinfo>

namespace AgcDriver::DriverDetail {

const ShaderRecompiler::SrtUserDataUse& StageCapture::UserDataUse() const {
    static const ShaderRecompiler::SrtUserDataUse none;
    return capture != nullptr ? capture->userDataUse : none;
}


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
    if (program.snapshot) {
        const auto& tail = program.snapshot->userDataTailRegister;
        if (tail && *tail >= program.firstUserSgpr) {
            const auto index = *tail - program.firstUserSgpr;
            mask |= index + 1u < 64u ? std::uint64_t{3} << index : ~std::uint64_t{0};
        } else if (tail) {
            return ~std::uint64_t{0};
        }
        if (StageLinksUserDataCallees(*program.snapshot, program.codeOffset)) return ~std::uint64_t{0};
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

struct PendingMaterialize {
    std::mutex mutex;
    bool finished = false;
    std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
    std::exception_ptr failure;
};

struct PendingMaterializes {
    static constexpr std::size_t Limit = 4096;
    static constexpr double CheapMs = 2.0;
    std::mutex mutex;
    std::map<std::pair<std::uint64_t, std::uint64_t>, std::shared_ptr<PendingMaterialize>> byKey;
    std::map<std::uint64_t, double> lastMs;
};

double ElapsedMs(std::chrono::steady_clock::time_point since) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - since).count();
}

PendingMaterializes& pendingMaterializes() {
    static auto* pending = new PendingMaterializes();
    return *pending;
}

}

namespace {

using MaterializeWork = std::function<std::shared_ptr<const ShaderRecompiler::RecompileResult>()>;

std::atomic<std::uint64_t> heapTemplatesInBackground{0};

bool cheapAt(std::uint64_t address, bool contended) {
    auto& pending = pendingMaterializes();
    std::lock_guard lock(pending.mutex);
    const auto found = pending.lastMs.find(address);
    return !contended && found != pending.lastMs.end() && found->second < PendingMaterializes::CheapMs;
}

std::shared_ptr<const ShaderRecompiler::RecompileResult> timedAt(std::uint64_t address, const MaterializeWork& work) {
    const auto started = std::chrono::steady_clock::now();
    auto result = work();
    const auto ms = ElapsedMs(started);
    auto& pending = pendingMaterializes();
    std::lock_guard lock(pending.mutex);
    pending.lastMs[address] = ms;
    return result;
}

std::shared_ptr<const ShaderRecompiler::RecompileResult> finishedInBackground(std::uint64_t address, std::uint64_t snapshotKey, const std::function<MaterializeWork()>& makeWork, bool* created = nullptr) {
    auto& pending = pendingMaterializes();
    const std::pair key{address, snapshotKey};
    std::shared_ptr<PendingMaterialize> job;
    bool started = false;
    {
        std::lock_guard lock(pending.mutex);
        if (pending.byKey.size() >= PendingMaterializes::Limit) {
            std::erase_if(pending.byKey, [](const auto& entry) {
                std::lock_guard entryLock(entry.second->mutex);
                return entry.second->finished;
            });
        }
        auto& slot = pending.byKey[key];
        if (slot == nullptr) {
            slot = std::make_shared<PendingMaterialize>();
            started = true;
        }
        job = slot;
    }
    if (created != nullptr) *created = started;
    if (started) {
        SubmitLibraryPrebuild([job, work = makeWork(), address] {
            std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
            std::exception_ptr failure;
            const auto started = std::chrono::steady_clock::now();
            try {
                result = work();
            } catch (...) {
                failure = std::current_exception();
            }
            {
                const auto ms = ElapsedMs(started);
                auto& registry = pendingMaterializes();
                std::lock_guard lock(registry.mutex);
                registry.lastMs[address] = ms;
            }
            {
                std::lock_guard lock(job->mutex);
                job->result = std::move(result);
                job->failure = failure;
                job->finished = true;
            }
        });
    }
    {
        std::lock_guard lock(job->mutex);
        if (!job->finished) throw DrawCompilePending("the draw's shader is still being specialized in the background");
    }
    {
        std::lock_guard lock(pending.mutex);
        if (const auto found = pending.byKey.find(key); found != pending.byKey.end() && found->second == job) pending.byKey.erase(found);
    }
    if (job->failure != nullptr) std::rethrow_exception(job->failure);
    return job->result;
}

template <typename Run>
std::function<MaterializeWork()> reboundWork(const ShaderRecompiler::PreparedShaderInvocation& invocation, ShaderRecompiler::MemoryRegionView memory, Run run) {
    return [&invocation, memory, run] {
        std::shared_ptr<const OwnedRecompileRequest> owned = OwnRecompileRequest(invocation.Request(), memory);
        auto rebound = std::make_shared<const ShaderRecompiler::PreparedShaderInvocation>(invocation.Rebound(owned->request));
        return MaterializeWork([owned, rebound, run] { return run(*rebound); });
    };
}

void dropHeapReadRegions(std::vector<ShaderRecompiler::MemoryRegion>& memory, const StageCapture& stage) {
    if (stage.heapReadWords == nullptr || stage.heapReadWords->empty()) return;
    const auto* first = reinterpret_cast<const std::byte*>(stage.heapReadWords->data());
    const auto* last = first + stage.heapReadWords->size() * sizeof(std::uint32_t);
    std::erase_if(memory, [&](const ShaderRecompiler::MemoryRegion& region) { return region.bytes.data() >= first && region.bytes.data() < last; });
}

constexpr std::uint64_t HeapTemplateJob = std::uint64_t{1} << 63u;

}

std::shared_ptr<const ShaderRecompiler::RecompileResult> MaterializeInBackground(const ShaderRecompiler::PreparedShaderInvocation& invocation, const std::shared_ptr<const ShaderRecompiler::ResourceCapture>& capture, ShaderRecompiler::MemoryRegionView memory, std::uint64_t address) {
    if (!AsyncDrawCompile()) return invocation.Materialize(*capture);
    bool contended = false;
    if (auto memoized = invocation.MaterializeIfMemoized(*capture, &contended)) return memoized;
    if (cheapAt(address, contended)) return timedAt(address, [&] { return invocation.Materialize(*capture); });
    return finishedInBackground(address, invocation.SnapshotKey(*capture), reboundWork(invocation, memory, [capture](const ShaderRecompiler::PreparedShaderInvocation& rebound) { return rebound.Materialize(*capture); }));
}

std::uint64_t HeapTemplatesBuiltInBackground() {
    return heapTemplatesInBackground.load(std::memory_order_relaxed);
}

std::shared_ptr<const ShaderRecompiler::RecompileResult> MaterializeHeapInBackground(const ShaderRecompiler::PreparedShaderInvocation& invocation, const std::shared_ptr<const ShaderRecompiler::ResourceCapture>& capture, ShaderRecompiler::HeapStageWords& words, ShaderRecompiler::MemoryRegionView memory, std::uint64_t address) {
    if (!AsyncDrawCompile()) return invocation.MaterializeHeap(*capture, words);
    ShaderRecompiler::HeapTemplateMiss miss;
    if (auto hit = invocation.MaterializeHeap(*capture, words, &miss); hit != nullptr || !miss.missing) return hit;
    const auto job = address | HeapTemplateJob;
    if (cheapAt(job, false)) return timedAt(job, [&] { return invocation.MaterializeHeap(*capture, words); });
    bool created = false;
    try {
        static_cast<void>(finishedInBackground(job, miss.key, reboundWork(invocation, memory, [capture](const ShaderRecompiler::PreparedShaderInvocation& rebound) {
            ShaderRecompiler::HeapStageWords built;
            return rebound.MaterializeHeap(*capture, built);
        }), &created));
    } catch (const DrawCompilePending&) {
        if (created) heapTemplatesInBackground.fetch_add(1, std::memory_order_relaxed);
        throw;
    }
    return invocation.MaterializeHeap(*capture, words);
}

bool Driver::materializeHeapDirect(const DrawStageContext& context, std::size_t i, const ShaderRecompiler::PreparedShaderInvocation& invocation, const ShaderRecompiler::RecompileRequest& request, PerformanceTimer& timing) {
    const auto& program = context.programs[i];
    auto& stageCapture = context.stageCaptures[i];
    HeapStageMaterialized materialized;
    try {
        materialized = MaterializeHeapStage(invocation, program.userData, request.context.memory, program.binary.codeAddress);
    } catch (const DrawCompilePending&) {
        throw;
    } catch (const std::exception& error) {
        throw stageFailure(program, request, error, request.context.memory);
    }
    if (materialized.result == nullptr) return false;
    RequireHeapStageMappable(*materialized.result);
    NoteHeapDirect();
    stageCapture.compiled = std::move(materialized.result);
    stageCapture.heapWords = stageCapture.compiled->heapTemplateId != 0 ? std::move(materialized.words) : nullptr;
    if (SampleHeapStageVerify()) {
        const auto fullCapture = context.shaderMemory.Capture(invocation);
        const auto expected = invocation.Materialize(*fullCapture);
        static_cast<void>(context.shaderMemory.TakeRecentRegions());
        NoteHeapDirectVerify(stageCapture.heapWords != nullptr ? *PatchedHeapResult(*stageCapture.compiled, *stageCapture.heapWords) : *stageCapture.compiled, *expected, fullCapture.get());
    }
    stageCapture.regions.clear();
    stageCapture.origin = StageOrigin::HeapUnread;
    auto& reads = materialized.reads;
    if (reads.complete) {
        auto owned = std::move(reads.values);
        std::size_t cursor = 0;
        for (const auto& [begin, end] : *reads.ranges) {
            const auto count = static_cast<std::size_t>((end - begin) / sizeof(std::uint32_t));
            stageCapture.regions.push_back({begin, std::as_bytes(std::span(owned->data() + cursor, count))});
            cursor += count;
        }
        stageCapture.heapReadWords = std::move(owned);
        const auto& use = reads.capture->userDataUse;
        stageCapture.userDataMask = UserDataMask(program, context.vertexInfos[i], use.asValue | use.asBase, *stageCapture.compiled);
        stageCapture.capture = std::move(reads.capture);
        stageCapture.origin = StageOrigin::HeapRead;
        context.memory.insert(context.memory.end(), stageCapture.regions.begin(), stageCapture.regions.end());
    }
    context.recompiled[i] = true;
    timing.Mark("heap_direct");
    context.phaseTiming.Phase(DrawRowRecompile);
    return true;
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

ShaderRecompiler::RecompileRequest Driver::drawStageRequest(const DrawStageContext& context, std::size_t i, std::uint32_t pushOffset, const Pm4::DrawParameters& drawParameters) {
    using Stage = ShaderRecompiler::ShaderStage;
    const auto& program = context.programs[i];
    const auto waveSize = Graphics::StageWaveSize(context.graphics.stages, program.binary.stage);
    const bool fixedSlots = context.localDevice->GraphicsPipelineLibraries();
    require(pushOffset <= Graphics::PushBlockBytes(fixedSlots), "stage push constant offset exceeds the pipeline push constant block");
    const bool hasFragment = std::ranges::any_of(context.programs, [](const DrawProgram& value) { return value.binary.stage == Stage::Fragment; });
    const auto pushBytes = Graphics::StagePushBudget(Graphics::StagePushLimit(context.graphics.stages, pushOffset, context.graphics.depthRemap.mode != 0u), pushOffset, program.binary.stage, fixedSlots, hasFragment);
    ShaderRecompiler::RecompileRequest request{
        program.binary,
        {waveSize, program.firstUserSgpr, program.userData, std::nullopt, program.binary.stage == Stage::Fragment ? std::optional(context.pixel) : std::nullopt, context.vertexInfos[i], context.memory, RegisteredFloatMode(*program.snapshot)},
        context.localDevice->Target(),
        {0, 0, pushOffset, pushBytes},
        ShaderRecompiler::GraphicsCompileContext{program.firstUserSgpr, context.linked, Graphics::StageMeshConfiguration(context.graphics.stages, program.binary.stage), context.graphics.stages.tessellation, {drawParameters.indexAddress, drawParameters.indexCount, drawParameters.indexSize, drawParameters.instanceCount}, program.binary.stage == Stage::Mesh ? context.meshBarycentrics : std::nullopt}
    };
    return request;
}

bool Driver::drawStageExcluded(const DrawStageContext& context, std::size_t i, std::uint32_t pushOffset, const Pm4::DrawParameters& drawParameters) {
    const auto& program = context.programs[i];
    auto request = drawStageRequest(context, i, pushOffset, drawParameters);
    request.target.gpuResolvedImages = true;
    const auto invocation = InvocationFor(*program.snapshot, program.codeOffset, request);
    return HeapExcludedFor(invocation);
}

std::shared_ptr<const ShaderRecompiler::RecompileResult> Driver::materializeDrawStage(const DrawStageContext& context, std::size_t i, std::uint32_t pushOffset, const Pm4::DrawParameters& drawParameters, bool heapDraw) try {
    PerformanceTimer timing("Shader.DrawStage");
    using Stage = ShaderRecompiler::ShaderStage;
    context.phaseTiming.Phase(DrawRowVectors);
    dropHeapReadRegions(context.memory, context.stageCaptures[i]);
    const auto& program = context.programs[i];
    auto request = drawStageRequest(context, i, pushOffset, drawParameters);

    const auto waitedBefore = traceCapSync() || context.profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
    if (heapDraw) request.target.gpuResolvedImages = true;
    const auto invocationFor = [&] {
        try {
            return InvocationFor(*program.snapshot, program.codeOffset, request, AsyncDrawCompile());
        } catch (const DrawCompilePending&) {
            throw;
        } catch (...) {
            TraceShaderRequestFailure(context.traceToken, context.submission, context.packetOffset, program, std::current_exception());
            throw;
        }
    };
    auto invocation = invocationFor();
    using StageReason = Graphics::HeapStageReason;
    const StageReason stageReason = heapDraw ? StageReason::Heap : !Graphics::DescriptorHeapEnabled() ? StageReason::Disabled : StageReason::Excluded;
    if (heapDraw) {
        const auto plan = invocation.PlanOrLoad();
        if (const auto* ineligible = HeapStageIneligibility(program.binary.stage, plan.get()))
            throw std::runtime_error(std::string("AGC graphics: a GPU heap stage cannot take the heap: ") + ineligible);
    }
    Graphics::NoteHeapStageReason(stageReason);
    timing.Mark("prepared_invocation");
    auto& stageCapture = context.stageCaptures[i];
    stageCapture.origin = StageOrigin::Captured;
    stageCapture.heapReadWords.reset();
    stageCapture.capture.reset();
    stageCapture.registeredViews.reset();
    stageCapture.supplied.clear();
    stageCapture.forgetSerial = GuestMemory::ForgetSerial();
    stageCapture.pushOffset = pushOffset;
    static const bool dumpGraphics = std::getenv("APS5_DUMP_GRAPHICS_SHADERS") != nullptr;
    const bool captured = dumpGraphics || traceCapSync() || context.dumpTarget != 0 || context.dumpSlot1 != 0;
    if (request.target.gpuResolvedImages && !captured) {
        if (materializeHeapDirect(context, i, invocation, request, timing)) return stageCapture.compiled;
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
    stageCapture.registeredViews = std::make_shared<const std::vector<ShaderRecompiler::RegisteredImageView>>(capture->snapshot.registeredImages);
    context.recompiled[i] = true;
    context.memory = context.shaderMemory.Regions();

    for (std::size_t j = 0; j < context.programs.size(); ++j) {
        if (context.matched[j] != nullptr && !context.recompiled[j] && (context.drawHit || j < i)) context.memory.insert(context.memory.end(), context.matchedRegions[j].begin(), context.matchedRegions[j].end());
        if (j < i && context.recompiled[j] && context.stageCaptures[j].Direct()) context.memory.insert(context.memory.end(), context.stageCaptures[j].regions.begin(), context.stageCaptures[j].regions.end());
    }
    request.context.memory = context.memory;
    if (dumpGraphics) static_cast<void>(dumpRequest(program.binary.codeAddress, request));
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
        if (auto shared = MaterializeHeapInBackground(invocation, capture, *words, request.context.memory, program.binary.codeAddress)) {
            words->dataOrigin = NextHeapDataOrigin();
            stageCapture.compiled = std::move(shared);
            stageCapture.heapWords = std::move(words);
            stageCapture.origin = StageOrigin::HeapRead;
            if (SampleHeapStageVerify()) NoteHeapDirectVerify(*PatchedHeapResult(*stageCapture.compiled, *stageCapture.heapWords), *invocation.Materialize(*capture));
        }
    }
    if (stageCapture.heapWords == nullptr) {
        try {
            stageCapture.compiled = MaterializeInBackground(invocation, capture, request.context.memory, program.binary.codeAddress);
        } catch (const DrawCompilePending&) {
            throw;
        } catch (const std::exception& error) {
            TraceMaterializationFailure(context.traceToken, context.submission, context.packetOffset, i, "capture-or-materialize", std::current_exception());
            throw stageFailure(program, request, error, request.context.memory);
        }
    }
    if (stageCapture.compiled->gpuResolvedImages) RequireHeapStageMappable(*stageCapture.compiled);
    stageCapture.capture = capture;
    timing.Mark("materialize");
    const auto walked = capture->userDataUse.asValue | capture->userDataUse.asBase;
    stageCapture.userDataMask = capture->plan == nullptr || !capture->plan->srtPlanComplete ? ~std::uint64_t{0} : UserDataMask(program, context.vertexInfos[i], walked, *stageCapture.compiled);
    context.phaseTiming.Phase(DrawRowRecompile);
    return stageCapture.compiled;
} catch (const DrawCompilePending&) {
    throw;
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
            variant->registeredViews = stageCapture.registeredViews;
            if (programs[i].binary.stage == ShaderRecompiler::ShaderStage::Mesh) {
                std::copy_n(programs[i].userData.begin() + ShaderRecompiler::MeshIndexBufferUserWord, variant->meshIndexDescriptor.size(), variant->meshIndexDescriptor.begin());
            }
            variant->shader = programs[i].snapshot;
            variant->forgetSerial = stageCapture.forgetSerial;
            variant->pushOffset = stageCapture.pushOffset;
            variant->userDataMask = stageCapture.userDataMask;
            if (stageCapture.origin == StageOrigin::HeapRead && stageCapture.heapWords != nullptr) {
                auto ringTemplate = std::make_shared<ShaderRecompiler::HeapStageWords>();
                ringTemplate->vertexAttributes = stageCapture.heapWords->vertexAttributes;
                ringTemplate->flattenedSrt = stageCapture.heapWords->flattenedSrt;
                ringTemplate->bindings = stageCapture.heapWords->bindings;
                if (const auto& base = stageCapture.heapWords->base; base != nullptr && !base->bindings.empty()) {
                    const auto& program = stageCapture.compiled->bindings;
                    ringTemplate->bindings.resize(program.size());
                    for (std::size_t index = 0; index < program.size(); ++index) {
                        if (ringTemplate->bindings[index].empty()) ringTemplate->bindings[index] = Graphics::DrawGuestWords(*stageCapture.compiled, stageCapture.heapWords.get(), program[index]);
                        if (ringTemplate->bindings[index] == program[index].guestDescriptor) ringTemplate->bindings[index].clear();
                    }
                }
                ringTemplate->dataOrigin = NextHeapDataOrigin();
                variant->ringTemplate = std::move(ringTemplate);
            }
            if (vertexInfos[i]) variant->vertexInfo = std::make_shared<const ShaderRecompiler::ShaderVertexStageInfo>(*vertexInfos[i]);

            std::vector<ShaderRecompiler::MemoryRegion> regions(stageCapture.regions.begin(), stageCapture.regions.end());
            for (const auto& read : decodeReads[i]) regions.push_back({read.address, read.Bytes()});
            std::stable_sort(regions.begin(), regions.end(), [](const ShaderRecompiler::MemoryRegion& a, const ShaderRecompiler::MemoryRegion& b) { return a.guestAddress < b.guestAddress; });
            const bool relocatable = decodeReads[i].empty() && (!vertexInfos[i] || vertexInfos[i]->resourcesNum == 0);
            tagVariantRuns(*variant, regions, stageCapture.UserDataUse(), relocatable, programs[i].hiddenUserWords, stageCapture.supplied);
            if (relocatable && (stageCapture.heapWords == nullptr || variant->ringTemplate != nullptr)) tagUserValueWords(*variant, programs[i], stageCapture.heapWords.get());
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
