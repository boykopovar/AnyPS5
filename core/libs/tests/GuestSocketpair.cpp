#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI socketpair_nid_postfix(int, int, int, int*);
std::int64_t APS5_VABI send_nid_postfix(int, const void*, std::uint64_t, int);
std::int64_t APS5_VABI recv_nid_postfix(int, void*, std::uint64_t, int);
int APS5_VABI getsockname_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI getpeername_nid_postfix(int, void*, std::uint32_t*);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI close_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}

static void Require(bool condition) {
    if (!condition) std::abort();
}

static void CheckUnnamedAddresses(int descriptor) {
    std::array<std::uint8_t, 4> address{9, 9, 9, 9};
    std::uint32_t length = address.size();
    Require(getsockname_nid_postfix(descriptor, address.data(), &length) == 0);
    Require(length == 2 && address[0] == 2 && address[1] == 1);
    address = {9, 9, 9, 9};
    length = address.size();
    Require(getpeername_nid_postfix(descriptor, address.data(), &length) == 0);
    Require(length == 2 && address[0] == 2 && address[1] == 1);
}

static void CheckBothDirections(int* pair) {
    const char message[] = "socketpair";
    char received[sizeof(message)]{};
    Require(send_nid_postfix(pair[0], message, sizeof(message), 0) == sizeof(message));
    Require(recv_nid_postfix(pair[1], received, sizeof(received), 0) == sizeof(received));
    Require(std::strcmp(message, received) == 0);
    std::memset(received, 0, sizeof(received));
    Require(send_nid_postfix(pair[1], message, sizeof(message), 0) == sizeof(message));
    Require(recv_nid_postfix(pair[0], received, sizeof(received), 0) == sizeof(received));
    Require(std::strcmp(message, received) == 0);
}

int main() {
    int stream[2] = {-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, stream) == 0);
    Require(stream[0] != stream[1] && stream[0] >= 0x10000000 && stream[1] >= 0x10000000);
    CheckUnnamedAddresses(stream[0]);
    CheckUnnamedAddresses(stream[1]);
    CheckBothDirections(stream);
    Require(close_nid_postfix(stream[0]) == 0);
    Require(close_nid_postfix(stream[1]) == 0);
    Require(close_nid_postfix(stream[0]) == -1 && *__error_nid_postfix() == 9);

    int datagram[2] = {-1, -1};
    Require(socketpair_nid_postfix(1, 2, 0, datagram) == 0);
    CheckUnnamedAddresses(datagram[0]);
    CheckBothDirections(datagram);
    Require(close_nid_postfix(datagram[0]) == 0);
    Require(close_nid_postfix(datagram[1]) == 0);

    int failed[2] = {7, 7};
    Require(socketpair_nid_postfix(2, 1, 0, failed) == -1 && *__error_nid_postfix() == 47);
    Require(failed[0] == -1 && failed[1] == -1);
    Require(socketpair_nid_postfix(1, 3, 0, failed) == -1 && *__error_nid_postfix() == 43);
    Require(failed[0] == -1 && failed[1] == -1);
    Require(socketpair_nid_postfix(1, 1, 6, failed) == -1 && *__error_nid_postfix() == 43);
    Require(socketpair_nid_postfix(1, 1, 0, nullptr) == -1 && *__error_nid_postfix() == 14);

    int connected[2] = {-1, -1};
    Require(socketpair_nid_postfix(1, 1, 0, connected) == 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(connect_nid_postfix(connected[0], address.data(), address.size()) == -1
        && *__error_nid_postfix() == 56);
    Require(close_nid_postfix(connected[0]) == 0);
    Require(close_nid_postfix(connected[1]) == 0);
}
