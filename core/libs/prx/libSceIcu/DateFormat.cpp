#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI udat_close_nid_postfix(UDateFormat* format) {
    if (!format) return;
    NotImplemented_nid_no_patch(__func__);
}

}
