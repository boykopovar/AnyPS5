#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioOut2SpeakerArrayCreate(AudioOut2SpeakerArrayHandle* handle, const void* vbap_params, const void* ambi_params) {
    (void)handle;
    (void)vbap_params;
    (void)ambi_params;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2SpeakerArrayDestroy(AudioOut2SpeakerArrayHandle handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2GetSpeakerArrayAmbisonicsCoefficients(AudioOut2SpeakerArrayHandle handle, uint32_t ambisonics_channel, float* coefficients, uint32_t num_coefficients) {
    (void)handle;
    (void)ambisonics_channel;
    (void)coefficients;
    (void)num_coefficients;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2GetSpeakerArrayCoefficients(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, uint32_t num_coefficients, uint8_t height_aware, float downmix_spread_radius) {
    (void)handle;
    (void)pos;
    (void)spread;
    (void)coefficients;
    (void)num_coefficients;
    (void)height_aware;
    (void)downmix_spread_radius;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

size_t APS5_VABI sceAudioOut2GetSpeakerArrayMemorySize(uint32_t num_speakers, uint8_t is_3d, uint8_t is_ambisonics) {
    (void)num_speakers;
    (void)is_3d;
    (void)is_ambisonics;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2GetSpeakerInfo(AudioOut2SpeakerInfo* info, uint32_t flags) {
    (void)info;
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
