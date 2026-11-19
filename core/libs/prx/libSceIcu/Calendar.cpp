#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

UCalendar* APS5_VABI ucal_clone_nid_postfix(const UCalendar*, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ucal_close_nid_postfix(UCalendar* cal) {
    if (!cal) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucal_get_nid_postfix(const UCalendar*, UCalendarDateFields, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucal_getCanonicalTimeZoneID_nid_postfix(
    const char16_t* id,
    std::int32_t len,
    char16_t* result,
    std::int32_t resultCapacity,
    std::int8_t* isSystemID,
    std::int32_t* status) {
    if (!status || *status > 0) {
        return 0;
    }
    if (isSystemID) {
        *isSystemID = 0;
    }
    if (!id || len == 0 || !result || resultCapacity <= 0) {
        *status = Icu::IllegalArgument;
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucal_getDefaultTimeZone_nid_postfix(char16_t*, std::int32_t, std::int32_t* ec) {
    if (!ec || *ec > 0) return 0;
    NotImplemented_nid_no_patch(__func__);
}

UEnumeration* APS5_VABI ucal_getKeywordValuesForLocale_nid_postfix(const char*, const char*, std::int8_t, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ucal_getTimeZoneDisplayName_nid_postfix(const void*, std::int32_t, const char*, char16_t*,
                                                               std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    NotImplemented_nid_no_patch(__func__);
}

UCalendar* APS5_VABI ucal_open_nid_postfix(const char16_t*, std::int32_t, const char*, std::int32_t, std::int32_t* status) {
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ucal_setGregorianChange_nid_postfix(UCalendar* cal, UDate, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    if (!cal) {
        *status = Icu::IllegalArgument;
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI ucal_setMillis_nid_postfix(UCalendar*, UDate, std::int32_t* status) {
    if (Icu::Failure(status)) return;
    NotImplemented_nid_no_patch(__func__);
}

}
