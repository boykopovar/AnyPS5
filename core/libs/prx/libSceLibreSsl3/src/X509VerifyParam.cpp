#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

static_assert(offsetof(LibreSslX509VerifyParam, flags) == 24);

namespace {

constexpr std::uint64_t PolicyCheck = 0x80;
constexpr std::uint64_t PolicyVariables = 0x100 | 0x200 | 0x400;

}

extern "C" {

int APS5_VABI X509_VERIFY_PARAM_set_flags_nid_postfix(LibreSslX509VerifyParam* param, std::uint64_t flags) {
    if (!param)
        throw std::invalid_argument("X509_VERIFY_PARAM_set_flags requires parameters");
    if ((flags & PolicyVariables) && !((param->flags | flags) & PolicyCheck))
        NotImplemented_nid_no_patch(__func__);
    param->flags |= flags;
    return 1;
}

}
