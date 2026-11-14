#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "getpeername check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int NotConnected = 57;
constexpr int InvalidArgument = 22;

static bool Fails(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

int main() {
    const int fresh = socket_nid_postfix(Inet, Stream, 0);
    Require(fresh >= 0);

    std::array<std::uint8_t, 16> address{};
    std::uint32_t size = address.size();
    Require(Fails(getpeername_nid_postfix(fresh, address.data(), &size), NotConnected));

    std::uint32_t invalidSize = 0;
    Require(Fails(getpeername_nid_postfix(fresh, address.data(), &invalidSize), NotConnected));

    std::uint32_t nullSize = address.size();
    Require(Fails(getpeername_nid_postfix(fresh, address.data(), nullptr), InvalidArgument));

    const int listener = socket_nid_postfix(Inet, Stream, 0);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> bindAddr{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(listener, bindAddr.data(), bindAddr.size()) == 0);
    Require(listen_nid_postfix(listener, 1) == 0);

    std::uint32_t listenSize = address.size();
    Require(Fails(getpeername_nid_postfix(listener, address.data(), &listenSize), NotConnected));

    Require(close_nid_postfix(fresh) == 0);
    Require(close_nid_postfix(listener) == 0);
    return 0;
}
