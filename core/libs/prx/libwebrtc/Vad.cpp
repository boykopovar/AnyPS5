#include "prx/libwebrtc/include/Api.hpp"
#include <cstdlib>

extern "C" {
#include "vad/vad_core.h"
}

extern "C" {

WebRtcVadInstance* APS5_VABI WebRtcVad_Create_nid_postfix(void) {
    auto* instance = static_cast<VadInstT*>(std::malloc(sizeof(VadInstT)));
    if (instance) instance->init_flag = 0;
    return reinterpret_cast<WebRtcVadInstance*>(instance);
}

void APS5_VABI WebRtcVad_Free_nid_postfix(WebRtcVadInstance* handle) {
    std::free(handle);
}

int APS5_VABI WebRtcVad_Init_nid_postfix(WebRtcVadInstance* handle) {
    return WebRtcVad_InitCore(reinterpret_cast<VadInstT*>(handle));
}

int APS5_VABI WebRtcVad_set_mode_nid_postfix(WebRtcVadInstance* handle, int mode) {
    auto* instance = reinterpret_cast<VadInstT*>(handle);
    if (!instance || instance->init_flag != 42) return -1;
    return WebRtcVad_set_mode_core(instance, mode);
}

int APS5_VABI WebRtcVad_Process_nid_postfix(WebRtcVadInstance* handle, int sampleRate, const std::int16_t* audio, std::size_t length) {
    auto* instance = reinterpret_cast<VadInstT*>(handle);
    if (!instance || instance->init_flag != 42 || !audio) return -1;
    if (sampleRate != 8000 && sampleRate != 16000 && sampleRate != 32000 && sampleRate != 48000) return -1;
    const auto samplesPerTenMs = static_cast<std::size_t>(sampleRate / 100);
    if (length != samplesPerTenMs && length != 2 * samplesPerTenMs && length != 3 * samplesPerTenMs) return -1;
    int result;
    switch (sampleRate) {
    case 8000: result = WebRtcVad_CalcVad8khz(instance, audio, length); break;
    case 16000: result = WebRtcVad_CalcVad16khz(instance, audio, length); break;
    case 32000: result = WebRtcVad_CalcVad32khz(instance, audio, length); break;
    default: result = WebRtcVad_CalcVad48khz(instance, audio, length); break;
    }
    return result > 0 ? 1 : result;
}

}
