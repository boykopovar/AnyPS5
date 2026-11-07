#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
bool IsInitialPthread_nid_no_patch();

int APS5_VABI pthread_main_np_nid_postfix() {
    return IsInitialPthread_nid_no_patch() ? 1 : 0;
}

int APS5_VABI scePthreadMain(void) {
    return pthread_main_np_nid_postfix();
}

}
