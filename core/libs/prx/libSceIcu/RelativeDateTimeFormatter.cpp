#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>

extern "C" {
URelativeDateTimeFormatter* ureldatefmt_open(const char*, void*, std::int32_t, std::int32_t, std::int32_t*);
void ureldatefmt_close(URelativeDateTimeFormatter*);
}

namespace {

std::int32_t ReadStatus(const std::int32_t* status, const char* caller) {
    Icu::RequireStatus(status);
    const int savedErrno = errno;
    std::int32_t value = 0;
    const bool ok = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
    return value;
}

void WriteStatus(std::int32_t* status, std::int32_t previous, std::int32_t value, const char* caller) {
    if (previous == value) return;
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = savedErrno;
    if (!ok) NotImplemented_nid_no_patch(caller);
}

bool Failure(std::int32_t* status, std::int32_t& value, const char* caller) {
    value = ReadStatus(status, caller);
    return value > 0;
}

}

extern "C" {

void APS5_VABI ureldatefmt_close_nid_postfix(URelativeDateTimeFormatter* reldatefmt) {
    if (!reldatefmt) return;
    ::ureldatefmt_close(reldatefmt);
}

std::int32_t APS5_VABI ureldatefmt_format_nid_postfix(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    std::int32_t previous = 0;
    if (Failure(status, previous, __func__)) return 0;
    if (result == nullptr ? resultCapacity != 0 : resultCapacity < 0) {
        WriteStatus(status, previous, Icu::IllegalArgument, __func__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ureldatefmt_formatNumeric_nid_postfix(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    std::int32_t previous = 0;
    if (Failure(status, previous, __func__)) return 0;
    if (result == nullptr ? resultCapacity != 0 : resultCapacity < 0) {
        WriteStatus(status, previous, Icu::IllegalArgument, __func__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

URelativeDateTimeFormatter* APS5_VABI ureldatefmt_open_nid_postfix(const char* locale, UNumberFormat* formatter, std::int32_t width, std::int32_t context, std::int32_t* status) {
    std::int32_t previous = 0;
    if (Failure(status, previous, __func__)) return nullptr;
    std::int32_t error = previous;
    auto* const result = ::ureldatefmt_open(locale, formatter, width, context, &error);
    try {
        WriteStatus(status, previous, error, __func__);
    } catch (...) {
        if (result) ::ureldatefmt_close(result);
        throw;
    }
    return result;
}

}
