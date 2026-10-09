#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include <mutex>

namespace AgcDriver {

void Submit(const Packet* packet, std::uint32_t queue) try {
    DriverDetail::Driver::Get().Submit(packet, queue);
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
    DriverDetail::Driver::Get().RegisterShader(shader);
}

void SuspendPoint() {
    DriverDetail::Driver::Get().SuspendPoint();
}

void RegisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    DriverDetail::Driver::Get().RegisterVideoOutput(handle, output);
}

void UnregisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    DriverDetail::Driver::Get().UnregisterVideoOutput(handle, output);
}

void PresentClear(const PresentationWindow& window, bool opaque, void (*gpuReady)(void*), void* context) {
    DriverDetail::Driver::Get().Present(window, nullptr, opaque, gpuReady, context);
}

void PresentBuffer(const PresentationWindow& window, const DisplayBuffer& buffer, void (*gpuReady)(void*), void* context) {
    DriverDetail::Driver::Get().Present(window, &buffer, true, gpuReady, context);
}

void ReleaseWindow(void* window) {
    DriverDetail::Driver::Get().ReleaseWindow(window);
}

void ReportFailure(std::exception_ptr error) {
    DriverDetail::Driver::Get().ReportFailure(error);
}

}

extern "C" void AgcDriverWaitIdle_nid_postfix() try {
    AgcDriver::WaitIdle();
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
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

extern "C" void AgcDriverRegisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    AgcDriver::RegisterVideoOutput(handle, output);
}

extern "C" void AgcDriverUnregisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    AgcDriver::UnregisterVideoOutput(handle, output);
}

extern "C" void AgcDriverPresentClear_nid_postfix(const AgcDriver::PresentationWindow& window, bool opaque, void (*gpuReady)(void*), void* context) {
    AgcDriver::PresentClear(window, opaque, gpuReady, context);
}

extern "C" void AgcDriverPresentBuffer_nid_postfix(const AgcDriver::PresentationWindow& window, const AgcDriver::DisplayBuffer& buffer, void (*gpuReady)(void*), void* context) {
    AgcDriver::PresentBuffer(window, buffer, gpuReady, context);
}

extern "C" void AgcDriverReleaseWindow_nid_postfix(void* window) {
    AgcDriver::ReleaseWindow(window);
}

extern "C" void AgcDriverReportFailure_nid_postfix(std::exception_ptr error) {
    AgcDriver::ReportFailure(error);
}

extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    AgcDriver::DriverDetail::Driver::Get().ResolveShaderAbi(shader, context, primitive);
}

extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    AgcDriver::DriverDetail::Driver::Get().ResolveGraphicsAbi(vertex, pixel, primitiveType);
}
extern "C" void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    AgcDriver::DriverDetail::Driver::Get().ResolveGraphicsStagesAbi(stages, context, primitive);
}
