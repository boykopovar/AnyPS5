#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioOutInit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOutOpen(int user_id, int type, int index, uint32_t len, uint32_t freq, uint32_t param) {
    (void)user_id;
    (void)type;
    (void)index;
    (void)len;
    (void)freq;
    (void)param;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOutClose(int handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOutOutput(int handle, const void* ptr) {
    (void)handle;
    (void)ptr;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOutOutputs(AudioOutOutputParam* param, uint32_t num) {
    (void)param;
    (void)num;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOutSetVolume(int handle, uint32_t flag, int* vol) {
    (void)handle;
    (void)flag;
    (void)vol;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOutGetPortState(int handle, AudioOutPortState* state) {
    (void)handle;
    (void)state;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
