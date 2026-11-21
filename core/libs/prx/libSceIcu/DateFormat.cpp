#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>

namespace {

bool Failure(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    const int savedErrno = errno;
    std::int32_t value = 0;
    const bool ok = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
    return value > 0;
}

void WriteStatus(std::int32_t* status, std::int32_t value, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

void APS5_VABI udat_close_nid_postfix(UDateFormat* format) {
    if (!format) return;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udat_countSymbols_nid_postfix(const UDateFormat* fmt, std::int32_t) {
    if (!fmt) return 0;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udat_format_nid_postfix(const UDateFormat*, double, char16_t* result, std::int32_t resultLength, UFieldPosition*, std::int32_t* status) {
    if (Failure(status, __func__)) return -1;
    if (result == nullptr ? resultLength != 0 : resultLength < 0) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udat_formatForFields_nid_postfix(const void* format, double dateToFormat, char16_t* result, std::int32_t resultLength, void* fpositer, std::int32_t* status) {
    (void)format;
    (void)dateToFormat;
    (void)fpositer;
    if (Failure(status, __func__)) return -1;
    if (!result ? resultLength != 0 : resultLength < 0) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

const UCalendar* APS5_VABI udat_getCalendar_nid_postfix(const UDateFormat*) {
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udat_getSymbols_nid_postfix(const void* fmt, std::int32_t, std::int32_t, char16_t*, std::int32_t, std::int32_t*) {
    if (!fmt) return -1;
    NotImplemented_nid_no_patch(__func__);
}

UDateFormat* APS5_VABI udat_open_nid_postfix(std::int32_t, std::int32_t, const char*, const char16_t*, std::int32_t,
                                                        const char16_t*, std::int32_t, std::int32_t* status) {
    if (!status) APS5_INVALID_ARG_EX;
    if (Failure(status, __func__)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI udat_toPattern_nid_postfix(const UDateFormat* fmt, std::int8_t, char16_t* result, std::int32_t resultLength, std::int32_t* status) {
    if (Failure(status, __func__)) return -1;
    if (!result ? resultLength != 0 : resultLength < 0) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return -1;
    }
    if (!fmt) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
