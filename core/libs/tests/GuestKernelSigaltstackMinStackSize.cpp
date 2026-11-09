#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    alignas(16) static char area[8192];
    GuestStack previous{};

    *__error_nid_postfix() = 0;
    GuestStack invalidMin{area, 2047, 0};
    Require(sigaltstack_nid_postfix(&invalidMin, &previous) == -1);
    Require(*__error_nid_postfix() == 12);

    *__error_nid_postfix() = 0;
    GuestStack exactMin{area, 2048, 0};
    Require(sigaltstack_nid_postfix(&exactMin, &previous) == 0);

    *__error_nid_postfix() = 0;
    GuestStack invalidFlags{area, 4096, 0x80};
    Require(sigaltstack_nid_postfix(&invalidFlags, &previous) == -1);
    Require(*__error_nid_postfix() == 22);

    const GuestStack disable{nullptr, 0, 4};
    Require(sigaltstack_nid_postfix(&disable, &previous) == 0);
    Require(previous.sp == area && previous.size == 2048 && previous.flags == 0);
}
