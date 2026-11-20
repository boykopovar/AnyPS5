#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uint64_t SpecInt32AsInt52 = 1ull << 34;
constexpr std::uint64_t SpecNonInt32AsInt52 = 1ull << 35;
constexpr std::uint64_t SpecNonIntAsDouble = 1ull << 37;
constexpr std::uint64_t SpecDoublePureNaN = 1ull << 38;
constexpr std::uint64_t SpecBoolean = 1ull << 40;
constexpr std::uint64_t SpecOther = 1ull << 41;
constexpr std::uint64_t SpecEmpty = 1ull << 42;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "int52AwareSpeculationFromValue check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

std::uint64_t Speculate(std::uint64_t value) {
    return _ZN3JSC30int52AwareSpeculationFromValueENS_7JSValueE(value);
}

std::uint64_t Int32(std::int32_t value) {
    return 0xfffe000000000000ull | static_cast<std::uint32_t>(value);
}

std::uint64_t Double(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits + (1ull << 49);
}

}

int main() {
    for (const std::int32_t value : {0, 1, 2, -1, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()})
        Require(Speculate(Int32(value)) == SpecInt32AsInt52);

    for (const double value : {0.0, 1.0, 42.0, -1.0, 2147483647.0, -2147483648.0})
        Require(Speculate(Double(value)) == SpecInt32AsInt52);

    const double limit = std::ldexp(1.0, 51);
    for (const double value : {2147483648.0, -2147483649.0, 4294967296.0, limit - 1, -limit})
        Require(Speculate(Double(value)) == SpecNonInt32AsInt52);

    for (const double value : {-0.0, 0.5, -1.5, limit, -limit - 2, std::ldexp(1.0, 63), -std::ldexp(1.0, 63),
             std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(), std::numeric_limits<double>::denorm_min(),
             std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        Require(Speculate(Double(value)) == SpecNonIntAsDouble);

    Require(Speculate(Double(std::numeric_limits<double>::quiet_NaN())) == SpecDoublePureNaN);
    Require(Speculate(0) == SpecEmpty);
    Require(Speculate(0x6) == SpecBoolean);
    Require(Speculate(0x7) == SpecBoolean);
    Require(Speculate(0x2) == SpecOther);
    Require(Speculate(0xa) == SpecOther);

    bool threw = false;
    try {
        Speculate(0x10000);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    std::puts("int52AwareSpeculationFromValue tests passed");
    return 0;
}
