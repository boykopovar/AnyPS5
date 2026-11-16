#include "prx/libc/include/Api.hpp"
#include <cstdint>
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    volatile std::uint16_t value = 3;
    std::uint16_t expected = 3;
    Require(__atomic_compare_exchange_2_nid_postfix(&value, &expected, 4, 0, 0) != 0);
    Require(value == 4);
    return 0;
}
