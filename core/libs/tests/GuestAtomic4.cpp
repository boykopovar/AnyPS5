#include "prx/libc/include/Api.hpp"
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    volatile unsigned int value = 5;
    Require(_Atomic_fetch_add_4_nid_postfix(&value, 3u, 0) == 5u);
    Require(value == 8u);
    return 0;
}
