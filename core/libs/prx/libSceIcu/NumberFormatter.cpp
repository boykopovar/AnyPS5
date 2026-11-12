#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI unumf_close_nid_postfix(UNumberFormatter* formatter) {
    if (!formatter) return;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI unumf_closeResult_nid_postfix(UFormattedNumber* result) {
    if (!result) return;
    NotImplemented_nid_no_patch(__func__);
}

UFormattedNumber* APS5_VABI unumf_openResult_nid_postfix(std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
