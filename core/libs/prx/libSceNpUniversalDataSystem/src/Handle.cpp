#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateHandle(int* handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemDestroyHandle(int handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemAbortHandle(int handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
