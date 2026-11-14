#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

UText* APS5_VABI utext_close(UText* ut) {
    if (!ut) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
