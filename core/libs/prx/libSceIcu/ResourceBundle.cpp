#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI ures_close(UResourceBundle* resB) {
    if (!resB) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
