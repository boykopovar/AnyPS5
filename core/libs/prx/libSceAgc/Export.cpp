#include "prx/libSceAgc/include/Agc.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcSuspendPointAndCheckStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
