#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static std::string ExactSmallestSubnormalDecimal() {
    std::string digits = "1";
    for (int i = 0; i < 1074; ++i) {
        int carry = 0;
        for (auto digit = digits.rbegin(); digit != digits.rend(); ++digit) {
            const int value = (*digit - '0') * 5 + carry;
            *digit = static_cast<char>('0' + value % 10);
            carry = value / 10;
        }
        if (carry != 0) digits.insert(digits.begin(), static_cast<char>('0' + carry));
    }
    Require(digits.size() == 751, "2^-1074 decimal digit count");
    std::string result = "0.";
    result.append(1074 - digits.size(), '0');
    result += digits;
    return result;
}

static void Check(const std::string& text, std::uint64_t expectedBits, int expectedErrno, const char* message) {
    char* end = nullptr;
    errno = 7;
    const double value = strtod_nid_postfix(text.c_str(), &end);
    Require(std::bit_cast<std::uint64_t>(value) == expectedBits, message);
    Require(end == text.c_str() + text.size(), message);
    Require(errno == expectedErrno, message);
}

static void CheckBoth(const std::string& text, std::uint64_t expectedBits, int expectedErrno, const char* message) {
    Check(text, expectedBits, expectedErrno, message);
    const std::u16string wide(text.begin(), text.end());
    char16_t* end = nullptr;
    errno = 7;
    const double value = wcstod_nid_postfix(wide.c_str(), &end);
    Require(std::bit_cast<std::uint64_t>(value) == expectedBits, message);
    Require(end == wide.c_str() + wide.size(), message);
    Require(errno == expectedErrno, message);
}

static void CheckHexadecimal() {
    CheckBoth("0x1p-1074", 1, 7, "hexadecimal smallest subnormal");
    CheckBoth("0x1.8p-1075", 1, ERANGE, "hexadecimal rounding up to the smallest subnormal");
    CheckBoth("0x1.fffffffffffffffffp-1075", 1, ERANGE, "long hexadecimal just below the smallest subnormal");
    CheckBoth("0x1.0000000000001p-1075", 1, ERANGE, "hexadecimal band threshold");
    CheckBoth("0x1.00000000000001p-1075", 0, ERANGE, "hexadecimal band below threshold");
    CheckBoth("-0x1.0000000000000fp-1075", 0x8000000000000000ull, ERANGE, "negative hexadecimal band below threshold");
    CheckBoth("0x0.800000000000040000001p-1074", 0, ERANGE, "fractional hexadecimal band below threshold");
    CheckBoth("0x1p-1075", 0, ERANGE, "hexadecimal half of the smallest subnormal");
    CheckBoth("0x1p-1100", 0, ERANGE, "hexadecimal underflow to zero");
    CheckBoth("0x1p-99999999999999999999", 0, ERANGE, "hexadecimal huge negative exponent");
    CheckBoth("0x0p-1080", 0, 7, "hexadecimal zero");
    CheckBoth("0x1.ffffffffffffe8p-1023", 0x000fffffffffffffull, 7, "inexact hexadecimal subnormal");
    CheckBoth("0x1.fffffffffffff8p-1023", 0x0010000000000000ull, 7, "hexadecimal rounding up to the smallest normal");
    CheckBoth(" 0x0000000000000000001.8P-1075", 1, ERANGE, "hexadecimal leading zeros");
    CheckBoth("0x1p+1100", 0x7ff0000000000000ull, ERANGE, "hexadecimal overflow");
}

static void CheckHexadecimalExponentSaturation() {
    CheckBoth("0x1p-4611686018427387904", 0, ERANGE, "hexadecimal exponent at the saturation limit");
    CheckBoth("0x1p-9223372036854775808", 0, ERANGE, "hexadecimal exponent past the 64-bit range");
    CheckBoth("0x1p+9223372036854775808", 0x7ff0000000000000ull, ERANGE, "hexadecimal positive exponent past the 64-bit range");
    CheckBoth("0x10000p-99999999999999999999", 0, ERANGE, "saturated exponent with integer digits");
    CheckBoth("0x0.00001p+99999999999999999999", 0x7ff0000000000000ull, ERANGE, "saturated exponent with fraction digits");
}

static void CheckLongHexadecimalDigitRuns() {
    constexpr long long count = 1 << 20;
    const std::string zeros(count, '0');
    const std::string integer = "0x1" + zeros + "p";
    const std::string fraction = "0x0." + zeros + "1p";
    CheckBoth(integer + std::to_string(-(4 * count + 1074)), 1, 7, "long integer digit run at the smallest subnormal");
    CheckBoth(integer + std::to_string(-(4 * count + 1075)), 0, ERANGE, "long integer digit run below the smallest subnormal");
    CheckBoth(integer + "-99999999999999999999", 0, ERANGE, "long integer digit run with a saturated exponent");
    CheckBoth(fraction + "+" + std::to_string(4 * count + 4 - 1074), 1, 7, "long fraction digit run at the smallest subnormal");
    CheckBoth(fraction + "+" + std::to_string(4 * count + 4 - 1075), 0, ERANGE, "long fraction digit run below the smallest subnormal");
    CheckBoth(fraction + "+" + std::to_string(4 * count + 4 + 1023), 0x7fe0000000000000ull, 7, "long fraction digit run at the largest power of two");
    CheckBoth(fraction + "+" + std::to_string(4 * count + 4 + 1024), 0x7ff0000000000000ull, ERANGE, "long fraction digit run overflow");
    CheckBoth(fraction + "+99999999999999999999", 0x7ff0000000000000ull, ERANGE, "long fraction digit run with a saturated exponent");
}

int main() {
    const std::string exact = ExactSmallestSubnormalDecimal();
    Check("1e-310", std::bit_cast<std::uint64_t>(1e-310), ERANGE, "inexact decimal subnormal errno");
    Check("4.9e-324", 1, ERANGE, "smallest inexact decimal subnormal errno");
    Check(exact, 1, ERANGE, "exact decimal subnormal errno");
    Check("-" + exact, 0x8000000000000001ull, ERANGE, "negative exact decimal subnormal errno");
    Check("1e-300", std::bit_cast<std::uint64_t>(1e-300), 7, "normal decimal errno preservation");
    Check("0x1p-1074", 1, 7, "hexadecimal subnormal errno preservation");
    Check("1e-330", 0, ERANGE, "decimal underflow-to-zero errno");
    CheckHexadecimal();
    CheckHexadecimalExponentSaturation();
    CheckLongHexadecimalDigitRuns();
}
