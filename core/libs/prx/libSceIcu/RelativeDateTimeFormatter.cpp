#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI ureldatefmt_close_nid_postfix(URelativeDateTimeFormatter* reldatefmt) {
    if (!reldatefmt) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ureldatefmt_format_nid_postfix(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    if (Icu::Failure(status)) return 0;
    if (result == nullptr ? resultCapacity != 0 : resultCapacity < 0) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

URelativeDateTimeFormatter* APS5_VABI ureldatefmt_open_nid_postfix(const char*, UNumberFormat*, std::int32_t, std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
