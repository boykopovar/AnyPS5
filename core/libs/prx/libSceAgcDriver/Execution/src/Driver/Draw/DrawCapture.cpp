#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <cstdlib>
#include <cstring>

namespace AgcDriver::DriverDetail {

void Driver::compileDrawStage(std::size_t i, std::uint32_t pushOffset, const QueueState& queue, const Submission& submission, std::span<const DrawProgram> programs, const Graphics::State& graphics, const ShaderRecompiler::ShaderPixelStageInfo& pixel, std::span<DrawStage> work, std::vector<ShaderRecompiler::MemoryRegion>& memory, std::span<const ShaderRecompiler::LinkedProgram> linked, const Pm4::DrawParameters& drawParameters, const std::shared_ptr<VulkanDevice>& localDevice, ShaderMemory& shaderMemory, bool drawHit, bool profile, std::uint64_t dumpTarget, std::uint64_t dumpSlot1, std::uint64_t& captures, DrawPhaseTiming& phaseTiming, std::array<double, DrawDriverPhaseCount>& phaseMs, std::string& rejected) {
    using Stage = ShaderRecompiler::ShaderStage;
    phaseTiming.Phase(DrawRowVectors);
    const auto& program = programs[i];
    const auto waveSize = program.plan->binary.stage == Stage::Fragment ? graphics.stages.fragmentWaveSize : graphics.stages.vertexWaveSize;
    ShaderRecompiler::RecompileRequest request{
        program.plan->binary,
        {waveSize, program.plan->firstUserSgpr, program.UserData(), std::nullopt, program.plan->binary.stage == Stage::Fragment ? std::optional(pixel) : std::nullopt, work[i].vertexInfo, memory},
        localDevice->Target(),
        {0, 0, pushOffset, (graphics.stages.mesh ? ShaderRecompiler::MeshDrawPushOffsetBytes : Graphics::PipelinePushConstantBytes) - pushOffset},
        ShaderRecompiler::GraphicsCompileContext{program.plan->firstUserSgpr, linked, graphics.stages.mesh, graphics.stages.tessellation, {drawParameters.indexAddress, drawParameters.indexCount, drawParameters.indexSize, drawParameters.instanceCount}}
    };
    const auto waitedBefore = traceCapSync() || profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
    const std::string* poisoned = nullptr;
    const auto handle = SourceHandleFor(*program.plan->snapshot, program.plan->codeOffset, localDevice->Serial(), request, false, FailureMemo() ? &poisoned : nullptr);
    if (handle == nullptr && poisoned != nullptr) {
        rejected = *poisoned;
        return;
    }
    auto& stageCapture = work[i].capture;
    stageCapture.forgetSerial = GuestMemory::ForgetSerial();
    stageCapture.pushOffset = pushOffset;
    const auto capture = [&] {
        const SampledReadScope sampling(evidenceReads);
        return shaderMemory.Capture(request, handle.get());
    }();

    stageCapture.regions = shaderMemory.TakeRecentRegions();
    work[i].recompiled = true;
    memory = shaderMemory.Regions();

    for (std::size_t j = 0; j < programs.size(); ++j) {
        if (work[j].matched != nullptr && !work[j].recompiled && (drawHit || j < i)) memory.insert(memory.end(), work[j].matchedRegions.begin(), work[j].matchedRegions.end());
    }
    request.context.memory = memory;
    if (traceCapSync()) traceCapture("draw-capture", program.plan->binary.codeAddress, submission.queue, memory, Graphics::Recorder::ThreadWaitedMs() - waitedBefore);
    if (profile) {
        ++captures;
        phaseTiming.Phase(DrawRowCapture);

        const auto waited = std::min(Graphics::Recorder::ThreadWaitedMs() - waitedBefore, phaseMs[DrawRowCapture]);
        phaseMs[DrawRowCapture] -= waited;
        phaseMs[DrawRowCaptureHookWaits] += waited;
    }
    if (dumpTarget != 0) {

        const auto slot0 = (static_cast<std::uint64_t>(readRegister(queue.context, 0x390)) << 40u) | (static_cast<std::uint64_t>(readRegister(queue.context, 0x318)) << 8u);
        if ((graphics.hasColorTarget && graphics.color.address == dumpTarget) || slot0 == dumpTarget) static_cast<void>(dumpRequest(program.plan->binary.codeAddress, request));
    }
    if (dumpSlot1 != 0) {
        const auto value = [&](std::uint32_t offset) -> std::uint64_t { const auto it = queue.context.find(offset); return it == queue.context.end() ? 0u : it->second; };
        const auto slot1 = (value(0x391) << 40u) | (value(0x327) << 8u);
        if (slot1 == dumpSlot1) {
            static_cast<void>(dumpRequest(program.plan->binary.codeAddress, request));
            if (std::FILE* file = std::fopen("draw_slot1.regs", "w")) {
                for (const auto& [offset, value] : queue.context) std::fprintf(file, "context %x %08x\n", offset, value);
                for (const auto& [offset, value] : queue.userConfig) std::fprintf(file, "uconfig %x %08x\n", offset, value);
                for (const auto& [offset, value] : queue.shader) std::fprintf(file, "shader %x %08x\n", offset, value);
                std::fclose(file);
            }
        }
    }

    static const bool reuseCapture = std::getenv("APS5_NO_CAPTURE_REUSE") == nullptr;
    phaseTiming.Phase(DrawRowCapture);

    stageCapture.compiled = reuseCapture ? ShaderRecompiler::Recompile(request, *capture) : std::make_shared<const ShaderRecompiler::RecompileResult>(ShaderRecompiler::Recompile(request));
    phaseTiming.Phase(DrawRowRecompile);
}

void Driver::cacheDrawStages(bool useDrawEntries, bool drawHit, const Pm4::DrawParameters& drawParameters, const std::optional<Graphics::IndirectDrawPath>& indirectCpu, std::span<const DrawProgram> programs, std::span<DrawStage> work, bool verifyHit, std::uint64_t drawKey, bool registerKey, const std::shared_ptr<const DrawPlan>& decode, DrawPhaseTiming& phaseTiming, const std::shared_ptr<DrawEntry>& entry, std::uint64_t shapeKey) {
    if (useDrawEntries && !drawHit && !(drawParameters.indirect && indirectCpu)) {
        phaseTiming.Phase(DrawRowVectors);
        std::uint64_t unstable = 0, mismatches = 0, differingSameRuns = 0, differingWords = 0, differingRunsChanged = 0;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            const auto& stageCapture = work[i].capture;
            if (work[i].reused) {
                work[i].fresh = work[i].matched;
                continue;
            }
            if (stageCapture.compiled == nullptr) continue;
            auto variant = std::make_shared<DispatchVariant>();
            variant->compiled = stageCapture.compiled;
            variant->shader = programs[i].plan->snapshot;
            variant->forgetSerial = stageCapture.forgetSerial;
            variant->pushOffset = stageCapture.pushOffset;
            if (work[i].vertexInfo) variant->vertexInfo = std::make_shared<const ShaderRecompiler::ShaderVertexStageInfo>(*work[i].vertexInfo);

            std::vector<ShaderRecompiler::MemoryRegion> regions(stageCapture.regions.begin(), stageCapture.regions.end());
            for (const auto& read : work[i].decodeReads) regions.push_back({read.address, std::as_bytes(std::span(read.bytes))});
            std::stable_sort(regions.begin(), regions.end(), [](const ShaderRecompiler::MemoryRegion& a, const ShaderRecompiler::MemoryRegion& b) { return a.guestAddress < b.guestAddress; });
            for (const auto& region : regions) {
                variant->runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
                const auto count = region.bytes.size() / sizeof(std::uint32_t);
                const auto offset = variant->words.size();
                variant->words.resize(offset + count);
                std::memcpy(variant->words.data() + offset, region.bytes.data(), count * sizeof(std::uint32_t));
            }
            if (traceDrawCache() && entry != nullptr && work[i].matched == nullptr && i < entry->stages.size()) {
                const auto& variants = entry->stages[i];
                const auto front = std::find_if(variants.begin(), variants.end(), [&](const std::shared_ptr<DispatchVariant>& kept) { return kept->pushOffset == variant->pushOffset; });
                if (front != variants.end() && (*front)->runs == variant->runs && (*front)->words.size() == variant->words.size()) {
                    ++differingSameRuns;
                    for (std::size_t w = 0; w < variant->words.size(); ++w) differingWords += (*front)->words[w] != variant->words[w] ? 1u : 0u;
                } else if (front != variants.end()) {
                    ++differingRunsChanged;
                }
            }
            if (verifyHit && work[i].matched != nullptr && (work[i].matched->runs != variant->runs || work[i].matched->words != variant->words)) {
                ++mismatches;
                static std::atomic<std::uint64_t> reports{0};
                if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: stage %zu (program 0x%llx) of a hit captured differently: %zu runs / %zu words matched, %zu / %zu fresh\n", i, static_cast<unsigned long long>(programs[i].plan->binary.codeAddress), work[i].matched->runs.size(), work[i].matched->words.size(), variant->runs.size(), variant->words.size());
            }
            if (insertCompare()) {
                const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DrawCache);
                if (!captureStable(stageCapture.regions)) {
                    ++unstable;
                    continue;
                }
            }
            work[i].fresh = std::move(variant);
        }
        insertDrawEntry(drawKey, work, decode, programs, registerKey && drawDataHits() ? shapeKey : 0);
        if (unstable != 0 || mismatches != 0 || differingSameRuns != 0 || differingRunsChanged != 0) {
            std::lock_guard cacheLock(drawCacheMutex);
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
