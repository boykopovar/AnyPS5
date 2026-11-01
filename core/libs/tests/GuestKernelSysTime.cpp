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
    for (int clockId : {-2, -100, 32, 100, INT32_MAX}) {
        *__error_nid_postfix() = 0;
        Require(clock_getres_nid_postfix(clockId, &res) == -1);
        Require(*__error_nid_postfix() == GUEST_EINVAL);
        Require(sceKernelClockGetres(static_cast<KernelClockid>(clockId), &res) == SCE_KERNEL_ERROR_EINVAL);
    }
}
