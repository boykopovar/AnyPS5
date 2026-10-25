#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int DEBUG_UNAVAILABLE = static_cast<int>(0x8A6C1000);
constexpr int DRIVER_FEATURE_UNAVAILABLE = static_cast<int>(0x8A6D0001);
constexpr int DRIVER_NULL_OUTPUT = static_cast<int>(0x8A6D0003);
constexpr int GPU_RESET_UNAVAILABLE = static_cast<int>(0x8A6C0045);
constexpr std::size_t TOOL_MEMORY_RANGE_SIZE = 12;

}

extern "C" {

int APS5_VABI sceAgcDriverGetShaderDebuggingStatus() {
    return 1;
}

int APS5_VABI sceAgcDriverRegisterDefaultOwner(uint32_t owner) {
    (void)owner;
    return 0;
}

int APS5_VABI sceAgcDriverSetValidationErrorOutputFrequency(uint32_t frequency) {
    (void)frequency;
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetSubmitValidationMode(uint32_t mode) {
    (void)mode;
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetSubmitValidationConfig(void* config) {
    (void)config;
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetSubmitValidationMode(uint32_t* mode) {
    (void)mode;
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetSubmitValidationConfig(const void* config) {
    (void)config;
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetHsOffchipParam(uint16_t* first, uint16_t* second) {
    (void)first;
    (void)second;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverNopDataWritePacket(void* destination, uint32_t address, const void* data, uint32_t bytes) {
    const auto alignedBytes = (static_cast<std::uint64_t>(bytes) + 3) & ~std::uint64_t{3};
    const auto totalBytes = alignedBytes + 8;
    std::memset(destination, 0, static_cast<std::size_t>(totalBytes));
    const auto header = (static_cast<std::uint32_t>(totalBytes) << 14) + 0x3ffe0000U | 0xc0001000U;
    std::memcpy(destination, &header, sizeof(header));
    std::memcpy(static_cast<std::byte*>(destination) + 4, &address, sizeof(address));
    if (data != nullptr && bytes != 0) std::memcpy(static_cast<std::byte*>(destination) + 8, data, bytes);
    return data == nullptr || bytes == 0 ? 2 : static_cast<int>(totalBytes / 4);
}

int APS5_VABI sceAgcDriverSetValidationDisabled() {
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetCaptureConfig() {
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetCaptureConfig() {
    return DEBUG_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetGpuPrintfWorkArea() {
    return DRIVER_FEATURE_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetGpuPrintfEnabled() {
    return DRIVER_FEATURE_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverDbgReadRegister() {
    return 0;
}

int APS5_VABI sceAgcDriverDbgWriteRegister() {
    return DRIVER_FEATURE_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetGpuRefClks() {
    return DRIVER_FEATURE_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverIsSuspendPointInFlight() {
    return DRIVER_FEATURE_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverIsPaDebug() {
    return 0;
}

int APS5_VABI sceAgcDriverGetExceptionStatus() {
    return 0;
}

int APS5_VABI sceAgcDriverGetTraceInitiator() {
    return 0;
}

int APS5_VABI sceAgcDriverRegisterGpuResetCallbacks() {
    return 0;
}

int APS5_VABI sceAgcDriverUnregisterGpuResetCallbacks() {
    return 0;
}

int APS5_VABI sceAgdResetGpu() {
    return GPU_RESET_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSysSubmitFlipHandleProxy() {
    return 0;
}

int APS5_VABI sceAgcDriverUserDataImmediateWrite() {
    return 0;
}

int APS5_VABI sceAgcDriverUserDataImmediateWritePacket() {
    return 0;
}

int APS5_VABI sceAgcDriverGetCoredumpMode(uint32_t* mode) {
    if (mode == nullptr) return DRIVER_NULL_OUTPUT;
    *mode = 0;
    return 0;
}

int APS5_VABI sceAgcDriverAllocateToolMemoryForGpuReset(uint64_t, uint64_t, uint64_t, void* range) {
    if (range != nullptr) std::memset(range, 0, TOOL_MEMORY_RANGE_SIZE);
    return DRIVER_FEATURE_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverGetAllocatedToolMemoryForGpuReset(uint64_t, void* range) {
    if (range != nullptr) std::memset(range, 0, TOOL_MEMORY_RANGE_SIZE);
    return DRIVER_FEATURE_UNAVAILABLE;
}

}
extern "C" {

int APS5_VABI sceAgcDriverAcquireHDRScopesACQ() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void* APS5_VABI sceAgcDriverGetCoredumpAddress() {
    return nullptr;
}

int APS5_VABI sceAgcDriverHp3dLockEnd() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverHp3dLockStart() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverIsEsvmProcess() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverReleaseHDRScopesACQ() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSetTargetRingForDiag() {
    return DRIVER_FEATURE_UNAVAILABLE;
}

int APS5_VABI sceAgcDriverSetValueToSharedGds() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSetWorkloadStreamInactive() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSubmitReprojectionAcb() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSuspendPointSubmitCdbg() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSysGetGfxAppGpuResetStatus() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
