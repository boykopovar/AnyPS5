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

int main() {
    const std::string exact = ExactSmallestSubnormalDecimal();
    Check("1e-310", std::bit_cast<std::uint64_t>(1e-310), ERANGE, "inexact decimal subnormal errno");
    Check("4.9e-324", 1, ERANGE, "smallest inexact decimal subnormal errno");
    Check(exact, 1, ERANGE, "exact decimal subnormal errno");
    Check("-" + exact, 0x8000000000000001ull, ERANGE, "negative exact decimal subnormal errno");
    Check("1e-300", std::bit_cast<std::uint64_t>(1e-300), 7, "normal decimal errno preservation");
    Check("0x1p-1074", 1, 7, "hexadecimal subnormal errno preservation");
    Check("1e-330", 0, ERANGE, "decimal underflow-to-zero errno");
}
