#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DrawWriteRanges.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawProgramTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/DrawWriteRanges.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/DeferredLabels.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <atomic>
#include <array>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdio>
#include <unordered_set>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <atomic>
#include <exception>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <typeinfo>

namespace AgcDriver::DriverDetail {
namespace {
namespace {
namespace {
void Driver::restartFinishedDraw(DrawTail& tail) {
    using Stage = ShaderRecompiler::ShaderStage;
    using Role = ShaderRecompiler::ProgramRole;
    const auto changed = [](const DrawProgram& program) {
        return std::any_of(program.memory.begin(), program.memory.end(), [](const ShaderRecompiler::MemoryRegion& region) { return !region.bytes.empty() && GuestMemory::CompareMapped(region.guestAddress, region.bytes) != GuestMemory::Compare::Equal; });
    };
    if (std::any_of(tail.programs->begin(), tail.programs->end(), changed)) {
        if (tail.programs != &tail.writablePrograms) {
            tail.writablePrograms = *tail.programs;
            tail.programs = &tail.writablePrograms;
        }
        for (auto& program : tail.writablePrograms) {
            if (program.nullPixel || !changed(program)) continue;
            auto snapshot = ProgramSnapshot(*tail.submission->shaders, program.binary.codeAddress, {program.snapshot->type});
            if (snapshot == program.snapshot) continue;
            const auto& fresh = *snapshot;
            program.codeOffset = static_cast<std::size_t>((program.binary.codeAddress - fresh.codeAddress) / sizeof(std::uint32_t));
            program.binary.code = std::span(fresh.code).subspan(program.codeOffset);
            program.binary.headerAddress = fresh.headerAddress;
            program.binary.header = fresh.header;
            program.memory = {{{fresh.codeAddress, std::as_bytes(std::span(fresh.code))}, {fresh.headerAddress, fresh.header}}};
            if (fresh.header.empty()) NoteRawGraphicsDraw(fresh);
            program.snapshot = std::move(snapshot);
        }
    }
    if (tail.drawHit && !tail.userPatches.empty()) {
        if (verifyDrawRecipe()) {
            std::lock_guard cacheLock(drawCacheMutex);
            ++drawEntryCounters.restartPatchSkipped;
        } else {
            auto patched = std::make_shared<DrawDecode>(*tail.decode);
            for (const auto& patch : tail.userPatches) patched->programs[patch.program].userData[patch.index] = patch.value;
            tail.decode = std::move(patched);
        }
    }
    const auto& programs = *tail.programs;
    const auto& graphics = tail.decode->state;
    const auto& roles = tail.decode->roles;
    const auto count = programs.size();
    tail.drawParameters = tail.resolvedParameters;
    tail.memory.clear();
    AppendProgramMemory(tail.memory, programs);
    tail.results.clear();
    tail.compiledStages.clear();
    tail.stages.clear();
    tail.pushCursorBytes = 0;
    tail.programResults.assign(count, nullptr);
    tail.stageCaptures.assign(count, {});
    tail.matched.assign(count, nullptr);
    tail.matchedRegions.assign(count, {});
    tail.fresh.assign(count, nullptr);
    tail.recompiled.assign(count, false);
    tail.dataCandidates.assign(count, {});
    tail.reused.assign(count, false);
    tail.stageData.assign(count, {});
    tail.dataEntry = nullptr;
    tail.drawHit = false;
    tail.verifyHit = false;
    tail.patchedStages = false;
    for (std::size_t i = 0; i < count; ++i) {
        tail.decodeReads[i].clear();
        tail.vertexInfos[i].reset();
        const auto& program = programs[i];
        if (program.binary.stage == Stage::Fragment || roles[i] == Role::GeometryBack) continue;
        tail.vertexInfos[i] = DecodeOrderedVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, tail.decodeReads[i], 8u - program.firstUserSgpr);
        if (!tail.vertexInfos[i]) continue;
        tail.vertexInfos[i]->paClVsOutCntl = PositionExportLayout(program.snapshot.get(), program.codeOffset, program.binary.code, tail.reg207, program.tailContinuation.get()) | graphics.layerExports | graphics.viewportExports;
        tail.vertexInfos[i]->depthRemapMode = graphics.depthRemap.mode;
        Graphics::ApplyPointSize(graphics, *tail.vertexInfos[i]);
    }
    tail.shaderMemory = std::make_unique<ShaderMemory>(tail.memory, &queryPendingWrite, &observePendingWrite, hookWaitCounter(), &QueryPendingSpan);
}

void Driver::commitDraw(std::shared_ptr<VulkanDevice> localDevice, std::uint32_t queue, const Graphics::State& graphics, const Pm4::DrawParameters& drawParameters, std::span<const Graphics::CompiledShader> stages, std::span<const Graphics::GuestMemorySnapshot> snapshots, const std::shared_ptr<const DrawRecipe>& recipe, const std::vector<std::shared_ptr<DispatchVariant>>& recipeStages, std::uint64_t drawKey, const std::shared_ptr<DrawEntry>& recipeDonor, VulkanDevice::DrawTraceContext trace, std::uint64_t shape, const std::vector<std::uint32_t>* contentKey) {
    const auto* tracePtr = trace.token != 0 ? &trace : nullptr;
    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
    std::lock_guard gpuLock(GuestMemory::GpuMutex());
    if (auto current = device.Load(); current != nullptr && current != localDevice) localDevice = std::move(current);
    struct SubmitDue {
        Driver& driver;
        const std::shared_ptr<VulkanDevice>& device;
        ~SubmitDue() noexcept(false) {
            if (std::uncaught_exceptions() == 0) driver.submitDueAfterCommit(device.get());
        }
    } submitDue{*this, localDevice};
    recordLabelsForPacket(localDevice.get(), queue);
    noteDrawWriters(stages, queue);
    if (recipe != nullptr) {
        if (localDevice->DrawFromRecipe(graphics, drawParameters, stages, snapshots, recipe, tracePtr) == RecipeOutcome::Recorded) return;
        VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Restart, VulkanDevice::RecipeKind::Draw);
    }
    std::shared_ptr<const DrawRecipe> built;
    localDevice->Draw(graphics, drawParameters, stages, snapshots, recipeStages.empty() ? nullptr : &built, tracePtr, shape, contentKey);
    if (built != nullptr) attachDrawRecipe(drawKey, recipeStages, std::move(built), recipeDonor);
}

}
}
}
}
