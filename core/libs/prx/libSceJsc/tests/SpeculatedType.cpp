#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uint64_t SpecBoolInt32 = 1ull << 32;
constexpr std::uint64_t SpecNonBoolInt32 = 1ull << 33;
constexpr std::uint64_t SpecAnyIntAsDouble = 1ull << 36;
constexpr std::uint64_t SpecNonIntAsDouble = 1ull << 37;
constexpr std::uint64_t SpecDoublePureNaN = 1ull << 38;
constexpr std::uint64_t SpecBoolean = 1ull << 40;
constexpr std::uint64_t SpecOther = 1ull << 41;
constexpr std::uint64_t SpecEmpty = 1ull << 42;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "speculationFromValue check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

std::uint64_t Speculate(std::uint64_t value) {
    return _ZN3JSC20speculationFromValueENS_7JSValueE(value);
}

std::uint64_t Int32(std::int32_t value) {
    return 0xfffe000000000000ull | static_cast<std::uint32_t>(value);
}

std::uint64_t Double(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits + (1ull << 49);
}

void TestInt32() {
    Require(Speculate(Int32(0)) == SpecBoolInt32);
    Require(Speculate(Int32(1)) == SpecBoolInt32);
    Require(Speculate(Int32(2)) == SpecNonBoolInt32);
    Require(Speculate(Int32(-1)) == SpecNonBoolInt32);
    Require(Speculate(Int32(std::numeric_limits<std::int32_t>::min())) == SpecNonBoolInt32);
}

void TestDouble() {
    Require(Speculate(Double(0.0)) == SpecAnyIntAsDouble);
    Require(Speculate(Double(-0.0)) == SpecNonIntAsDouble);
    Require(Speculate(Double(42.0)) == SpecAnyIntAsDouble);
    Require(Speculate(Double(0.5)) == SpecNonIntAsDouble);
    Require(Speculate(Double(2251799813685247.0)) == SpecAnyIntAsDouble);
    Require(Speculate(Double(-2251799813685248.0)) == SpecAnyIntAsDouble);
    Require(Speculate(Double(2251799813685248.0)) == SpecNonIntAsDouble);
    Require(Speculate(Double(-2251799813685249.0)) == SpecNonIntAsDouble);
    Require(Speculate(Double(std::numeric_limits<double>::infinity())) == SpecNonIntAsDouble);
    Require(Speculate(Double(-std::numeric_limits<double>::infinity())) == SpecNonIntAsDouble);
    Require(Speculate(Double(std::numeric_limits<double>::quiet_NaN())) == SpecDoublePureNaN);
    Require(Speculate(Double(std::numeric_limits<double>::max())) == SpecNonIntAsDouble);
    Require(Speculate(Double(-std::numeric_limits<double>::max())) == SpecNonIntAsDouble);
    Require(Speculate(Double(9223372036854775808.0)) == SpecNonIntAsDouble);
    Require(Speculate(Double(-9223372036854775808.0)) == SpecNonIntAsDouble);
}

void TestImmediates() {
    Require(Speculate(0) == SpecEmpty);
    Require(Speculate(0x6) == SpecBoolean);
    Require(Speculate(0x7) == SpecBoolean);
    Require(Speculate(0x2) == SpecOther);
    Require(Speculate(0xa) == SpecOther);
}

void TestCell() {
    bool threw = false;
    try {
        Speculate(0x10000);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    TestInt32();
    TestDouble();
    TestImmediates();
    TestCell();
    return 0;
}
