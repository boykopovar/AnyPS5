#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    int pair[2] = {-1, -1};
    *__error_nid_postfix() = 0;
    int res = socketpair_nid_postfix(999, 1, 0, pair);
    Require(res == -1);
    Require(*__error_nid_postfix() == 47);

    *__error_nid_postfix() = 0;
    res = socketpair_nid_postfix(1, 999, 0, pair);
    Require(res == -1);
    Require(*__error_nid_postfix() == 22);

    *__error_nid_postfix() = 0;
    res = socketpair_nid_postfix(1, 1, 0, nullptr);
    Require(res == -1);
    Require(*__error_nid_postfix() == 14);

    *__error_nid_postfix() = 0;
    res = socketpair_nid_postfix(1, 1, 0, pair);
    Require(res == 0);
    Require(pair[0] >= 0);
    Require(pair[1] >= 0);
    close_nid_postfix(pair[0]);
    close_nid_postfix(pair[1]);

    *__error_nid_postfix() = 0;
    res = socketpair_nid_postfix(1, 2, 0, pair);
    Require(res == 0);
    Require(pair[0] >= 0);
    Require(pair[1] >= 0);
    close_nid_postfix(pair[0]);
    close_nid_postfix(pair[1]);

    return 0;
}
