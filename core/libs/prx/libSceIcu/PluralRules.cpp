#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>

namespace {

constexpr std::int32_t PluralTypeCount = 2;

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

void APS5_VABI uplrules_close_nid_postfix(UPluralRules* uplrules) {
    if (!uplrules) {
        return;
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

}
