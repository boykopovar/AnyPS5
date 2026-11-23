#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>

namespace {

constexpr int ReasonConnectionTypeNotSet = 144;

template <typename T>
T CopyIn(const T* source, const char* function) {
    T value{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    errno = savedErrno;
    if (!copied) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

} // namespace

extern "C" {

int APS5_VABI SSL_do_handshake_nid_postfix(LibreSslSsl* ssl) {
    const LibreSslSsl connection = CopyIn(ssl, __func__);
    const LibreSslSslInternal internal = CopyIn(connection.internal, __func__);
    if (!internal.handshakeFunc) {
        LibreSslPutError_nid_no_patch(SslLayout::ErrorLibrarySsl, SslLayout::ErrorFunctionAny, ReasonConnectionTypeNotSet);
        return -1;
    }
    if (SslLayout::Read<int>(&internal, SslLayout::InternalRenegotiate) != 0) {
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if ((internal.state & (SslLayout::StateInit | SslLayout::StateBefore)) != 0) {
        return reinterpret_cast<SslStateFunction>(internal.handshakeFunc)(ssl);
    }
    return 1;
}

}
