#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEbadf = 9;
constexpr int PosixEinval = 22;
constexpr int PosixEnotsock = 38;
constexpr int PosixEopnotsupp = 45;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int GetErrno() {
    return *__error_nid_postfix();
}

void TestListenBacklogBounds() {
    const int sock = socket_nid_postfix(2, 1, 0);
    Require(sock >= 0);

    const int negativeBacklogs[] = {-1, -2, -5, -100, -1000};
    for (const int backlog : negativeBacklogs) {
        Require(listen_nid_postfix(sock, backlog) == 0);
    }

    Require(listen_nid_postfix(sock, 0) == 0);

    const int positiveBacklogs[] = {1, 2, 4, 5, 10, 32, 64, 128};
    for (const int backlog : positiveBacklogs) {
        Require(listen_nid_postfix(sock, backlog) == 0);
    }

    const int largeBacklogs[] = {256, 512, 1024, 65535, 1000000};
    for (const int backlog : largeBacklogs) {
        Require(listen_nid_postfix(sock, backlog) == 0);
    }

    Require(_listen_nid_postfix(sock, -1) == 0);
    Require(_listen_nid_postfix(sock, 0) == 0);
    Require(_listen_nid_postfix(sock, 128) == 0);
    Require(_listen_nid_postfix(sock, 65535) == 0);

    Require(close_nid_postfix(sock) == 0);
}

void TestListenOnBoundStreamSocket() {
    const int sock = socket_nid_postfix(2, 1, 0);
    Require(sock >= 0);

    std::array<std::uint8_t, 16> addr{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(sock, addr.data(), addr.size()) == 0);

    Require(listen_nid_postfix(sock, -1) == 0);
    Require(listen_nid_postfix(sock, 0) == 0);
    Require(listen_nid_postfix(sock, 5) == 0);
    Require(listen_nid_postfix(sock, 1024) == 0);

    Require(_listen_nid_postfix(sock, 5) == 0);

    Require(close_nid_postfix(sock) == 0);
}

void TestListenOnDatagramSocket() {
    const int udp = socket_nid_postfix(2, 2, 0);
    Require(udp >= 0);

    const int backlogs[] = {-1, 0, 1, 5, 128, 1024};
    for (const int backlog : backlogs) {
        Require(listen_nid_postfix(udp, backlog) == -1 && GetErrno() == PosixEopnotsupp);
        Require(_listen_nid_postfix(udp, backlog) == -1 && GetErrno() == PosixEopnotsupp);
    }

    const int udp6 = socket_nid_postfix(28, 2, 0);
    Require(udp6 >= 0);
    Require(listen_nid_postfix(udp6, 1) == -1 && GetErrno() == PosixEopnotsupp);
    Require(_listen_nid_postfix(udp6, 1) == -1 && GetErrno() == PosixEopnotsupp);
    Require(close_nid_postfix(udp6) == 0);

    Require(close_nid_postfix(udp) == 0);
}

void TestListenOnNonSocketDescriptor() {
    int pipeEnds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeEnds) == 0);
    Require(pipeEnds[0] >= 0 && pipeEnds[1] >= 0);

    Require(listen_nid_postfix(pipeEnds[0], 1) == -1 && GetErrno() == PosixEnotsock);
    Require(listen_nid_postfix(pipeEnds[1], 1) == -1 && GetErrno() == PosixEnotsock);
    Require(_listen_nid_postfix(pipeEnds[0], 1) == -1 && GetErrno() == PosixEnotsock);

    Require(listen_nid_postfix(pipeEnds[0], -1) == -1 && GetErrno() == PosixEnotsock);
    Require(listen_nid_postfix(pipeEnds[0], 1024) == -1 && GetErrno() == PosixEnotsock);

    const int file = open_nid_postfix("/dev/urandom", 0, 0);
    Require(file >= 0);
    Require(listen_nid_postfix(file, 1) == -1 && GetErrno() == PosixEnotsock);
    Require(_listen_nid_postfix(file, 1) == -1 && GetErrno() == PosixEnotsock);
    Require(close_nid_postfix(file) == 0);

    Require(close_nid_postfix(pipeEnds[0]) == 0);
    Require(close_nid_postfix(pipeEnds[1]) == 0);

    Require(listen_nid_postfix(pipeEnds[0], 1) == -1 && GetErrno() == PosixEbadf);
    Require(_listen_nid_postfix(pipeEnds[1], 1) == -1 && GetErrno() == PosixEbadf);
}

void TestListenOnInvalidDescriptor() {
    const int badFds[] = {-1, -2, -50, 1024, 2048, 65535};
    for (const int fd : badFds) {
        Require(listen_nid_postfix(fd, 1) == -1 && GetErrno() == PosixEbadf);
        Require(_listen_nid_postfix(fd, 1) == -1 && GetErrno() == PosixEbadf);
    }

    const int sock = socket_nid_postfix(2, 1, 0);
    Require(sock >= 0);
    Require(close_nid_postfix(sock) == 0);

    Require(listen_nid_postfix(sock, 1) == -1 && GetErrno() == PosixEbadf);
    Require(_listen_nid_postfix(sock, 1) == -1 && GetErrno() == PosixEbadf);
}

void TestListenOnConnectedStreamSocket() {
    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, pair) == 0);
    Require(pair[0] >= 0 && pair[1] >= 0);

    Require(listen_nid_postfix(pair[0], 1) == -1 && GetErrno() == PosixEinval);
    Require(listen_nid_postfix(pair[1], 1) == -1 && GetErrno() == PosixEinval);
    Require(_listen_nid_postfix(pair[0], 1) == -1 && GetErrno() == PosixEinval);

    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
}

} // namespace

int main() {
    TestListenBacklogBounds();
    TestListenOnBoundStreamSocket();
    TestListenOnDatagramSocket();
    TestListenOnNonSocketDescriptor();
    TestListenOnInvalidDescriptor();
    TestListenOnConnectedStreamSocket();
    return 0;
}
