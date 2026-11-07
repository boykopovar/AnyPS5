#include "prx/libSceAgcDriver/Execution/include/FloatProbeTolerance.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

namespace AgcDriver {
namespace {

struct Interval {
    double low;
    double high;
};

bool IsNan(std::uint32_t value) {
    return (value & 0x7fffffffu) > 0x7f800000u;
}

double Ulp(double value) {
    int exponent = 0;
    std::frexp(value, &exponent);
    return std::ldexp(1.0, std::max(exponent - 24, -149));
}

Interval Around(double exact, double ulps) {
    const auto error = ulps * Ulp(exact);
    return {exact - error, exact + error};
}

Interval Allowed(Float32ProbeOp op, double input) {
    switch (op) {
    case Float32ProbeOp::Log2:
        if (input >= 0.5 && input <= 2.0) return {std::log2(input) - std::ldexp(1.0, -21), std::log2(input) + std::ldexp(1.0, -21)};
        return Around(std::log2(input), 3.0);
    case Float32ProbeOp::Sqrt: {
        const auto inverse = Around(1.0 / std::sqrt(input), 2.0);
        return {Around(1.0 / inverse.high, 2.5).low, Around(1.0 / inverse.low, 2.5).high};
    }
    case Float32ProbeOp::Rsq: return Around(1.0 / std::sqrt(input), 2.0);
    case Float32ProbeOp::Rcp: return Around(1.0 / input, 2.5);
    case Float32ProbeOp::Exp2: return Around(std::exp2(input), 3.0 + 2.0 * std::fabs(input));
    }
    return {0.0, 0.0};
}

}

bool Float32ResultsAgree(Float32ProbeOp op, std::uint32_t input, std::uint32_t first, std::uint32_t second) {
    if (IsNan(first) || IsNan(second)) return IsNan(first) && IsNan(second);
    const auto allowed = Allowed(op, static_cast<double>(std::bit_cast<float>(input)));
    if (!std::isfinite(allowed.low) || !std::isfinite(allowed.high)) return first == second;
    const auto difference = std::fabs(static_cast<double>(std::bit_cast<float>(first)) - static_cast<double>(std::bit_cast<float>(second)));
    return difference <= allowed.high - allowed.low;
}

}
