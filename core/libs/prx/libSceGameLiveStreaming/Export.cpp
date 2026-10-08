#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceGameLiveStreamingInitialize(size_t heap_size) {
    if (heap_size == 0) APS5_INVALID_ARG_EX;
    return 0;
}

int APS5_VABI sceGameLiveStreamingTerminate(void) {
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetCurrentStatus2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetProgramInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
