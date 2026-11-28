#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal quadrant trig: %s\n", message);
    std::abort();
}

template <typename T>
bool Near(T value, T expected, T ulps) {
    return std::fabs(value - expected) <= ulps * std::numeric_limits<T>::epsilon() * std::fmax(std::fabs(expected), T(1));
}

void TestAtanDouble() {
    const double pi = 3.14159265358979323846;
    Require(_Atan_nid_postfix(0.0, 0x2) == pi / 2, "_Atan 0 0x2 is pi/2");
    Require(_Atan_nid_postfix(0.0, 0x6) == pi, "_Atan 0 0x6 is pi");
    Require(_Atan_nid_postfix(0.0, 0xe) == -pi, "_Atan 0 0xe is -pi");
    const double zero = _Atan_nid_postfix(0.0, 0x8);
    Require(zero == 0.0 && std::signbit(zero), "_Atan 0 0x8 is -0");
    Require(Near(_Atan_nid_postfix(1.0, 0x0), pi / 4, 1.0), "_Atan 1 0 is pi/4");
    const double samples[] = {1e-300, 1e-8, 0.1, 0.25, 0.5, 0.75, 0.9999, 1.0};
    for (double z : samples) {
        const double angle = atan_nid_postfix(z);
        Require(_Atan_nid_postfix(z, 0x0) == angle, "_Atan 0 is atan");
        Require(_Atan_nid_postfix(z, 0x8) == -angle, "_Atan 0x8 negates atan");
        Require(Near(_Atan_nid_postfix(z, 0x2), std::atan2(1.0, z), 2.0), "_Atan 0x2 is pi/2 - atan, the atan of the inverted argument");
        Require(Near(_Atan_nid_postfix(z, 0x4), std::atan2(1.0, -z), 2.0), "_Atan 0x4 is pi/2 + atan");
        Require(Near(_Atan_nid_postfix(z, 0x6), std::atan2(z, -1.0), 2.0), "_Atan 0x6 is pi - atan");
        Require(Near(_Atan_nid_postfix(z, 0xe), std::atan2(-z, -1.0), 2.0), "_Atan 0xe is -(pi - atan)");
        Require(Near(_Atan_nid_postfix(z, 0xa), -std::atan2(1.0, z), 2.0), "_Atan 0xa is -(pi/2 - atan)");
    }
    const double cosines[] = {0.0, 0.2, 0.5, 0.7};
    for (double x : cosines) {
        const double z = x / std::sqrt((1.0 - x) * (1.0 + x));
        Require(Near(_Atan_nid_postfix(z, 0x2), std::acos(x), 4.0), "_Atan 0x2 gives acos as Dinkumware's acos calls it");
        Require(Near(_Atan_nid_postfix(z, 0x4), std::acos(-x), 4.0), "_Atan 0x4 gives acos of a negative argument");
    }
}

void TestAtanOtherWidths() {
    const float samples[] = {0.0f, 0.1f, 0.5f, 1.0f};
    for (float z : samples) {
        Require(_FAtan_nid_postfix(z, 0x0) == static_cast<float>(atan_nid_postfix(z)), "_FAtan 0 is atan rounded to float");
        Require(Near(_FAtan_nid_postfix(z, 0x6), std::atan2(z, -1.0f), 1.0f), "_FAtan 0x6 is pi - atan");
        Require(Near(_FAtan_nid_postfix(z, 0xa), -std::atan2(1.0f, z), 1.0f), "_FAtan 0xa is -(pi/2 - atan)");
        const long double wide = z;
        Require(_LAtan_nid_postfix(wide, 0x0) == atanl_nid_postfix(wide), "_LAtan 0 is atanl");
        Require(Near(_LAtan_nid_postfix(wide, 0x4), std::atan2(1.0L, -wide), 4.0L), "_LAtan 0x4 is pi/2 + atan");
        Require(Near(_LAtan_nid_postfix(wide, 0xe), std::atan2(-wide, -1.0L), 4.0L), "_LAtan 0xe is -(pi - atan)");
    }
}

void TestTan() {
    const double samples[] = {0.0, -0.0, 0.5, -1.25, 3.0, 1e10, 1e-300};
    for (double x : samples) {
        const double value = _Tan_nid_postfix(x, 0);
        const double expected = tan_nid_postfix(x);
        Require(value == expected && std::signbit(value) == std::signbit(expected), "_Tan 0 is tan");
        Require(_FTan_nid_postfix(static_cast<float>(x), 0) == tanf_nid_postfix(static_cast<float>(x)), "_FTan 0 is tanf");
        Require(_LTan_nid_postfix(x, 0) == tanl_nid_postfix(x), "_LTan 0 is tanl");
    }
    Require(std::isnan(_Tan_nid_postfix(std::numeric_limits<double>::quiet_NaN(), 0)), "_Tan NaN is NaN");
    Require(std::isnan(_Tan_nid_postfix(std::numeric_limits<double>::infinity(), 0)), "_Tan infinity is NaN");
}

constexpr int GuestEdom = 33;
constexpr int GuestErange = 34;

void TestTanHalfTurns() {
    const double pi = 3.14159265358979323846;
    const double quarter = tan_nid_postfix(0.25 * pi);
    Require(_Tan_nid_postfix(0.25, 1) == quarter, "_Tan 1/4 half turn is tan of pi/4");
    Require(_Tan_nid_postfix(-0.25, 1) == -quarter, "_Tan -1/4 half turn is -tan of pi/4");
    Require(_Tan_nid_postfix(0.75, 1) == -1.0 / quarter, "_Tan 3/4 half turn stays in [-1/4, 1/4] and inverts");
    Require(_Tan_nid_postfix(2.25, 3) == quarter, "_Tan removes whole turns and reads only bit 0 of the quadrant argument");
    Require(_Tan_nid_postfix(1.25, 2) == tan_nid_postfix(1.25), "_Tan with bit 0 clear takes radians");
    const double one = _Tan_nid_postfix(1.0, 1);
    Require(one == 0.0 && !std::signbit(one), "_Tan one half turn is +0");
    const double huge = _Tan_nid_postfix(1e300, 1);
    Require(huge == 0.0 && !std::signbit(huge), "_Tan of a huge even count of half turns is +0");
    const double negativeZero = _Tan_nid_postfix(-0.0, 1);
    Require(negativeZero == 0.0 && std::signbit(negativeZero), "_Tan -0 is -0");
    *__error_nid_postfix() = 0;
    Require(_Tan_nid_postfix(0.5, 1) == -std::numeric_limits<double>::infinity(), "_Tan 1/2 half turn is -infinity");
    Require(*__error_nid_postfix() == GuestErange, "_Tan 1/2 half turn raises overflow");
    Require(_Tan_nid_postfix(-0.5, 1) == -std::numeric_limits<double>::infinity(), "_Tan -1/2 half turn is -infinity");
    *__error_nid_postfix() = 0;
    Require(std::isnan(_Tan_nid_postfix(-std::numeric_limits<double>::infinity(), 1)), "_Tan infinite half turns is NaN");
    Require(*__error_nid_postfix() == GuestEdom, "_Tan of an infinity raises invalid");
    Require(std::isnan(_Tan_nid_postfix(std::numeric_limits<double>::quiet_NaN(), 1)), "_Tan NaN half turns is NaN");
    const double samples[] = {0.1, -0.3, 0.4999, 2.7, -7.125, 1048576.375, 1e-300, 5e-324};
    for (double x : samples) {
        const long double expected = std::tan(3.14159265358979323846264338327950288L * std::remainder(static_cast<long double>(x), 1.0L));
        Require(Near(static_cast<long double>(_Tan_nid_postfix(x, 1)), expected, 8.0L / std::numeric_limits<long double>::epsilon() * std::numeric_limits<double>::epsilon()), "_Tan half turns is tan(pi x)");
    }
}

void TestTanHalfTurnsOtherWidths() {
    const float quarter = tanf_nid_postfix(0.25f * 3.14159265358979323846f);
    Require(_FTan_nid_postfix(0.25f, 1) == quarter, "_FTan 1/4 half turn is tanf of pi/4");
    Require(_FTan_nid_postfix(-1.75f, 1) == 1.0f / quarter, "_FTan -7/4 half turns reduces to -1/4 in an odd quadrant");
    Require(_FTan_nid_postfix(0.5f, 1) == -std::numeric_limits<float>::infinity(), "_FTan 1/2 half turn is -infinity");
    Require(Near(_FTan_nid_postfix(0.1f, 1), static_cast<float>(std::tan(3.14159265358979323846 * 0.1f)), 4.0f), "_FTan 0.1 half turn");
    const long double pi = 3.14159265358979323846264338327950288L;
    const long double wideQuarter = tanl_nid_postfix(0.25L * pi);
    Require(_LTan_nid_postfix(0.25L, 1) == wideQuarter, "_LTan 1/4 half turn is tanl of pi/4");
    Require(_LTan_nid_postfix(0.75L, 1) == -1.0L / wideQuarter, "_LTan 3/4 half turn inverts");
    Require(_LTan_nid_postfix(0.5L, 1) == -std::numeric_limits<long double>::infinity(), "_LTan 1/2 half turn is -infinity");
    Require(Near(_LTan_nid_postfix(0.1L, 1), tanl_nid_postfix(0.1L * pi), 8.0L), "_LTan 0.1 half turn");
}

void TestSincos() {
    const double samples[] = {1e-300, -0.5, 1.0, 3.0, -100.25, 1e10, 1e300};
    for (double x : samples) {
        double cosine = 0.0;
        Require(_Sincos_nid_postfix(x, &cosine) == sin_nid_postfix(x), "_Sincos returns sin");
        Require(cosine == cos_nid_postfix(x), "_Sincos stores cos");
        const float narrow = static_cast<float>(x);
        if (!std::isfinite(narrow)) continue;
        float cosineFloat = 0.0f;
        Require(_FSincos_nid_postfix(narrow, &cosineFloat) == sinf_nid_postfix(narrow), "_FSincos returns sinf");
        Require(cosineFloat == cosf_nid_postfix(narrow), "_FSincos stores cosf");
    }
    double cosine = 0.0;
    const double negativeZero = _Sincos_nid_postfix(-0.0, &cosine);
    Require(negativeZero == 0.0 && std::signbit(negativeZero) && cosine == 1.0, "_Sincos -0 is -0 with a cosine of 1");
    Require(std::isnan(_Sincos_nid_postfix(std::numeric_limits<double>::quiet_NaN(), &cosine)) && std::isnan(cosine), "_Sincos NaN gives NaN for both");
    *__error_nid_postfix() = 0;
    cosine = 0.0;
    Require(std::isnan(_Sincos_nid_postfix(std::numeric_limits<double>::infinity(), &cosine)) && std::isnan(cosine), "_Sincos infinity gives NaN for both");
    Require(*__error_nid_postfix() == GuestEdom, "_Sincos of an infinity raises invalid");
    bool threw = false;
    try {
        _Sincos_nid_postfix(1.0, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "_Sincos rejects a null cosine pointer");
    unsigned char storage[sizeof(long double)];
    std::memset(storage, 0xab, sizeof(storage));
    long double* wide = reinterpret_cast<long double*>(storage);
    Require(_LSincos_nid_postfix(0.75L, wide) == sinl_nid_postfix(0.75L), "_LSincos returns sinl");
    long double stored = 0.0L;
    std::memcpy(&stored, storage, 10);
    Require(stored == cosl_nid_postfix(0.75L), "_LSincos stores cosl");
    for (std::size_t i = 10; i < sizeof(storage); ++i) Require(storage[i] == 0xab, "_LSincos writes only the 80-bit value");
}

}

int main() {
    TestAtanDouble();
    TestAtanOtherWidths();
    TestTan();
    TestTanHalfTurns();
    TestTanHalfTurnsOtherWidths();
    TestSincos();
    return 0;
}
