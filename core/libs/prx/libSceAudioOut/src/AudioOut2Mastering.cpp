#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioOut2MasteringInit(uint32_t flags) {
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2MasteringTerm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2MasteringGetState(AudioOut2MasteringStatesHeader* state, uint32_t output, AudioOut2UserHandle user) {
    (void)state;
    (void)output;
    (void)user;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2MasteringSetParam(const AudioOut2MasteringParamsHeader* param, uint32_t output, uint32_t flags) {
    (void)param;
    (void)output;
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
