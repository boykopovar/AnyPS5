#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <stdexcept>

namespace {
struct PkeyHeader {
    int type;
};
}

extern "C" int APS5_VABI EVP_PKEY_id_nid_postfix(const void* value) {
    if (!value) throw std::invalid_argument("EVP_PKEY_id: null key");
    PkeyHeader header{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&header, value, sizeof(header));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return header.type;
}
