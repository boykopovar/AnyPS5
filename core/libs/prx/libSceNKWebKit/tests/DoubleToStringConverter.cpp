#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WtfAbi::DoubleConversionStringBuilder;
using NKWebKit::WtfAbi::DoubleToStringConverter;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "DoubleToStringConverter check failed at line %u\n", location.line());
        std::abort();
    }
}

std::string FormatExponential(const DoubleToStringConverter* converter, double value, int digits, bool* outOk = nullptr) {
    char buffer[256]{};
    DoubleConversionStringBuilder builder{{buffer, static_cast<int>(sizeof(buffer))}, 0};
    bool ok = _ZNK3WTF17double_conversion23DoubleToStringConverter13ToExponentialEdiPNS0_13StringBuilderE(
        converter, value, digits, &builder);
    if (outOk)
        *outOk = ok;
    else
        Require(ok);
    if (!ok) {
        Require(builder.position == 0);
        return {};
    }
    Require(builder.position > 0 && builder.position < builder.buffer.length);
    buffer[builder.position] = '\0';
    return std::string(buffer, static_cast<std::size_t>(builder.position));
}

template<typename Fn>
void ExpectThrows(Fn&& fn, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try {
        fn();
    } catch (const std::exception&) {
        threw = true;
    }
    if (!threw) {
        std::fprintf(stderr, "Expected exception at line %u\n", location.line());
        std::abort();
    }
}

}  // namespace

int main() {
    const DoubleToStringConverter* ecma = _ZN3WTF17double_conversion23DoubleToStringConverter19EcmaScriptConverterEv();
    Require(ecma != nullptr);
    Require(ecma == _ZN3WTF17double_conversion23DoubleToStringConverter19EcmaScriptConverterEv());
    Require(ecma->flags == 9);
    Require(std::string_view(ecma->infinitySymbol) == "Infinity");
    Require(std::string_view(ecma->nanSymbol) == "NaN");
    Require(ecma->exponentCharacter == 'e');
    Require(ecma->decimalInShortestLow == -6);
    Require(ecma->decimalInShortestHigh == 21);

    Require(FormatExponential(ecma, 3.1415, -1) == "3.1415e+0");
    Require(FormatExponential(ecma, 3.1415, 0) == "3e+0");
    Require(FormatExponential(ecma, 3.1415, 3) == "3.142e+0");
    Require(FormatExponential(ecma, 3.12, 1) == "3.1e+0");
    Require(FormatExponential(ecma, 5.0, 3) == "5.000e+0");
    Require(FormatExponential(ecma, 0.001, 2) == "1.00e-3");
    Require(FormatExponential(ecma, -123456.0, 2) == "-1.23e+5");
    Require(FormatExponential(ecma, 0.0, -1) == "0e+0");
    Require(FormatExponential(ecma, -0.0, -1) == "0e+0");
    Require(FormatExponential(ecma, -0.0, 2) == "0.00e+0");
    Require(FormatExponential(ecma, 1.5, 20) == "1.5" + std::string(19, '0') + "e+0");
    Require(FormatExponential(ecma, 1.5, 100) == "1.5" + std::string(99, '0') + "e+0");

    Require(FormatExponential(ecma, std::numeric_limits<double>::infinity(), -1) == "Infinity");
    Require(FormatExponential(ecma, -std::numeric_limits<double>::infinity(), 2) == "-Infinity");
    Require(FormatExponential(ecma, std::numeric_limits<double>::quiet_NaN(), 2) == "NaN");

    DoubleToStringConverter custom{0, "inf", "nan", 'E', -6, 21, 6, 0};
    Require(FormatExponential(&custom, 12.5, 1) == "1.3E1");
    Require(FormatExponential(&custom, 12.5, -1) == "1.25E1");
    Require(FormatExponential(ecma, 12.5, 1) == "1.3e+1");
    Require(FormatExponential(ecma, 2.5, 0) == "3e+0");
    Require(FormatExponential(ecma, 1.25, 1) == "1.3e+0");
    Require(FormatExponential(ecma, 0.125, 1) == "1.3e-1");
    Require(FormatExponential(ecma, 9.5, 0) == "1e+1");
    Require(FormatExponential(ecma, 0.15, 0) == "1e-1");
    Require(FormatExponential(&custom, -0.0, 2) == "-0.00E0");
    Require(FormatExponential(&custom, 0.005, -1) == "5E-3");
    Require(FormatExponential(&custom, -std::numeric_limits<double>::infinity(), -1) == "-inf");

    DoubleToStringConverter noSpecials{0, nullptr, nullptr, 'e', -6, 21, 6, 0};
    bool ok = true;
    FormatExponential(&noSpecials, std::numeric_limits<double>::infinity(), -1, &ok);
    Require(!ok);
    ok = true;
    FormatExponential(&noSpecials, -std::numeric_limits<double>::infinity(), -1, &ok);
    Require(!ok);
    ok = true;
    FormatExponential(&noSpecials, std::numeric_limits<double>::quiet_NaN(), -1, &ok);
    Require(!ok);

    ok = true;
    FormatExponential(ecma, 1.0, -2, &ok);
    Require(!ok);
    ok = true;
    FormatExponential(ecma, 1.0, 101, &ok);
    Require(!ok);

    char small[4]{};
    DoubleConversionStringBuilder tooSmall{{small, static_cast<int>(sizeof(small))}, 0};
    ExpectThrows([&] {
        _ZNK3WTF17double_conversion23DoubleToStringConverter13ToExponentialEdiPNS0_13StringBuilderE(
            ecma, 1.0, 2, &tooSmall);
    });

    DoubleConversionStringBuilder finalized{{small, static_cast<int>(sizeof(small))}, -1};
    ExpectThrows([&] {
        _ZNK3WTF17double_conversion23DoubleToStringConverter13ToExponentialEdiPNS0_13StringBuilderE(
            ecma, 1.0, -1, &finalized);
    });

    ExpectThrows([&] {
        _ZNK3WTF17double_conversion23DoubleToStringConverter13ToExponentialEdiPNS0_13StringBuilderE(
            nullptr, 1.0, -1, &tooSmall);
    });
    ExpectThrows([&] {
        _ZNK3WTF17double_conversion23DoubleToStringConverter13ToExponentialEdiPNS0_13StringBuilderE(
            ecma, 1.0, -1, nullptr);
    });
    return 0;
}
