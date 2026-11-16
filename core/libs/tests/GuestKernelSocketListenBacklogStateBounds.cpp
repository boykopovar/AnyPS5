#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int AfInet = 2;
constexpr int AfInet6 = 28;

constexpr int SockStream = 1;
constexpr int SockDgram = 2;

constexpr int SolSocket = 0xffff;
constexpr int SoAcceptConnection = 0x2;

constexpr int PosixEbadf = 9;
constexpr int PosixEinval = 22;
constexpr int PosixEnotsock = 38;
constexpr int PosixEopnotsupp = 45;
constexpr int PosixEnotconn = 57;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

std::array<std::uint8_t, 16> MakeLoopbackAddress() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    return address;
}

void TestErrorPrecedenceAndDescriptorValidation() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    Require(listen_nid_postfix(-1, 5) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(_listen_nid_postfix(-1, 5) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(listen_nid_postfix(-100, 5) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(_listen_nid_postfix(-100, 5) == -1 && *__error_nid_postfix() == PosixEbadf);

    Require(listen_nid_postfix(99999, 5) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(_listen_nid_postfix(99999, 5) == -1 && *__error_nid_postfix() == PosixEbadf);

    Require(listen_nid_postfix(pipeFds[0], 5) == -1 && *__error_nid_postfix() == PosixEnotsock);
    Require(_listen_nid_postfix(pipeFds[0], 5) == -1 && *__error_nid_postfix() == PosixEnotsock);
    Require(listen_nid_postfix(pipeFds[1], 5) == -1 && *__error_nid_postfix() == PosixEnotsock);
    Require(_listen_nid_postfix(pipeFds[1], 5) == -1 && *__error_nid_postfix() == PosixEnotsock);

    const int dgram = socket_nid_postfix(AfInet, SockDgram, 0);
    Require(dgram >= 0);

    for (const int backlog : {-10, -1, 0, 1, 5, 128}) {
        Require(listen_nid_postfix(dgram, backlog) == -1 && *__error_nid_postfix() == PosixEopnotsupp);
        Require(_listen_nid_postfix(dgram, backlog) == -1 && *__error_nid_postfix() == PosixEopnotsupp);
    }

    const int v6Dgram = socket_nid_postfix(AfInet6, SockDgram, 0);
    if (v6Dgram >= 0) {
        Require(listen_nid_postfix(v6Dgram, 5) == -1 && *__error_nid_postfix() == PosixEopnotsupp);
        Require(_listen_nid_postfix(v6Dgram, 5) == -1 && *__error_nid_postfix() == PosixEopnotsupp);
        Require(close_nid_postfix(v6Dgram) == 0);
    }

    Require(close_nid_postfix(dgram) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);

    Require(listen_nid_postfix(dgram, 5) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(_listen_nid_postfix(dgram, 5) == -1 && *__error_nid_postfix() == PosixEbadf);
}

void TestBacklogBoundsAndReconfiguration() {
    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);

    int accepting = -1;
    std::uint32_t len = sizeof(accepting);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
    Require(accepting == 0);

    for (const int backlog : {-100, -1, 0, 1, 5, 32, 128, 512, 1024, 65535}) {
        Require(listen_nid_postfix(sock, backlog) == 0);
        accepting = -1;
        len = sizeof(accepting);
        Require(getsockopt_nid_postfix(sock, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
        Require(accepting == SoAcceptConnection);

        Require(_listen_nid_postfix(sock, backlog) == 0);
        accepting = -1;
        len = sizeof(accepting);
        Require(_getsockopt_nid_postfix(sock, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
        Require(accepting == SoAcceptConnection);
    }

    Require(close_nid_postfix(sock) == 0);

    const int v6Sock = socket_nid_postfix(AfInet6, SockStream, 0);
    if (v6Sock >= 0) {
        accepting = -1;
        len = sizeof(accepting);
        Require(getsockopt_nid_postfix(v6Sock, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
        Require(accepting == 0);

        for (const int backlog : {-1, 0, 1, 10, 128}) {
            Require(listen_nid_postfix(v6Sock, backlog) == 0);
            accepting = -1;
            len = sizeof(accepting);
            Require(getsockopt_nid_postfix(v6Sock, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
            Require(accepting == SoAcceptConnection);
        }

        Require(close_nid_postfix(v6Sock) == 0);
    }
}

void TestSocketStateTransitions() {
    const int listener = socket_nid_postfix(AfInet, SockStream, 0);
    Require(listener >= 0);

    int accepting = -1;
    std::uint32_t len = sizeof(accepting);
    Require(getsockopt_nid_postfix(listener, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
    Require(accepting == 0);

    std::array<std::uint8_t, 32> peerAddr{};
    std::uint32_t peerLen = peerAddr.size();
    Require(getpeername_nid_postfix(listener, peerAddr.data(), &peerLen) == -1 && *__error_nid_postfix() == PosixEnotconn);

    auto addr = MakeLoopbackAddress();
    Require(bind_nid_postfix(listener, addr.data(), addr.size()) == 0);

    accepting = -1;
    len = sizeof(accepting);
    Require(getsockopt_nid_postfix(listener, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
    Require(accepting == 0);

    Require(listen_nid_postfix(listener, 1) == 0);

    accepting = -1;
    len = sizeof(accepting);
    Require(getsockopt_nid_postfix(listener, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
    Require(accepting == SoAcceptConnection);

    peerLen = peerAddr.size();
    Require(getpeername_nid_postfix(listener, peerAddr.data(), &peerLen) == -1 && *__error_nid_postfix() == PosixEnotconn);

    std::uint32_t localLen = addr.size();
    Require(getsockname_nid_postfix(listener, addr.data(), &localLen) == 0);

    const int client = socket_nid_postfix(AfInet, SockStream, 0);
    Require(client >= 0);

    accepting = -1;
    len = sizeof(accepting);
    Require(getsockopt_nid_postfix(client, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
    Require(accepting == 0);

    Require(connect_nid_postfix(client, addr.data(), addr.size()) == 0);

    accepting = -1;
    len = sizeof(accepting);
    Require(getsockopt_nid_postfix(client, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
    Require(accepting == 0);

    Require(listen_nid_postfix(client, 1) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(_listen_nid_postfix(client, 1) == -1 && *__error_nid_postfix() == PosixEinval);

    const int server = accept_nid_postfix(listener, nullptr, nullptr);
    Require(server >= 0);

    accepting = -1;
    len = sizeof(accepting);
    Require(getsockopt_nid_postfix(server, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
    Require(accepting == 0);

    Require(listen_nid_postfix(server, 1) == -1 && *__error_nid_postfix() == PosixEinval);
    Require(_listen_nid_postfix(server, 1) == -1 && *__error_nid_postfix() == PosixEinval);

    Require(close_nid_postfix(client) == 0);
    Require(close_nid_postfix(server) == 0);
    Require(close_nid_postfix(listener) == 0);

    Require(listen_nid_postfix(listener, 1) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(listen_nid_postfix(client, 1) == -1 && *__error_nid_postfix() == PosixEbadf);
    Require(listen_nid_postfix(server, 1) == -1 && *__error_nid_postfix() == PosixEbadf);
}

void TestUnboundListenAutobind() {
    const int sock = socket_nid_postfix(AfInet, SockStream, 0);
    Require(sock >= 0);

    std::array<std::uint8_t, 32> addr{};
    std::uint32_t len = addr.size();
    Require(getsockname_nid_postfix(sock, addr.data(), &len) == 0);
    Require(len == 16);
    Require(addr[0] == 16);
    Require(addr[1] == AfInet);
    Require(addr[2] == 0 && addr[3] == 0);

    Require(listen_nid_postfix(sock, 1) == 0);

    len = addr.size();
    Require(getsockname_nid_postfix(sock, addr.data(), &len) == 0);
    Require(len == 16);
    Require(addr[0] == 16);
    Require(addr[1] == AfInet);
    const std::uint16_t port = (static_cast<std::uint16_t>(addr[2]) << 8) | addr[3];
    Require(port > 0);

    int accepting = -1;
    len = sizeof(accepting);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoAcceptConnection, &accepting, &len) == 0);
    Require(accepting == SoAcceptConnection);

    Require(close_nid_postfix(sock) == 0);
}

} // namespace

int main() {
    TestErrorPrecedenceAndDescriptorValidation();
    TestBacklogBoundsAndReconfiguration();
    TestSocketStateTransitions();
    TestUnboundListenAutobind();
    return 0;
}
