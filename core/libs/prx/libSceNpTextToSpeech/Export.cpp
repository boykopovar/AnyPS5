#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNpTextToSpeech/include/InventoryExports.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_NP_TTS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8194F004u);
constexpr std::uint32_t INITIALIZE_PARAM_VALUE = 0x20000;
constexpr int SCE_NP_TTS_ERROR_NOT_INITIALIZED = static_cast<int>(0x8194F009);

}

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI sceNpTextToSpeechCreateContext() {
    return SCE_NP_TTS_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpTextToSpeechDestroyContext() {
    return SCE_NP_TTS_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpTextToSpeechGetEvent() {
    return SCE_NP_TTS_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNpTextToSpeechInitialize(const void* param) {
    if (param == nullptr) return SCE_NP_TTS_ERROR_INVALID_ARGUMENT;
    std::uint8_t bytes[4];
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(bytes, param, sizeof(bytes));
    errno = savedErrno;
    if (!copied) return SCE_NP_TTS_ERROR_INVALID_ARGUMENT;
    std::uint32_t value;
    std::memcpy(&value, bytes, sizeof(value));
    if (value != INITIALIZE_PARAM_VALUE) return SCE_NP_TTS_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpTextToSpeechTerminate() {
    return SCE_NP_TTS_ERROR_NOT_INITIALIZED;
}

int APS5_VABI libSceNpTextToSpeechUnknown__minus_wGjkwcjXWs_nid_no_patch() {
    return SCE_NP_TTS_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("-wGjkwcjXWs", libSceNpTextToSpeechUnknown__minus_wGjkwcjXWs_nid_no_patch);

int APS5_VABI libSceNpTextToSpeechUnknown_APYz0RAIugo_nid_no_patch() {
    return SCE_NP_TTS_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("APYz0RAIugo", libSceNpTextToSpeechUnknown_APYz0RAIugo_nid_no_patch);

int APS5_VABI libSceNpTextToSpeechUnknown_Fp7i5mx3JWM_nid_no_patch() {
    return SCE_NP_TTS_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("Fp7i5mx3JWM", libSceNpTextToSpeechUnknown_Fp7i5mx3JWM_nid_no_patch);

int APS5_VABI libSceNpTextToSpeechUnknown_i3MidGD_minus_Zfk_nid_no_patch() {
    return SCE_NP_TTS_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("i3MidGD-Zfk", libSceNpTextToSpeechUnknown_i3MidGD_minus_Zfk_nid_no_patch);

int APS5_VABI libSceNpTextToSpeechUnknown_oIMRm6q4pUM_nid_no_patch() {
    return SCE_NP_TTS_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("oIMRm6q4pUM", libSceNpTextToSpeechUnknown_oIMRm6q4pUM_nid_no_patch);

int APS5_VABI libSceNpTextToSpeechUnknown_rIXr2WILhUE_nid_no_patch() {
    return SCE_NP_TTS_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("rIXr2WILhUE", libSceNpTextToSpeechUnknown_rIXr2WILhUE_nid_no_patch);

}
