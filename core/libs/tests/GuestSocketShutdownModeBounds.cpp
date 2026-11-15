#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEbadf = 9;
constexpr int PosixEinval = 22;
constexpr int PosixEpipe = 32;
constexpr int PosixEnotsock = 38;
constexpr int PosixEnotconn = 57;

constexpr int ShutRd = 0;
constexpr int ShutWr = 1;
constexpr int ShutRdWr = 2;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int GetErrno() {
    return *__error_nid_postfix();
}

void TestInvalidHowArgument() {
    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, pair) == 0);
    Require(pair[0] >= 0 && pair[1] >= 0);

    const int invalidHows[] = {-1, -5, -100, 3, 4, 10, 99, 1000};
    for (const int how : invalidHows) {
        Require(shutdown_nid_postfix(pair[0], how) == -1 && GetErrno() == PosixEinval);
        Require(shutdown_nid_postfix(pair[1], how) == -1 && GetErrno() == PosixEinval);
    }

    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
}

void TestShutdownOnNonSocketFd() {
    int pipeEnds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeEnds) == 0);
    Require(pipeEnds[0] >= 0 && pipeEnds[1] >= 0);

    Require(shutdown_nid_postfix(pipeEnds[0], ShutRd) == -1 && GetErrno() == PosixEnotsock);
    Require(shutdown_nid_postfix(pipeEnds[0], ShutWr) == -1 && GetErrno() == PosixEnotsock);
    Require(shutdown_nid_postfix(pipeEnds[0], ShutRdWr) == -1 && GetErrno() == PosixEnotsock);
    Require(shutdown_nid_postfix(pipeEnds[1], ShutRdWr) == -1 && GetErrno() == PosixEnotsock);

    Require(shutdown_nid_postfix(pipeEnds[0], 99) == -1 && GetErrno() == PosixEnotsock);
    Require(shutdown_nid_postfix(pipeEnds[0], -1) == -1 && GetErrno() == PosixEnotsock);

    const int file = open_nid_postfix("/dev/urandom", 0, 0);
    Require(file >= 0);
    Require(shutdown_nid_postfix(file, ShutRdWr) == -1 && GetErrno() == PosixEnotsock);
    Require(shutdown_nid_postfix(file, 99) == -1 && GetErrno() == PosixEnotsock);
    Require(close_nid_postfix(file) == 0);

    Require(close_nid_postfix(pipeEnds[0]) == 0);
    Require(close_nid_postfix(pipeEnds[1]) == 0);

    Require(shutdown_nid_postfix(pipeEnds[0], ShutRd) == -1 && GetErrno() == PosixEbadf);
    Require(shutdown_nid_postfix(pipeEnds[1], ShutRdWr) == -1 && GetErrno() == PosixEbadf);
}

void TestShutdownOnInvalidFd() {
    const int badFds[] = {-1, -5, -100, 1024, 2048, 65535};
    for (const int badFd : badFds) {
        Require(shutdown_nid_postfix(badFd, ShutRd) == -1 && GetErrno() == PosixEbadf);
        Require(shutdown_nid_postfix(badFd, ShutWr) == -1 && GetErrno() == PosixEbadf);
        Require(shutdown_nid_postfix(badFd, ShutRdWr) == -1 && GetErrno() == PosixEbadf);
        Require(shutdown_nid_postfix(badFd, 99) == -1 && GetErrno() == PosixEbadf);
    }
}

void TestShutdownOnUnconnectedSocket() {
    const int udp = socket_nid_postfix(2, 2, 0);
    Require(udp >= 0);

    Require(shutdown_nid_postfix(udp, ShutRd) == -1 && GetErrno() == PosixEnotconn);
    Require(shutdown_nid_postfix(udp, ShutWr) == -1 && GetErrno() == PosixEnotconn);
    Require(shutdown_nid_postfix(udp, ShutRdWr) == -1 && GetErrno() == PosixEnotconn);

    Require(close_nid_postfix(udp) == 0);
}

void TestPostShutdownReadSemantics() {
    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, pair) == 0);
    Require(pair[0] >= 0 && pair[1] >= 0);

    Require(shutdown_nid_postfix(pair[0], ShutRd) == 0);

    char recvBuf[16]{};
    Require(recv_nid_postfix(pair[0], recvBuf, sizeof(recvBuf), 0) == 0);
    Require(recvfrom_nid_postfix(pair[0], recvBuf, sizeof(recvBuf), 0, nullptr, nullptr) == 0);

    GuestMsghdr msg{};
    Require(recvmsg_nid_postfix(pair[0], &msg, 0) == 0);

    int available = -1;
    Require(ioctl_nid_postfix(pair[0], 0x4004667f, &available) == 0 && available == 0);

    Require(send_nid_postfix(pair[0], "ping", 4, 0) == 4);
    Require(recv_nid_postfix(pair[1], recvBuf, sizeof(recvBuf), 0) == 4);

    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
}

void TestPostShutdownWriteSemantics() {
    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, pair) == 0);
    Require(pair[0] >= 0 && pair[1] >= 0);

    Require(shutdown_nid_postfix(pair[0], ShutWr) == 0);

    Require(send_nid_postfix(pair[0], "fail", 4, 0) == -1 && GetErrno() == PosixEpipe);
    Require(sendto_nid_postfix(pair[0], "fail", 4, 0, nullptr, 0) == -1 && GetErrno() == PosixEpipe);

    GuestMsghdr msg{};
    Require(sendmsg_nid_postfix(pair[0], &msg, 0) == -1 && GetErrno() == PosixEpipe);

    Require(send_nid_postfix(pair[1], "pong", 4, 0) == 4);
    char recvBuf[16]{};
    Require(recv_nid_postfix(pair[0], recvBuf, sizeof(recvBuf), 0) == 4);

    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
}

void TestPostShutdownReadWriteSemantics() {
    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, pair) == 0);
    Require(pair[0] >= 0 && pair[1] >= 0);

    Require(shutdown_nid_postfix(pair[0], ShutRdWr) == 0);

    Require(send_nid_postfix(pair[0], "fail", 4, 0) == -1 && GetErrno() == PosixEpipe);
    char recvBuf[16]{};
    Require(recv_nid_postfix(pair[0], recvBuf, sizeof(recvBuf), 0) == 0);

    Require(shutdown_nid_postfix(pair[0], ShutRdWr) == 0);
    Require(shutdown_nid_postfix(pair[0], ShutRd) == 0);
    Require(shutdown_nid_postfix(pair[0], ShutWr) == 0);

    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);
}

void TestShutdownProgressiveTransitions() {
    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, pair) == 0);
    Require(pair[0] >= 0 && pair[1] >= 0);

    Require(shutdown_nid_postfix(pair[0], ShutRd) == 0);
    char recvBuf[16]{};
    Require(recv_nid_postfix(pair[0], recvBuf, sizeof(recvBuf), 0) == 0);
    Require(send_nid_postfix(pair[0], "data", 4, 0) == 4);
    Require(recv_nid_postfix(pair[1], recvBuf, sizeof(recvBuf), 0) == 4);

    Require(shutdown_nid_postfix(pair[0], ShutWr) == 0);
    Require(send_nid_postfix(pair[0], "data", 4, 0) == -1 && GetErrno() == PosixEpipe);
    Require(recv_nid_postfix(pair[0], recvBuf, sizeof(recvBuf), 0) == 0);

    Require(shutdown_nid_postfix(pair[0], ShutRdWr) == 0);

    Require(close_nid_postfix(pair[0]) == 0);
    Require(close_nid_postfix(pair[1]) == 0);

    Require(shutdown_nid_postfix(pair[0], ShutRdWr) == -1 && GetErrno() == PosixEbadf);
}

} // namespace

int main() {
    TestInvalidHowArgument();
    TestShutdownOnNonSocketFd();
    TestShutdownOnInvalidFd();
    TestShutdownOnUnconnectedSocket();
    TestPostShutdownReadSemantics();
    TestPostShutdownWriteSemantics();
    TestPostShutdownReadWriteSemantics();
    TestShutdownProgressiveTransitions();
    return 0;
}
