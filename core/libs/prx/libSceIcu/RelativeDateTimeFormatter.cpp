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

void APS5_VABI ureldatefmt_close_nid_postfix(URelativeDateTimeFormatter* reldatefmt) {
    if (!reldatefmt) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int32_t APS5_VABI ureldatefmt_format_nid_postfix(const URelativeDateTimeFormatter*, double, std::int32_t, char16_t* result, std::int32_t resultCapacity, std::int32_t* status) {
    if (Failure(status, __func__)) return 0;
    if (result == nullptr ? resultCapacity != 0 : resultCapacity < 0) {
        WriteStatus(status, Icu::IllegalArgument, __func__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

URelativeDateTimeFormatter* APS5_VABI ureldatefmt_open_nid_postfix(const char*, UNumberFormat*, std::int32_t, std::int32_t, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
