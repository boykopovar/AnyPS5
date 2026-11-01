#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>
#include <cstdio>

static constexpr int GUEST_EINVAL = 22;

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    KernelTimespec time{-1, -1};
    for (const int clockId : {-1, 999}) {
        *__error_nid_postfix() = 0;
        Require(clock_gettime_nid_postfix(clockId, &time) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);
        *__error_nid_postfix() = 0;
        Require(sceKernelClockGettime(clockId, &time) == SCE_KERNEL_ERROR_EINVAL);
        Require(*__error_nid_postfix() == GUEST_EINVAL);
    }
    std::puts("guest_kernel_sys_time passed");
    return 0;
}
