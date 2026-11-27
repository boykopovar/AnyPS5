#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

extern "C" {

int APS5_VABI EVP_CIPHER_CTX_set_flags_nid_postfix(LibreSslEvpCipherCtx* context, int flags) {
    if (!context) throw std::invalid_argument("EVP_CIPHER_CTX_set_flags: null context");
    LibreSslEvpCipherCtx copy{};
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&copy, context, sizeof(copy));
    errno = savedErrno;
    if (!readable) throw std::invalid_argument("EVP_CIPHER_CTX_set_flags: unreadable context");
    copy.flags |= static_cast<std::uint64_t>(flags);
    const int writeErrno = errno;
    const bool writable = GuestCopyOut_nid_no_patch(context, &copy, sizeof(copy));
    errno = writeErrno;
    if (!writable) throw std::invalid_argument("EVP_CIPHER_CTX_set_flags: unwritable context");
    return 1;
}

}
