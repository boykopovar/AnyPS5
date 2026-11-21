#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>

extern "C" {

const void* APS5_VABI EVP_MD_CTX_md_nid_postfix(const void* context) {
    if (!context)
        return nullptr;
    const void* digest;
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&digest, context, sizeof(digest));
    errno = savedErrno;
    if (!readable) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return digest;
}

}
