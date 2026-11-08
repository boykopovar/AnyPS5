#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    alignas(16) static char stack_area[8192];
    GuestStack ss{};
    Require(sigaltstack_nid_postfix(nullptr, &ss) == 0);
    Require((ss.flags & 4) != 0);

    GuestStack new_ss{stack_area, sizeof(stack_area), 0};
    GuestStack old_ss{};
    Require(sigaltstack_nid_postfix(&new_ss, &old_ss) == 0);
    Require((old_ss.flags & 4) != 0);

    GuestStack check_ss{};
    Require(sigaltstack_nid_postfix(nullptr, &check_ss) == 0);
    Require(check_ss.sp == stack_area);
    Require(check_ss.size == sizeof(stack_area));
    Require(check_ss.flags == 0);

    GuestStack disable_ss{nullptr, 0, 2};
    Require(sigaltstack_nid_postfix(&disable_ss, &old_ss) == 0);
    Require(old_ss.sp == stack_area);
    Require(old_ss.flags == 0);

    Require(sigaltstack_nid_postfix(nullptr, &check_ss) == 0);
    Require((check_ss.flags & 2) != 0 || check_ss.sp == nullptr);

    GuestStack enable_ss{stack_area, sizeof(stack_area), 0};
    Require(sigaltstack_nid_postfix(&enable_ss, &old_ss) == 0);

    GuestStack final_disable{nullptr, 0, 1};
    Require(sigaltstack_nid_postfix(&final_disable, &old_ss) == 0);
    Require(old_ss.sp == stack_area);
}
