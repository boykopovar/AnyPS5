#include <cstdint>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioOut2MasteringInit(uint32_t flags) {
    if (flags != 0) throw std::runtime_error(std::string(__func__) + ": the meaning of flags " + std::to_string(flags) + " is unknown");
    return 0;
}

int APS5_VABI sceAudioOut2MasteringTerm(void) {
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
    (void)output;
    (void)flags;
    if (param == nullptr) throw std::runtime_error(std::string(__func__) + ": the result for a null param is unknown");
    return 0;
}

}
