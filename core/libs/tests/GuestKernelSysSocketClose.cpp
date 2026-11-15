#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int GuestEbadf = 9;
constexpr int GuestUnix = 1;
constexpr int GuestInet = 2;
constexpr int GuestStream = 1;
constexpr int GuestDatagram = 2;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "socketclose check failed at line %u (guest errno %d)\n", location.line(), *__error_nid_postfix());
        std::abort();
    }
}

bool FailsWithEbadf(int result) {
    return result == -1 && *__error_nid_postfix() == GuestEbadf;
}

std::array<unsigned char, 16> Loopback() {
    return {16, GuestInet, 0, 0, 127, 0, 0, 1};
}

void TestUnopenedDescriptors() {
    Require(FailsWithEbadf(__sys_socketclose_nid_postfix(-1)));
    Require(FailsWithEbadf(__sys_socketclose_nid_postfix(1000000)));
}

void TestDatagramSocket() {
    const int descriptor = socket_nid_postfix(GuestInet, GuestDatagram, 0);
    Require(descriptor >= 0);
    auto address = Loopback();
    Require(bind_nid_postfix(descriptor, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(descriptor, address.data(), &size) == 0 && size == address.size());

    Require(__sys_socketclose_nid_postfix(descriptor) == 0);
    size = address.size();
    Require(FailsWithEbadf(getsockname_nid_postfix(descriptor, address.data(), &size)));
    Require(FailsWithEbadf(__sys_socketclose_nid_postfix(descriptor)));
    Require(FailsWithEbadf(close_nid_postfix(descriptor)));
}

void TestStreamSocket() {
    const int descriptor = socket_nid_postfix(GuestInet, GuestStream, 0);
    Require(descriptor >= 0);
    Require(__sys_socketclose_nid_postfix(descriptor) == 0);
    Require(FailsWithEbadf(__sys_socketclose_nid_postfix(descriptor)));
}

void TestSocketPair() {
    int pair[2]{-1, -1};
    Require(socketpair_nid_postfix(GuestUnix, GuestStream, 0, pair) == 0);
    Require(__sys_socketclose_nid_postfix(pair[0]) == 0);
    char end = 1;
    Require(recv_nid_postfix(pair[1], &end, 1, 0) == 0);
    Require(FailsWithEbadf(__sys_socketclose_nid_postfix(pair[0])));
    Require(__sys_socketclose_nid_postfix(pair[1]) == 0);
    Require(FailsWithEbadf(__sys_socketclose_nid_postfix(pair[1])));
}

void TestPipe() {
    int pipe[2]{-1, -1};
    Require(pipe_nid_postfix(pipe) == 0);
    Require(__sys_socketclose_nid_postfix(pipe[1]) == 0);
    char end = 1;
    Require(read_nid_postfix(pipe[0], &end, 1) == 0);
    Require(FailsWithEbadf(close_nid_postfix(pipe[1])));
    Require(__sys_socketclose_nid_postfix(pipe[0]) == 0);
    Require(FailsWithEbadf(__sys_socketclose_nid_postfix(pipe[0])));
}

}

int main() {
    TestUnopenedDescriptors();
    TestDatagramSocket();
    TestStreamSocket();
    TestSocketPair();
    TestPipe();
}
