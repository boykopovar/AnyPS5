#include "prx/libSceRazorCpu_debug/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceRazorCpuStartCapture(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuStopCapture(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRazorCpuSetLiveServerThreadSettings(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
