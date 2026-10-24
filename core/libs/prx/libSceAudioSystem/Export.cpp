#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioSystemSystemResume() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioSystemSystemSuspend() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
