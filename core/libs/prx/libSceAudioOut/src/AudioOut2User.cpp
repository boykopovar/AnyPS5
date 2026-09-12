#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioOut2UserCreate(uint32_t user_id, AudioOut2UserHandle* handle) {
    (void)user_id;
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2UserDestroy(AudioOut2UserHandle handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
