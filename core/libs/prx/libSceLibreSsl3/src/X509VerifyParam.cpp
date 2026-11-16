#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

static_assert(offsetof(LibreSslX509VerifyParam, flags) == 24);

namespace {

constexpr std::uint64_t PolicyCheck = 0x80;
constexpr std::uint64_t PolicyVariables = 0x100 | 0x200 | 0x400;

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

int APS5_VABI X509_VERIFY_PARAM_set_flags_nid_postfix(LibreSslX509VerifyParam* param, std::uint64_t flags) {
    if (!param)
        throw std::invalid_argument("X509_VERIFY_PARAM_set_flags requires parameters");
    auto* const guestFlags = reinterpret_cast<unsigned char*>(param) + offsetof(LibreSslX509VerifyParam, flags);
    const auto current = CopyInPreservingErrno<std::uint64_t>(guestFlags, __func__);
    if ((flags & PolicyVariables) && !((current | flags) & PolicyCheck))
        NotImplemented_nid_no_patch(__func__);
    const std::uint64_t updated = current | flags;
    CopyOutPreservingErrno(guestFlags, updated, __func__);
    return 1;
}

}
