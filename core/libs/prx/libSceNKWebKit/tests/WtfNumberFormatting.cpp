#include "prx/libSceNKWebKit/include/WtfNumberFormatting.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <locale>
#include <string>
#include <string_view>
#include <utility>

namespace {

using NKWebKit::WtfAbi::String;

std::uint64_t assertions = 0;

void Require(bool condition, std::string_view label) {
    ++assertions;
    if (condition) return;
    std::fprintf(stderr, "WTF number formatting failed: %.*s\n", static_cast<int>(label.size()), label.data());
    std::abort();
}

String Format(unsigned mode, double number, unsigned digits, int policy) {
    switch (mode) {
    case 0: return _ZN3WTF6String6numberEd(number);
    case 1: return _ZN3WTF6String6numberEf(static_cast<float>(number));
    case 2: return _ZN3WTF6String24numberToStringFixedWidthEdj(number, digits);
    case 3: return _ZN3WTF6String28numberToStringFixedPrecisionEdjNS_29TrailingZerosTruncatingPolicyE(number, digits, policy);
    case 4: return _ZN3WTF6String28numberToStringFixedPrecisionEfjNS_29TrailingZerosTruncatingPolicyE(static_cast<float>(number), digits, policy);
    default: std::abort();
    }
}

std::string Text(const String& value) {
    Require(value.impl != nullptr, "non-null StringImpl");
    Require((value.impl->hashAndFlags & 4) != 0, "8-bit result");
    Require(value.impl->data != nullptr, "character storage");
    const auto* bytes = static_cast<const char*>(value.impl->data);
    for (unsigned index = 0; index < value.impl->length; ++index)
        Require(bytes[index] != 0 && static_cast<unsigned char>(bytes[index]) < 128, "ASCII without embedded NUL");
    return {bytes, value.impl->length};
}

void Check(String value, std::string_view expected) {
    const auto actual = Text(value);
    if (actual != expected) {
        std::fprintf(stderr, "expected [%.*s], got [%s]\n", static_cast<int>(expected.size()), expected.data(), actual.c_str());
        std::abort();
    }
    Require(value.impl->length == expected.size(), "exact length");
    auto* impl = value.impl;
    const auto references = impl->refCount;
    {
        String copy(value);
        Require(copy.impl == impl, "shared copy");
        Require(impl->refCount == references + 2, "copy reference");
        String moved(std::move(copy));
        Require(copy.impl == nullptr && moved.impl == impl, "move ownership");
        Require(impl->refCount == references + 2, "move reference count");
        Require(Text(moved) == expected, "moved content");
    }
    Require(impl->refCount == references, "released copy");
    Require(Text(value) == expected, "surviving original");
}

struct Case {
    unsigned mode;
    std::uint64_t bits;
    unsigned digits;
    int policy;
    std::string_view expected;
};

constexpr auto cases = std::to_array<Case>({
    {0, 0x0000000000000000ULL, 0U, 0, "0"},
    {0, 0x8000000000000000ULL, 0U, 0, "0"},
    {0, 0x3ff0000000000000ULL, 0U, 0, "1"},
    {0, 0xbff0000000000000ULL, 0U, 0, "-1"},
    {0, 0x3fb999999999999aULL, 0U, 0, "0.1"},
    {0, 0x3ff3333333333333ULL, 0U, 0, "1.2"},
    {0, 0x3eb0c6f7a0b5ed8dULL, 0U, 0, "0.000001"},
    {0, 0x3e7ad7f29abcaf48ULL, 0U, 0, "1e-7"},
    {0, 0x4415af1d78b58c40ULL, 0U, 0, "100000000000000000000"},
    {0, 0x444b1ae4d6e2ef50ULL, 0U, 0, "1e+21"},
    {0, 0x444b1ae4d6e2ef4fULL, 0U, 0, "999999999999999900000"},
    {0, 0x3eb0c6f7a0b5ed8cULL, 0U, 0, "9.999999999999997e-7"},
    {0, 0x3eb0c6f7a0b5ed8eULL, 0U, 0, "0.0000010000000000000002"},
    {0, 0x0000000000000001ULL, 0U, 0, "5e-324"},
    {0, 0x0010000000000000ULL, 0U, 0, "2.2250738585072014e-308"},
    {0, 0x7fefffffffffffffULL, 0U, 0, "1.7976931348623157e+308"},
    {0, 0x7ff0000000000000ULL, 0U, 0, "Infinity"},
    {0, 0xfff0000000000000ULL, 0U, 0, "-Infinity"},
    {0, 0x7ff8000000000000ULL, 0U, 0, "NaN"},
    {0, 0xfff8000000000000ULL, 0U, 0, "NaN"},
    {1, 0x0000000000000000ULL, 0U, 0, "0"},
    {1, 0x8000000000000000ULL, 0U, 0, "0"},
    {1, 0x3fb999999999999aULL, 0U, 0, "0.1"},
    {1, 0x3ff3333333333333ULL, 0U, 0, "1.2"},
    {1, 0x3eb0c6f7a0b5ed8dULL, 0U, 0, "0.000001"},
    {1, 0x3e7ad7f29abcaf48ULL, 0U, 0, "1e-7"},
    {1, 0x4415af1d78b58c40ULL, 0U, 0, "100000000000000000000"},
    {1, 0x444b1ae4d6e2ef50ULL, 0U, 0, "1e+21"},
    {1, 0x36a0000000000000ULL, 0U, 0, "1e-45"},
    {1, 0x3810000000000000ULL, 0U, 0, "1.1754944e-38"},
    {1, 0x47efffffe0000000ULL, 0U, 0, "3.4028235e+38"},
    {1, 0x7ff0000000000000ULL, 0U, 0, "Infinity"},
    {1, 0xfff0000000000000ULL, 0U, 0, "-Infinity"},
    {1, 0x7ff8000000000000ULL, 0U, 0, "NaN"},
    {2, 0x0000000000000000ULL, 0U, 0, "0"},
    {2, 0x8000000000000000ULL, 2U, 0, "0.00"},
    {2, 0x3ff0000000000000ULL, 100U, 0, "1.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {2, 0x3ff4000000000000ULL, 1U, 0, "1.3"},
    {2, 0xbff4000000000000ULL, 1U, 0, "-1.3"},
    {2, 0x3ff3ffffffffffffULL, 1U, 0, "1.2"},
    {2, 0x3ff4000000000001ULL, 1U, 0, "1.3"},
    {2, 0x4005666666666666ULL, 2U, 0, "2.67"},
    {2, 0x3ff0147ae147ae14ULL, 2U, 0, "1.00"},
    {2, 0x3fc0000000000000ULL, 2U, 0, "0.13"},
    {2, 0x3fd8000000000000ULL, 2U, 0, "0.38"},
    {2, 0xbf50624dd2f1a9fcULL, 2U, 0, "-0.00"},
    {2, 0x3f747ae147ae147bULL, 2U, 0, "0.01"},
    {2, 0xbf747ae147ae147bULL, 2U, 0, "-0.01"},
    {2, 0x4023ff7ced916873ULL, 2U, 0, "10.00"},
    {2, 0x444b1ae4d6e2ef50ULL, 2U, 0, ""},
    {2, 0x444b1ae4d6e2ef4fULL, 2U, 0, "999999999999999868928.00"},
    {2, 0x4415af1d78b58c40ULL, 0U, 0, "100000000000000000000"},
    {2, 0x0000000000000001ULL, 100U, 0, "0.0000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {2, 0x3ff0000000000000ULL, 101U, 0, ""},
    {2, 0x7ff0000000000000ULL, 101U, 0, "Infinity"},
    {2, 0xfff0000000000000ULL, 4294967295U, 0, "-Infinity"},
    {2, 0x7ff8000000000000ULL, 4294967295U, 0, "NaN"},
    {3, 0x0000000000000000ULL, 1U, 0, "0"},
    {3, 0x0000000000000000ULL, 1U, 1, "0"},
    {3, 0x8000000000000000ULL, 5U, 0, "0.0000"},
    {3, 0x8000000000000000ULL, 5U, 1, "0"},
    {3, 0x3ff0000000000000ULL, 100U, 0, "1.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {3, 0x3ff0000000000000ULL, 100U, 1, "1"},
    {3, 0x3ff4000000000000ULL, 2U, 0, "1.3"},
    {3, 0x3ff4000000000000ULL, 2U, 1, "1.3"},
    {3, 0x3ff3ffffffffffffULL, 2U, 0, "1.2"},
    {3, 0x3ff3ffffffffffffULL, 2U, 1, "1.2"},
    {3, 0x3ff4000000000001ULL, 2U, 0, "1.3"},
    {3, 0x3ff4000000000001ULL, 2U, 1, "1.3"},
    {3, 0x4023fae147ae147bULL, 2U, 0, "10"},
    {3, 0x4023fae147ae147bULL, 2U, 1, "10"},
    {3, 0x408f380000000000ULL, 2U, 0, "1.0e+3"},
    {3, 0x408f380000000000ULL, 2U, 1, "1e+3"},
    {3, 0x3eb0c2ac1dbbe3d8ULL, 2U, 0, "0.0000010"},
    {3, 0x3eb0c2ac1dbbe3d8ULL, 2U, 1, "0.000001"},
    {3, 0x3eb0c6f7a0b5ed8dULL, 1U, 0, "0.000001"},
    {3, 0x3eb0c6f7a0b5ed8dULL, 1U, 1, "0.000001"},
    {3, 0x3e7ad7f29abcaf48ULL, 1U, 0, "1e-7"},
    {3, 0x3e7ad7f29abcaf48ULL, 1U, 1, "1e-7"},
    {3, 0x3ff3333333333333ULL, 4U, 0, "1.200"},
    {3, 0x3ff3333333333333ULL, 4U, 1, "1.2"},
    {3, 0x3ff3be76c8b43958ULL, 2U, 0, "1.2"},
    {3, 0x3ff3be76c8b43958ULL, 2U, 1, "1.2"},
    {3, 0x0000000000000001ULL, 1U, 0, "5e-324"},
    {3, 0x0000000000000001ULL, 1U, 1, "5e-324"},
    {3, 0x0000000000000001ULL, 100U, 0, "4.940656458412465441765687928682213723650598026143247644255856825006755072702087518652998363616359924e-324"},
    {3, 0x0000000000000001ULL, 100U, 1, "4.940656458412465441765687928682213723650598026143247644255856825006755072702087518652998363616359924e-324"},
    {3, 0x7fefffffffffffffULL, 1U, 0, "2e+308"},
    {3, 0x7fefffffffffffffULL, 1U, 1, "2e+308"},
    {3, 0x7fefffffffffffffULL, 100U, 0, "1.797693134862315708145274237317043567980705675258449965989174768031572607800285387605895586327668782e+308"},
    {3, 0x7fefffffffffffffULL, 100U, 1, "1.797693134862315708145274237317043567980705675258449965989174768031572607800285387605895586327668782e+308"},
    {3, 0x3ff0000000000000ULL, 0U, 0, ""},
    {3, 0x3ff0000000000000ULL, 0U, 1, ""},
    {3, 0x3ff0000000000000ULL, 101U, 0, ""},
    {3, 0x3ff0000000000000ULL, 101U, 1, ""},
    {3, 0x7ff0000000000000ULL, 0U, 0, "Infinity"},
    {3, 0x7ff0000000000000ULL, 0U, 1, "Infinity"},
    {3, 0xfff0000000000000ULL, 101U, 0, "-Infinity"},
    {3, 0xfff0000000000000ULL, 101U, 1, "-Infinity"},
    {3, 0x7ff8000000000000ULL, 4294967295U, 0, "NaN"},
    {3, 0x7ff8000000000000ULL, 4294967295U, 1, "NaN"},
    {3, 0x3ff3333333333333ULL, 4U, -1, "1.200"},
    {3, 0x3ff3333333333333ULL, 4U, 2, "1.200"},
    {3, 0x3ff3333333333333ULL, 4U, 2147483647, "1.200"},
    {3, 0x3ff3333333333333ULL, 4U, (-2147483647 - 1), "1.200"},
    {4, 0x3ff3333333333333ULL, 17U, 0, "1.2000000476837158"},
    {4, 0x3ff3333333333333ULL, 17U, 1, "1.2000000476837158"},
    {4, 0x3fb999999999999aULL, 20U, 0, "0.10000000149011611938"},
    {4, 0x3fb999999999999aULL, 20U, 1, "0.10000000149011611938"},
    {4, 0x3ff4000000000000ULL, 2U, 0, "1.3"},
    {4, 0x3ff4000000000000ULL, 2U, 1, "1.3"},
    {4, 0x36a0000000000000ULL, 20U, 0, "1.4012984643248170709e-45"},
    {4, 0x36a0000000000000ULL, 20U, 1, "1.4012984643248170709e-45"},
    {4, 0x47efffffe0000000ULL, 40U, 0, "340282346638528859811704183484516925440.0"},
    {4, 0x47efffffe0000000ULL, 40U, 1, "340282346638528859811704183484516925440"},
    {4, 0x0000000000000000ULL, 100U, 0, "0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {4, 0x0000000000000000ULL, 100U, 1, "0"},
    {4, 0x8000000000000000ULL, 5U, 0, "0.0000"},
    {4, 0x8000000000000000ULL, 5U, 1, "0"},
    {4, 0x3ff0000000000000ULL, 0U, 0, ""},
    {4, 0x3ff0000000000000ULL, 0U, 1, ""},
    {4, 0x7ff0000000000000ULL, 0U, 0, "Infinity"},
    {4, 0x7ff0000000000000ULL, 0U, 1, "Infinity"},
    {4, 0x7ff8000000000000ULL, 101U, 0, "NaN"},
    {4, 0x7ff8000000000000ULL, 101U, 1, "NaN"},
});

void CheckCases(bool doubleOnly = false) {
    for (const auto& item : cases) {
        if (doubleOnly && (item.mode == 1 || item.mode == 4)) continue;
        Check(Format(item.mode, std::bit_cast<double>(item.bits), item.digits, item.policy), item.expected);
    }
}

void CheckInvalidDigits() {
    constexpr std::array<unsigned, 6> invalid{101, 123, 0x7fffffffU, 0x80000000U, 0xfffffffeU, 0xffffffffU};
    for (const auto digits : invalid) {
        Check(Format(2, 1.25, digits, 0), "");
        Check(Format(3, 1.25, digits, 0), "");
        Check(Format(4, 1.25, digits, 1), "");
        for (unsigned mode = 2; mode <= 4; ++mode) {
            Check(Format(mode, std::numeric_limits<double>::infinity(), digits, 0), "Infinity");
            Check(Format(mode, -std::numeric_limits<double>::infinity(), digits, 1), "-Infinity");
            Check(Format(mode, std::numeric_limits<double>::quiet_NaN(), digits, 1), "NaN");
        }
    }
}

template<typename Float, typename Bits>
void CheckRoundTrip(Float number, Bits bits) {
    String value = [&] {
        if constexpr (sizeof(Float) == sizeof(float)) return _ZN3WTF6String6numberEf(number);
        else return _ZN3WTF6String6numberEd(number);
    }();
    const auto text = Text(value);
    if (!std::isfinite(number)) return;
    Float parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    Require(result.ec == std::errc{} && result.ptr == text.data() + text.size(), "round-trip parse");
    Require(number == 0 ? parsed == 0 && !std::signbit(parsed) : std::bit_cast<Bits>(parsed) == bits, "round-trip bits");
    Require(text.find("E") == std::string::npos, "lower-case exponent");
    const auto exponent = text.find('e');
    if (exponent != std::string::npos) {
        Require(exponent + 2 < text.size(), "exponent digits");
        Require(text[exponent + 1] == '+' || text[exponent + 1] == '-', "explicit exponent sign");
        Require(text[exponent + 2] != '0', "no exponent leading zero");
    }
}

void CheckRoundTrips() {
    constexpr std::array<std::uint64_t, 7> mantissas{0, 1, 2, 0x7ffffffffffffULL, 0x8000000000000ULL, 0xffffffffffffeULL, 0xfffffffffffffULL};
    for (std::uint64_t exponent = 0; exponent < 2047; ++exponent) {
        for (const auto mantissa : mantissas) {
            for (std::uint64_t sign = 0; sign < 2; ++sign) {
                const auto bits = (sign << 63) | (exponent << 52) | mantissa;
                CheckRoundTrip(std::bit_cast<double>(bits), bits);
            }
        }
    }
    constexpr std::array<std::uint32_t, 7> floatMantissas{0, 1, 2, 0x3fffffU, 0x400000U, 0x7ffffeU, 0x7fffffU};
    for (std::uint32_t exponent = 0; exponent < 255; ++exponent) {
        for (const auto mantissa : floatMantissas) {
            for (std::uint32_t sign = 0; sign < 2; ++sign) {
                const auto bits = (sign << 31) | (exponent << 23) | mantissa;
                CheckRoundTrip(std::bit_cast<float>(bits), bits);
            }
        }
    }
}

void CheckExactQuarters() {
    for (int quarter = -1000; quarter <= 1000; ++quarter) {
        const auto absolute = static_cast<unsigned>(quarter < 0 ? -quarter : quarter);
        const auto hundredths = (absolute % 4) * 25;
        std::string expected = quarter < 0 ? "-" : "";
        expected += std::to_string(absolute / 4);
        expected += '.';
        if (hundredths < 10) expected += '0';
        expected += std::to_string(hundredths);
        Check(Format(2, static_cast<double>(quarter) / 4, 2, 0), expected);
    }
}

class CommaPunctuation : public std::numpunct<char> {
protected:
    char do_decimal_point() const override { return ','; }
    char do_thousands_sep() const override { return '_'; }
    std::string do_grouping() const override { return "\3"; }
};

void CheckLocale() {
    const auto original = std::locale();
    std::locale::global(std::locale(original, new CommaPunctuation));
    CheckCases();
    std::locale::global(original);
}

struct FloatCase {
    std::uint32_t bits;
    unsigned digits;
    int policy;
    bool shortest;
    std::string_view expected;
};

constexpr auto floatCases = std::to_array<FloatCase>({
    {0x00000000U, 0U, 0, true, "0"},
    {0x00000000U, 2U, 0, false, "0.0"},
    {0x00000000U, 2U, 1, false, "0"},
    {0x00000000U, 9U, 0, false, "0.00000000"},
    {0x00000000U, 100U, 0, false, "0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0x80000000U, 0U, 0, true, "0"},
    {0x80000000U, 2U, 0, false, "0.0"},
    {0x80000000U, 2U, 1, false, "0"},
    {0x80000000U, 9U, 0, false, "0.00000000"},
    {0x80000000U, 100U, 0, false, "0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0x00000001U, 0U, 0, true, "1e-45"},
    {0x00000001U, 2U, 0, false, "1.4e-45"},
    {0x00000001U, 2U, 1, false, "1.4e-45"},
    {0x00000001U, 9U, 0, false, "1.40129846e-45"},
    {0x00000001U, 100U, 0, false, "1.401298464324817070923729583289916131280261941876515771757068283889791082685860601486638188362121582e-45"},
    {0x80000001U, 0U, 0, true, "-1e-45"},
    {0x80000001U, 2U, 0, false, "-1.4e-45"},
    {0x80000001U, 2U, 1, false, "-1.4e-45"},
    {0x80000001U, 9U, 0, false, "-1.40129846e-45"},
    {0x80000001U, 100U, 0, false, "-1.401298464324817070923729583289916131280261941876515771757068283889791082685860601486638188362121582e-45"},
    {0x007fffffU, 0U, 0, true, "1.1754942e-38"},
    {0x007fffffU, 2U, 0, false, "1.2e-38"},
    {0x007fffffU, 2U, 1, false, "1.2e-38"},
    {0x007fffffU, 9U, 0, false, "1.17549421e-38"},
    {0x007fffffU, 100U, 0, false, "1.175494210692441075487029444849287348827052428745893333857174530571588870475618904265502351336181164e-38"},
    {0x00800000U, 0U, 0, true, "1.1754944e-38"},
    {0x00800000U, 2U, 0, false, "1.2e-38"},
    {0x00800000U, 2U, 1, false, "1.2e-38"},
    {0x00800000U, 9U, 0, false, "1.17549435e-38"},
    {0x00800000U, 100U, 0, false, "1.175494350822287507968736537222245677818665556772087521508751706278417259454727172851562500000000000e-38"},
    {0x7f7fffffU, 0U, 0, true, "3.4028235e+38"},
    {0x7f7fffffU, 2U, 0, false, "3.4e+38"},
    {0x7f7fffffU, 2U, 1, false, "3.4e+38"},
    {0x7f7fffffU, 9U, 0, false, "3.40282347e+38"},
    {0x7f7fffffU, 100U, 0, false, "340282346638528859811704183484516925440.0000000000000000000000000000000000000000000000000000000000000"},
    {0xff7fffffU, 0U, 0, true, "-3.4028235e+38"},
    {0xff7fffffU, 2U, 0, false, "-3.4e+38"},
    {0xff7fffffU, 2U, 1, false, "-3.4e+38"},
    {0xff7fffffU, 9U, 0, false, "-3.40282347e+38"},
    {0xff7fffffU, 100U, 0, false, "-340282346638528859811704183484516925440.0000000000000000000000000000000000000000000000000000000000000"},
    {0x3dcccccdU, 0U, 0, true, "0.1"},
    {0x3dcccccdU, 2U, 0, false, "0.10"},
    {0x3dcccccdU, 2U, 1, false, "0.1"},
    {0x3dcccccdU, 9U, 0, false, "0.100000001"},
    {0x3dcccccdU, 100U, 0, false, "0.1000000014901161193847656250000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0x3fa00000U, 0U, 0, true, "1.25"},
    {0x3fa00000U, 2U, 0, false, "1.3"},
    {0x3fa00000U, 2U, 1, false, "1.3"},
    {0x3fa00000U, 9U, 0, false, "1.25000000"},
    {0x3fa00000U, 100U, 0, false, "1.250000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0xbfa00000U, 0U, 0, true, "-1.25"},
    {0xbfa00000U, 2U, 0, false, "-1.3"},
    {0xbfa00000U, 2U, 1, false, "-1.3"},
    {0xbfa00000U, 9U, 0, false, "-1.25000000"},
    {0xbfa00000U, 100U, 0, false, "-1.250000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0x3f9fffffU, 0U, 0, true, "1.2499999"},
    {0x3f9fffffU, 2U, 0, false, "1.2"},
    {0x3f9fffffU, 2U, 1, false, "1.2"},
    {0x3f9fffffU, 9U, 0, false, "1.24999988"},
    {0x3f9fffffU, 100U, 0, false, "1.249999880790710449218750000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0x3fa00001U, 0U, 0, true, "1.2500001"},
    {0x3fa00001U, 2U, 0, false, "1.3"},
    {0x3fa00001U, 2U, 1, false, "1.3"},
    {0x3fa00001U, 9U, 0, false, "1.25000012"},
    {0x3fa00001U, 100U, 0, false, "1.250000119209289550781250000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0x411fffffU, 0U, 0, true, "9.999999"},
    {0x411fffffU, 2U, 0, false, "10"},
    {0x411fffffU, 2U, 1, false, "10"},
    {0x411fffffU, 9U, 0, false, "9.99999905"},
    {0x411fffffU, 100U, 0, false, "9.999999046325683593750000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0x41200000U, 0U, 0, true, "10"},
    {0x41200000U, 2U, 0, false, "10"},
    {0x41200000U, 2U, 1, false, "10"},
    {0x41200000U, 9U, 0, false, "10.0000000"},
    {0x41200000U, 100U, 0, false, "10.00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0x41200001U, 0U, 0, true, "10.000001"},
    {0x41200001U, 2U, 0, false, "10"},
    {0x41200001U, 2U, 1, false, "10"},
    {0x41200001U, 9U, 0, false, "10.0000010"},
    {0x41200001U, 100U, 0, false, "10.00000095367431640625000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0x358637bdU, 0U, 0, true, "0.000001"},
    {0x358637bdU, 2U, 0, false, "0.0000010"},
    {0x358637bdU, 2U, 1, false, "0.000001"},
    {0x358637bdU, 9U, 0, false, "9.99999997e-7"},
    {0x358637bdU, 100U, 0, false, "9.999999974752427078783512115478515625000000000000000000000000000000000000000000000000000000000000000e-7"},
    {0x60ad78ecU, 0U, 0, true, "100000000000000000000"},
    {0x60ad78ecU, 2U, 0, false, "1.0e+20"},
    {0x60ad78ecU, 2U, 1, false, "1e+20"},
    {0x60ad78ecU, 9U, 0, false, "1.00000002e+20"},
    {0x60ad78ecU, 100U, 0, false, "100000002004087734272.0000000000000000000000000000000000000000000000000000000000000000000000000000000"},
    {0x7f800000U, 0U, 0, true, "Infinity"},
    {0x7f800000U, 2U, 0, false, "Infinity"},
    {0x7f800000U, 2U, 1, false, "Infinity"},
    {0x7f800000U, 9U, 0, false, "Infinity"},
    {0x7f800000U, 100U, 0, false, "Infinity"},
    {0xff800000U, 0U, 0, true, "-Infinity"},
    {0xff800000U, 2U, 0, false, "-Infinity"},
    {0xff800000U, 2U, 1, false, "-Infinity"},
    {0xff800000U, 9U, 0, false, "-Infinity"},
    {0xff800000U, 100U, 0, false, "-Infinity"},
    {0x7fc00001U, 0U, 0, true, "NaN"},
    {0x7fc00001U, 2U, 0, false, "NaN"},
    {0x7fc00001U, 2U, 1, false, "NaN"},
    {0x7fc00001U, 9U, 0, false, "NaN"},
    {0x7fc00001U, 100U, 0, false, "NaN"},
});

void CheckFloatCases() {
    for (const auto& test : floatCases) {
        const float number = std::bit_cast<float>(test.bits);
        if (test.shortest) Check(_ZN3WTF6String6numberEf(number), test.expected);
        else Check(_ZN3WTF6String28numberToStringFixedPrecisionEfjNS_29TrailingZerosTruncatingPolicyE(number, test.digits, test.policy), test.expected);
    }
}

void CheckRoundingModes() {
    const int original = std::fegetround();
    for (const auto mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        if (std::fesetround(mode) == 0) {
            CheckCases(true);
            CheckFloatCases();
        }
    }
    Require(std::fesetround(original) == 0, "restore rounding mode");
}

}

int main() {
    CheckCases();
    CheckInvalidDigits();
    CheckRoundTrips();
    CheckExactQuarters();
    CheckLocale();
    CheckRoundingModes();
    std::printf("WTF number formatting: %llu assertions passed\n", static_cast<unsigned long long>(assertions));
}
