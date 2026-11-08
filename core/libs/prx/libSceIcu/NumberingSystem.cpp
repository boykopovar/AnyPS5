#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI unumsys_close_nid_postfix(UNumberingSystem* unumsys) {
    if (!unumsys) return;
    NotImplemented_nid_no_patch(__func__);
}

}
