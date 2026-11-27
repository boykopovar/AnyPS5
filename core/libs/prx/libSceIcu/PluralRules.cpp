#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>

namespace {

constexpr std::int32_t PluralTypeCount = 2;

bool Failure(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    std::int32_t value = 0;
    const bool ok = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    if (!ok) NotImplemented_nid_no_patch(caller);
    return value > 0;
}

void WriteStatus(std::int32_t* status, std::int32_t value, const char* caller) {
    const bool ok = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    if (!ok) NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

void APS5_VABI uplrules_close_nid_postfix(UPluralRules* uplrules) {
    if (!uplrules) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

UEnumeration* APS5_VABI uplrules_getKeywords_nid_postfix(const UPluralRules* uplrules, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    if (!uplrules) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

UPluralRules* APS5_VABI uplrules_openForType_nid_postfix(const char*, std::int32_t type, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    if (type >= PluralTypeCount) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI uplrules_selectWithFormat_nid_postfix(const UPluralRules* uplrules, double, const UNumberFormat* fmt, char16_t* keyword, std::int32_t capacity, std::int32_t* status) {
    if (Failure(status, __func__)) return 0;
    if (!uplrules || !fmt || (keyword == nullptr ? capacity != 0 : capacity < 0)) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
