#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioOut2PortCreate(AudioOut2ContextHandle ctx, const AudioOut2PortParam* params, AudioOut2PortHandle* port) {
    (void)ctx;
    (void)params;
    (void)port;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2PortDestroy(AudioOut2PortHandle port) {
    (void)port;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2PortGetState(AudioOut2PortHandle port, AudioOut2PortState* state) {
    (void)port;
    (void)state;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2PortSetAttributes(AudioOut2PortHandle port, const AudioOut2Attribute* attributes, uint32_t num) {
    (void)port;
    (void)attributes;
    (void)num;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
