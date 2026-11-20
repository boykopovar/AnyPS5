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

void APS5_VABI unumf_formatDecimal_nid_postfix(const UNumberFormatter* formatter, const char*, std::int32_t, UFormattedNumber*, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!formatter) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI unumf_formatDouble_nid_postfix(const UNumberFormatter* formatter, double, UFormattedNumber*, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!formatter) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

UNumberFormatter* APS5_VABI unumf_openForSkeletonAndLocale_nid_postfix(const char16_t*, std::int32_t, const char*, std::int32_t*) {
    NotImplemented_nid_no_patch(__func__);
}

UFormattedNumber* APS5_VABI unumf_openResult_nid_postfix(std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI unumf_resultGetAllFieldPositions_nid_postfix(const UFormattedNumber* result, UFieldPositionIterator*, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!result) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI unumf_resultToString_nid_postfix(const UFormattedNumber* result, char16_t*, std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    if (!result) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
