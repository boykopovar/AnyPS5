#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadPriority.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/FrameRecorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Screenshot.hpp"
#include "prx/libkernel/Time/include/SleepWake.hpp"
#include <algorithm>
#include <mutex>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

namespace AgcDriver {

void Submit(const Packet* packet, std::uint32_t queue) try {
    KernelNoteGpuSubmit_nid_no_patch();
    auto& driver = DriverDetail::Driver::Get();
    auto* const recorder = FrameRecorder::Active();
    if (recorder == nullptr) {
        driver.Submit(packet, queue);
        return;
    }
    GuestMemory::CheckRange(packet, sizeof(Packet), alignof(Packet));
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
#ifdef _WIN32
    static const auto hdesk = [] {
        if (const auto hwinsta = OpenWindowStationA("WinSta0", FALSE, 0x037F)) {
            SetProcessWindowStation(hwinsta);
        }
        return OpenDesktopA("Default", 0, FALSE, 0x01FF);
    }();
    if (hdesk != nullptr) {
        SetThreadDesktop(hdesk);
    }
#endif
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

extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader) try {
    AgcDriver::RegisterShader(shader);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

extern "C" void AgcDriverSuspendPoint_nid_postfix() try {
    AgcDriver::SuspendPoint();
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

extern "C" std::uint32_t AgcDriverReserveEopFlip_nid_postfix(const AgcDriver::FlipInfo& info) {
    const auto token = AgcDriver::DriverDetail::Driver::Get().ReserveEopFlip(info);
    if (auto* const recorder = AgcDriver::FrameRecorder::Active(); recorder != nullptr && token != 0) recorder->ReserveEopFlip(info, token);
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

extern "C" void AgcDriverSerialProgress_nid_postfix(std::uint64_t* accepted, std::uint64_t* completed) {
    const auto [submitted, done] = AgcDriver::DriverDetail::Driver::Get().SerialProgress();
    *accepted = submitted;
    *completed = done;
}

extern "C" void AgcDriverSubmitFlip_nid_postfix(const AgcDriver::FlipInfo& info) try {
    auto* const recorder = AgcDriver::FrameRecorder::Active();
    if (recorder == nullptr) {
        AgcDriver::DriverDetail::Driver::Get().SubmitFlip(info);
        return;
    }
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
