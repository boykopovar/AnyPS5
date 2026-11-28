#pragma once

#ifdef _WIN32
#include <winsock2.h>
#else
#include <cerrno>
#include <sys/socket.h>
#endif

#include "prx/libkernel/Socket/include/SocketPoll.hpp"

#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace MbedSocketIo {

static inline int HostError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

static inline bool HostInterrupted(int error) {
#ifdef _WIN32
    return error == WSAEINTR;
#else
    return error == EINTR;
#endif
}

static inline bool HostWouldBlock(int error) {
#ifdef _WIN32
    return error == WSAEWOULDBLOCK;
#else
    return error == EAGAIN || error == EWOULDBLOCK;
#endif
}

static inline bool HostReset(int error) {
#ifdef _WIN32
    return error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENETRESET;
#else
    return error == ECONNRESET || error == EPIPE;
#endif
}

static inline bool HostTimedOut(int error) {
#ifdef _WIN32
    return error == WSAETIMEDOUT;
#else
    (void)error;
    return false;
#endif
}

static inline int Transfer(const std::shared_ptr<void>& socketOwner, std::uintptr_t nativeSocket,
    int& hostError, bool send, void* buffer, std::size_t length) {
    const int size = static_cast<int>(std::min<std::size_t>(length, INT_MAX));
    for (;;) {
#ifdef _WIN32
        auto* owner = static_cast<KernelSocketPoll::NativeOwner*>(socketOwner.get());
        const auto result = owner && owner->transfer
            ? owner->transfer(owner, send, buffer, size)
            : (send
                ? ::send(static_cast<SOCKET>(nativeSocket), static_cast<const char*>(buffer), size, 0)
                : ::recv(static_cast<SOCKET>(nativeSocket), static_cast<char*>(buffer), size, 0));
#else
        (void)socketOwner;
        const auto result = send
            ? ::send(static_cast<int>(nativeSocket), buffer, static_cast<std::size_t>(size), MSG_NOSIGNAL)
            : ::recv(static_cast<int>(nativeSocket), buffer, static_cast<std::size_t>(size), 0);
#endif
        if (result >= 0) return static_cast<int>(result);
        const int error = HostError();
        if (HostInterrupted(error)) continue;
        if (HostWouldBlock(error) || HostTimedOut(error)) {
            return send ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_SSL_WANT_READ;
        }
        if (HostReset(error)) return MBEDTLS_ERR_NET_CONN_RESET;
        hostError = error;
        return send ? MBEDTLS_ERR_NET_SEND_FAILED : MBEDTLS_ERR_NET_RECV_FAILED;
    }
}

static inline int Send(const std::shared_ptr<void>& socketOwner, std::uintptr_t nativeSocket,
    int& hostError, const unsigned char* buffer, std::size_t length) {
    return Transfer(socketOwner, nativeSocket, hostError, true, const_cast<unsigned char*>(buffer), length);
}

static inline int Receive(const std::shared_ptr<void>& socketOwner, std::uintptr_t nativeSocket,
    int& hostError, unsigned char* buffer, std::size_t length) {
    return Transfer(socketOwner, nativeSocket, hostError, false, buffer, length);
}

}
