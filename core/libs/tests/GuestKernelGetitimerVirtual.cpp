#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

static constexpr int ITIMER_VIRTUAL = 1;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EINVAL = 22;

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    GuestItimerval timer{{11, 22}, {33, 44}};
    *__error_nid_postfix() = 0;
    Require(getitimer_nid_postfix(ITIMER_VIRTUAL, &timer) == 0);
    Require(timer.value.tv_sec == 0 && timer.value.tv_usec == 0);
    Require(timer.interval.tv_sec == 0 && timer.interval.tv_usec == 0);

    *__error_nid_postfix() = 0;
    Require(getitimer_nid_postfix(ITIMER_VIRTUAL, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EFAULT);

    *__error_nid_postfix() = 0;
    Require(getitimer_nid_postfix(999, &timer) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    *__error_nid_postfix() = 0;
    Require(getitimer_nid_postfix(999, nullptr) == -1);
    Require(*__error_nid_postfix() == GUEST_EINVAL);

    return 0;
}
