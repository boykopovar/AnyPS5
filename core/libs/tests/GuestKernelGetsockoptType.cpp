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
        std::fprintf(stderr, "getsockopt SO_TYPE check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int Inet = 2;
constexpr int Stream = 1;
constexpr int Dgram = 2;
constexpr int SolSocket = 65535;
constexpr int SoType = 0x1008;
constexpr int SoError = 0x1007;

int main() {
    const int sockStream = socket_nid_postfix(Inet, Stream, 0);
    Require(sockStream >= 0);

    int optval = 0;
    std::uint32_t optlen = sizeof(optval);
    Require(getsockopt_nid_postfix(sockStream, SolSocket, SoType, &optval, &optlen) == 0);
    Require(optval == Stream);
    Require(optlen == sizeof(optval));

    Require(getsockopt_nid_postfix(sockStream, SolSocket, SoError, &optval, &optlen) == 0);
    Require(optval == 0);

    const int sockDgram = socket_nid_postfix(Inet, Dgram, 0);
    Require(sockDgram >= 0);

    optval = 0;
    optlen = sizeof(optval);
    Require(getsockopt_nid_postfix(sockDgram, SolSocket, SoType, &optval, &optlen) == 0);
    Require(optval == Dgram);

    Require(close_nid_postfix(sockStream) == 0);
    Require(close_nid_postfix(sockDgram) == 0);
    return 0;
}
