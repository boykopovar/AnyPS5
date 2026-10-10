#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceCoredumpWriteUserData() {
 return 0;
}

int APS5_VABI __tls_get_addr_nid_postfix(void) {
    return 0;
}

int APS5_VABI pthread_cancel_nid_postfix(void) {
    return 0;
}
}
