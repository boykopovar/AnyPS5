#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>

static constexpr int SCE_OK = 0;
static constexpr int GUEST_EINVAL = 22;

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    KernelTimespec res{-1, -1};
    for (int clockId : {32, 100, INT32_MAX}) {
        *__error_nid_postfix() = 0;
        Require(clock_getres_nid_postfix(clockId, &res) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);
        Require(sceKernelClockGetres(static_cast<KernelClockid>(clockId), &res) == SCE_KERNEL_ERROR_EINVAL);
    }
    for (int cpuClockId : {-2, -100}) {
        KernelTimespec cpuRes{-1, -1};
        Require(clock_getres_nid_postfix(cpuClockId, &cpuRes) == SCE_OK);
        Require(cpuRes.tv_sec >= 0 && cpuRes.tv_nsec >= 0);
        Require(sceKernelClockGetres(static_cast<KernelClockid>(cpuClockId), &cpuRes) == SCE_OK);
    }
}
