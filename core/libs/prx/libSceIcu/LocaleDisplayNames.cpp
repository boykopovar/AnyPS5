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

}

extern "C" {

void APS5_VABI uldn_close_nid_postfix(ULocaleDisplayNames* names) {
    if (!names) return;
    NotImplemented_nid_no_patch(__func__);
}

ULocaleDisplayNames* APS5_VABI uldn_openForContext_nid_postfix(const char*, std::int32_t*, std::int32_t, std::int32_t* status) {
    if (Failure(status, __func__)) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

}
