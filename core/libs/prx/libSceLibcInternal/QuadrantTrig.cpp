#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <cmath>

namespace {

constexpr int NegateFlag = 0x8;
constexpr int QuadrantFlags = 0x6;
constexpr double HalfPiHigh = 1.57079632679489655800e+00;
constexpr double HalfPiLow = 6.12323399573676603587e-17;
constexpr long double HalfPiLong = 1.57079632679489661923132169163975144L;

bool KnownAtanArguments(long double x, int flags) {
    return (flags & ~(NegateFlag | QuadrantFlags)) == 0 && x >= 0.0L && x <= 1.0L;
}

double AtanDouble(double x, int flags) {
    const double angle = atan_nid_postfix(x);
    const int quadrant = (flags & QuadrantFlags) >> 1;
    const double sign = quadrant == 1 || quadrant == 3 ? -1.0 : 1.0;
    const double high = quadrant == 0 ? 0.0 : quadrant == 3 ? 2.0 * HalfPiHigh : HalfPiHigh;
    const double low = quadrant == 0 ? 0.0 : quadrant == 3 ? 2.0 * HalfPiLow : HalfPiLow;
    const double result = quadrant == 0 ? angle : high + (sign * angle + low);
    return (flags & NegateFlag) != 0 ? -result : result;
}

}

extern "C" {

double APS5_VABI _Atan_nid_postfix(double x, int flags) {
    if (!KnownAtanArguments(x, flags)) NotImplemented_nid_no_patch("_Atan: an argument outside [0, 1] or flags other than 0x2, 0x4 and 0x8");
    return AtanDouble(x, flags);
}

float APS5_VABI _FAtan_nid_postfix(float x, int flags) {
    if (!KnownAtanArguments(x, flags)) NotImplemented_nid_no_patch("_FAtan: an argument outside [0, 1] or flags other than 0x2, 0x4 and 0x8");
    return static_cast<float>(AtanDouble(x, flags));
}

long double APS5_VABI _LAtan_nid_postfix(long double x, int flags) {
    if (!KnownAtanArguments(x, flags)) NotImplemented_nid_no_patch("_LAtan: an argument outside [0, 1] or flags other than 0x2, 0x4 and 0x8");
    const long double angle = atanl_nid_postfix(x);
    const int quadrant = (flags & QuadrantFlags) >> 1;
    long double result = angle;
    if (quadrant == 1) result = HalfPiLong - angle;
    if (quadrant == 2) result = HalfPiLong + angle;
    if (quadrant == 3) result = 2.0L * HalfPiLong - angle;
    return (flags & NegateFlag) != 0 ? -result : result;
}

double APS5_VABI _Tan_nid_postfix(double x, int quadrants) {
    if (quadrants != 0) NotImplemented_nid_no_patch("_Tan: a nonzero quadrant count");
    return tan_nid_postfix(x);
}

float APS5_VABI _FTan_nid_postfix(float x, int quadrants) {
    if (quadrants != 0) NotImplemented_nid_no_patch("_FTan: a nonzero quadrant count");
    return tanf_nid_postfix(x);
}

long double APS5_VABI _LTan_nid_postfix(long double x, int quadrants) {
    if (quadrants != 0) NotImplemented_nid_no_patch("_LTan: a nonzero quadrant count");
    return tanl_nid_postfix(x);
}

}
