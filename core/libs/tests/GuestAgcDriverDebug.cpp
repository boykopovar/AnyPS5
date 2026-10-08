#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceAgcDriverGetShaderDebuggingStatus(void);
int APS5_VABI sceAgcDriverRequestCaptureStart(void);
int APS5_VABI sceAgcDriverRequestCaptureStop(void);
int APS5_VABI sceAgcDriverTriggerCapture(void);
int APS5_VABI sceAgcDriverSetSubmitValidationMode(std::uint32_t);
int APS5_VABI sceAgcDriverGetSubmitValidationMode(std::uint32_t*);
int APS5_VABI sceAgcDriverSetSubmitValidationConfig(const void*);
int APS5_VABI sceAgcDriverGetSubmitValidationConfig(void*);
int APS5_VABI sceAgcDriverSetValidationErrorOutputFrequency(std::uint32_t);
bool APS5_VABI sceAgcDriverIsCaptureInProgress(void);
bool APS5_VABI sceAgcDriverIsSubmitValidationEnabled(void);
}

static constexpr int DebugUnavailable = static_cast<int>(0x8A6C1000);
static void Require(bool value) { if (!value) std::abort(); }

int main() {
    Require(sceAgcDriverGetShaderDebuggingStatus() == 1);
    Require(sceAgcDriverRequestCaptureStart() == DebugUnavailable);
    Require(!sceAgcDriverIsCaptureInProgress());
    Require(sceAgcDriverTriggerCapture() == DebugUnavailable);
    Require(sceAgcDriverRequestCaptureStop() == DebugUnavailable);
    Require(!sceAgcDriverIsCaptureInProgress());
    std::uint32_t mode = 7u;
    Require(sceAgcDriverSetSubmitValidationMode(1u) == DebugUnavailable);
    Require(sceAgcDriverGetSubmitValidationMode(&mode) == DebugUnavailable && mode == 7u);
    Require(sceAgcDriverGetSubmitValidationMode(nullptr) == DebugUnavailable);
    std::uint32_t config[16];
    for (auto& word : config) word = 0xA5A5A5A5u;
    Require(sceAgcDriverSetSubmitValidationConfig(config) == DebugUnavailable);
    Require(sceAgcDriverGetSubmitValidationConfig(config) == DebugUnavailable);
    Require(sceAgcDriverGetSubmitValidationConfig(nullptr) == DebugUnavailable);
    for (const auto word : config) Require(word == 0xA5A5A5A5u);
    Require(sceAgcDriverSetValidationErrorOutputFrequency(60u) == DebugUnavailable);
    Require(!sceAgcDriverIsSubmitValidationEnabled());
}
