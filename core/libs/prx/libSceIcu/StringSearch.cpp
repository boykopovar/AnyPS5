#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>

namespace {

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

template <typename T>
T CopyInPreservingErrno(const void* guest, const char* caller) {
    PreservedErrno guard;
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, guest, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
    return value;
}

template <typename T>
void CopyOutPreservingErrno(void* guest, const T& value, const char* caller) {
    PreservedErrno guard;
    if (!GuestCopyOut_nid_no_patch(guest, &value, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
}

}

extern "C" {

UStringSearch* APS5_VABI usearch_open_nid_postfix(const char16_t*, std::int32_t, const char16_t*, std::int32_t, const char* locale, UBreakIterator*, std::int32_t* status) {
    Icu::RequireStatus(status);
    if (CopyInPreservingErrno<std::int32_t>(status, __func__) > 0) return nullptr;
    if (locale == nullptr) {
        CopyOutPreservingErrno(status, Icu::IllegalArgument, __func__);
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

}
