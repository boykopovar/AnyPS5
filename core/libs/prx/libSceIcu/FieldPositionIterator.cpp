#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI ufieldpositer_close_nid_postfix(UFieldPositionIterator* fpositer) {
    if (!fpositer) return;
    NotImplemented_nid_no_patch(__func__);
}

UFieldPositionIterator* APS5_VABI ufieldpositer_open_nid_postfix(std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
