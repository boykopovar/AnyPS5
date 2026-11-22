#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ExtendedPrecisionParts.hpp"

#include <cmath>
#include <cstddef>
#include <iterator>

namespace {

constexpr double DoubleCoefficients[] = {0x1.2f8d8bf29def7p-3, 0x1.3991accfeb3f4p-3, 0x1.7466fd968cb62p-3, 0x1.c71c4da12c9c6p-3,
                                         0x1.24924948ae94ap-2, 0x1.9999999975ff7p-2, 0x1.55555555555bcp-1};

constexpr float FloatCoefficients[] = {0x1.32042ap-2f, 0x1.995c22p-2f, 0x1.55558p-1f};

}

extern "C" {

double APS5_VABI _Logpoly_nid_postfix(double w) {
    double result = DoubleCoefficients[0];
    for (std::size_t index = 1; index < std::size(DoubleCoefficients); ++index) result = std::fma(result, w, DoubleCoefficients[index]);
    return result;
}

float APS5_VABI _FLogpoly_nid_postfix(float w) {
    float result = FloatCoefficients[0];
    for (std::size_t index = 1; index < std::size(FloatCoefficients); ++index) result = std::fma(result, w, FloatCoefficients[index]);
    return result;
}

long double APS5_VABI _LLogpoly_nid_postfix(long double w) {
    return _LPoly_nid_postfix(w, LibcDetail::LogPolynomial, static_cast<int>(std::size(LibcDetail::LogPolynomial)) - 1);
}

}
