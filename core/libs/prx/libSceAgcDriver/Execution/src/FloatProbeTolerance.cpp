#include "prx/libSceAgcDriver/Execution/include/FloatProbeTolerance.hpp"

#include <bit>
#include <cmath>
#include <cstdlib>

namespace AgcDriver {
namespace {

bool IsNan(std::uint32_t value) {
    return (value & 0x7fffffffu) > 0x7f800000u;
}

std::int64_t Ordered(std::uint32_t value) {
    const auto magnitude = static_cast<std::int64_t>(value & 0x7fffffffu);
    return (value & 0x80000000u) != 0u ? -magnitude : magnitude;
}

double UlpBound(Float32ProbeOp op, float input) {
    switch (op) {
    case Float32ProbeOp::Log2: return 3.0;
    case Float32ProbeOp::Sqrt: return 4.5;
    case Float32ProbeOp::Rsq: return 2.0;
    case Float32ProbeOp::Rcp: return 2.5;
    case Float32ProbeOp::Exp2: return 3.0 + 2.0 * std::fabs(static_cast<double>(input));
    }
    return 0.0;
}

}

bool Float32ResultsAgree(Float32ProbeOp op, std::uint32_t input, std::uint32_t first, std::uint32_t second) {
    if (IsNan(first) || IsNan(second)) return IsNan(first) && IsNan(second);
    const auto value = std::bit_cast<float>(input);
    if (op == Float32ProbeOp::Log2 && value >= 0.5f && value <= 2.0f) {
        return std::fabs(static_cast<double>(std::bit_cast<float>(first)) - static_cast<double>(std::bit_cast<float>(second))) <= 2.0 * std::ldexp(1.0, -21);
    }
    return static_cast<double>(std::llabs(Ordered(first) - Ordered(second))) <= 2.0 * UlpBound(op, value);
}

}
