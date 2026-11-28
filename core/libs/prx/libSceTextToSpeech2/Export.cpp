#include "prx/libSceTextToSpeech2/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_KERNEL_ERROR_EOPNOTSUPP = static_cast<std::int32_t>(0x8002002D);
constexpr std::int32_t SCE_TEXT_TO_SPEECH2_ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x81A10004);

}

static_assert(sizeof(TextToSpeechInitParameter) == 64);
static_assert(sizeof(TextToSpeechOpenParameter) == 64);
static_assert(sizeof(TextToSpeechSpeakParameter) == 64);
static_assert(offsetof(TextToSpeechSpeakParameter, reserved) == 24);

extern "C" {

int APS5_VABI sceTextToSpeech2GetSystemStatus(std::int32_t* status) {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

std::int32_t APS5_VABI sceTextToSpeech2Initialize(const TextToSpeechInitParameter* param) {
    (void)param;
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Cancel() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Close() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2GetSpeechStatus(std::int32_t* status) {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Open(const TextToSpeechOpenParameter* parameter) {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Speak(const TextToSpeechSpeakParameter* parameter) {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Terminate() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2ConvertTextInt() {
    return SCE_TEXT_TO_SPEECH2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTextToSpeech2GetAudioInt() {
    return SCE_TEXT_TO_SPEECH2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTextToSpeech2InitializeInt() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2OpenInt() {
    return SCE_TEXT_TO_SPEECH2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTextToSpeech2RegisterTextConversionItem() {
    return SCE_TEXT_TO_SPEECH2_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTextToSpeech2SynthesizeInt() {
    return SCE_TEXT_TO_SPEECH2_ERROR_NOT_INITIALIZED;
}

}
