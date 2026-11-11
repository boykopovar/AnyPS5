#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestZeroInput() {
    *__error_nid_postfix() = 42;
    Require(sceKernelError(0) == 0);
    Require(*__error_nid_postfix() == 42);

    *__error_nid_postfix() = 0;
    Require(sceKernelError(0) == 0);
    Require(*__error_nid_postfix() == 0);
}

void TestStandardPosixErrnoDomain() {
    constexpr std::array<int, 15> knownErrnos{{
        1, 2, 3, 4, 5, 9, 14, 22, 35, 45, 60, 88, 89, 95, 96
    }};

    constexpr std::array<int, 15> expectedErrors{{
        SCE_KERNEL_ERROR_EPERM,
        SCE_KERNEL_ERROR_ENOENT,
        SCE_KERNEL_ERROR_ESRCH,
        SCE_KERNEL_ERROR_EINTR,
        SCE_KERNEL_ERROR_EIO,
        SCE_KERNEL_ERROR_EBADF,
        SCE_KERNEL_ERROR_EFAULT,
        SCE_KERNEL_ERROR_EINVAL,
        SCE_KERNEL_ERROR_EAGAIN,
        SCE_KERNEL_ERROR_EOPNOTSUPP,
        SCE_KERNEL_ERROR_ETIMEDOUT,
        SCE_KERNEL_ERROR_EDOOFUS,
        SCE_KERNEL_ERROR_EBADMSG,
        SCE_KERNEL_ERROR_ENOTRECOVERABLE,
        SCE_KERNEL_ERROR_EOWNERDEAD
    }};

    for (std::size_t i = 0; i < knownErrnos.size(); ++i) {
        *__error_nid_postfix() = 100 + static_cast<int>(i);
        const auto result = sceKernelError(knownErrnos[i]);
        Require(result == expectedErrors[i]);
        Require(result == static_cast<std::int32_t>(0x80020000u + static_cast<std::uint32_t>(knownErrnos[i])));
        Require(*__error_nid_postfix() == 100 + static_cast<int>(i));
    }

    Require(SCE_KERNEL_ERROR_EWOULDBLOCK == SCE_KERNEL_ERROR_EAGAIN);
    Require(SCE_KERNEL_ERROR_ENOTSUP == SCE_KERNEL_ERROR_EOPNOTSUPP);
    Require(SCE_KERNEL_ERROR_ELAST == SCE_KERNEL_ERROR_EOWNERDEAD);
    Require(SCE_KERNEL_ERROR_ELAST == static_cast<std::int32_t>(0x80020060u));
    Require(SCE_KERNEL_ERROR_ENOTRECOVERABLE == static_cast<std::int32_t>(0x8002005fu));

    for (int err = 1; err <= 96; ++err) {
        *__error_nid_postfix() = 77;
        const auto result = sceKernelError(err);
        Require(result == static_cast<std::int32_t>(0x80020000u + static_cast<std::uint32_t>(err)));
        Require(*__error_nid_postfix() == 77);
    }
}

void TestNegativeInputs() {
    *__error_nid_postfix() = 55;
    Require(sceKernelError(-1) == static_cast<std::int32_t>(0x8001ffffu));
    Require(*__error_nid_postfix() == 55);

    *__error_nid_postfix() = 56;
    Require(sceKernelError(-2) == static_cast<std::int32_t>(0x8001fffeu));
    Require(*__error_nid_postfix() == 56);

    *__error_nid_postfix() = 57;
    Require(sceKernelError(-22) == static_cast<std::int32_t>(0x80020000u - 22u));
    Require(*__error_nid_postfix() == 57);

    *__error_nid_postfix() = 58;
    Require(sceKernelError(-96) == static_cast<std::int32_t>(0x80020000u - 96u));
    Require(*__error_nid_postfix() == 58);
}

void TestExtremesAndBoundaries() {
    constexpr auto intMin = std::numeric_limits<std::int32_t>::min();
    constexpr auto intMax = std::numeric_limits<std::int32_t>::max();

    *__error_nid_postfix() = 88;
    Require(sceKernelError(intMin) == 0x20000);
    Require(sceKernelError(intMin + 1) == 0x20001);
    Require(sceKernelError(intMax) == 0x1ffff);
    Require(sceKernelError(intMax - 1) == 0x1fffe);
    Require(*__error_nid_postfix() == 88);
}

void TestModuloWrappingTransitions() {
    constexpr auto wrapToZeroInput = static_cast<std::int32_t>(0x7ffe0000u);
    constexpr auto wrapToOneInput = static_cast<std::int32_t>(0x7ffe0001u);
    constexpr auto wrapToNegOneInput = static_cast<std::int32_t>(0x7ffdffffu);
    constexpr auto wrapToIntMaxInput = static_cast<std::int32_t>(0xfffdffffu);
    constexpr auto wrapToIntMinInput = static_cast<std::int32_t>(0xfffe0000u);
    constexpr auto wrapToIntMinPlusOneInput = static_cast<std::int32_t>(0xfffe0001u);
    constexpr auto biasSelfInput = static_cast<std::int32_t>(0x80020000u);

    *__error_nid_postfix() = 99;
    Require(sceKernelError(wrapToZeroInput) == 0);
    Require(sceKernelError(wrapToOneInput) == 1);
    Require(sceKernelError(wrapToNegOneInput) == -1);
    Require(sceKernelError(wrapToIntMaxInput) == std::numeric_limits<std::int32_t>::max());
    Require(sceKernelError(wrapToIntMinInput) == std::numeric_limits<std::int32_t>::min());
    Require(sceKernelError(wrapToIntMinPlusOneInput) == std::numeric_limits<std::int32_t>::min() + 1);
    Require(sceKernelError(biasSelfInput) == 0x40000);
    Require(*__error_nid_postfix() == 99);
}

void TestModuloArithmeticIdentity() {
    constexpr std::array<std::int32_t, 24> sampleInputs{{
        -2147483648, -2147483647, -2000000000, -1000000000,
        -131073, -131072, -131071, -100, -96, -22, -2, -1,
        1, 2, 22, 96, 100, 131071, 131072, 131073,
        1000000000, 2000000000, 2147483646, 2147483647
    }};

    for (const auto input : sampleInputs) {
        *__error_nid_postfix() = 12;
        const auto result = sceKernelError(input);
        const auto recovered = static_cast<std::uint32_t>(result) - 0x80020000u;
        Require(recovered == static_cast<std::uint32_t>(input));
        Require(*__error_nid_postfix() == 12);
    }
}

} // namespace

int main() {
    TestZeroInput();
    TestStandardPosixErrnoDomain();
    TestNegativeInputs();
    TestExtremesAndBoundaries();
    TestModuloWrappingTransitions();
    TestModuloArithmeticIdentity();
    return 0;
}
