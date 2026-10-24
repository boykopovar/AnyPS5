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

extern "C" {
double APS5_VABI j0_nid_postfix(double);
double APS5_VABI j1_nid_postfix(double);
double APS5_VABI jn_nid_postfix(int, double);
double APS5_VABI y0_nid_postfix(double);
double APS5_VABI y1_nid_postfix(double);
double APS5_VABI yn_nid_postfix(int, double);
float APS5_VABI y0f_nid_postfix(float);
float APS5_VABI y1f_nid_postfix(float);
float APS5_VABI ynf_nid_postfix(int, float);
int* APS5_VABI __error_nid_postfix();
}

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
    CheckIntegerOrders();
    CheckValues();
    return 0;
}
