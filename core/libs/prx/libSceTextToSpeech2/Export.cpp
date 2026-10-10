#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t SCE_KERNEL_ERROR_EOPNOTSUPP = static_cast<std::int32_t>(0x8002002D);

}

extern "C" {

int APS5_VABI sceTextToSpeech2GetSystemStatus() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

std::int32_t APS5_VABI sceTextToSpeech2Initialize(const void* param) {
    (void)param;
    return SCE_KERNEL_ERROR_EOPNOTSUPP;
}

int APS5_VABI sceTextToSpeech2Cancel() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceTextToSpeech2Close() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceTextToSpeech2GetSpeechStatus() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceTextToSpeech2Open() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceTextToSpeech2Speak() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

int APS5_VABI sceTextToSpeech2Terminate() {
    throw std::runtime_error(std::string(__func__) + ": unknown signature");

}

}

