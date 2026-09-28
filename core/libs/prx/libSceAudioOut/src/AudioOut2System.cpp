#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioOut2Initialize(void) {
    return 0;
}

int APS5_VABI sceAudioOut2GetSystemState(AudioOut2SystemState* state) {
    if (!state) return static_cast<int>(0x80260502);
    state->loudness = 0.0f;
    return 0;
}

int APS5_VABI sceAudioOut2SetSystemDebugState(const AudioOut2SystemDebugStateParam* param) {
    (void)param;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
