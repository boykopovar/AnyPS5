#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    int pair[2] = {-1, -1};
    constexpr int nonblock = 0x20000000;
    constexpr int cloexec = 0x10000000;

    *__error_nid_postfix() = 0;
    int res = socketpair_nid_postfix(1, 1, 999, pair);
    Require(res == -1);
    Require(*__error_nid_postfix() == 93);

    *__error_nid_postfix() = 0;
    res = socketpair_nid_postfix(1, 1, nonblock | cloexec, pair);
    Require(res == 0);
    Require(pair[0] >= 0);
    Require(pair[1] >= 0);
    close_nid_postfix(pair[0]);
    close_nid_postfix(pair[1]);

    return 0;
}
