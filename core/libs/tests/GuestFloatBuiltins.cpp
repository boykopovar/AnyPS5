#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <xmmintrin.h>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: requirement failed\n", location.file_name(), location.line());
    std::abort();
}

static __complex__ float TestComplex(float real, float imaginary) {
    __complex__ float value{};
    __real__ value = real;
    __imag__ value = imaginary;
    return value;
}

static __complex__ double TestComplex(double real, double imaginary) {
    __complex__ double value{};
    __real__ value = real;
    __imag__ value = imaginary;
    return value;
}

static __complex__ long double TestComplex(long double real, long double imaginary) {
    __complex__ long double value{};
    __real__ value = real;
    __imag__ value = imaginary;
    return value;
}

static std::uint64_t Bits(double value) { return std::bit_cast<std::uint64_t>(value); }
static std::uint32_t Bits(float value) { return std::bit_cast<std::uint32_t>(value); }
static double Double(std::uint64_t bits) { return std::bit_cast<double>(bits); }
static float Float(std::uint32_t bits) { return std::bit_cast<float>(bits); }
static Uint128 Make(std::uint64_t high, std::uint64_t low) { return (static_cast<Uint128>(high) << 64) | low; }

static constexpr unsigned GuestMxcsr = 0x1f80 | 0x6000 | 0x8040;

static void CheckArithmetic() {
    Require(__adddf3_nid_postfix(1., 2.) == 3. && __subsf3_nid_postfix(1.f, 2.f) == -1.f);
    Require(__muldf3_nid_postfix(3., -4.) == -12. && __divsf3_nid_postfix(1.f, 4.f) == .25f);
    Require(Bits(__adddf3_nid_postfix(INFINITY, -INFINITY)) == 0x7ff8000000000000ull);
    Require(Bits(__muldf3_nid_postfix(0., INFINITY)) == 0x7ff8000000000000ull);
    Require(Bits(__divdf3_nid_postfix(0., 0.)) == 0x7ff8000000000000ull);
    Require(Bits(__subsf3_nid_postfix(INFINITY, INFINITY)) == 0x7fc00000u);
    Require(Bits(__mulsf3_nid_postfix(0.f, -INFINITY)) == 0x7fc00000u);
    Require(Bits(__adddf3_nid_postfix(Double(0xfff0000000000001ull), 1.)) == 0xfff8000000000001ull);
    Require(Bits(__adddf3_nid_postfix(Double(0x7ff8000000000002ull), Double(0x7ff0000000000003ull))) == 0x7ff8000000000002ull);
    Require(Bits(__muldf3_nid_postfix(2., Double(0x7ff0000000000003ull))) == 0x7ff8000000000003ull);
    Require(Bits(__subdf3_nid_postfix(1., Double(0x7ff8000000000004ull))) == 0xfff8000000000004ull);
    Require(Bits(__negdf2_nid_postfix(Double(0x7ff8000000000004ull))) == 0xfff8000000000004ull);
    Require(Bits(__negsf2_nid_postfix(0.f)) == 0x80000000u);
    Require(Bits(__divdf3_nid_postfix(-0x1p-1000, 0x1p30)) == 0x8000000000000000ull);
    Require(Bits(__divsf3_nid_postfix(0x1p-120f, 0x1p10f)) == 0u);
    Require(Bits(__divdf3_nid_postfix(0x1p-1022, 0x1.0000000000001p0)) == 0u);
    Require(__divdf3_nid_postfix(0x1p-1000, 0x1p-22) == 0x1p-978 && __divsf3_nid_postfix(1.f, 3.f) == 1.f / 3.f);
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(GuestMxcsr);
    const double sum = __adddf3_nid_postfix(1., 0x1.8p-53);
    const float denormal = __mulsf3_nid_postfix(0x1p-126f, .5f);
    const double quotient = __divdf3_nid_postfix(1., 3.);
    const unsigned after = _mm_getcsr();
    _mm_setcsr(saved);
    Require(sum == 1. + 0x1p-52);
    Require(Bits(denormal) == 0x00400000u);
    Require(Bits(quotient) == 0x3fd5555555555555ull);
    Require(after == GuestMxcsr);
}

static void CheckComparisons() {
    Require(__ledf2_nid_postfix(1., 2.) == -1 && __ledf2_nid_postfix(2., 1.) == 1 && __ledf2_nid_postfix(2., 2.) == 0);
    Require(__ltdf2_nid_postfix(-2., -1.) == -1 && __gtdf2_nid_postfix(-1., -2.) == 1);
    Require(__eqdf2_nid_postfix(0., -0.) == 0 && __nedf2_nid_postfix(1., -1.) != 0);
    Require(__ledf2_nid_postfix(NAN, 1.) == 1 && __gedf2_nid_postfix(1., NAN) == -1 && __gtdf2_nid_postfix(NAN, NAN) == -1);
    Require(__unorddf2_nid_postfix(1., NAN) == 1 && __unorddf2_nid_postfix(1., INFINITY) == 0);
    Require(__eqsf2_nid_postfix(-0.f, 0.f) == 0 && __ltsf2_nid_postfix(-INFINITY, -1e30f) == -1);
    Require(__gesf2_nid_postfix(NAN, 0.f) == -1 && __unordsf2_nid_postfix(NAN, 0.f) == 1);
    Require(__ledf2_nid_postfix(Double(0x0000000000000001ull), 0.) == 1);
}

static void CheckSingleComparisonsAndAddition() {
    Require(__lesf2_nid_postfix(1.f, 2.f) <= 0 && __lesf2_nid_postfix(2.f, 2.f) <= 0 && __lesf2_nid_postfix(2.f, 1.f) > 0);
    Require(__lesf2_nid_postfix(NAN, 1.f) > 0 && __lesf2_nid_postfix(1.f, NAN) > 0);
    Require(__gtsf2_nid_postfix(-1.f, -2.f) > 0 && __gtsf2_nid_postfix(-2.f, -1.f) <= 0 && __gtsf2_nid_postfix(2.f, 2.f) <= 0);
    Require(__gtsf2_nid_postfix(1.f, NAN) <= 0 && __gtsf2_nid_postfix(NAN, NAN) <= 0);
    Require(__nesf2_nid_postfix(1.f, -1.f) != 0 && __nesf2_nid_postfix(NAN, 0.f) != 0 && __nesf2_nid_postfix(0.f, -0.f) == 0);
    Require(__addsf3_nid_postfix(1.f, 2.f) == 3.f && __addsf3_nid_postfix(-1.5f, .25f) == -1.25f);
    Require(__addsf3_nid_postfix(0x1p-149f, 0x1p-149f) == 0x1p-148f);
    Require(__addsf3_nid_postfix(0x1.fffffep127f, 0x1.fffffep127f) == INFINITY);
    Require(Bits(__addsf3_nid_postfix(1.f, -1.f)) == 0u);
}

static void CheckFloatConversions() {
    Require(Bits(__extendsfdf2_nid_postfix(Float(0x7f800001u))) == 0x7ff0000000000001ull);
    Require(Bits(__extendsfdf2_nid_postfix(Float(0xffc00001u))) == 0xfff8000000000001ull);
    Require(__extendsfdf2_nid_postfix(Float(1u)) == std::ldexp(1., -149));
    Require(__extendsfdf2_nid_postfix(Float(0x80400000u)) == -std::ldexp(1., -127));
    Require(__extendsfdf2_nid_postfix(1.5f) == 1.5 && __extendsfdf2_nid_postfix(-INFINITY) == -INFINITY);
    Require(Bits(__truncdfsf2_nid_postfix(Double(0x7ff0000020000001ull))) == 0x7fc00001u);
    Require(Bits(__truncdfsf2_nid_postfix(Double(0xfff4000000123456ull))) == 0xffd23456u);
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(GuestMxcsr);
    const float third = __truncdfsf2_nid_postfix(1. / 3.);
    const float tiny = __truncdfsf2_nid_postfix(0x1p-140);
    _mm_setcsr(saved);
    Require(Bits(third) == 0x3eaaaaabu && Bits(tiny) == 0x00000200u);
}

static void CheckFloatToInteger() {
    Require(__fixdfsi_nid_postfix(-3.7) == -3 && __fixsfsi_nid_postfix(.99f) == 0);
    Require(__fixdfsi_nid_postfix(0x1p31) == INT32_MIN && __fixdfsi_nid_postfix(1e10) == 1410065408);
    Require(__fixsfsi_nid_postfix(Float(0x7fc00000u)) == INT32_MIN);
    Require(__fixdfdi_nid_postfix(0x1p63) == INT64_MIN && __fixdfdi_nid_postfix(-0x1p62) == -0x4000000000000000);
    Require(__fixsfdi_nid_postfix(-0x1p40f) == -(1ll << 40));
    Require(__fixdfti_nid_postfix(0x1p100) == static_cast<Int128>(Make(1ull << 36, 0)));
    Require(__fixsfti_nid_postfix(-0x1p70f) == -static_cast<Int128>(Make(1ull << 6, 0)));
    Require(__fixdfdi_nid_postfix(0x1p64) == 0 && __fixdfdi_nid_postfix(0x1p120) == 1ll << 56);
    Require(__fixdfti_nid_postfix(0x1p200) == static_cast<Int128>(1) << 72);
    Require(__fixunsdfsi_nid_postfix(-1.) == 0 && __fixunsdfsi_nid_postfix(4294967295.) == 0xffffffffu);
    Require(__fixunsdfsi_nid_postfix(0x1p32) == 1);
    Require(__fixunssfsi_nid_postfix(0x1p32f) == 0 && __fixunssfsi_nid_postfix(0x1p55f) == 1u << 23 && __fixunssfdi_nid_postfix(0x1p63f) == 1ull << 63);
    Require(__fixunsdfdi_nid_postfix(18446744073709549568.) == 18446744073709549568ull);
    Require(__fixunssfti_nid_postfix(0x1p127f) == Make(1ull << 63, 0) && __fixunsdfti_nid_postfix(-2.) == 0);
    Require(__fixxfdi_nid_postfix(-5.5L) == -5 && __fixxfdi_nid_postfix(0x1p63L) == INT64_MIN);
    Require(__fixunsxfdi_nid_postfix(0x1p64L) == 1 && __fixunsxfdi_nid_postfix(18446744073709551615.L) == ~0ull);
    Require(__fixxfti_nid_postfix(-0x1p70L) == -static_cast<Int128>(Make(1ull << 6, 0)));
    Require(__fixunsxfti_nid_postfix(0x1.8p100L) == Make(3ull << 35, 0) && __fixunsxfti_nid_postfix(-1.L) == 0);
    Require(__fixunsxfsi_nid_postfix(3e9L) == 3000000000u && __fixunsxfsi_nid_postfix(0x1p32L) == 1);
}

static void CheckIntegerToFloat() {
    Require(__floatsidf_nid_postfix(INT32_MIN) == -0x1p31 && __floatunsidf_nid_postfix(0xffffffffu) == 4294967295.);
    Require(__floatdidf_nid_postfix(-3) == -3. && __floatdisf_nid_postfix(1ll << 40) == 0x1p40f);
    Require(__floatundidf_nid_postfix(~0ull) == 0x1p64 && __floatundisf_nid_postfix(~0ull) == 0x1p64f);
    Require(__floatdixf_nid_postfix(INT64_MIN) == -0x1p63L && __floatundixf_nid_postfix(~0ull) == 18446744073709551615.L);
    const unsigned saved = _mm_getcsr();
    _mm_setcsr(GuestMxcsr);
    const float tie = __floatsisf_nid_postfix(16777217);
    const float up = __floatunsisf_nid_postfix(16777219u);
    const float negative = __floatsisf_nid_postfix(-16777219);
    const double wide = __floattidf_nid_postfix(static_cast<Int128>(Make(0, (1ull << 63) | 0x601)));
    _mm_setcsr(saved);
    Require(tie == 16777216.f && up == 16777220.f && negative == -16777220.f);
    Require(wide == 0x1p63 + 0x800);
    Require(__floattidf_nid_postfix(static_cast<Int128>(Make(1, 1))) == 0x1p64);
    Require(__floattisf_nid_postfix(-static_cast<Int128>(Make(1ull << 40, 0))) == -0x1p104f);
    Require(__floatuntisf_nid_postfix(Make(~0ull, ~0ull)) == INFINITY);
    Require(__floatuntidf_nid_postfix(Make(~0ull, ~0ull)) == 0x1p128);
    Require(__floatuntidf_nid_postfix(0) == 0. && !std::signbit(__floattidf_nid_postfix(0)));
    Require(__floattixf_nid_postfix(-static_cast<Int128>(Make(1, 1))) == -0x1p64L);
    Require(__floattixf_nid_postfix(-static_cast<Int128>(Make(1, 3))) == -(0x1p64L + 4));
    Require(__floatuntixf_nid_postfix(Make(~0ull, ~0ull)) == 0x1p128L);
    Require(__floatuntixf_nid_postfix(Make(0, ~0ull)) == 18446744073709551615.L);
}

static void CheckComplex() {
    const __complex__ float product = __mulsc3_nid_postfix(1.f, 2.f, 3.f, 4.f);
    Require(__real__ product == -5.f && __imag__ product == 10.f);
    const __complex__ double infinite = __muldc3_nid_postfix(INFINITY, NAN, 1., 0.);
    Require(__real__ infinite == INFINITY);
    const __complex__ long double extended = __mulxc3_nid_postfix(1.L, 1.L, 1.L, -1.L);
    Require(__real__ extended == 2.L && __imag__ extended == 0.L);
    const __complex__ double quotient = __divdc3_nid_postfix(6., 8., 3., 4.);
    Require(__real__ quotient == 2. && __imag__ quotient == 0.);
    const __complex__ float single = __divsc3_nid_postfix(1.f, 0.f, 0.f, 1.f);
    Require(__real__ single == 0.f && __imag__ single == -1.f);
    const __complex__ double byZero = __divdc3_nid_postfix(1., 0., 0., 0.);
    Require(__real__ byZero == INFINITY);
    const __complex__ long double large = __divxc3_nid_postfix(1e4000L, 1e4000L, 1e4000L, 1e4000L);
    Require(__real__ large == 1.L && __imag__ large == 0.L);
    const __complex__ double byInfinity = __divdc3_nid_postfix(1., 1., INFINITY, INFINITY);
    Require(__real__ byInfinity == 0. && __imag__ byInfinity == 0.);

    static_assert(sizeof(__complex__ float) == 8);
    static_assert(sizeof(__complex__ double) == 16);
    static_assert(sizeof(__complex__ long double) == 2 * sizeof(long double));

    const __complex__ double z = TestComplex(3., 4.);
    Require(creal_nid_postfix(z) == 3. && cimag_nid_postfix(z) == 4.);
    Require(cabs_nid_postfix(z) == 5.);
    Require(Bits(carg_nid_postfix(TestComplex(1., 1.))) == 0x3FE921FB54442D18ull);
    Require(Bits(carg_nid_postfix(TestComplex(-1., 0.))) == 0x400921FB54442D18ull);
    Require(Bits(carg_nid_postfix(TestComplex(-1., -0.))) == 0xC00921FB54442D18ull);
    const __complex__ double cj = conj_nid_postfix(z);
    Require(__real__ cj == 3. && __imag__ cj == -4.);
    const __complex__ double cjZero = conj_nid_postfix(TestComplex(0.0, 0.0));
    Require(__real__ cjZero == 0.0 && !std::signbit(__real__ cjZero) && __imag__ cjZero == 0.0 && std::signbit(__imag__ cjZero));
    const __complex__ double pj = cproj_nid_postfix(TestComplex(std::numeric_limits<double>::infinity(), 2.));
    Require(std::isinf(__real__ pj) && __imag__ pj == 0.);
    const __complex__ double pjn = cproj_nid_postfix(TestComplex(1., -std::numeric_limits<double>::infinity()));
    Require(std::isinf(__real__ pjn) && std::signbit(__imag__ pjn));
    const __complex__ double pjNeg = cproj_nid_postfix(TestComplex(-std::numeric_limits<double>::infinity(), -0.0));
    Require(std::isinf(__real__ pjNeg) && __real__ pjNeg > 0.0 && __imag__ pjNeg == 0.0 && std::signbit(__imag__ pjNeg));
    const __complex__ double pjNanInf = cproj_nid_postfix(TestComplex(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()));
    Require(std::isinf(__real__ pjNanInf) && __real__ pjNanInf > 0.0 && __imag__ pjNanInf == 0.0 && !std::signbit(__imag__ pjNanInf));
    const __complex__ double pnorm = cproj_nid_postfix(z);
    Require(__real__ pnorm == 3. && __imag__ pnorm == 4.);
    Require(std::isinf(cabs_nid_postfix(TestComplex(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()))));
    Require(std::isinf(cabs_nid_postfix(TestComplex(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()))));

    const __complex__ float zf = TestComplex(3.f, -4.f);
    Require(crealf_nid_postfix(zf) == 3.f && cimagf_nid_postfix(zf) == -4.f);
    Require(cabsf_nid_postfix(zf) == 5.f);
    Require(Bits(cargf_nid_postfix(TestComplex(0.f, 1.f))) == 0x3FC90FDBu);
    Require(cargf_nid_postfix(TestComplex(0x1.e3465ap-1f, 0x1.319e92p-3f)) == 0x1.412088p-3f);
    Require(cargf_nid_postfix(TestComplex(-0x1.00924ap+6f, -0x1.5eaaaap+6f)) == -0x1.19ea4p+1f);
    const __complex__ float cjf = conjf_nid_postfix(zf);
    Require(__real__ cjf == 3.f && __imag__ cjf == 4.f);
    const __complex__ float pjf = cprojf_nid_postfix(TestComplex(std::numeric_limits<float>::infinity(), -1.f));
    Require(std::isinf(__real__ pjf) && std::signbit(__imag__ pjf));
    Require(__imag__ pjf == 0.f);
    const __complex__ float pfnorm = cprojf_nid_postfix(zf);
    Require(__real__ pfnorm == 3.f && __imag__ pfnorm == -4.f);

    const __complex__ long double zl = TestComplex(5.L, 12.L);
    Require(creall_nid_postfix(zl) == 5.L && cimagl_nid_postfix(zl) == 12.L);
    Require(cabsl_nid_postfix(zl) == 13.L);
    Require(cabsl_nid_postfix(TestComplex(3e3000L, 4e3000L)) == 5e3000L);
    Require(cargl_nid_postfix(TestComplex(-1.L, 0.L)) == 3.14159265358979323846264338327950288L);
    const __complex__ long double cjl = conjl_nid_postfix(zl);
    Require(__real__ cjl == 5.L && __imag__ cjl == -12.L);
    const __complex__ long double pjl = cprojl_nid_postfix(TestComplex(-std::numeric_limits<long double>::infinity(), 1.L));
    Require(std::isinf(__real__ pjl) && __real__ pjl > 0.L && __imag__ pjl == 0.L);
    const __complex__ long double pjl2 = cprojl_nid_postfix(TestComplex(1.L, -std::numeric_limits<long double>::infinity()));
    Require(std::isinf(__real__ pjl2) && __real__ pjl2 > 0.L && __imag__ pjl2 == 0.L && std::signbit(__imag__ pjl2));
    const __complex__ long double plnorm = cprojl_nid_postfix(zl);
    Require(__real__ plnorm == 5.L && __imag__ plnorm == 12.L);
}

int main() {
    CheckArithmetic();
    CheckComparisons();
    CheckSingleComparisonsAndAddition();
    CheckFloatConversions();
    CheckFloatToInteger();
    CheckIntegerToFloat();
    CheckComplex();
    Require(__powixf2_nid_postfix(2.L, 10) == 1024.L && __powixf2_nid_postfix(2.L, -2) == .25L);
    Require(__powixf2_nid_postfix(-3.L, 3) == -27.L && __powixf2_nid_postfix(5.L, 0) == 1.L);
    long double signaling;
    const unsigned char signalingBytes[10] = {1, 0, 0, 0, 0, 0, 0, 0x80, 0xff, 0x7f};
    std::memset(&signaling, 0, sizeof(signaling));
    std::memcpy(&signaling, signalingBytes, sizeof(signalingBytes));
    const long double same = __powixf2_nid_postfix(signaling, 1);
    Require(std::memcmp(&same, signalingBytes, sizeof(signalingBytes)) == 0);
    return 0;
}
