#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestCpu.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>

extern "C" int signgam_nid_postfix;

constexpr int GuestInvalid = 0x01;
constexpr int GuestDivideByZero = 0x04;
constexpr int GuestToNearest = 0x000;
constexpr int GuestDownward = 0x400;
constexpr int GuestUpward = 0x800;
constexpr int GuestTowardZero = 0xc00;

static void Require(bool value, std::source_location where = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: lgammal pole check failed\n", where.file_name(), where.line());
    std::abort();
}

int main() {
    const auto savedControl = GuestCpu::X87Control();
    const auto savedMxcsr = GuestCpu::Mxcsr();
    const int savedErrno = *__error_nid_postfix();
    const int savedSign = signgam_nid_postfix;
    const std::array poles{0.0L, -0.0L, -1.0L, -2.0L, -3.0L, -64.0L, -4096.0L, -0x1p63L, -0x1p100L, -std::numeric_limits<long double>::max()};
    unsigned checked = 0;
    for (const unsigned precision : {0u, 0x200u, 0x300u}) {
        for (const int rounding : {GuestToNearest, GuestDownward, GuestUpward, GuestTowardZero}) {
            Require(fesetround_nid_postfix(rounding) == 0);
            const auto control = static_cast<std::uint16_t>((savedControl & ~0xf00u) | precision | rounding | 0x3fu);
            GuestCpu::SetX87Control(control);
            GuestCpu::SetMxcsr(GuestCpu::Mxcsr() | 0x1f80u);
            const auto mxcsr = GuestCpu::Mxcsr() & ~0x3fu;
            for (const int pending : {0, GuestInvalid}) {
                for (const long double input : poles) {
                    Require(feclearexcept_nid_postfix(0x3f) == 0);
                    Require(feraiseexcept_nid_postfix(pending) == 0);
                    *__error_nid_postfix() = 91;
                    signgam_nid_postfix = 7;
                    const long double result = lgammal_nid_postfix(input);
                    const int flags = fetestexcept_nid_postfix(0x3f);
                    Require(std::isinf(result) && !std::signbit(result));
                    if (flags != (pending | GuestDivideByZero)) {
                        std::fprintf(stderr, "lgammal(%La): flags 0x%x, expected 0x%x (x87 status 0x%x, MXCSR 0x%x, precision 0x%x, rounding 0x%x)\n", input, static_cast<unsigned>(flags), static_cast<unsigned>(pending | GuestDivideByZero), static_cast<unsigned>(GuestCpu::X87Status()), static_cast<unsigned>(GuestCpu::Mxcsr()), precision, static_cast<unsigned>(rounding));
                        Require(false);
                    }
                    Require(*__error_nid_postfix() == 34);
                    Require(signgam_nid_postfix == 7);
                    Require(GuestCpu::X87Control() == control);
                    Require((GuestCpu::Mxcsr() & ~0x3fu) == mxcsr);
                    ++checked;
                }
            }
        }
    }
    GuestCpu::SetX87Control(static_cast<std::uint16_t>((savedControl & ~0xf00u) | 0x300u | 0x3fu));
    GuestCpu::SetMxcsr(savedMxcsr | 0x1f80u);
    Require(fesetround_nid_postfix(GuestToNearest) == 0);
    for (const long double input : {-2.5L, -0.5L, 0.5L, 1.0L, 2.0L, 3.5L}) {
        const long double expected = std::lgamma(input);
        const long double actual = lgammal_nid_postfix(input);
        Require(actual == expected);
    }
    Require(std::isinf(lgammal_nid_postfix(std::numeric_limits<long double>::infinity())));
    Require(std::isinf(lgammal_nid_postfix(-std::numeric_limits<long double>::infinity())));
    Require(std::isnan(lgammal_nid_postfix(std::numeric_limits<long double>::quiet_NaN())));
    Require(feclearexcept_nid_postfix(0x3f) == 0);
    GuestCpu::SetX87Control(savedControl);
    GuestCpu::SetMxcsr(savedMxcsr);
    *__error_nid_postfix() = savedErrno;
    signgam_nid_postfix = savedSign;
    std::printf("lgammal: %u pole/control/errno cases and non-pole controls passed\n", checked);
    return 0;
}
