#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestEinval = 22;
constexpr int GuestO_CLOEXEC = 0x00100000;
constexpr int GetDescriptorFlags = 1;
constexpr int SetDescriptorFlags = 2;
constexpr int GetStatusFlags = 3;
constexpr int SetStatusFlags = 4;
constexpr int CloseOnExec = 1;
constexpr int NonBlocking = 4;
constexpr int UnixDomain = 1;
constexpr int StreamSocket = 1;

void Require(bool condition) {
    if (!condition) std::abort();
}

int dup3(int oldfd, int newfd, int flags) {
    if (oldfd == newfd) {
        *__error_nid_postfix() = GuestEinval;
        return -1;
    }
    if ((flags & ~GuestO_CLOEXEC) != 0) {
        *__error_nid_postfix() = GuestEinval;
        return -1;
    }
    if (oldfd < 0 || newfd < 0) {
        *__error_nid_postfix() = GuestEbadf;
        return -1;
    }
    const int result = dup2_nid_postfix(oldfd, newfd);
    if (result < 0) {
        return -1;
    }
    if ((flags & GuestO_CLOEXEC) != 0) {
        if (fcntl_nid_postfix(newfd, SetDescriptorFlags, CloseOnExec) < 0) {
            const int error = *__error_nid_postfix();
            close_nid_postfix(newfd);
            *__error_nid_postfix() = error;
            return -1;
        }
    } else {
        fcntl_nid_postfix(newfd, SetDescriptorFlags, 0);
    }
    return result;
}

void TestSameDescriptorRejection() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    *__error_nid_postfix() = 0;
    Require(dup3(pipeFds[0], pipeFds[0], 0) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(dup3(pipeFds[0], pipeFds[0], GuestO_CLOEXEC) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(dup3(pipeFds[1], pipeFds[1], 0) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(dup3(pipeFds[1], pipeFds[1], GuestO_CLOEXEC) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(dup3(1000000, 1000000, 0) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(dup3(1000000, 1000000, GuestO_CLOEXEC) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(dup3(-1, -1, 0) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    *__error_nid_postfix() = 0;
    Require(dup3(-5, -5, GuestO_CLOEXEC) == -1);
    Require(*__error_nid_postfix() == GuestEinval);

    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestInvalidFlags() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);
    const int target = 260;

    const int invalidFlags[] = {
        1,
        2,
        4,
        8,
        0x10,
        0x100,
        0x1000,
        -1,
        GuestO_CLOEXEC | 1,
        GuestO_CLOEXEC | 2,
        GuestO_CLOEXEC | 0x8,
        ~GuestO_CLOEXEC
    };

    for (int flag : invalidFlags) {
        *__error_nid_postfix() = 0;
        Require(dup3(pipeFds[0], target, flag) == -1);
        Require(*__error_nid_postfix() == GuestEinval);
    }

    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestInvalidOldDescriptor() {
    const int target = 270;

    *__error_nid_postfix() = 0;
    Require(dup3(-1, target, 0) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(dup3(-1, target, GuestO_CLOEXEC) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(dup3(-100, target, 0) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(dup3(-100, target, GuestO_CLOEXEC) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(dup3(1000000, target, 0) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(dup3(1000000, target, GuestO_CLOEXEC) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);
    const int closed = pipeFds[0];
    Require(close_nid_postfix(closed) == 0);

    *__error_nid_postfix() = 0;
    Require(dup3(closed, target, 0) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(dup3(closed, target, GuestO_CLOEXEC) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestNegativeNewDescriptor() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    *__error_nid_postfix() = 0;
    Require(dup3(pipeFds[0], -1, 0) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(dup3(pipeFds[0], -1, GuestO_CLOEXEC) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(dup3(pipeFds[0], -100, 0) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    *__error_nid_postfix() = 0;
    Require(dup3(pipeFds[0], -100, GuestO_CLOEXEC) == -1);
    Require(*__error_nid_postfix() == GuestEbadf);

    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestDuplicationAndFlags() {
    int pipeFds[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds) == 0);

    const int target1 = 280;
    Require(dup3(pipeFds[0], target1, 0) == target1);
    Require(fcntl_nid_postfix(target1, GetDescriptorFlags) == 0);

    const int target2 = 281;
    Require(dup3(pipeFds[0], target2, GuestO_CLOEXEC) == target2);
    Require(fcntl_nid_postfix(target2, GetDescriptorFlags) == CloseOnExec);

    Require(dup3(pipeFds[0], target2, 0) == target2);
    Require(fcntl_nid_postfix(target2, GetDescriptorFlags) == 0);

    Require(dup3(pipeFds[0], target1, GuestO_CLOEXEC) == target1);
    Require(fcntl_nid_postfix(target1, GetDescriptorFlags) == CloseOnExec);

    Require(close_nid_postfix(target1) == 0);
    Require(close_nid_postfix(target2) == 0);
    Require(close_nid_postfix(pipeFds[0]) == 0);
    Require(close_nid_postfix(pipeFds[1]) == 0);
}

void TestPreserveAccessModesAndPayload() {
    int pipeFds1[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds1) == 0);

    const int target = 290;
    const int originalStatus = fcntl_nid_postfix(pipeFds1[0], GetStatusFlags);
    Require(originalStatus >= 0);

    Require(dup3(pipeFds1[0], target, GuestO_CLOEXEC) == target);
    Require(fcntl_nid_postfix(target, GetStatusFlags) == originalStatus);
    Require(fcntl_nid_postfix(target, GetDescriptorFlags) == CloseOnExec);

    Require(fcntl_nid_postfix(pipeFds1[0], SetStatusFlags, NonBlocking) == 0);
    Require((fcntl_nid_postfix(pipeFds1[0], GetStatusFlags) & NonBlocking) != 0);
    Require((fcntl_nid_postfix(target, GetStatusFlags) & NonBlocking) != 0);

    Require(fcntl_nid_postfix(target, SetStatusFlags, 0) == 0);
    Require((fcntl_nid_postfix(pipeFds1[0], GetStatusFlags) & NonBlocking) == 0);
    Require((fcntl_nid_postfix(target, GetStatusFlags) & NonBlocking) == 0);

    const char message[] = "dup3_mode_test";
    const auto messageLength = static_cast<std::int64_t>(sizeof(message));
    Require(write_nid_postfix(pipeFds1[1], message, messageLength) == messageLength);

    char readBuffer[32]{};
    Require(read_nid_postfix(target, readBuffer, sizeof(readBuffer)) == messageLength);
    Require(std::memcmp(readBuffer, message, sizeof(message)) == 0);

    int pipeFds2[2]{-1, -1};
    Require(pipe_nid_postfix(pipeFds2) == 0);

    const char dummyMessage[] = "dummy_data";
    const auto dummyLength = static_cast<std::int64_t>(sizeof(dummyMessage));
    Require(write_nid_postfix(pipeFds2[1], dummyMessage, dummyLength) == dummyLength);

    Require(dup3(pipeFds1[0], pipeFds2[0], GuestO_CLOEXEC) == pipeFds2[0]);
    Require(fcntl_nid_postfix(pipeFds2[0], GetDescriptorFlags) == CloseOnExec);

    const char replacementMessage[] = "replacement_payload";
    const auto replacementLength = static_cast<std::int64_t>(sizeof(replacementMessage));
    Require(write_nid_postfix(pipeFds1[1], replacementMessage, replacementLength) == replacementLength);

    char replacedBuffer[32]{};
    Require(read_nid_postfix(pipeFds2[0], replacedBuffer, sizeof(replacedBuffer)) == replacementLength);
    Require(std::memcmp(replacedBuffer, replacementMessage, sizeof(replacementMessage)) == 0);

    Require(close_nid_postfix(pipeFds1[0]) == 0);

    const char survivingMessage[] = "surviving_after_close";
    const auto survivingLength = static_cast<std::int64_t>(sizeof(survivingMessage));
    Require(write_nid_postfix(pipeFds1[1], survivingMessage, survivingLength) == survivingLength);

    char survivingBuffer[32]{};
    Require(read_nid_postfix(target, survivingBuffer, sizeof(survivingBuffer)) == survivingLength);
    Require(std::memcmp(survivingBuffer, survivingMessage, sizeof(survivingMessage)) == 0);

    Require(close_nid_postfix(target) == 0);
    Require(close_nid_postfix(pipeFds2[0]) == 0);
    Require(close_nid_postfix(pipeFds2[1]) == 0);
    Require(close_nid_postfix(pipeFds1[1]) == 0);
}

void TestSocketDuplication() {
    int sockets[2]{-1, -1};
    Require(socketpair_nid_postfix(UnixDomain, StreamSocket, 0, sockets) == 0);

    const int target = 295;
    Require(dup3(sockets[0], target, GuestO_CLOEXEC) == target);
    Require(fcntl_nid_postfix(target, GetDescriptorFlags) == CloseOnExec);

    const char pingMessage[] = "socket_dup3_ping";
    const auto pingLength = sizeof(pingMessage);
    Require(send_nid_postfix(sockets[1], pingMessage, pingLength, 0) == static_cast<std::int64_t>(pingLength));

    char socketBuffer[32]{};
    Require(recv_nid_postfix(target, socketBuffer, sizeof(socketBuffer), 0) == static_cast<std::int64_t>(pingLength));
    Require(std::memcmp(socketBuffer, pingMessage, pingLength) == 0);

    Require(close_nid_postfix(sockets[0]) == 0);

    const char pongMessage[] = "socket_dup3_pong";
    const auto pongLength = sizeof(pongMessage);
    Require(send_nid_postfix(sockets[1], pongMessage, pongLength, 0) == static_cast<std::int64_t>(pongLength));

    Require(recv_nid_postfix(target, socketBuffer, sizeof(socketBuffer), 0) == static_cast<std::int64_t>(pongLength));
    Require(std::memcmp(socketBuffer, pongMessage, pongLength) == 0);

    Require(close_nid_postfix(target) == 0);
    Require(close_nid_postfix(sockets[1]) == 0);
}

} // namespace

int main() {
    TestSameDescriptorRejection();
    TestInvalidFlags();
    TestInvalidOldDescriptor();
    TestNegativeNewDescriptor();
    TestDuplicationAndFlags();
    TestPreserveAccessModesAndPayload();
    TestSocketDuplication();
    return 0;
}
