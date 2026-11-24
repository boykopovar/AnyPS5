#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    Require(ualarm_nid_postfix(0, 0) == 0);
    Require(ualarm_nid_postfix(500000, 0) == 0);
    const std::uint32_t remaining = ualarm_nid_postfix(0, 0);
    Require(remaining > 400000 && remaining <= 500000);
    Require(ualarm_nid_postfix(0, 0) == 0);

    Require(ualarm_nid_postfix(1500000, 250000) == 0);
    GuestItimerval armed{};
    Require(getitimer_nid_postfix(0, &armed) == 0);
    Require(armed.value.tv_sec == 1 && armed.value.tv_usec > 499000 && armed.value.tv_usec <= 500000);
    Require(armed.interval.tv_sec == 0 && armed.interval.tv_usec == 250000);
    const std::uint32_t left = ualarm_nid_postfix(0, 0);
    Require(left > 1400000 && left <= 1500000);
    Require(ualarm_nid_postfix(0, 0) == 0);

    return 0;
}
