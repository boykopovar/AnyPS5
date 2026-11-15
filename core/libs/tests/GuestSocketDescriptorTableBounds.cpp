#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int PosixEbadf = 9;
constexpr int PosixEinval = 22;
constexpr int PosixEmfile = 24;
constexpr int PosixEnotsock = 38;

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int GetErrno() {
    return *__error_nid_postfix();
}

void TestDescriptorTableUpperLimits() {
    const int sock = socket_nid_postfix(2, 2, 0);
    Require(sock >= 0 && sock < 1024);

    Require(dup2_nid_postfix(sock, 1024) == -1 && GetErrno() == PosixEmfile);
    Require(dup2_nid_postfix(sock, 1025) == -1 && GetErrno() == PosixEmfile);
    Require(dup2_nid_postfix(sock, 2048) == -1 && GetErrno() == PosixEmfile);
    Require(dup2_nid_postfix(sock, 65535) == -1 && GetErrno() == PosixEmfile);
    Require(dup2_nid_postfix(sock, -1) == -1 && GetErrno() == PosixEbadf);
    Require(dup2_nid_postfix(sock, -10) == -1 && GetErrno() == PosixEbadf);
    Require(dup2_nid_postfix(-1, 0) == -1 && GetErrno() == PosixEbadf);
    Require(dup2_nid_postfix(1024, 0) == -1 && GetErrno() == PosixEbadf);

    Require(fcntl_nid_postfix(sock, 0, 1024) == -1 && GetErrno() == PosixEmfile);
    Require(fcntl_nid_postfix(sock, 0, 1025) == -1 && GetErrno() == PosixEmfile);
    Require(fcntl_nid_postfix(sock, 0, 2048) == -1 && GetErrno() == PosixEmfile);
    Require(fcntl_nid_postfix(sock, 0, -1) == -1 && GetErrno() == PosixEinval);
    Require(_fcntl_nid_postfix(sock, 0, 1024) == -1 && GetErrno() == PosixEmfile);

    Require(close_nid_postfix(sock) == 0);
}

void TestSelectDescriptorTableLimits() {
    Require(select_nid_postfix(-1, nullptr, nullptr, nullptr, nullptr) == -1 && GetErrno() == PosixEinval);
    Require(select_nid_postfix(-10, nullptr, nullptr, nullptr, nullptr) == -1 && GetErrno() == PosixEinval);
    Require(select_nid_postfix(1025, nullptr, nullptr, nullptr, nullptr) == -1 && GetErrno() == PosixEinval);
    Require(select_nid_postfix(2048, nullptr, nullptr, nullptr, nullptr) == -1 && GetErrno() == PosixEinval);

    GuestTimeval zeroTimeout{0, 0};
    Require(select_nid_postfix(0, nullptr, nullptr, nullptr, &zeroTimeout) == 0);

    std::array<std::uint64_t, 1024 / 64> words{};
    words[1023 / 64] |= (std::uint64_t{1} << (1023 % 64));
    Require(select_nid_postfix(1024, words.data(), nullptr, nullptr, &zeroTimeout) == -1 && GetErrno() == PosixEbadf);
}

void TestPollInvalidDescriptorHandling() {
    GuestPollDescriptor negEntries[3] = {
        {-1, 0x1, 0},
        {-5, 0x1, 0},
        {-100, 0x1, 0}
    };
    Require(poll_nid_postfix(negEntries, 3, 0) == 0);
    Require(negEntries[0].revents == 0);
    Require(negEntries[1].revents == 0);
    Require(negEntries[2].revents == 0);

    GuestPollDescriptor badEntries[3] = {
        {1024, 0x1, 0},
        {2048, 0x1, 0},
        {999, 0x1, 0}
    };
    Require(poll_nid_postfix(badEntries, 3, 0) == 3);
    Require(badEntries[0].revents == 0x20);
    Require(badEntries[1].revents == 0x20);
    Require(badEntries[2].revents == 0x20);
}

void TestInvalidFdRejectionAcrossSocketCalls() {
    const int invalidFds[] = {-1, -2, -100, 1024, 1025, 2048, 65535, 1000000};
    const std::uint8_t address[16] = {16, 2, 0, 0, 127, 0, 0, 1};
    std::uint8_t addressBuffer[28]{};
    std::uint32_t addressLength = sizeof(addressBuffer);
    int optionValue = 1;
    std::uint32_t optionLength = sizeof(optionValue);
    char buffer[16]{};
    GuestMsghdr msg{};

    for (const int fd : invalidFds) {
        Require(bind_nid_postfix(fd, address, sizeof(address)) == -1 && GetErrno() == PosixEbadf);
        Require(_bind_nid_postfix(fd, address, sizeof(address)) == -1 && GetErrno() == PosixEbadf);

        Require(listen_nid_postfix(fd, 5) == -1 && GetErrno() == PosixEbadf);
        Require(_listen_nid_postfix(fd, 5) == -1 && GetErrno() == PosixEbadf);

        Require(accept_nid_postfix(fd, nullptr, nullptr) == -1 && GetErrno() == PosixEbadf);
        Require(_accept_nid_postfix(fd, nullptr, nullptr) == -1 && GetErrno() == PosixEbadf);

        Require(connect_nid_postfix(fd, address, sizeof(address)) == -1 && GetErrno() == PosixEbadf);
        Require(_connect_nid_postfix(fd, address, sizeof(address)) == -1 && GetErrno() == PosixEbadf);

        Require(shutdown_nid_postfix(fd, 0) == -1 && GetErrno() == PosixEbadf);
        Require(shutdown_nid_postfix(fd, 1) == -1 && GetErrno() == PosixEbadf);
        Require(shutdown_nid_postfix(fd, 2) == -1 && GetErrno() == PosixEbadf);
        Require(shutdown_nid_postfix(fd, 99) == -1 && GetErrno() == PosixEbadf);
        Require(shutdown_nid_postfix(fd, -1) == -1 && GetErrno() == PosixEbadf);

        addressLength = sizeof(addressBuffer);
        Require(getsockname_nid_postfix(fd, addressBuffer, &addressLength) == -1 && GetErrno() == PosixEbadf);
        addressLength = sizeof(addressBuffer);
        Require(_getsockname_nid_postfix(fd, addressBuffer, &addressLength) == -1 && GetErrno() == PosixEbadf);

        addressLength = sizeof(addressBuffer);
        Require(getpeername_nid_postfix(fd, addressBuffer, &addressLength) == -1 && GetErrno() == PosixEbadf);
        addressLength = sizeof(addressBuffer);
        Require(_getpeername_nid_postfix(fd, addressBuffer, &addressLength) == -1 && GetErrno() == PosixEbadf);

        optionLength = sizeof(optionValue);
        Require(getsockopt_nid_postfix(fd, 0xffff, 4, &optionValue, &optionLength) == -1 && GetErrno() == PosixEbadf);
        optionLength = sizeof(optionValue);
        Require(_getsockopt_nid_postfix(fd, 0xffff, 4, &optionValue, &optionLength) == -1 && GetErrno() == PosixEbadf);

        Require(setsockopt_nid_postfix(fd, 0xffff, 4, &optionValue, sizeof(optionValue)) == -1 && GetErrno() == PosixEbadf);
        Require(_setsockopt_nid_postfix(fd, 0xffff, 4, &optionValue, sizeof(optionValue)) == -1 && GetErrno() == PosixEbadf);

        Require(send_nid_postfix(fd, "test", 4, 0) == -1 && GetErrno() == PosixEbadf);
        Require(recv_nid_postfix(fd, buffer, sizeof(buffer), 0) == -1 && GetErrno() == PosixEbadf);

        Require(sendto_nid_postfix(fd, "test", 4, 0, nullptr, 0) == -1 && GetErrno() == PosixEbadf);
        Require(_sendto_nid_postfix(fd, "test", 4, 0, nullptr, 0) == -1 && GetErrno() == PosixEbadf);

        Require(recvfrom_nid_postfix(fd, buffer, sizeof(buffer), 0, nullptr, nullptr) == -1 && GetErrno() == PosixEbadf);
        Require(_recvfrom_nid_postfix(fd, buffer, sizeof(buffer), 0, nullptr, nullptr) == -1 && GetErrno() == PosixEbadf);

        Require(sendmsg_nid_postfix(fd, &msg, 0) == -1 && GetErrno() == PosixEbadf);
        Require(_sendmsg_nid_postfix(fd, &msg, 0) == -1 && GetErrno() == PosixEbadf);

        Require(recvmsg_nid_postfix(fd, &msg, 0) == -1 && GetErrno() == PosixEbadf);
        Require(_recvmsg_nid_postfix(fd, &msg, 0) == -1 && GetErrno() == PosixEbadf);

        Require(ioctl_nid_postfix(fd, 0x20006601, nullptr) == -1 && GetErrno() == PosixEbadf);
        Require(_ioctl_nid_postfix(fd, 0x20006601, nullptr) == -1 && GetErrno() == PosixEbadf);

        Require(close_nid_postfix(fd) == -1 && GetErrno() == PosixEbadf);
        Require(_close_nid_postfix(fd) == -1 && GetErrno() == PosixEbadf);

        Require(dup_nid_postfix(fd) == -1 && GetErrno() == PosixEbadf);

        Require(fcntl_nid_postfix(fd, 3, 0) == -1 && GetErrno() == PosixEbadf);
        Require(_fcntl_nid_postfix(fd, 3, 0) == -1 && GetErrno() == PosixEbadf);
    }
}

void TestSendfileDescriptorRejection() {
    const int file = open_nid_postfix("/dev/urandom", 0, 0);
    Require(file >= 0);

    Require(sendfile_nid_postfix(file, -1, 0, 16, nullptr, nullptr, 0) == -1 && GetErrno() == PosixEbadf);
    Require(sendfile_nid_postfix(file, 1024, 0, 16, nullptr, nullptr, 0) == -1 && GetErrno() == PosixEbadf);

    const int sock = socket_nid_postfix(2, 2, 0);
    Require(sock >= 0);

    Require(sendfile_nid_postfix(-1, sock, 0, 16, nullptr, nullptr, 0) == -1 && GetErrno() == PosixEbadf);
    Require(sendfile_nid_postfix(1024, sock, 0, 16, nullptr, nullptr, 0) == -1 && GetErrno() == PosixEbadf);

    Require(close_nid_postfix(sock) == 0);
    Require(close_nid_postfix(file) == 0);
}

void TestClosedSocketDescriptorRejection() {
    const int sock = socket_nid_postfix(2, 2, 0);
    Require(sock >= 0 && sock < 1024);

    Require(close_nid_postfix(sock) == 0);
    Require(close_nid_postfix(sock) == -1 && GetErrno() == PosixEbadf);
    Require(_close_nid_postfix(sock) == -1 && GetErrno() == PosixEbadf);

    const std::uint8_t address[16] = {16, 2, 0, 0, 127, 0, 0, 1};
    std::uint8_t addressBuffer[28]{};
    std::uint32_t addressLength = sizeof(addressBuffer);
    int optionValue = 1;
    std::uint32_t optionLength = sizeof(optionValue);
    char buffer[16]{};
    GuestMsghdr msg{};

    Require(bind_nid_postfix(sock, address, sizeof(address)) == -1 && GetErrno() == PosixEbadf);
    Require(_bind_nid_postfix(sock, address, sizeof(address)) == -1 && GetErrno() == PosixEbadf);

    Require(listen_nid_postfix(sock, 5) == -1 && GetErrno() == PosixEbadf);
    Require(_listen_nid_postfix(sock, 5) == -1 && GetErrno() == PosixEbadf);

    Require(accept_nid_postfix(sock, nullptr, nullptr) == -1 && GetErrno() == PosixEbadf);
    Require(_accept_nid_postfix(sock, nullptr, nullptr) == -1 && GetErrno() == PosixEbadf);

    Require(connect_nid_postfix(sock, address, sizeof(address)) == -1 && GetErrno() == PosixEbadf);
    Require(_connect_nid_postfix(sock, address, sizeof(address)) == -1 && GetErrno() == PosixEbadf);

    Require(shutdown_nid_postfix(sock, 2) == -1 && GetErrno() == PosixEbadf);

    addressLength = sizeof(addressBuffer);
    Require(getsockname_nid_postfix(sock, addressBuffer, &addressLength) == -1 && GetErrno() == PosixEbadf);
    addressLength = sizeof(addressBuffer);
    Require(getpeername_nid_postfix(sock, addressBuffer, &addressLength) == -1 && GetErrno() == PosixEbadf);

    optionLength = sizeof(optionValue);
    Require(getsockopt_nid_postfix(sock, 0xffff, 4, &optionValue, &optionLength) == -1 && GetErrno() == PosixEbadf);
    Require(setsockopt_nid_postfix(sock, 0xffff, 4, &optionValue, sizeof(optionValue)) == -1 && GetErrno() == PosixEbadf);

    Require(send_nid_postfix(sock, "test", 4, 0) == -1 && GetErrno() == PosixEbadf);
    Require(recv_nid_postfix(sock, buffer, sizeof(buffer), 0) == -1 && GetErrno() == PosixEbadf);

    Require(sendto_nid_postfix(sock, "test", 4, 0, nullptr, 0) == -1 && GetErrno() == PosixEbadf);
    Require(recvfrom_nid_postfix(sock, buffer, sizeof(buffer), 0, nullptr, nullptr) == -1 && GetErrno() == PosixEbadf);

    Require(sendmsg_nid_postfix(sock, &msg, 0) == -1 && GetErrno() == PosixEbadf);
    Require(recvmsg_nid_postfix(sock, &msg, 0) == -1 && GetErrno() == PosixEbadf);

    Require(ioctl_nid_postfix(sock, 0x20006601, nullptr) == -1 && GetErrno() == PosixEbadf);
    Require(dup_nid_postfix(sock) == -1 && GetErrno() == PosixEbadf);
}

void TestNonSocketDescriptorRejectionVsClosed() {
    int ends[2]{-1, -1};
    Require(pipe_nid_postfix(ends) == 0);
    Require(ends[0] >= 0 && ends[1] >= 0);

    const std::uint8_t address[16] = {16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(ends[0], address, sizeof(address)) == -1 && GetErrno() == PosixEnotsock);
    Require(listen_nid_postfix(ends[0], 5) == -1 && GetErrno() == PosixEnotsock);
    Require(connect_nid_postfix(ends[0], address, sizeof(address)) == -1 && GetErrno() == PosixEnotsock);

    Require(close_nid_postfix(ends[0]) == 0);
    Require(close_nid_postfix(ends[1]) == 0);

    Require(bind_nid_postfix(ends[0], address, sizeof(address)) == -1 && GetErrno() == PosixEbadf);
    Require(listen_nid_postfix(ends[0], 5) == -1 && GetErrno() == PosixEbadf);
    Require(connect_nid_postfix(ends[0], address, sizeof(address)) == -1 && GetErrno() == PosixEbadf);
}

} // namespace

int main() {
    TestDescriptorTableUpperLimits();
    TestSelectDescriptorTableLimits();
    TestPollInvalidDescriptorHandling();
    TestInvalidFdRejectionAcrossSocketCalls();
    TestSendfileDescriptorRejection();
    TestClosedSocketDescriptorRejection();
    TestNonSocketDescriptorRejectionVsClosed();
    return 0;
}
