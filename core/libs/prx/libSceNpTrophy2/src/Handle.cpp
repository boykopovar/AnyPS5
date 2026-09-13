#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2CreateHandle(int* handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2DestroyHandle(int handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpTrophy2AbortHandle(int handle) {
    (void)handle;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
