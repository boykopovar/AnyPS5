#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/General.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace {

template <typename T>
struct QuadConstants;

template <>
struct QuadConstants<float> {
    static constexpr float Pi = 3.1415926535897932384626433832795029F;
    static constexpr float PiBy4 = 0.78539816339744830961566084581987572F;
    static constexpr float TwoByPi = 0.63661977236758134307553505349005745F;
    static constexpr float HugeRad = static_cast<float>(0.63661977236758134307553505349005744L * 4096.0L);
};

template <>
struct QuadConstants<double> {
    static constexpr double Pi = 3.1415926535897932384626433832795029;
    static constexpr double PiBy4 = 0.78539816339744830961566084581987572;
    static constexpr double TwoByPi = 0.63661977236758134307553505349005745;
    static constexpr double HugeRad = static_cast<double>(0.63661977236758134307553505349005744L * 134217728.0L);
};

template <>
struct QuadConstants<long double> {
    static constexpr long double Pi = 3.1415926535897932384626433832795029L;
    static constexpr long double PiBy4 = 0.78539816339744830961566084581987572L;
    static constexpr long double TwoByPi = 0.63661977236758134307553505349005745L;
    static constexpr long double HugeRad = 0.63661977236758134307553505349005744L * 4294967296.0L;
};

constexpr double HalfPiParts[] = {0x1.921fb54442d18p+0, 0x1.1a62633145c07p-54, -0x1.f1976b7ed8fbcp-110};

struct Remainder {
    unsigned int octant;
    long double high;
    long double low;
};

void AddExactly(long double& high, long double& low, long double term) {
    const long double sum = high + term;
    const long double termPart = sum - high;
    low += (high - (sum - termPart)) + (term - termPart);
    high = sum;
}

Remainder Normalized(unsigned int octant, long double high, long double low) {
    const long double sum = high + low;
    return {octant, sum, low - (sum - high)};
}

template <typename T>
T RoundRemainder(const Remainder& remainder) {
    if constexpr (std::is_same_v<T, long double>) {
        return remainder.high;
    } else {
        const T result = static_cast<T>(remainder.high);
        const long double residual = (remainder.high - static_cast<long double>(result)) + remainder.low;
        if (residual == 0.0L) return result;
        const T neighbour = std::nextafter(result, residual > 0.0L ? std::numeric_limits<T>::infinity() : -std::numeric_limits<T>::infinity());
        const long double half = (static_cast<long double>(neighbour) - static_cast<long double>(result)) / 2.0L;
        if (std::fabs(residual) < std::fabs(half)) return result;
        if (std::fabs(residual) > std::fabs(half)) return neighbour;
        return static_cast<T>(static_cast<long double>(result) + half);
    }
}

template <typename T>
T DropFraction(T x, int scale) {
    if (std::fabs(x) >= std::ldexp(T(1), std::numeric_limits<T>::digits - scale)) return x;
    const T factor = std::ldexp(T(1), scale);
    return std::trunc(x * factor) / factor;
}

template <typename T>
T MultiplyKeepingSubnormals(T x, T y) {
    const T product = x * y;
    if (product != T(0)) return product;
    int xExponent = 0;
    int yExponent = 0;
    const T xFraction = std::frexp(x, &xExponent);
    const T yFraction = std::frexp(y, &yExponent);
    return std::ldexp(xFraction * yFraction, xExponent + yExponent);
}

std::int64_t TruncateToInt64(long double value) {
    if (!(value >= -9223372036854775808.0L && value < 9223372036854775808.0L)) return std::numeric_limits<std::int64_t>::min();
    return static_cast<std::int64_t>(value);
}

Remainder NearestRemainder(long double x) {
    const long double magnitude = std::fabs(x);
    const int e0 = std::ilogb(magnitude) - 23;
    long double scaled = std::scalbn(magnitude, -e0);
    double chunks[3] = {};
    for (int index = 0; index < 2; ++index) {
        chunks[index] = static_cast<double>(std::trunc(scaled));
        scaled = (scaled - chunks[index]) * 16777216.0L;
    }
    chunks[2] = static_cast<double>(scaled);
    int count = 3;
    while (count > 1 && chunks[count - 1] == 0.0) --count;
    double parts[3] = {};
    const unsigned int octant = static_cast<unsigned int>(__kernel_rem_pio2_nid_postfix(chunks, parts, e0, count, 3));
    long double high = parts[0];
    long double low = 0.0L;
    AddExactly(high, low, parts[1]);
    AddExactly(high, low, parts[2]);
    if (x < 0.0L) return Normalized((8u - octant) & 7u, -high, -low);
    return Normalized(octant, high, low);
}

template <typename T>
T RemainderFromQuadrant(T x, T quadrant) {
    const Remainder nearest = NearestRemainder(x);
    const unsigned int offset = (nearest.octant - static_cast<unsigned int>(TruncateToInt64(std::fmod(quadrant, T(8))))) & 7u;
    if (offset == 0) return RoundRemainder<T>(nearest);
    if (offset != 1 && offset != 7) throw std::logic_error("_Quad: quadrant " + std::to_string(static_cast<long double>(quadrant)) + " is not next to the nearest one");
    const long double sign = offset == 1 ? 1.0L : -1.0L;
    long double high = sign * HalfPiParts[0];
    long double low = 0.0L;
    AddExactly(high, low, nearest.high);
    AddExactly(high, low, sign * HalfPiParts[1]);
    AddExactly(high, low, nearest.low);
    AddExactly(high, low, sign * HalfPiParts[2]);
    return RoundRemainder<T>(Normalized(0, high, low));
}

template <typename T>
unsigned int ReduceQuadrantUnits(T* px) {
    using C = QuadConstants<T>;
    T x = *px;
    const T whole = DropFraction(x, -1);
    if (whole != T(0)) {
        x = *px - whole;
        *px = x;
    }
    unsigned int quadrant = static_cast<unsigned int>(static_cast<int>(x + x));
    *px = DropFraction(*px, 1);
    if (*px != T(0)) x -= *px;
    if (T(0.25) < x) {
        x -= T(0.5);
        ++quadrant;
    } else if (x < -T(0.25)) {
        x += T(0.5);
        --quadrant;
    }
    *px = MultiplyKeepingSubnormals(x, C::Pi);
    return quadrant;
}

template <typename T>
unsigned int ReduceRadians(T* px) {
    using C = QuadConstants<T>;
    const T x = *px;
    if (-C::PiBy4 < x && x < C::PiBy4) return 0;
    if (-C::HugeRad < x && x < C::HugeRad) {
        T quadrant = x * C::TwoByPi;
        quadrant = T(0) <= quadrant ? quadrant + T(0.5) : quadrant - T(0.5);
        quadrant = DropFraction(quadrant, 0);
        if (quadrant == T(0)) return 0;
        *px = RemainderFromQuadrant(x, quadrant);
        return static_cast<unsigned int>(TruncateToInt64(quadrant)) & 3u;
    }
    const Remainder nearest = NearestRemainder(x);
    *px = RoundRemainder<T>(nearest);
    return nearest.octant & 3u;
}

template <typename T>
unsigned int Quad(T* px, int retcode, const char* function) {
    if (px == nullptr) throw std::invalid_argument(std::string(function) + ": null argument pointer");
    if (!std::isfinite(*px)) NotImplemented_nid_no_patch((std::string(function) + ": a NaN or infinite argument").c_str());
    return (retcode & 1) != 0 ? ReduceQuadrantUnits(px) : ReduceRadians(px);
}

template <typename T>
unsigned int Quadph(T* px, T phase, const char* function) {
    using C = QuadConstants<T>;
    if (px == nullptr) throw std::invalid_argument(std::string(function) + ": null argument pointer");
    if (!std::isfinite(*px) || !std::isfinite(phase)) NotImplemented_nid_no_patch((std::string(function) + ": a NaN or infinite argument or phase").c_str());
    unsigned int quadrant = ReduceRadians(px);
    T whole = DropFraction(phase, 1);
    phase -= whole;
    const T limit = static_cast<T>(std::numeric_limits<std::int64_t>::max() / 2);
    if (whole < -limit || limit < whole) whole = std::fmod(whole, limit + T(1));
    quadrant += static_cast<unsigned int>(TruncateToInt64(whole * T(2))) & 3u;
    const T shifted = *px + phase * C::Pi;
    if (C::PiBy4 <= shifted) {
        phase -= T(0.5);
        ++quadrant;
        *px += phase * C::Pi;
    } else if (shifted <= -C::PiBy4) {
        phase += T(0.5);
        --quadrant;
        *px += phase * C::Pi;
    } else {
        *px = shifted;
    }
    return quadrant;
}

}

extern "C" {

unsigned int APS5_VABI _Quad_nid_postfix(double* px, int retcode) {
    return Quad(px, retcode, "_Quad");
}

unsigned int APS5_VABI _FQuad_nid_postfix(float* px, int retcode) {
    return Quad(px, retcode, "_FQuad");
}

unsigned int APS5_VABI _LQuad_nid_postfix(long double* px, int retcode) {
    return Quad(px, retcode, "_LQuad");
}

unsigned int APS5_VABI _Quadph_nid_postfix(double* px, double phase) {
    return Quadph(px, phase, "_Quadph");
}

unsigned int APS5_VABI _FQuadph_nid_postfix(float* px, float phase) {
    return Quadph(px, phase, "_FQuadph");
}

unsigned int APS5_VABI _LQuadph_nid_postfix(long double* px, long double phase) {
    return Quadph(px, phase, "_LQuadph");
}

}
