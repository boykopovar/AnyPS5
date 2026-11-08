#include "FieldPositionIterator.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

void APS5_VABI ufieldpositer_close_nid_postfix(UFieldPositionIterator* fpositer) {
    if (!fpositer) return;
    NotImplemented_nid_no_patch(__func__);
}

}
