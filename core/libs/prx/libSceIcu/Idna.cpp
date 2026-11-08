#include "Idna.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI uidna_close_nid_postfix(UIDNA* idna) {
    if (!idna) return;
    NotImplemented_nid_no_patch(__func__);
}

}
