#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_KERNEL_ERROR_EOPNOTSUPP = static_cast<std::int32_t>(0x8002002D);

}

extern "C" {

int APS5_VABI sceTextToSpeech2GetSystemStatus() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

std::int32_t APS5_VABI sceTextToSpeech2Initialize(const void* param) {
    (void)param;
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Cancel() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Close() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2GetSpeechStatus() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Open() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Speak() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Terminate() {
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

}
