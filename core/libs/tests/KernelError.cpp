#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

static void Require(bool value) { if (!value) throw std::runtime_error("Kernel error conversion check failed"); }

int main() {
    *__error_nid_postfix() = 123;
    const std::array<std::pair<std::int32_t, std::int32_t>, 8> cases{{
        {0, 0}, {1, SCE_KERNEL_ERROR_EPERM}, {2, SCE_KERNEL_ERROR_ENOENT},
        {4, SCE_KERNEL_ERROR_EINTR}, {14, SCE_KERNEL_ERROR_EFAULT}, {22, SCE_KERNEL_ERROR_EINVAL},
        {95, SCE_KERNEL_ERROR_ENOTRECOVERABLE}, {96, SCE_KERNEL_ERROR_EOWNERDEAD}
    }};
    for (const auto [input, expected] : cases) {
        Require(sceKernelError(input) == expected);
        Require(*__error_nid_postfix() == 123);
    }
    Require(SCE_KERNEL_ERROR_ENOTRECOVERABLE == static_cast<std::int32_t>(0x8002005fu));
    Require(SCE_KERNEL_ERROR_EOWNERDEAD == static_cast<std::int32_t>(0x80020060u));
    Require(sceKernelError(-1) == static_cast<std::int32_t>(0x8001ffffu));
    Require(sceKernelError(std::numeric_limits<std::int32_t>::max()) == 0x1ffff);
    Require(sceKernelError(std::numeric_limits<std::int32_t>::min()) == 0x20000);
    Require(*__error_nid_postfix() == 123);
}
