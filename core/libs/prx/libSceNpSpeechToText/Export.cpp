#include "prx/libSceNpSpeechToText/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

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

int APS5_VABI sceNpSpeechToTextInitialize() {
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
