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
        std::fprintf(stderr, "getpeername enotsock check failed at line %d, errno %d\n", line, *__error_nid_postfix());
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int EBADF = 9;
constexpr int ENOTSOCK = 88;

static bool Fails(std::int64_t result, int error) {
    return result == -1 && *__error_nid_postfix() == error;
}

int main() {
    std::array<std::uint8_t, 16> address{};
    std::uint32_t size = address.size();

    Require(Fails(getpeername_nid_postfix(-1, address.data(), &size), EBADF));
    Require(Fails(getpeername_nid_postfix(99999, address.data(), &size), EBADF));

    int pipefds[2];
    Require(pipe_nid_postfix(pipefds) == 0);
    Require(Fails(getpeername_nid_postfix(pipefds[0], address.data(), &size), ENOTSOCK));
    Require(Fails(getpeername_nid_postfix(pipefds[1], address.data(), &size), ENOTSOCK));
    Require(close_nid_postfix(pipefds[0]) == 0);
    Require(close_nid_postfix(pipefds[1]) == 0);

    return 0;
}
