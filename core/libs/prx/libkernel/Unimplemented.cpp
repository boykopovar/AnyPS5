#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceCoredumpWriteUserData() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI __tls_get_addr_nid_postfix(void) {
    NotImplemented_nid_no_patch("vNe1w4diLCs");
    return 0;
}

int APS5_VABI sceKernelReleaseFlexibleMemory(void) {
    NotImplemented_nid_no_patch("teiItL2boFw");
    return 0;
}

// Canonical lib is libScePosix (dead import of Cyberpunk 2077): 35 of its
// 36 sibling imports resolve to libkernel, and libScePosix is not in the
// game's NEEDED list so only a NEEDED module can satisfy the loader here.
int APS5_VABI pthread_cancel_nid_postfix(void) {
    NotImplemented_nid_no_patch("0D4-FVvEikw");
    return 0;
}
}
