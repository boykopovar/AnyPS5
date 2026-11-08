#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI uplrules_close_nid_postfix(UPluralRules* uplrules) {
    if (!uplrules) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
