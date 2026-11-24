#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Ssl.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>

namespace {

template <typename T>
T CopyIn(const T* source, const char* function) {
    T value{};
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    if (!copied) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

}

extern "C" {

const LibreSsl3Stack* APS5_VABI SSL_get0_verified_chain_nid_postfix(const LibreSsl3Ssl* ssl) {
    if (!ssl) return nullptr;
    const LibreSsl3Ssl session = CopyIn(ssl, __func__);
    if (!session.s3) return nullptr;
    return CopyIn(session.s3, __func__).hs.verifiedChain;
}

}
