#include "prx/libSceNpSpeechToText/include/InventoryExports.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_NP_STT_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8193F003u);
constexpr std::uint32_t INITIALIZE_PARAM_VALUE = 0x20000;
constexpr int SCE_NP_STT_ERROR_NOT_INITIALIZED = static_cast<int>(0x8193F000);

}

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI sceNpSpeechToTextCreateContext() {
    return SCE_NP_STT_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpSpeechToTextDestroyContext() {
    return SCE_NP_STT_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpSpeechToTextGetEvent() {
    return SCE_NP_STT_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpSpeechToTextInitialize(const void* param) {
    if (param == nullptr) return SCE_NP_STT_ERROR_INVALID_ARGUMENT;
    std::uint8_t bytes[64];
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(bytes, param, sizeof(bytes));
    errno = savedErrno;
    if (!copied) return SCE_NP_STT_ERROR_INVALID_ARGUMENT;
    std::uint32_t value;
    std::memcpy(&value, bytes, sizeof(value));
    if (value != INITIALIZE_PARAM_VALUE) return SCE_NP_STT_ERROR_INVALID_ARGUMENT;
    for (std::size_t index = 4; index < sizeof(bytes); ++index) {
        if (bytes[index] != 0) return SCE_NP_STT_ERROR_INVALID_ARGUMENT;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpSpeechToTextStart() {
    return SCE_NP_STT_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpSpeechToTextStop() {
    return SCE_NP_STT_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpSpeechToTextTerminate() {
    return SCE_NP_STT_ERROR_NOT_INITIALIZED;
}

}
