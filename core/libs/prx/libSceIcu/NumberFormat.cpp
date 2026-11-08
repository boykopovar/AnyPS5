#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI unum_close_nid_postfix(UNumberFormat* fmt) {
    if (!fmt) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
