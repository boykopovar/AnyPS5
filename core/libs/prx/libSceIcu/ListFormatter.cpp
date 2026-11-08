#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI ulistfmt_close_nid_postfix(UListFormatter* listfmt) {
    if (!listfmt) return;
    NotImplemented_nid_no_patch(__func__);
}

}
