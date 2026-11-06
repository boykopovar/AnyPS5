#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int Datagram = 2;
constexpr int SoMaxConn = 128;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorFault = 14;
constexpr int ErrorInvalid = 22;
constexpr int ErrorNotSupported = 45;
constexpr int ErrorNameTooLong = 63;

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s (errno %d)\n", what, *__error_nid_postfix());
    std::abort();
}

bool Fails(long long result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

std::array<std::uint8_t, 16> LoopbackAddress(std::uint16_t port = 0) {
    std::array<std::uint8_t, 16> address{16, Inet};
    address[2] = static_cast<std::uint8_t>((port >> 8) & 0xff);
    address[3] = static_cast<std::uint8_t>(port & 0xff);
    address[4] = 127;
    address[7] = 1;
    return address;
}

void AuditListenBacklogBounds() {
    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0, "socket failed");
    auto address = LoopbackAddress();
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0, "bind failed");
    std::uint32_t length = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &length) == 0 && length == address.size(), "getsockname failed");

    Require(listen_nid_postfix(listener, 1) == 0, "listen 1 failed");
    Require(listen_nid_postfix(listener, SoMaxConn) == 0, "listen SOMAXCONN failed");
    Require(listen_nid_postfix(listener, 0) == 0, "listen 0 failed");

    Require(listen_nid_postfix(listener, -1) == 0, "listen -1 failed");
    Require(listen_nid_postfix(listener, -128) == 0, "listen -128 failed");
    Require(listen_nid_postfix(listener, -1000) == 0, "listen -1000 failed");
    Require(listen_nid_postfix(listener, std::numeric_limits<int>::min()) == 0, "listen INT_MIN failed");

    Require(listen_nid_postfix(listener, SoMaxConn + 1) == 0, "listen 129 failed");
    Require(listen_nid_postfix(listener, 256) == 0, "listen 256 failed");
    Require(listen_nid_postfix(listener, 1024) == 0, "listen 1024 failed");
    Require(listen_nid_postfix(listener, 65535) == 0, "listen 65535 failed");
    Require(listen_nid_postfix(listener, std::numeric_limits<int>::max()) == 0, "listen INT_MAX failed");

    const int client = socket_nid_postfix(Inet, Stream, 0);
    Require(client >= 0, "client socket failed");
    Require(connect_nid_postfix(client, address.data(), address.size()) == 0, "connect failed");
    const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
    Require(accepted >= 0, "accept failed");
    Require(close_nid_postfix(accepted) == 0, "close accepted failed");
    Require(close_nid_postfix(client) == 0, "close client failed");
    Require(close_nid_postfix(listener) == 0, "close listener failed");

    for (const int backlog : {0, -1, SoMaxConn, 256, std::numeric_limits<int>::max()}) {
        const int underscore = socket_nid_postfix(Inet, Stream, 0);
        Require(underscore >= 0, "underscore socket failed");
        auto uaddr = LoopbackAddress();
        Require(bind_nid_postfix(underscore, uaddr.data(), uaddr.size()) == 0, "bind underscore failed");
        Require(_listen_nid_postfix(underscore, backlog) == 0, "_listen failed");
        Require(close_nid_postfix(underscore) == 0, "close underscore failed");
    }
}

void AuditListenBacklogConnectionCycles() {
    for (const int backlog : {0, -1, -50, 1, SoMaxConn, SoMaxConn + 1, 512}) {
        const int listener = socket_nid_postfix(Inet, Stream, 0);
        Require(listener >= 0, "socket failed");
        auto address = LoopbackAddress();
        Require(bind_nid_postfix(listener, address.data(), address.size()) == 0, "bind failed");
        std::uint32_t length = address.size();
        Require(getsockname_nid_postfix(listener, address.data(), &length) == 0 && length == address.size(), "getsockname failed");
        Require(listen_nid_postfix(listener, backlog) == 0, "listen with backlog failed");

        const int client = socket_nid_postfix(Inet, Stream, 0);
        Require(client >= 0, "client socket failed");
        Require(connect_nid_postfix(client, address.data(), address.size()) == 0, "connect failed");
        const int accepted = accept_nid_postfix(listener, nullptr, nullptr);
        Require(accepted >= 0, "accept failed");
        Require(close_nid_postfix(accepted) == 0, "close accepted failed");
        Require(close_nid_postfix(client) == 0, "close client failed");

        Require(listen_nid_postfix(listener, SoMaxConn) == 0, "dynamic listen update to SOMAXCONN failed");
        const int secondClient = socket_nid_postfix(Inet, Stream, 0);
        Require(secondClient >= 0, "second client socket failed");
        Require(connect_nid_postfix(secondClient, address.data(), address.size()) == 0, "second connect failed");
        const int secondAccepted = accept_nid_postfix(listener, nullptr, nullptr);
        Require(secondAccepted >= 0, "second accept failed");
        Require(close_nid_postfix(secondAccepted) == 0, "close second accepted failed");
        Require(close_nid_postfix(secondClient) == 0, "close second client failed");

        Require(close_nid_postfix(listener) == 0, "close listener failed");
    }
}

void AuditListenPrecedenceAndRejection() {
    const int dgram = socket_nid_postfix(Inet, Datagram, 0);
    Require(dgram >= 0, "dgram socket failed");
    for (const int backlog : {-1, 0, 1, SoMaxConn, 1024}) {
        *__error_nid_postfix() = 0;
        Require(Fails(listen_nid_postfix(dgram, backlog), ErrorNotSupported), "listen on dgram must fail with EOPNOTSUPP");
        *__error_nid_postfix() = 0;
        Require(Fails(_listen_nid_postfix(dgram, backlog), ErrorNotSupported), "_listen on dgram must fail with EOPNOTSUPP");
    }
    Require(close_nid_postfix(dgram) == 0, "close dgram failed");

    const int server = socket_nid_postfix(Inet, Stream, 0);
    Require(server >= 0, "server socket failed");
    auto srvAddress = LoopbackAddress();
    Require(bind_nid_postfix(server, srvAddress.data(), srvAddress.size()) == 0, "bind server failed");
    std::uint32_t srvLength = srvAddress.size();
    Require(getsockname_nid_postfix(server, srvAddress.data(), &srvLength) == 0 && srvLength == srvAddress.size(), "getsockname server failed");
    Require(listen_nid_postfix(server, 5) == 0, "listen server failed");

    const int client = socket_nid_postfix(Inet, Stream, 0);
    Require(client >= 0, "client socket failed");
    Require(connect_nid_postfix(client, srvAddress.data(), srvAddress.size()) == 0, "connect client failed");
    const int accepted = accept_nid_postfix(server, nullptr, nullptr);
    Require(accepted >= 0, "accept server failed");

    for (const int backlog : {-1, 0, 1, SoMaxConn, 1024}) {
        *__error_nid_postfix() = 0;
        Require(Fails(listen_nid_postfix(client, backlog), ErrorInvalid), "listen on connected client must fail with EINVAL");
        *__error_nid_postfix() = 0;
        Require(Fails(listen_nid_postfix(accepted, backlog), ErrorInvalid), "listen on connected incoming must fail with EINVAL");
    }

    Require(close_nid_postfix(accepted) == 0, "close accepted failed");
    Require(close_nid_postfix(client) == 0, "close client failed");
    Require(close_nid_postfix(server) == 0, "close server failed");

    for (const int badFd : {-1, -10, 1000000}) {
        for (const int backlog : {-1, 0, SoMaxConn, 1024}) {
            *__error_nid_postfix() = 0;
            Require(Fails(listen_nid_postfix(badFd, backlog), ErrorBadDescriptor), "listen on bad descriptor must fail with EBADF");
            *__error_nid_postfix() = 0;
            Require(Fails(_listen_nid_postfix(badFd, backlog), ErrorBadDescriptor), "_listen on bad descriptor must fail with EBADF");
        }
    }
}

void AuditPointerFaultValidation() {
    const int sock = socket_nid_postfix(Inet, Stream, 0);
    Require(sock >= 0, "socket failed");
    auto address = LoopbackAddress();

    *__error_nid_postfix() = 0;
    Require(Fails(bind_nid_postfix(sock, nullptr, address.size()), ErrorFault), "bind with null address must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(bind_nid_postfix(-1, nullptr, address.size()), ErrorFault), "bind(-1) with null address must fail with EFAULT");

    *__error_nid_postfix() = 0;
    Require(Fails(connect_nid_postfix(sock, nullptr, address.size()), ErrorFault), "connect with null address must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(connect_nid_postfix(-1, nullptr, address.size()), ErrorFault), "connect(-1) with null address must fail with EFAULT");

    char sendByte = 'z';
    *__error_nid_postfix() = 0;
    Require(Fails(sendto_nid_postfix(sock, nullptr, 1, 0, nullptr, 0), ErrorFault), "sendto with null buffer must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(sendto_nid_postfix(sock, &sendByte, 1, 0, nullptr, address.size()), ErrorFault), "sendto with null address must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(sendto_nid_postfix(-1, &sendByte, 1, 0, nullptr, address.size()), ErrorFault), "sendto(-1) with null address must fail with EFAULT");

    char recvByte = 0;
    *__error_nid_postfix() = 0;
    Require(Fails(recvfrom_nid_postfix(sock, nullptr, 1, 0, nullptr, nullptr), ErrorFault), "recvfrom with null buffer must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(recvfrom_nid_postfix(sock, &recvByte, 1, 0, address.data(), nullptr), ErrorFault), "recvfrom with address and null length must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(recvfrom_nid_postfix(sock, &recvByte, 1, 0, address.data(), reinterpret_cast<std::uint32_t*>(std::uintptr_t{1})), ErrorFault),
        "recvfrom with unmapped address length must fail with EFAULT");

    std::uint32_t validLength = address.size();
    *__error_nid_postfix() = 0;
    Require(Fails(getsockname_nid_postfix(sock, address.data(), nullptr), ErrorFault), "getsockname with null length must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(getsockname_nid_postfix(sock, address.data(), reinterpret_cast<std::uint32_t*>(std::uintptr_t{1})), ErrorFault),
        "getsockname with unmapped length must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(getsockname_nid_postfix(sock, nullptr, &validLength), ErrorFault), "getsockname with null address must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(getsockname_nid_postfix(-1, address.data(), nullptr), ErrorFault), "getsockname(-1) with null length must fail with EFAULT");

    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(sock, address.data(), nullptr), ErrorFault), "getpeername with null length must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(sock, address.data(), reinterpret_cast<std::uint32_t*>(std::uintptr_t{1})), ErrorFault),
        "getpeername with unmapped length must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(sock, nullptr, &validLength), ErrorFault), "getpeername with null address must fail with EFAULT");
    *__error_nid_postfix() = 0;
    Require(Fails(getpeername_nid_postfix(-1, address.data(), nullptr), ErrorFault), "getpeername(-1) with null length must fail with EFAULT");

    Require(close_nid_postfix(sock) == 0, "close failed");
}

void AuditErrorPrecedence() {
    const auto address = LoopbackAddress();

    *__error_nid_postfix() = 0;
    Require(Fails(bind_nid_postfix(-1, nullptr, 256), ErrorNameTooLong), "bind length > 255 precedes null pointer and descriptor");
    *__error_nid_postfix() = 0;
    Require(Fails(bind_nid_postfix(-1, nullptr, 0), ErrorInvalid), "bind length 0 precedes null pointer and descriptor");
    *__error_nid_postfix() = 0;
    Require(Fails(bind_nid_postfix(-1, nullptr, 1), ErrorInvalid), "bind length 1 precedes null pointer and descriptor");
    *__error_nid_postfix() = 0;
    Require(Fails(bind_nid_postfix(-1, nullptr, address.size()), ErrorFault), "bind null pointer precedes descriptor check");
    *__error_nid_postfix() = 0;
    Require(Fails(bind_nid_postfix(-1, address.data(), address.size()), ErrorBadDescriptor), "bind valid address with bad descriptor fails with EBADF");

    *__error_nid_postfix() = 0;
    Require(Fails(connect_nid_postfix(-1, nullptr, 256), ErrorNameTooLong), "connect length > 255 precedes null pointer and descriptor");
    *__error_nid_postfix() = 0;
    Require(Fails(connect_nid_postfix(-1, nullptr, 1), ErrorInvalid), "connect length 1 precedes null pointer and descriptor");
    *__error_nid_postfix() = 0;
    Require(Fails(connect_nid_postfix(-1, nullptr, address.size()), ErrorFault), "connect null pointer precedes descriptor check");
    *__error_nid_postfix() = 0;
    Require(Fails(connect_nid_postfix(-1, address.data(), address.size()), ErrorBadDescriptor), "connect valid address with bad descriptor fails with EBADF");

    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0, "socket failed");
    auto listenerAddress = LoopbackAddress();
    Require(bind_nid_postfix(listener, listenerAddress.data(), listenerAddress.size()) == 0, "bind listener failed");
    std::uint32_t listenerLength = listenerAddress.size();
    Require(getsockname_nid_postfix(listener, listenerAddress.data(), &listenerLength) == 0 && listenerLength == listenerAddress.size(), "getsockname listener failed");
    Require(listen_nid_postfix(listener, 5) == 0, "listen listener failed");

    *__error_nid_postfix() = 0;
    Require(Fails(connect_nid_postfix(listener, nullptr, listenerAddress.size()), ErrorFault),
        "connect on listener with null address must fail with EFAULT before listening check");
    *__error_nid_postfix() = 0;
    Require(Fails(connect_nid_postfix(listener, listenerAddress.data(), 1), ErrorInvalid),
        "connect on listener with 1-byte address must fail with EINVAL before listening check");
    *__error_nid_postfix() = 0;
    Require(Fails(connect_nid_postfix(listener, listenerAddress.data(), listenerAddress.size()), ErrorNotSupported),
        "connect on listener with valid address must fail with EOPNOTSUPP");

    Require(close_nid_postfix(listener) == 0, "close listener failed");
}

}

int main() {
    AuditListenBacklogBounds();
    AuditListenBacklogConnectionCycles();
    AuditListenPrecedenceAndRejection();
    AuditPointerFaultValidation();
    AuditErrorPrecedence();
    return 0;
}
