#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

static constexpr int DebugUnavailable = static_cast<int>(0x8A6C1000);
static constexpr int FeatureUnavailable = static_cast<int>(0x8A6D0001);
static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(sceAgcDriverGetShaderDebuggingStatus() == 1);

    Require(sceAgcDriverSetValidationErrorOutputFrequency(0u) == DebugUnavailable);
    Require(sceAgcDriverSetValidationErrorOutputFrequency(60u) == DebugUnavailable);
    Require(sceAgcDriverSetSubmitValidationMode(0u) == DebugUnavailable);
    Require(sceAgcDriverSetSubmitValidationMode(1u) == DebugUnavailable);

    std::uint32_t mode = 0xA5A5A5A5u;
    Require(sceAgcDriverGetSubmitValidationMode(&mode) == DebugUnavailable);
    Require(sceAgcDriverGetSubmitValidationMode(nullptr) == DebugUnavailable);
    Require(mode == 0xA5A5A5A5u);

    std::array<std::uint8_t, 64> config{};
    config.fill(0x5A);
    Require(sceAgcDriverSetSubmitValidationConfig(config.data()) == DebugUnavailable);
    Require(sceAgcDriverSetSubmitValidationConfig(nullptr) == DebugUnavailable);
    Require(sceAgcDriverGetSubmitValidationConfig(config.data()) == DebugUnavailable);
    Require(sceAgcDriverGetSubmitValidationConfig(nullptr) == DebugUnavailable);
    for (const std::uint8_t byte : config) Require(byte == 0x5A);

    Require(sceAgcDriverSetValidationDisabled() == DebugUnavailable);
    Require(sceAgcDriverGetCaptureConfig() == DebugUnavailable && sceAgcDriverSetCaptureConfig() == DebugUnavailable);
    Require(sceAgcDriverGetGpuPrintfWorkArea() == FeatureUnavailable && sceAgcDriverSetGpuPrintfEnabled() == FeatureUnavailable);
    Require(sceAgcDriverDbgReadRegister() == 0 && sceAgcDriverDbgWriteRegister() == FeatureUnavailable);
    Require(sceAgcDriverGetGpuRefClks() == FeatureUnavailable && sceAgcDriverIsSuspendPointInFlight() == FeatureUnavailable);
    Require(sceAgcDriverIsPaDebug() == 0 && sceAgcDriverGetExceptionStatus() == 0 && sceAgcDriverGetTraceInitiator() == 0);
    Require(sceAgcDriverRegisterGpuResetCallbacks() == 0 && sceAgcDriverUnregisterGpuResetCallbacks() == 0);
    Require(sceAgdResetGpu() == static_cast<int>(0x8A6C0045));
    Require(sceAgcDriverSysSubmitFlipHandleProxy() == 0);
    Require(sceAgcDriverUserDataImmediateWrite() == 0 && sceAgcDriverUserDataImmediateWritePacket() == 0);

    std::uint32_t coredumpMode = 0xA5A5A5A5u;
    Require(sceAgcDriverGetCoredumpMode(nullptr) == static_cast<int>(0x8A6D0003));
    Require(sceAgcDriverGetCoredumpMode(&coredumpMode) == 0 && coredumpMode == 0);

    std::array<std::uint8_t, 16> range{};
    range.fill(0x5A);
    Require(sceAgcDriverAllocateToolMemoryForGpuReset(1, 2, 3, range.data()) == FeatureUnavailable);
    for (std::size_t i = 0; i < range.size(); ++i) Require(range[i] == (i < 12 ? 0 : 0x5A));
    Require(sceAgcDriverAllocateToolMemoryForGpuReset(1, 2, 3, nullptr) == FeatureUnavailable);
    range.fill(0x5A);
    Require(sceAgcDriverGetAllocatedToolMemoryForGpuReset(1, range.data()) == FeatureUnavailable);
    for (std::size_t i = 0; i < range.size(); ++i) Require(range[i] == (i < 12 ? 0 : 0x5A));
    Require(sceAgcDriverGetAllocatedToolMemoryForGpuReset(1, nullptr) == FeatureUnavailable);
}
