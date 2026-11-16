#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WtfAbi::HTMLIntegerParsingError;
using NKWebKit::WtfAbi::StringView;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "HTML integer check failed at line %u\n", location.line());
        std::abort();
    }
}

void Check(StringView input, bool hasValue, unsigned valueOrError) {
    auto result = _ZN7WebCore27parseHTMLNonNegativeIntegerEN3WTF10StringViewE(input);
    Require(result.hasValue == hasValue);
    if (hasValue)
        Require(result.value == valueOrError);
    else
        Require(static_cast<unsigned>(result.error) == valueOrError);
}

void Both(std::string_view text, bool hasValue, unsigned valueOrError) {
    Check({text.data(), static_cast<unsigned>(text.size()), true}, hasValue, valueOrError);
    std::u16string wide(text.begin(), text.end());
    Check({wide.data(), static_cast<unsigned>(wide.size()), false}, hasValue, valueOrError);
}

void Run() {
    Require(static_cast<int>(HTMLIntegerParsingError::NegativeOverflow) == 0);
    Require(static_cast<int>(HTMLIntegerParsingError::PositiveOverflow) == 1);
    Require(static_cast<int>(HTMLIntegerParsingError::Other) == 2);
    Both("0", true, 0);
    Both("42", true, 42);
    Both(" \t\n\r\f+123suffix", true, 123);
    for (char space : std::string_view(" \t\n\r\f"))
        Both(std::string(1, space) + "7", true, 7);
    Both("-0", true, 0);
    Both("-0000000tail", true, 0);
    Both("+0", true, 0);
    Both("2147483647", true, 2147483647U);
    Both("+2147483647tail", true, 2147483647U);
    Both("00000000000000000000000000000000000000000000042", true, 42);
    Both("12.34", true, 12);
    Both("12 34", true, 12);
    Both(std::string_view("12\0" "34", 5), true, 12);
    Both("", false, 2);
    Both(" \t\r\n\f", false, 2);
    Both("+", false, 2);
    Both("-", false, 2);
    Both("+ 1", false, 2);
    Both("--1", false, 2);
    Both("+-1", false, 2);
    Both("x1", false, 2);
    Both(".1", false, 2);
    Both("\v1", false, 2);
    Both(std::string_view("\0" "12", 3), false, 2);
    Both(std::string("\xA0") + "1", false, 2);
    Both("-1", false, 0);
    Both("-2147483647", false, 0);
    Both("-2147483648", false, 0);
    Both("-2147483649", false, 0);
    Both("2147483648", false, 1);
    Both("4294967295", false, 1);
    Both("4294967296", false, 1);
    Both(std::string(10000, '9'), false, 1);
    Both("-" + std::string(10000, '9'), false, 0);
    Check({u"\u20031", 2, false}, false, 2);
    Check({u"\uFF11", 1, false}, false, 2);
    Check({u"7\uFF11", 2, false}, true, 7);
    Check({u"7\U0001F600", 3, false}, true, 7);
    for (bool is8Bit : {false, true}) {
        Check({nullptr, 0, is8Bit}, false, 2);
        bool threw = false;
        try {
            _ZN7WebCore27parseHTMLNonNegativeIntegerEN3WTF10StringViewE({nullptr, 1, is8Bit});
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw);
    }
}

}  // namespace

int main() {
    try {
        Run();
        std::puts("HTML non-negative integer checks passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "HTML integer exception: %s\n", error.what());
        return 1;
    }
}
