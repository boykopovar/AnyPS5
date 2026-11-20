#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

constexpr int EncryptionInitial = 0;
constexpr int EncryptionOneRtt = 3;
constexpr std::uint64_t InitialCapacity = 64;

static_assert(offsetof(LibreSsl3Ssl, tls13) == 232);

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

LibreSsl3QuicPendingData* CreatePendingData(LibreSsl3Ssl* ssl) {
    auto* pending = static_cast<LibreSsl3QuicPendingData*>(malloc_nid_postfix(sizeof(LibreSsl3QuicPendingData)));
    if (!pending) return nullptr;
    pending->data = nullptr;
    pending->length = 0;
    pending->capacity = 0;
    ssl->tls13 = pending;
    return pending;
}

bool Reserve(LibreSsl3QuicPendingData& pending, std::size_t extra) {
    const auto added = static_cast<std::uint64_t>(extra);
    if (added > std::numeric_limits<std::uint64_t>::max() - pending.length) return false;
    const std::uint64_t needed = pending.length + added;
    if (needed <= pending.capacity) return true;
    std::uint64_t capacity = std::max(pending.capacity, InitialCapacity);
    while (capacity < needed) {
        if (capacity > std::numeric_limits<std::uint64_t>::max() / 2) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    if (capacity > std::numeric_limits<std::size_t>::max()) return false;
    auto* const grown = static_cast<std::uint8_t*>(malloc_nid_postfix(static_cast<std::size_t>(capacity)));
    if (!grown) return false;
    if (pending.length) std::memcpy(grown, pending.data, static_cast<std::size_t>(pending.length));
    free_nid_postfix(pending.data);
    pending.data = grown;
    pending.capacity = capacity;
    return true;
}

int AppendPendingData(LibreSsl3Ssl* ssl, const std::uint8_t* data, std::size_t length) {
    auto* pending = static_cast<LibreSsl3QuicPendingData*>(ssl->tls13);
    if (!pending) {
        pending = CreatePendingData(ssl);
        if (!pending) return 0;
    }
    if (!Reserve(*pending, length)) return 0;
    if (length && !CopyIn(pending->data + pending->length, data, length)) return 0;
    pending->length += length;
    return 1;
}

}

extern "C" {

int APS5_VABI SSL_provide_quic_data_nid_postfix(LibreSsl3Ssl* ssl, int level, const std::uint8_t* data,
                                               std::size_t length) {
    if (!ssl) return 0;
    if (!ssl->quicMethod) return 0;
    if (level < EncryptionInitial || level > EncryptionOneRtt) return 0;
    if (!data && length != 0) return 0;
    return AppendPendingData(ssl, data, length);
}

int APS5_VABI SSL_get_peer_quic_transport_params_nid_postfix(const LibreSsl3Ssl* ssl, const std::uint8_t** params,
                                                            std::size_t* paramsLength) {
    if (!ssl) return 0;
    const std::uint8_t* const value = ssl->quicTransportParams;
    const std::size_t length = static_cast<std::size_t>(ssl->quicTransportParamsLength);
    if (params && !CopyOut(params, &value, sizeof(value))) return 0;
    if (paramsLength && !CopyOut(paramsLength, &length, sizeof(length))) return 0;
    return 1;
}

}
