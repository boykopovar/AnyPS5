#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int ShutRd = 0;
constexpr int ShutWr = 1;
constexpr int ShutRdWr = 2;

constexpr int ErrorBadDescriptor = 9;
constexpr int ErrorInvalid = 22;
constexpr int ErrorNotSocket = 38;
constexpr int ErrorNotConnected = 57;

constexpr int AfUnix = 1;
constexpr int AfInet = 2;
constexpr int SockStream = 1;
constexpr int SockDgram = 2;

void Require(bool condition) {
    if (!condition) {
        std::fprintf(stderr, "assertion failed, errno %d\n", *__error_nid_postfix());
        std::abort();
    }
}

bool Fails(long result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

void AuditInvalidDescriptors() {
    Require(Fails(shutdown_nid_postfix(-1, ShutRd), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(-1, ShutWr), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(-1, ShutRdWr), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(-100, ShutRd), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(1024, ShutRd), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(65535, ShutWr), ErrorBadDescriptor));

    const int socket = socket_nid_postfix(AfInet, SockStream, 0);
    Require(socket >= 0);
    Require(close_nid_postfix(socket) == 0);
    Require(Fails(shutdown_nid_postfix(socket, ShutRd), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(socket, ShutWr), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(socket, ShutRdWr), ErrorBadDescriptor));

    Require(Fails(shutdown_nid_postfix(-1, -1), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(-1, 3), ErrorBadDescriptor));
    Require(Fails(shutdown_nid_postfix(-1, 999), ErrorBadDescriptor));
}

void AuditNonSocketDescriptors() {
    int ends[2]{-1, -1};
    Require(pipe_nid_postfix(ends) == 0);
    Require(Fails(shutdown_nid_postfix(ends[0], ShutRd), ErrorNotSocket));
    Require(Fails(shutdown_nid_postfix(ends[0], ShutWr), ErrorNotSocket));
    Require(Fails(shutdown_nid_postfix(ends[0], ShutRdWr), ErrorNotSocket));
    Require(Fails(shutdown_nid_postfix(ends[0], -1), ErrorNotSocket));
    Require(Fails(shutdown_nid_postfix(ends[0], 3), ErrorNotSocket));
    Require(Fails(shutdown_nid_postfix(ends[1], ShutRd), ErrorNotSocket));
    Require(close_nid_postfix(ends[0]) == 0);
    Require(close_nid_postfix(ends[1]) == 0);
}

void AuditInvalidHowFlags() {
    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    Require(stream >= 0);
    for (const int how : {-100, -2, -1, 3, 4, 10, 100}) {
        Require(Fails(shutdown_nid_postfix(stream, how), ErrorInvalid));
    }
    Require(close_nid_postfix(stream) == 0);

    const int datagram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(datagram >= 0);
    for (const int how : {-50, -1, 3, 5, 256}) {
        Require(Fails(shutdown_nid_postfix(datagram, how), ErrorInvalid));
    }
    Require(close_nid_postfix(datagram) == 0);

    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);
    for (const int how : {-1, 3, 99}) {
        Require(Fails(shutdown_nid_postfix(pair[0], how), ErrorInvalid));
        Require(Fails(shutdown_nid_postfix(pair[1], how), ErrorInvalid));
    }
    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
}

void AuditNotConnectedSockets() {
    const int stream = socket_nid_postfix(AfInet, SockStream, 0);
    Require(stream >= 0);
    Require(Fails(shutdown_nid_postfix(stream, ShutRd), ErrorNotConnected));
    Require(Fails(shutdown_nid_postfix(stream, ShutWr), ErrorNotConnected));
    Require(Fails(shutdown_nid_postfix(stream, ShutRdWr), ErrorNotConnected));
    Require(close_nid_postfix(stream) == 0);

    const int datagram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(datagram >= 0);
    Require(Fails(shutdown_nid_postfix(datagram, ShutRd), ErrorNotConnected));
    Require(Fails(shutdown_nid_postfix(datagram, ShutWr), ErrorNotConnected));
    Require(Fails(shutdown_nid_postfix(datagram, ShutRdWr), ErrorNotConnected));
    Require(close_nid_postfix(datagram) == 0);
}

void AuditConnectedSocketPair() {
    {
        int pair[2]{-1, -1};
        Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);
        Require(shutdown_nid_postfix(pair[0], ShutWr) == 0);
        Require(close_nid_postfix(pair[0]) == 0);
        Require(close_nid_postfix(pair[1]) == 0);
    }
    {
        int pair[2]{-1, -1};
        Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);
        Require(shutdown_nid_postfix(pair[0], ShutRd) == 0);
        char buffer[16]{};
        Require(recv_nid_postfix(pair[0], buffer, sizeof(buffer), 0) == 0);
        Require(close_nid_postfix(pair[0]) == 0);
        Require(close_nid_postfix(pair[1]) == 0);
    }
    {
        int pair[2]{-1, -1};
        Require(socketpair_nid_postfix(AfUnix, SockStream, 0, pair) == 0);
        Require(shutdown_nid_postfix(pair[0], ShutRdWr) == 0);
        Require(close_nid_postfix(pair[0]) == 0);
        Require(close_nid_postfix(pair[1]) == 0);
    }
}

void AuditConnectedTcpStream() {
    const int listener = socket_nid_postfix(AfInet, SockStream, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, AfInet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(listen_nid_postfix(listener, 4) == 0);
    std::uint32_t addressSize = address.size();
    Require(getsockname_nid_postfix(listener, address.data(), &addressSize) == 0);

    {
        const int client = socket_nid_postfix(AfInet, SockStream, 0);
        Require(client >= 0);
        Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
        const int server = accept_nid_postfix(listener, nullptr, nullptr);
        Require(server >= 0);

        Require(shutdown_nid_postfix(client, ShutWr) == 0);
        const char payload[] = "ping";
        Require(send_nid_postfix(server, payload, sizeof(payload), 0) == sizeof(payload));
        char buffer[16]{};
        Require(recv_nid_postfix(client, buffer, sizeof(buffer), 0) == sizeof(payload));
        Require(std::strcmp(buffer, payload) == 0);
        Require(recv_nid_postfix(server, buffer, sizeof(buffer), 0) == 0);

        Require(close_nid_postfix(client) == 0);
        Require(close_nid_postfix(server) == 0);
    }

    {
        const int client = socket_nid_postfix(AfInet, SockStream, 0);
        Require(client >= 0);
        Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
        const int server = accept_nid_postfix(listener, nullptr, nullptr);
        Require(server >= 0);

        Require(shutdown_nid_postfix(client, ShutRd) == 0);
        char buffer[16]{};
        Require(recv_nid_postfix(client, buffer, sizeof(buffer), 0) == 0);

        Require(close_nid_postfix(client) == 0);
        Require(close_nid_postfix(server) == 0);
    }

    {
        const int client = socket_nid_postfix(AfInet, SockStream, 0);
        Require(client >= 0);
        Require(connect_nid_postfix(client, address.data(), address.size()) == 0);
        const int server = accept_nid_postfix(listener, nullptr, nullptr);
        Require(server >= 0);

        Require(shutdown_nid_postfix(client, ShutRdWr) == 0);
        Require(shutdown_nid_postfix(server, ShutRdWr) == 0);

        Require(close_nid_postfix(client) == 0);
        Require(close_nid_postfix(server) == 0);
    }

    Require(close_nid_postfix(listener) == 0);
}

} // namespace

int main() {
    AuditInvalidDescriptors();
    AuditNonSocketDescriptors();
    AuditInvalidHowFlags();
    AuditNotConnectedSockets();
    AuditConnectedSocketPair();
    AuditConnectedTcpStream();
    return 0;
}
