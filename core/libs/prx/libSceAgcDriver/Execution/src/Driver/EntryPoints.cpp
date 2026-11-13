#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadPriority.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/NativeDepthCapture.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Screenshot.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libkernel/Time/include/SleepWake.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <exception>
#include <mutex>
#include <stdexcept>
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

namespace AgcDriver {

RetiredOwners DiscardNativeDepthForReplay_nid_no_patch() {
    return DriverDetail::Driver::Get().DiscardNativeDepthForReplay();
}

void RestoreNativeDepthForReplay_nid_no_patch(std::span<const NativeDepthSnapshot> snapshots) {
    DriverDetail::Driver::Get().RestoreNativeDepthForReplay(snapshots);
}

void Submit(const Packet* packet, std::uint32_t queue) try {
    KernelNoteGpuSubmit_nid_no_patch();
    auto& driver = DriverDetail::Driver::Get();
    auto* const recorder = FrameRecorder::Active();
    if (recorder == nullptr) {
        driver.Submit(packet, queue);
        return;
    }
    GuestMemory::CheckRange(packet, sizeof(Packet), alignof(Packet));
    recorder->RewindTailFlips(driver.RewindTailFlips());
    const auto flips = driver.FlipSubmissions();
    recorder->Submit(*packet, queue, [&] { driver.Submit(packet, queue); });
    if (driver.FlipSubmissions() != flips) recorder->FrameBoundary();
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

void WaitIdle() {
    DriverDetail::Driver::Get().WaitIdle();
}

void Shutdown() {
    DriverDetail::Driver::Get().Shutdown();
}

void RegisterShader(const Shader* shader) {
    if (auto* const recorder = FrameRecorder::Active()) recorder->RegisterShader(shader);
    DriverDetail::Driver::Get().RegisterShader(shader);
}

void SuspendPoint() {
    auto* const recorder = FrameRecorder::Active();
    if (recorder == nullptr) {
        DriverDetail::Driver::Get().SuspendPoint();
        return;
    }
    recorder->Suspend([] { DriverDetail::Driver::Get().SuspendPoint(); });
}

void RegisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    if (auto* const recorder = FrameRecorder::Active()) recorder->RegisterVideoOutput(handle);
    DriverDetail::Driver::Get().RegisterVideoOutput(handle, output);
}

void UnregisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    if (auto* const recorder = FrameRecorder::Active()) recorder->UnregisterVideoOutput(handle);
    DriverDetail::Driver::Get().UnregisterVideoOutput(handle, output);
}

void AttachWindow(const PresentationWindow& window) {
    DriverDetail::Driver::Get().AttachWindow(window);
}

void PresentClear(const PresentationWindow& window, bool opaque, const DisplayBuffer* plane, void (*gpuReady)(void*), void* context) {
    DriverDetail::Driver::Get().Present(window, nullptr, plane, opaque, gpuReady, context);
}

void PresentBuffer(const PresentationWindow& window, const DisplayBuffer& buffer, const DisplayBuffer* plane, void (*gpuReady)(void*), void* context) {
    if (auto* const recorder = FrameRecorder::Active()) recorder->PresentBuffer(buffer);
    DriverDetail::Driver::Get().Present(window, &buffer, plane, true, gpuReady, context);
}

void ReleaseWindow(void* window) {
    DriverDetail::Driver::Get().ReleaseWindow(window);
}

void ReportFailure(std::exception_ptr error) {
    DriverDetail::Driver::Get().ReportFailure(error);
}

}

extern "C" void AgcDriverRaiseWorkerThreadPriority_nid_postfix(const char* role) {
    AgcDriver::RaiseWorkerThreadPriority(role);
}

extern "C" void AgcDriverWaitIdle_nid_postfix() try {
    AgcDriver::WaitIdle();
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

extern "C" void AgcDriverSubmitGraphics_nid_postfix(std::uint32_t* commands, std::uint32_t dwords) {
    const Packet packet{commands, dwords, 0, {}};
    AgcDriver::Submit(&packet, 0);
}

extern "C" void AgcDriverUseGcnShaders_nid_postfix() {
    ShaderRecompiler::SetGcnGuestShaders(true);
}

static std::mutex& VulkanLoaderMutex() {
    static std::mutex mutex;
    return mutex;
}

extern "C" void AgcDriverLockVulkanLoader_nid_postfix() {
    VulkanLoaderMutex().lock();
}

extern "C" void AgcDriverUnlockVulkanLoader_nid_postfix() {
    VulkanLoaderMutex().unlock();
}

extern "C" void AgcDriverShutdown_nid_postfix() {
    AgcDriver::Shutdown();
}

extern "C" void AgcDriverRequestScreenshot_nid_postfix() {
    AgcDriver::Screenshot::Request();
}

extern "C" void AgcDriverTracePresent_nid_postfix(std::uint32_t width, std::uint32_t height) {
    AgcDriver::DriverDetail::Driver::Get().TracePresent(width, height);
}

extern "C" std::size_t AgcDriverRestoreNativeDepthCapture_nid_no_patch(const char* directory) {
    if (directory == nullptr) throw std::invalid_argument("native depth capture: null capture directory");
    const auto snapshots = AgcDriver::ReadNativeDepthCapture(std::filesystem::path(directory) / AgcDriver::NativeDepthCaptureName);
    if (!snapshots.empty()) AgcDriver::DriverDetail::Driver::Get().RestoreNativeDepth(snapshots);
    return snapshots.size();
}

extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader) try {
    AgcDriver::RegisterShader(shader);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

extern "C" std::uint32_t AgcDriverInstructionWords_nid_postfix(std::span<const std::uint32_t> code, std::uint32_t index) {
    try {
        return ShaderRecompiler::DecodeRdnaInstruction(index * 4u, code, index).wordCount;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[agc] instruction at word %u of a fused front half cannot be decoded: %s\n", index, error.what());
        return 0;
    }
}

extern "C" void AgcDriverSuspendPoint_nid_postfix() try {
    AgcDriver::SuspendPoint();
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

extern "C" std::uint32_t AgcDriverReserveEopFlip_nid_postfix(const AgcDriver::FlipInfo& info) {
    const auto token = AgcDriver::DriverDetail::Driver::Get().ReserveEopFlip(info);
    if (auto* const recorder = AgcDriver::FrameRecorder::Active(); recorder != nullptr && token != 0) {
        recorder->RewindTailFlips(AgcDriver::DriverDetail::Driver::Get().RewindTailFlips());
        recorder->ReserveEopFlip(info, token);
    }
    return token;
}

extern "C" std::uint32_t AgcDriverReserveEopFlipAt_nid_postfix(const AgcDriver::FlipInfo& info, std::uint32_t token) {
    return AgcDriver::DriverDetail::Driver::Get().ReserveEopFlipAt(info, token);
}

extern "C" std::size_t AgcDriverAwaitedWaits_nid_postfix(AgcDriver::AwaitedWait* waits, std::size_t capacity) {
    const auto awaited = AgcDriver::DriverDetail::Driver::Get().AwaitedWaits();
    const auto count = std::min(capacity, awaited.size());
    std::copy_n(awaited.begin(), count, waits);
    return count;
}

extern "C" void AgcDriverWaitForPipelineJournal_nid_postfix() {
    AgcDriver::DriverDetail::Driver::Get().WaitForPipelineJournal();
}

extern "C" void AgcDriverSerialProgress_nid_postfix(std::uint64_t* accepted, std::uint64_t* completed) {
    const auto [submitted, done] = AgcDriver::DriverDetail::Driver::Get().SerialProgress();
    *accepted = submitted;
    *completed = done;
}

extern "C" bool AgcDriverHoldSettled_nid_postfix(std::int64_t milliseconds) {
    return AgcDriver::DriverDetail::Driver::Get().WaitSettledFor(std::chrono::milliseconds(milliseconds));
}

extern "C" void AgcDriverReleaseSettled_nid_postfix() {
    AgcDriver::DriverDetail::Driver::Get().ReleaseSettled();
}

extern "C" bool AgcDriverSettledHeld_nid_postfix() {
    return AgcDriver::DriverDetail::Driver::Get().SettledHeld();
}

extern "C" bool AgcDriverSettled_nid_postfix() {
    return AgcDriver::DriverDetail::Driver::Get().Settled();
}

extern "C" void AgcDriverSubmitFlip_nid_postfix(const AgcDriver::FlipInfo& info) try {
    auto* const recorder = AgcDriver::FrameRecorder::Active();
    if (recorder == nullptr) {
        AgcDriver::DriverDetail::Driver::Get().SubmitFlip(info);
        return;
    }
    recorder->RewindTailFlips(AgcDriver::DriverDetail::Driver::Get().RewindTailFlips());
    recorder->Flip(info, [&] { AgcDriver::DriverDetail::Driver::Get().SubmitFlip(info); });
    recorder->FrameBoundary();
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

extern "C" std::uint64_t AgcDriverDispatchesAdoptedAhead_nid_postfix() {
    return AgcDriver::DriverDetail::Driver::Get().DispatchesAdoptedAhead();
}

extern "C" std::uint64_t AgcDriverDispatchesRejectedAhead_nid_postfix() {
    return AgcDriver::DriverDetail::Driver::Get().DispatchesRejectedAhead();
}

extern "C" void AgcDriverSetAheadAdoptHook_nid_postfix(void (*hook)()) {
    AgcDriver::DriverDetail::Driver::Get().SetAheadAdoptHook(hook);
}

extern "C" void AgcDriverRegisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    AgcDriver::RegisterVideoOutput(handle, output);
}

extern "C" void AgcDriverUnregisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    AgcDriver::UnregisterVideoOutput(handle, output);
}

extern "C" void AgcDriverAttachWindow_nid_postfix(const AgcDriver::PresentationWindow& window) {
    AgcDriver::AttachWindow(window);
}

extern "C" void AgcDriverPresentClear_nid_postfix(const AgcDriver::PresentationWindow& window, bool opaque, const AgcDriver::DisplayBuffer* plane, void (*gpuReady)(void*), void* context) {
    AgcDriver::PresentClear(window, opaque, plane, gpuReady, context);
}

extern "C" void AgcDriverPresentBuffer_nid_postfix(const AgcDriver::PresentationWindow& window, const AgcDriver::DisplayBuffer& buffer, const AgcDriver::DisplayBuffer* plane, void (*gpuReady)(void*), void* context) {
    AgcDriver::PresentBuffer(window, buffer, plane, gpuReady, context);
}

extern "C" void AgcDriverReleaseWindow_nid_postfix(void* window) {
    AgcDriver::ReleaseWindow(window);
}

extern "C" void AgcDriverReportFailure_nid_postfix(std::exception_ptr error) {
    AgcDriver::ReportFailure(error);
}

extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) try {
    if (auto* const recorder = AgcDriver::FrameRecorder::Active()) recorder->ResolveShaderAbi(shader, context, primitive);
    AgcDriver::DriverDetail::Driver::Get().ResolveShaderAbi(shader, context, primitive);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) try {
    if (auto* const recorder = AgcDriver::FrameRecorder::Active()) recorder->ResolveGraphicsAbi(vertex, pixel, primitiveType);
    AgcDriver::DriverDetail::Driver::Get().ResolveGraphicsAbi(vertex, pixel, primitiveType);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}
extern "C" void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) try {
    if (auto* const recorder = AgcDriver::FrameRecorder::Active()) recorder->ResolveGraphicsStagesAbi(stages, context, primitive);
    AgcDriver::DriverDetail::Driver::Get().ResolveGraphicsStagesAbi(stages, context, primitive);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

extern "C" void AgcDriverPrepareCommandWrite_nid_no_patch(std::uint64_t address, std::size_t bytes) {
    if (AgcDriver::Graphics::Recorder::SnapshotWriteOverlaps(address, bytes))
        AgcDriver::GuestMemory::FlushGpuWrites(address, bytes);
}

namespace {

std::atomic<void (*)()> transactionHookForTest{nullptr};

template<class Work>
void captureFailure(std::exception_ptr* failure, Work&& work) noexcept {
    try {
        work();
        *failure = nullptr;
    } catch (...) {
        *failure = std::current_exception();
    }
}

}

extern "C" void AgcDriverFlushGuestMemory_nid_no_patch(std::uint64_t address, std::size_t bytes, std::exception_ptr* failure) {
    captureFailure(failure, [&] {
        AgcDriver::GuestMemory::FlushGpuWrites(address, bytes);
        static_cast<void>(AgcDriver::Graphics::StorageTexture::FlushPending(address, bytes));
        AgcDriver::GuestMemory::FlushGpuWrites(address, bytes);
    });
}

extern "C" void AgcDriverGpuOwnedRanges_nid_no_patch(std::uint64_t address, std::size_t bytes, std::vector<std::pair<std::uint64_t, std::uint64_t>>& ranges, std::exception_ptr* failure) {
    captureFailure(failure, [&] { ranges = AgcDriver::Graphics::GpuOwnedRanges(address, bytes); });
}

extern "C" void AgcDriverResidentOwnersOver_nid_no_patch(std::uint64_t address, std::size_t bytes, bool gpuOnly, std::size_t* owners, std::exception_ptr* failure) {
    captureFailure(failure, [&] { *owners = AgcDriver::Graphics::ResidentOwnersOver(address, bytes, gpuOnly); });
}

extern "C" void AgcDriverGuestMemoryChangedSince_nid_no_patch(std::uint64_t address, std::size_t bytes, std::uint64_t since, std::vector<std::pair<std::uint64_t, std::uint64_t>>& changed, std::uint64_t* generation, std::exception_ptr* failure) {
    captureFailure(failure, [&] { *generation = AgcDriver::GuestMemory::ChangedSince(address, bytes, since, changed); });
}

extern "C" void AgcDriverSetGuestMemoryTransactionHookForTest_nid_no_patch(void (*hook)()) {
    transactionHookForTest.store(hook, std::memory_order_release);
}

extern "C" void AgcDriverGuestMemoryTransaction_nid_no_patch(void (*body)(void* context), void* context, std::exception_ptr* failure) {
    captureFailure(failure, [&] {
        if (AgcDriver::GuestMemory::GpuMutex().HeldByThisThread()) throw std::runtime_error("a guest memory transaction was entered with the GPU lock held; it waits for flushes that need that lock");
        auto& driver = AgcDriver::DriverDetail::Driver::Get();
        driver.WaitIdle();
        const auto idle = driver.SerialProgress().second;
        if (const auto hook = transactionHookForTest.load(std::memory_order_acquire)) hook();
        {
            const AgcDriver::Graphics::FlushExclusion exclusion;
            driver.WaitDeviceIdle();
            body(context);
        }
        if (driver.SerialProgress().first != idle) throw std::runtime_error("a submission was accepted during a guest memory transaction; the caller must stop submitting first");
    });
}

extern "C" void AgcDriverRetireGpuOwners_nid_no_patch(std::uint64_t address, std::size_t bytes, AgcDriver::RetiredOwners* retired, std::exception_ptr* failure) {
    captureFailure(failure, [&] {
        const auto owners = AgcDriver::Graphics::RetireGpuOwnersOver(address, bytes);
        retired->color += owners.color;
        retired->depthOwners += owners.depthOwners;
        retired->depthSurfaces += owners.depthSurfaces;
        retired->depthPlanes += owners.depthPlanes;
    });
}

extern "C" void AgcDriverOmitGpuOwnersForTest_nid_no_patch(bool omit) {
    AgcDriver::Graphics::OmitGpuOwnersForTest(omit);
}
