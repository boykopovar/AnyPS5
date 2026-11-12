#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNpTextToSpeech/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

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

int APS5_VABI sceNpTextToSpeechInitialize() {
    return 0;
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
