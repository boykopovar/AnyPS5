#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI udat_close_nid_postfix(UDateFormat* format) {
    if (!format) return;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udat_format_nid_postfix(const UDateFormat*, double, char16_t* result, std::int32_t resultLength, UFieldPosition*, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    if (result == nullptr ? resultLength != 0 : resultLength < 0) {
        *status = Icu::IllegalArgument;
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udat_formatForFields_nid_postfix(const void* format, double dateToFormat, char16_t* result, std::int32_t resultLength, void* fpositer, std::int32_t* status) {
    (void)format;
    (void)dateToFormat;
    (void)fpositer;
    if (Icu::Failure(status)) return -1;
    if (!result ? resultLength != 0 : resultLength < 0) {
        *status = Icu::IllegalArgument;
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udat_getSymbols_nid_postfix(const void* fmt, std::int32_t, std::int32_t, char16_t*, std::int32_t, std::int32_t*) {
    if (!fmt) return -1;
    NotImplemented_nid_no_patch(__func__);
}

UDateFormat* APS5_VABI udat_open_nid_postfix(std::int32_t, std::int32_t, const char*, const char16_t*, std::int32_t,
                                                        const char16_t*, std::int32_t, std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    if (Icu::Failure(status)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udat_toPattern_nid_postfix(const UDateFormat* fmt, std::int8_t, char16_t* result, std::int32_t resultLength, std::int32_t* status) {
    if (Icu::Failure(status)) return -1;
    if (!result ? resultLength != 0 : resultLength < 0) {
        *status = Icu::IllegalArgument;
        return -1;
    }
    if (!fmt) {
        *status = Icu::IllegalArgument;
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
