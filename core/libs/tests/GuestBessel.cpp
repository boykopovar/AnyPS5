#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cerrno>
#include <cfenv>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <limits>

static void Require(bool value) { if (!value) std::abort(); }

static double Value(std::uint64_t bits) { return std::bit_cast<double>(bits); }

static std::uint64_t Bits(double value) { return std::bit_cast<std::uint64_t>(value); }

struct UnaryCase { std::uint64_t input; std::uint64_t output; };
struct OrderCase { int order; std::uint64_t input; std::uint64_t output; };

static void CheckUnary(const char* name, double (APS5_VABI *function)(double), const UnaryCase* cases, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        *__error_nid_postfix() = 12345;
        const double result = function(Value(cases[index].input));
        if (Bits(result) != cases[index].output || *__error_nid_postfix() != 12345) {
            std::fprintf(stderr, "Guest %s failed for %016llx: %016llx\n", name, static_cast<unsigned long long>(cases[index].input),
                         static_cast<unsigned long long>(Bits(result)));
            std::abort();
        }
    }
}

static void CheckOrder(const char* name, double (APS5_VABI *function)(int, double), const OrderCase* cases, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        *__error_nid_postfix() = 12345;
        const double result = function(cases[index].order, Value(cases[index].input));
        if (Bits(result) != cases[index].output || *__error_nid_postfix() != 12345) {
            std::fprintf(stderr, "Guest %s failed for %d, %016llx: %016llx\n", name, cases[index].order,
                         static_cast<unsigned long long>(cases[index].input), static_cast<unsigned long long>(Bits(result)));
            std::abort();
        }
    }
}

static void CheckFirstKind() {
    const UnaryCase zero[] = {
        {0x0000000000000000ull, 0x3ff0000000000000ull}, {0x8000000000000000ull, 0x3ff0000000000000ull}, {0x3ddb7cdfd9d7bdbbull, 0x3ff0000000000000ull},
        {0x3e40000000000000ull, 0x3ff0000000000000ull}, {0x3eb0000000000000ull, 0x3feffffffffff800ull}, {0x3f1fffffffffffffull, 0x3feffffffe000000ull},
        {0x3f20000000000000ull, 0x3feffffffe000000ull}, {0x3fd0000000000000ull, 0x3fef807fc72aa864ull}, {0xbfe0000000000000ull, 0x3fee07f1d54c3f34ull},
        {0x3fe8000000000000ull, 0x3feba7df6a752a19ull}, {0x3fefffffffffffffull, 0x3fe87c7fdbd7b8f0ull}, {0x3ff0000000000000ull, 0x3fe87c7fdbd7b8f0ull},
        {0xbff8000000000000ull, 0x3fe060e46ce9651bull}, {0x3fffffffffffffffull, 0x3fcca873fb24cefcull}, {0x7ff0000000000000ull, 0x0000000000000000ull},
        {0xfff0000000000000ull, 0x0000000000000000ull}, {0x7ff8000000000123ull, 0x7ff8000000000123ull}, {0x3fee4317b35275e1ull, 0x3fe93c4854d71c27ull},
        {0x3fe7084d7429a0caull, 0x3febfce08c913631ull}, {0x3ff896d4ffdc2a13ull, 0x3fdf6faac7e7d3ceull}, {0x3ff28d079588dbb8ull, 0x3fe61dc352e1dc33ull},
        {0x3ffe7d3678a4d9a8ull, 0x3fd1d44a54769967ull},
    };
    CheckUnary("j0", j0_nid_postfix, zero, std::size(zero));
    const UnaryCase one[] = {
        {0x0000000000000000ull, 0x0000000000000000ull}, {0x8000000000000000ull, 0x8000000000000000ull}, {0x3ddb7cdfd9d7bdbbull, 0x3dcb7cdfd9d7bdbbull},
        {0x3e40000000000000ull, 0x3e30000000000000ull}, {0x3eb0000000000000ull, 0x3e9ffffffffffc00ull}, {0x3f1fffffffffffffull, 0x3f0ffffffeffffffull},
        {0x3f20000000000000ull, 0x3f0fffffff000000ull}, {0x3fd0000000000000ull, 0x3fbfc02a9c749ee9ull}, {0xbfe0000000000000ull, 0xbfcf02a71f4870d7ull},
        {0x3fe8000000000000ull, 0x3fd65a01d66b68bdull}, {0x3fefffffffffffffull, 0x3fdc29c9ee970c6cull}, {0x3ff0000000000000ull, 0x3fdc29c9ee970c6cull},
        {0xbff8000000000000ull, 0xbfe1da9da9d6fc81ull}, {0x3fffffffffffffffull, 0x3fe27487958371f0ull}, {0x7ff0000000000000ull, 0x0000000000000000ull},
        {0xfff0000000000000ull, 0x8000000000000000ull}, {0x7ff8000000000123ull, 0x7ff8000000000123ull}, {0x3fecd8b073b795c0ull, 0x3fda038c68b70948ull},
        {0x3fffe99bfa2fcbc2ull, 0x3fe2775e2433b1d8ull}, {0x3ffa917f997cc231ull, 0x3fe267a06350cc21ull},
    };
    CheckUnary("j1", j1_nid_postfix, one, std::size(one));
}

static void CheckSecondKind() {
    const UnaryCase zero[] = {
        {0x0000000000000000ull, 0xfff0000000000000ull}, {0x8000000000000000ull, 0xfff0000000000000ull}, {0xbff0000000000000ull, 0x7ff8000000000000ull},
        {0x8000000000000001ull, 0x7ff8000000000000ull}, {0x7ff0000000000000ull, 0x0000000000000000ull},
    };
    CheckUnary("y0", y0_nid_postfix, zero, std::size(zero));
    const UnaryCase one[] = {
        {0x0000000000000000ull, 0xfff0000000000000ull}, {0x8000000000000000ull, 0xfff0000000000000ull}, {0xbff0000000000000ull, 0x7ff8000000000000ull},
        {0x8000000000000001ull, 0x7ff8000000000000ull}, {0x7ff0000000000000ull, 0x0000000000000000ull}, {0x3bc79ca10c924223ull, 0xc40b9be638027fc9ull},
        {0x3c90000000000000ull, 0xc3445f306dc9c883ull}, {0x0000000000000001ull, 0xfff0000000000000ull},
    };
    CheckUnary("y1", y1_nid_postfix, one, std::size(one));
    Require(std::isnan(y0_nid_postfix(-std::numeric_limits<double>::infinity())) && std::isnan(y1_nid_postfix(-std::numeric_limits<double>::infinity())));
    Require(std::isnan(y0_nid_postfix(std::numeric_limits<double>::quiet_NaN())) && std::isnan(y1_nid_postfix(std::numeric_limits<double>::quiet_NaN())));
    for (const double input : {0.0, -0.0, -1.0, -std::numeric_limits<double>::denorm_min()}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        y0_nid_postfix(input);
        y1_nid_postfix(input);
        yn_nid_postfix(3, input);

        Require(std::fetestexcept(FE_DIVBYZERO | FE_INVALID) == 0);
    }
}

static void CheckFloatSecondKindZero() {
    const char* names[] = {"y0f", "y1f", "ynf"};
    for (int function = 0; function < 3; ++function) {
        for (const int order : {INT_MIN, -3, -2, -1, 0, 1, 2, 3, INT_MAX}) {
            for (const float input : {0.0f, -0.0f}) {
                for (const int flags : {0, FE_INEXACT, FE_DIVBYZERO}) {
                    std::feclearexcept(FE_ALL_EXCEPT);
                    std::feraiseexcept(flags);
                    const int expectedFlags = std::fetestexcept(FE_ALL_EXCEPT);
                    *__error_nid_postfix() = 12345;
                    const float result = function == 0 ? y0f_nid_postfix(input) : function == 1 ? y1f_nid_postfix(input) : ynf_nid_postfix(order, input);
                    const int actualFlags = std::fetestexcept(FE_ALL_EXCEPT);
                    if (std::bit_cast<std::uint32_t>(result) != 0xff800000u || actualFlags != expectedFlags || *__error_nid_postfix() != 12345) {
                        std::fprintf(stderr, "Guest %s(%d, %08x): result %08x, flags %x, expected %x, errno %d\n", names[function], order,
                                     std::bit_cast<std::uint32_t>(input), std::bit_cast<std::uint32_t>(result), actualFlags, expectedFlags, *__error_nid_postfix());
                        std::abort();
                    }
                }
            }
        }
    }
    std::feclearexcept(FE_ALL_EXCEPT);
}

static void CheckFloatSecondKind() {
    struct FloatCase { std::uint32_t input; std::uint32_t output; };
    const FloatCase cases[] = {
        {0x3f800000u, 0x3db4c010u},
        {0xbf800000u, 0x7fc00000u},
        {0x00000000u, 0xff800000u},
        {0x80000000u, 0xff800000u},
        {0x00000001u, 0xc283a57cu},
        {0x80000001u, 0x7fc00000u},
        {0x007fffffu, 0xc25eb226u},
        {0x807fffffu, 0x7fc00000u},
        {0x00800000u, 0xc25eb226u},
        {0x80800000u, 0x7fc00000u},
        {0x3e000000u, 0xbfb1e10eu},
        {0xbe000000u, 0x7fc00000u},
        {0x3f000000u, 0xbee397f5u},
        {0xbf000000u, 0x7fc00000u},
        {0x3f400000u, 0xbe0c7705u},
        {0xbf400000u, 0x7fc00000u},
        {0x3fa00000u, 0x3e843500u},
        {0xbfa00000u, 0x7fc00000u},
        {0x3fc00000u, 0x3ec3d057u},
        {0xbfc00000u, 0x7fc00000u},
        {0x3fe00000u, 0x3eee550eu},
        {0xbfe00000u, 0x7fc00000u},
        {0x40800000u, 0xbc8ac74du},
        {0xc0800000u, 0x7fc00000u},
        {0x41200000u, 0x3d640774u},
        {0xc1200000u, 0x7fc00000u},
        {0x4e800000u, 0xb7cac9c3u},
        {0xce800000u, 0x7fc00000u},
        {0x7f7fffffu, 0x9f469478u},
        {0xff7fffffu, 0x7fc00000u},
        {0x7f800000u, 0x00000000u},
        {0x7fc00123u, 0x7fc00123u},
        {0xffc00123u, 0xffc00123u},
        {0x7f800123u, 0x7fc00123u},
        {0xff800123u, 0xffc00123u},
        {0x31ffffffu, 0xc13fcf5du},
        {0xb1ffffffu, 0x7fc00000u},
        {0x32000000u, 0xc13fcf5du},
        {0xb2000000u, 0x7fc00000u},
        {0x32000001u, 0xc13fcf5du},
        {0xb2000001u, 0x7fc00000u},
        {0x38ffffffu, 0xc0b9ee39u},
        {0xb8ffffffu, 0x7fc00000u},
        {0x39000000u, 0xc0b9ee38u},
        {0xb9000000u, 0x7fc00000u},
        {0x39000001u, 0xc0b9ee38u},
        {0xb9000001u, 0x7fc00000u},
        {0x3f7fffffu, 0x3db4c00au},
        {0xbf7fffffu, 0x7fc00000u},
        {0x3f800001u, 0x3db4c01eu},
        {0xbf800001u, 0x7fc00000u},
        {0x3fffffffu, 0x3f02a7fau},
        {0xbfffffffu, 0x7fc00000u},
        {0x40000000u, 0x3f02a7fbu},
        {0xc0000000u, 0x7fc00000u},
        {0x40000001u, 0x3f02a7fdu},
        {0xc0000001u, 0x7fc00000u},
        {0x4036db67u, 0x3ed732d5u},
        {0xc036db67u, 0x7fc00000u},
        {0x4036db68u, 0x3ed732d3u},
        {0xc036db68u, 0x7fc00000u},
        {0x4036db69u, 0x3ed732d2u},
        {0xc036db69u, 0x7fc00000u},
        {0x40f71c57u, 0x3e2ed535u},
        {0xc0f71c57u, 0x7fc00000u},
        {0x40f71c58u, 0x3e2ed53bu},
        {0xc0f71c58u, 0x7fc00000u},
        {0x40f71c59u, 0x3e2ed544u},
        {0xc0f71c59u, 0x7fc00000u},
        {0x40ffffffu, 0x3e64e2cdu},
        {0xc0ffffffu, 0x7fc00000u},
        {0x41000000u, 0x3e64e2d2u},
        {0xc1000000u, 0x7fc00000u},
        {0x41000001u, 0x3e64e2dcu},
        {0xc1000001u, 0x7fc00000u},
        {0x7effffffu, 0x1f7bfb6du},
        {0xfeffffffu, 0x7fc00000u},
        {0x7f000000u, 0x9e018611u},
        {0xff000000u, 0x7fc00000u},
        {0x7f000001u, 0x1f8d328du},
        {0xff000001u, 0x7fc00000u},
    };
    for (const auto& test : cases) {
        *__error_nid_postfix() = 12345;
        const float result = y0f_nid_postfix(std::bit_cast<float>(test.input));
        if (std::bit_cast<std::uint32_t>(result) != test.output || *__error_nid_postfix() != 12345) {
            std::fprintf(stderr, "Guest y0f failed for %08x: %08x, expected %08x, errno %d\n", test.input,
                         std::bit_cast<std::uint32_t>(result), test.output, *__error_nid_postfix());
            std::abort();
        }
    }
    for (const float input : {-1.0f, -std::numeric_limits<float>::denorm_min(), -std::numeric_limits<float>::max()}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::bit_cast<std::uint32_t>(y0f_nid_postfix(input)) == 0x7fc00000u);
        Require(std::fetestexcept(FE_DIVBYZERO | FE_INVALID) == 0);
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    *__error_nid_postfix() = 12345;
    Require(std::isnan(y0f_nid_postfix(-std::numeric_limits<float>::infinity())));
    Require(std::fetestexcept(FE_INVALID) != 0 && *__error_nid_postfix() == 12345);
    std::feclearexcept(FE_ALL_EXCEPT);
}

static void CheckFloatOne() {
    struct FloatCase { std::uint32_t input; std::uint32_t first; std::uint32_t second; };
    const FloatCase cases[] = {
        {0x3fa00000u, 0x3f02b834u, 0xbf1598e2u},
        {0x3fc00000u, 0x3f0ed4edu, 0xbed31a1fu},
        {0x00000000u, 0x00000000u, 0xff800000u},
        {0x00000001u, 0x00000000u, 0xff800000u},
        {0x3f000000u, 0x3e781539u, 0xbfbc5935u},
        {0x3f800000u, 0x3ee14e4fu, 0xbf47fd90u},
        {0x7f7fffffu, 0x9f469478u, 0x9e3f4fecu},
        {0x247fffffu, 0x23ffffffu, 0xda22f984u},
        {0x24800000u, 0x24000000u, 0xda22f983u},
        {0x24800001u, 0x24000001u, 0xda22f982u},
        {0x31ffffffu, 0x317fffffu, 0xcca2f984u},
        {0x32000000u, 0x31800000u, 0xcca2f983u},
        {0x32000001u, 0x31800001u, 0xcca2f982u},
        {0x3fffffffu, 0x3f13a43du, 0xbddb33dcu},
        {0x40000000u, 0x3f13a43du, 0xbddb33d3u},
        {0x40000001u, 0x3f13a43du, 0xbddb33beu},
        {0x401fffffu, 0x3efe8321u, 0x3e156b8au},
        {0x40200000u, 0x3efe8322u, 0x3e156b6fu},
        {0x40200001u, 0x3efe8321u, 0x3e156b75u},
        {0x4036db67u, 0x3ec7e1b3u, 0x3e90a361u},
        {0x4036db68u, 0x3ec7e1b0u, 0x3e90a363u},
        {0x4036db69u, 0x3ec7e1adu, 0x3e90a365u},
        {0x40f71c57u, 0x3e3e6931u, 0xbe61384fu},
        {0x40f71c58u, 0x3e3e6936u, 0xbe61384au},
        {0x40f71c59u, 0x3e3e693eu, 0xbe613841u},
        {0x40ffffffu, 0x3e70447fu, 0xbe21daa3u},
        {0x41000000u, 0x3e704483u, 0xbe21da9cu},
        {0x41000001u, 0x3e70448du, 0xbe21da8cu},
        {0x47ffffffu, 0xbad3244au, 0x3ac5228cu},
        {0x48000000u, 0xbad4ace7u, 0x3ac37abdu},
        {0x48000001u, 0xbad7b424u, 0x3ac0221du},
        {0x7effffffu, 0x1f7bfb6du, 0x1f0d3ccfu},
        {0x7f000000u, 0x9e018611u, 0x9f8f85b0u},
        {0x7f000001u, 0x1f8d328du, 0x1e732ee7u},
        {0x7f800000u, 0x00000000u, 0x00000000u},
        {0x7fc00123u, 0x7fc00123u, 0x7fc00123u},
    };
    for (const auto& test : cases) {
        for (const std::uint32_t sign : {0u, 0x80000000u}) {
            const std::uint32_t input = test.input | sign;
            const std::uint32_t first = test.first ^ sign;
            const std::uint32_t second = test.input > 0x7f800000u ? test.second | sign : sign != 0 && test.input != 0 ? 0x7fc00000u : test.second;
            for (const bool secondKind : {false, true}) {
                if (secondKind && sign != 0 && test.input == 0x7f800000u) continue;
                *__error_nid_postfix() = 12345;
                const float x = std::bit_cast<float>(input);
                const auto actual = std::bit_cast<std::uint32_t>(secondKind ? y1f_nid_postfix(x) : j1f_nid_postfix(x));
                const auto expected = secondKind ? second : first;
                if (actual != expected || *__error_nid_postfix() != 12345) {
                    std::fprintf(stderr, "Guest %s failed for %08x: %08x, expected %08x, errno %d\n", secondKind ? "y1f" : "j1f", input,
                                 actual, expected, *__error_nid_postfix());
                    std::abort();
                }
            }
        }
    }
    for (const float input : {-1.0f, -std::numeric_limits<float>::denorm_min()}) {
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::isnan(y1f_nid_postfix(input)));
        Require(std::fetestexcept(FE_DIVBYZERO | FE_INVALID) == 0);
    }
    std::feclearexcept(FE_ALL_EXCEPT);
    *__error_nid_postfix() = 12345;
    Require(std::isnan(y1f_nid_postfix(-std::numeric_limits<float>::infinity())));
    Require(std::fetestexcept(FE_INVALID) != 0 && *__error_nid_postfix() == 12345);
    std::feclearexcept(FE_ALL_EXCEPT);
}

static void CheckFloatIntegerOrders() {
    struct FloatOrderCase { int order; std::uint32_t input; std::uint32_t first; std::uint32_t second; };
    const FloatOrderCase cases[] = {
        {0, 0x3f000000u, 0x3f703f8fu, 0xbee397f5u},
        {1, 0x3f000000u, 0x3e781539u, 0xbfbc5935u},
        {2, 0x3f000000u, 0x3cfab54bu, 0xc0ae1fb6u},
        {3, 0x3e000000u, 0x382a8005u, 0xc5234b29u},
        {4, 0x3f800000u, 0x3b224f1cu, 0xc2051d1au},
        {0, 0x00000001u, 0x3f800000u, 0xc283a57cu},
        {1, 0x00000001u, 0x00000000u, 0xff800000u},
        {2, 0x00000001u, 0x00000000u, 0xff800000u},
        {3, 0x80000001u, 0x80000000u, 0x7fc00000u},
        {2, 0x307fffffu, 0x1ffffffeu, 0xdda2f985u},
        {2, 0x30800000u, 0x20000000u, 0xdda2f983u},
        {2, 0x30800001u, 0x20000002u, 0xdda2f981u},
        {3, 0x30800000u, 0x0faaaaabu, 0xeda2f983u},
        {33, 0x30000000u, 0x00000000u, 0xff800000u},
        {34, 0x30000000u, 0x00000000u, 0xff800000u},
        {3, 0x403fffffu, 0x3e9e3d76u, 0xbf09dddfu},
        {3, 0x40400000u, 0x3e9e3d77u, 0xbf09dddeu},
        {3, 0x40400001u, 0x3e9e3d7bu, 0xbf09ddddu},
        {3, 0x3f800000u, 0x3ca04354u, 0xc0ba49dfu},
        {40, 0x3fc00000u, 0x00000000u, 0xff800000u},
        {300, 0x3f800000u, 0x00000000u, 0xff800000u},
        {100, 0x42c7ffffu, 0x3dc55bccu, 0xbe2aed81u},
        {100, 0x42c80000u, 0x3dc55becu, 0xbe2aed70u},
        {100, 0x42c80001u, 0x3dc55bfau, 0xbe2aed5cu},
        {6, 0x501502f9u, 0xb6120061u, 0x3700ca5bu},
        {7, 0x71800000u, 0x2644a000u, 0xa55d4605u},
        {8, 0x7f7fffffu, 0x1e3f4fecu, 0x9f469478u},
        {-1, 0x3f000000u, 0xbe781539u, 0x3fbc5935u},
        {-3, 0x3fa00000u, 0xbd170347u, 0x404ef7cau},
        {-4, 0x3fa00000u, 0x3bc093cdu, 0xc1654535u},
        {3, 0xbfa00000u, 0xbd170347u, 0x7fc00000u},
        {4, 0xbfa00000u, 0x3bc093cdu, 0x7fc00000u},
        {INT_MIN, 0x3f000000u, 0x3e781539u, 0xbfbc5935u},
        {INT_MIN, 0xbf000000u, 0x3e781539u, 0x7fc00000u},
        {INT_MAX, 0x30000000u, 0x00000000u, 0xff800000u},
        {0, 0x00000000u, 0x3f800000u, 0xff800000u},
        {1, 0x80000000u, 0x80000000u, 0xff800000u},
        {2, 0x00000000u, 0x00000000u, 0xff800000u},
        {3, 0x80000000u, 0x80000000u, 0xff800000u},
        {-3, 0x00000000u, 0x80000000u, 0xff800000u},
        {-4, 0x80000000u, 0x00000000u, 0xff800000u},
        {0, 0x7f800000u, 0x00000000u, 0x00000000u},
        {1, 0x7f800000u, 0x00000000u, 0x00000000u},
        {2, 0x7f800000u, 0x00000000u, 0x00000000u},
        {-3, 0x7f800000u, 0x80000000u, 0x00000000u},
        {INT_MIN, 0x7f800000u, 0x00000000u, 0x00000000u},
        {INT_MAX, 0x7f800000u, 0x00000000u, 0x00000000u},
        {2, 0xff800000u, 0x00000000u, 0xffc00000u},
        {3, 0xff800000u, 0x80000000u, 0xffc00000u},
        {-3, 0xff800000u, 0x00000000u, 0xffc00000u},
        {0, 0x7fc00123u, 0x7fc00123u, 0x7fc00123u},
        {1, 0xffc00123u, 0xffc00123u, 0xffc00123u},
        {3, 0x7fc00123u, 0x7fc00123u, 0x7fc00123u},
        {INT_MIN, 0xffc00123u, 0xffc00123u, 0xffc00123u},
    };
    for (const auto& test : cases) {
        for (int kind = 0; kind < 2; ++kind) {
            *__error_nid_postfix() = 12345;
            const float input = std::bit_cast<float>(test.input);
            const float result = kind == 0 ? jnf_nid_postfix(test.order, input) : ynf_nid_postfix(test.order, input);
            const std::uint32_t expected = kind == 0 ? test.first : test.second;
            const bool invalidInfinity = kind == 1 && test.input == 0xff800000u;
            if ((invalidInfinity ? !std::isnan(result) : std::bit_cast<std::uint32_t>(result) != expected) || *__error_nid_postfix() != 12345) {
                std::fprintf(stderr, "Guest %s(%d, %08x): result %08x, expected %08x, errno %d\n", kind == 0 ? "jnf" : "ynf", test.order,
                             test.input, std::bit_cast<std::uint32_t>(result), expected, *__error_nid_postfix());
                std::abort();
            }
        }
    }
    for (const int order : {INT_MIN, -3, -2, -1, 0, 1, 2, 3, INT_MAX}) {
        for (const std::uint32_t bits : {0x80000001u, 0xbf800000u, 0xff7fffffu}) {
            for (const int flags : {0, FE_INEXACT, FE_DIVBYZERO}) {
                std::feclearexcept(FE_ALL_EXCEPT);
                std::feraiseexcept(flags);
                const int expectedFlags = std::fetestexcept(FE_ALL_EXCEPT);
                *__error_nid_postfix() = 12345;
                const float result = ynf_nid_postfix(order, std::bit_cast<float>(bits));
                Require(std::bit_cast<std::uint32_t>(result) == 0x7fc00000u);
                Require(std::fetestexcept(FE_ALL_EXCEPT) == expectedFlags && *__error_nid_postfix() == 12345);
            }
        }
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(std::isnan(ynf_nid_postfix(order, -std::numeric_limits<float>::infinity())));
        Require((std::fetestexcept(FE_INVALID) & FE_INVALID) != 0);
    }
    std::feclearexcept(FE_ALL_EXCEPT);
}

static void CheckIntegerOrders() {
    const OrderCase first[] = {
        {2, 0x3ddb7cdfd9d7bdbbull, 0x3b979ca10c924224ull}, {5, 0xbddb7cdfd9d7bdbbull, 0xb4cfed4f3ec0c466ull}, {33, 0x3e00000000000000ull, 0x0000000000000000ull},
        {34, 0x3e00000000000000ull, 0x0000000000000000ull}, {-3, 0x3e00000000000000ull, 0xb9c5555555555555ull}, {2, 0x3ff8000000000000ull, 0x3fcdb50c80d5039aull},
        {5, 0x3ff0000000000000ull, 0x3f305e3d487e8377ull}, {10, 0x3fe0000000000000ull, 0x3d52637b5990470aull}, {-3, 0x3ff4000000000000ull, 0xbfa2e068dde7f417ull},
        {3, 0xbff4000000000000ull, 0xbfa2e068dde7f417ull}, {4, 0xbff4000000000000ull, 0x3f781279da92ee26ull}, {40, 0x3ff8000000000000ull, 0x34f2a1a3311d8462ull},
        {300, 0x3ff0000000000000ull, 0x0000000000000000ull}, {3, 0x8000000000000000ull, 0x8000000000000000ull}, {3, 0xfff0000000000000ull, 0x8000000000000000ull},
        {2, 0xfff0000000000000ull, 0x0000000000000000ull}, {7, 0x7ff8000000000123ull, 0x7ff8000000000123ull}, {-1, 0x3fe0000000000000ull, 0xbfcf02a71f4870d7ull},
        {0, 0xbfe0000000000000ull, 0x3fee07f1d54c3f34ull}, {3, 0x3fe91c9d97f75a72ull, 0x3f83d672272043afull}, {3, 0x3ff33bf06c96fafdull, 0x3fa0ea842d47fdfdull},
        {3, 0x3ffa0a4f9419df40ull, 0x3fb36db8f4e44fabull}, {3, 0x3ffb2f9912a3a9a6ull, 0x3fb5c4cf9b0be24full},
    };
    CheckOrder("jn", jn_nid_postfix, first, std::size(first));
    const OrderCase second[] = {
        {2, 0x7ff0000000000000ull, 0x0000000000000000ull}, {-3, 0x7ff0000000000000ull, 0x0000000000000000ull}, {3, 0x0000000000000000ull, 0xfff0000000000000ull},
        {-4, 0x8000000000000000ull, 0xfff0000000000000ull}, {3, 0xbff0000000000000ull, 0x7ff8000000000000ull}, {5, 0x7ff8000000000123ull, 0x7ff8000000000123ull},
        {INT_MIN, 0x7ff0000000000000ull, 0x0000000000000000ull},
    };
    CheckOrder("yn", yn_nid_postfix, second, std::size(second));
    for (const double x : {0.75, 2.5, 3.0, 17.25, 1.0e5, 0x1p303}) {
        Require(Bits(jn_nid_postfix(0, x)) == Bits(j0_nid_postfix(x)) && Bits(jn_nid_postfix(1, x)) == Bits(j1_nid_postfix(x)));
        Require(Bits(jn_nid_postfix(-1, x)) == Bits(j1_nid_postfix(-x)) && Bits(jn_nid_postfix(-3, x)) == Bits(-jn_nid_postfix(3, x)));
        Require(Bits(jn_nid_postfix(4, -x)) == Bits(jn_nid_postfix(4, x)) && Bits(jn_nid_postfix(5, -x)) == Bits(-jn_nid_postfix(5, x)));
        Require(Bits(yn_nid_postfix(0, x)) == Bits(y0_nid_postfix(x)) && Bits(yn_nid_postfix(1, x)) == Bits(y1_nid_postfix(x)));
        Require(Bits(yn_nid_postfix(-1, x)) == Bits(-y1_nid_postfix(x)) && Bits(yn_nid_postfix(-4, x)) == Bits(yn_nid_postfix(4, x)));
        if (x < 0x1p302) Require(Bits(yn_nid_postfix(INT_MIN, x)) == Bits(yn_nid_postfix(2, x)));
    }
}

static bool Near(double actual, double expected, double tolerance) {
    return std::fabs(actual - expected) <= std::fabs(expected) * tolerance;
}

struct ReferenceCase { int order; double x; double value; };

static void CheckValues() {
    const ReferenceCase first[] = {
        {0, 0.3, 0.97762624653829609}, {0, 1.7, 0.39798485944610949}, {0, 2.5, -0.048383776468197996}, {0, 7.25, 0.29199692419177900},
        {0, 25.0, 0.096266783275958116}, {0, 123.5, -0.070484621663252289}, {1, 0.3, 0.14831881627310401}, {1, 1.7, 0.57776523152902322},
        {1, 2.5, 0.49709410246427404}, {1, 7.25, 0.068581700653131745}, {1, 25.0, -0.12535024958028990}, {1, 123.5, -0.013948992295178035},
        {2, 0.3, 0.011165861949063964}, {2, 1.7, 0.28173894235274136}, {2, 2.5, 0.44605905843961723}, {2, 7.25, -0.27307783435643231},
        {2, 25.0, -0.10629480324238131}, {2, 123.5, 0.070258727051184628}, {3, 0.3, 0.00055934304774884612}, {3, 1.7, 0.085149926948015264},
        {3, 2.5, 0.21660039103911352}, {3, 7.25, -0.21924533340150819}, {3, 25.0, 0.10834308106150890}, {3, 123.5, 0.016224578596430978},
        {7, 0.3, 3.3805443102187481e-10}, {7, 1.7, 5.8087210824311728e-5}, {7, 2.5, 0.00077655318753348495}, {7, 7.25, 0.25934915542688296},
        {7, 25.0, -0.010168168212703074}, {7, 123.5, 0.027311804641119322},
    };
    const ReferenceCase second[] = {
        {0, 0.3, -0.80727357780451947}, {0, 1.7, 0.45202700018163463}, {0, 2.5, 0.49807035961523189}, {0, 7.25, 0.048422529865202829},
        {0, 25.0, -0.12724943226800614}, {0, 123.5, -0.013663522162735081}, {1, 0.3, -2.2931051383885290}, {1, 1.7, -0.28472624506406838},
        {1, 2.5, 0.14591813796678580}, {1, 7.25, -0.28934799419758705}, {1, 25.0, -0.098829964783237410}, {1, 123.5, 0.070429882264699084},
        {2, 0.3, -14.480094011452341}, {2, 1.7, -0.78699905319818566}, {2, 2.5, -0.38133584924180325}, {2, 7.25, -0.12824266619557167},
        {2, 25.0, 0.11934303508534715}, {2, 123.5, 0.014804087057709964}, {3, 0.3, -190.77481501430935}, {3, 1.7, -1.5670362330493096},
        {3, 2.5, -0.75605549675367100}, {3, 7.25, 0.21859341974485785}, {3, 25.0, 0.11792485039689295}, {3, 123.5, -0.069950397663639652},
        {7, 0.3, -1.3463966616414596e+8}, {7, 1.7, -807.61347285004828}, {7, 2.5, -62.829855967240050}, {7, 7.25, -0.35448064613382143},
        {7, 25.0, 0.16252257251113247}, {7, 123.5, -0.066461555200665692},
    };
    for (const auto& test : first) {
        const double result = test.order == 0 ? j0_nid_postfix(test.x) : test.order == 1 ? j1_nid_postfix(test.x) : jn_nid_postfix(test.order, test.x);
        if (!Near(result, test.value, 1e-12) || !Near(jn_nid_postfix(test.order, -test.x), test.order % 2 != 0 ? -test.value : test.value, 1e-12)) {
            std::fprintf(stderr, "Guest J%d(%g) = %.17g, expected %.17g\n", test.order, test.x, result, test.value);
            std::abort();
        }
    }
    for (const auto& test : second) {
        const double result = test.order == 0 ? y0_nid_postfix(test.x) : test.order == 1 ? y1_nid_postfix(test.x) : yn_nid_postfix(test.order, test.x);
        if (!Near(result, test.value, 1e-12) || !Near(yn_nid_postfix(test.order, test.x), test.value, 1e-12)) {
            std::fprintf(stderr, "Guest Y%d(%g) = %.17g, expected %.17g\n", test.order, test.x, result, test.value);
            std::abort();
        }
    }
    *__error_nid_postfix() = 12345;
    for (const double x : {0x1p-30, 0.5, 3.0, 1.0e10, 0x1p310, std::numeric_limits<double>::max()}) {
        Require(std::isfinite(j0_nid_postfix(x)) && std::isfinite(j1_nid_postfix(x)) && std::isfinite(y0_nid_postfix(x)) && std::isfinite(y1_nid_postfix(x)));
        Require(std::isfinite(jn_nid_postfix(6, x)) && !std::isnan(yn_nid_postfix(6, x)));
    }
    Require(*__error_nid_postfix() == 12345);
    *__error_nid_postfix() = 0;
}

int main() {
    CheckFirstKind();
    CheckSecondKind();
    CheckFloatSecondKindZero();
    CheckFloatSecondKind();
    CheckFloatOne();
    CheckFloatIntegerOrders();
    CheckIntegerOrders();
    CheckValues();
    return 0;
}
