#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_KERNEL_ERROR_EOPNOTSUPP = static_cast<std::int32_t>(0x8002002D);

}

struct TextToSpeechInitParameter {
    std::uint32_t version;
    std::int32_t threadPriority;
    std::uint64_t cpuAffinity;
    std::uint8_t reserved[48];
};

struct TextToSpeechOpenParameter {
    std::uint32_t language;
    std::uint32_t voiceType;
    std::uint8_t reserved[56];
};

struct TextToSpeechSpeakParameter {
    const char16_t* text;
    std::uint32_t length;
    std::uint32_t mode;
    std::uint32_t volume;
    std::int32_t speed;
    std::uint8_t reserved[40];
};

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

}
