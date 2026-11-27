#include "prx/libSceTtsEngine/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_TTS_ENGINE_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x81A10003);
constexpr int SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED = static_cast<int>(0x81A10004);

std::atomic<bool> g_initialized{false};

}

extern "C" {

int APS5_VABI sceTtsEngineInitialize(void) {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        return SCE_TTS_ENGINE_ERROR_ALREADY_INITIALIZED;
    }
    return 0;
}

int APS5_VABI sceTtsEngineTerminate(void) {
    g_initialized.store(false);
    return 0;
}

APS5_EXPORT("ULO80qzqyLo", sceTtsEngineUnknown00);
int APS5_VABI sceTtsEngineUnknown00(void) {
    if (!g_initialized.load()) return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch("ULO80qzqyLo");
    return 0;
}

APS5_EXPORT("BGb-DqWHvH8", sceTtsEngineUnknown01);
int APS5_VABI sceTtsEngineUnknown01(void) {
    if (!g_initialized.load()) return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch("BGb-DqWHvH8");
    return 0;
}

int APS5_VABI sceTtsEngineSetText(void) {
    if (!g_initialized.load()) return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTtsEngineGetAudio(void) {
    if (!g_initialized.load()) return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTtsEngineClear(void) {
    if (!g_initialized.load()) return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTtsEngineSetPitch(void) {
    if (!g_initialized.load()) return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTtsEngineSetSpeed(void) {
    if (!g_initialized.load()) return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTtsEngineSetVolume(void) {
    if (!g_initialized.load()) return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceTtsEngineSetDepth(void) {
    if (!g_initialized.load()) return SCE_TTS_ENGINE_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
