#include "prx/libSceTtsEngine/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED = static_cast<int>(0x81A10004);

}

extern "C" {

int APS5_VABI sceTtsEngineInitialize(void) {
    return 0;
}

int APS5_VABI sceTtsEngineTerminate(void) {
    return 0;
}

APS5_EXPORT("ULO80qzqyLo", sceTtsEngineUnknown00);
int APS5_VABI sceTtsEngineUnknown00(void) {
    return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
}

APS5_EXPORT("BGb-DqWHvH8", sceTtsEngineUnknown01);
int APS5_VABI sceTtsEngineUnknown01(void) {
    return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTtsEngineSetText(void) {
    return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTtsEngineGetAudio(void) {
    return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTtsEngineClear(void) {
    return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTtsEngineSetPitch(void) {
    return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTtsEngineSetSpeed(void) {
    return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTtsEngineSetVolume(void) {
    return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceTtsEngineSetDepth(void) {
    return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
}

}
