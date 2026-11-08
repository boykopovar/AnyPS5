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
        std::fprintf(stderr, "getsockopt SO_ACCEPTCONN check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int SolSocket = 65535;
constexpr int SoAcceptconn = 0x1009;

int main() {
    const int sock = socket_nid_postfix(Inet, Stream, 0);
    Require(sock >= 0);

    int optval = -1;
    std::uint32_t optlen = sizeof(optval);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoAcceptconn, &optval, &optlen) == 0);
    Require(optval == 0);
    Require(optlen == sizeof(optval));

    std::array<std::uint8_t, 16> address{16, Inet, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(sock, address.data(), address.size()) == 0);

    optval = -1;
    optlen = sizeof(optval);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoAcceptconn, &optval, &optlen) == 0);
    Require(optval == 0);

    Require(listen_nid_postfix(sock, 1) == 0);

    optval = -1;
    optlen = sizeof(optval);
    Require(getsockopt_nid_postfix(sock, SolSocket, SoAcceptconn, &optval, &optlen) == 0);
    Require(optval == 1);

    Require(close_nid_postfix(sock) == 0);
    return 0;
}
