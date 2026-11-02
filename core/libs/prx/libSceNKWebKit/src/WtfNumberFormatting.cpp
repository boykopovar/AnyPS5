#include "prx/libSceNKWebKit/include/WtfNumberFormatting.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using NKWebKit::WtfAbi::String;

struct Decimal {
    std::string digits;
    int point;
};

String Result(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

std::string Special(double number) {
    if (std::isnan(number)) return "NaN";
    return std::signbit(number) ? "-Infinity" : "Infinity";
}

void Multiply(std::vector<std::uint32_t>& limbs, std::uint32_t factor) {
    std::uint64_t carry = 0;
    for (auto& limb : limbs) {
        const std::uint64_t product = static_cast<std::uint64_t>(limb) * factor + carry;
        limb = static_cast<std::uint32_t>(product % 1000000000);
        carry = product / 1000000000;
    }
    while (carry) {
        limbs.push_back(static_cast<std::uint32_t>(carry % 1000000000));
        carry /= 1000000000;
    }
}

Decimal Exact(double number) {
    const auto bits = std::bit_cast<std::uint64_t>(number) & 0x7fffffffffffffffULL;
    std::uint64_t mantissa = bits & 0xfffffffffffffULL;
    const int encodedExponent = static_cast<int>(bits >> 52);
    int exponent = -1074;
    if (encodedExponent) {
        mantissa |= 1ULL << 52;
        exponent = encodedExponent - 1075;
    }
    if (!mantissa) return {"0", 1};
    const int scale = std::max(-exponent, 0);
    std::vector<std::uint32_t> limbs;
    while (mantissa) {
        limbs.push_back(static_cast<std::uint32_t>(mantissa % 1000000000));
        mantissa /= 1000000000;
    }
    while (exponent > 0) {
        const int count = std::min(exponent, 29);
        Multiply(limbs, 1U << count);
        exponent -= count;
    }
    while (exponent < 0) {
        const int count = std::min(-exponent, 13);
        std::uint32_t factor = 1;
        for (int index = 0; index < count; ++index) factor *= 5;
        Multiply(limbs, factor);
        exponent += count;
    }
    std::string digits = std::to_string(limbs.back());
    for (std::size_t index = limbs.size() - 1; index > 0; --index) {
        const auto part = std::to_string(limbs[index - 1]);
        digits.append(9 - part.size(), '0');
        digits += part;
    }
    const int point = static_cast<int>(digits.size()) - scale;
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    return {std::move(digits), point};
}

void Round(Decimal& value, int count) {
    if (count <= 0) {
        const bool increase = count == 0 && value.digits.front() >= '5';
        value.digits = increase ? "1" : "0";
        value.point = increase ? value.point + 1 : 1;
        return;
    }
    if (count >= static_cast<int>(value.digits.size())) {
        value.digits.append(static_cast<std::size_t>(count) - value.digits.size(), '0');
        return;
    }
    const bool increase = value.digits[static_cast<std::size_t>(count)] >= '5';
    value.digits.resize(static_cast<std::size_t>(count));
    if (!increase) return;
    for (auto index = value.digits.size(); index > 0; --index) {
        if (value.digits[index - 1] != '9') {
            ++value.digits[index - 1];
            return;
        }
        value.digits[index - 1] = '0';
    }
    value.digits.front() = '1';
    ++value.point;
}

std::string Plain(const Decimal& value) {
    if (value.point <= 0) return "0." + std::string(static_cast<std::size_t>(-value.point), '0') + value.digits;
    if (value.point >= static_cast<int>(value.digits.size()))
        return value.digits + std::string(static_cast<std::size_t>(value.point) - value.digits.size(), '0');
    auto result = value.digits;
    result.insert(static_cast<std::size_t>(value.point), 1, '.');
    return result;
}

std::string Scientific(const Decimal& value) {
    std::string result(1, value.digits.front());
    if (value.digits.size() > 1) result += "." + value.digits.substr(1);
    const int exponent = value.point - 1;
    result += exponent >= 0 ? "e+" : "e-";
    result += std::to_string(exponent >= 0 ? exponent : -exponent);
    return result;
}

void Truncate(std::string& text) {
    const auto dot = text.find('.');
    if (dot == std::string::npos) return;
    const auto exponent = text.find('e', dot);
    const auto end = exponent == std::string::npos ? text.size() : exponent;
    auto position = end;
    while (position > dot + 1 && text[position - 1] == '0') --position;
    if (position == dot + 1) position = dot;
    text.erase(position, end - position);
}

template<typename Float>
String Shortest(Float number) {
    if (!std::isfinite(number)) return Result(Special(number));
    if (number == 0) return Result("0");
    char buffer[64];
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), std::fabs(number), std::chars_format::scientific);
    if (converted.ec != std::errc{}) throw std::runtime_error("WTF shortest number formatting failed");
    const std::string_view text(buffer, static_cast<std::size_t>(converted.ptr - buffer));
    const auto separator = text.find('e');
    if (separator == std::string_view::npos) throw std::runtime_error("WTF scientific exponent is missing");
    int exponent = 0;
    auto exponentText = text.substr(separator + 1);
    if (exponentText.front() == '+') exponentText.remove_prefix(1);
    const auto parsed = std::from_chars(exponentText.data(), exponentText.data() + exponentText.size(), exponent);
    if (parsed.ec != std::errc{} || parsed.ptr != exponentText.data() + exponentText.size())
        throw std::runtime_error("WTF scientific exponent is invalid");
    Decimal value{std::string(text.substr(0, separator)), exponent + 1};
    value.digits.erase(std::remove(value.digits.begin(), value.digits.end(), '.'), value.digits.end());
    auto result = exponent >= -6 && exponent < 21 ? Plain(value) : Scientific(value);
    if (number < 0) result.insert(result.begin(), '-');
    return Result(result);
}

String Precision(double number, unsigned precision, int policy) {
    if (!std::isfinite(number)) return Result(Special(number));
    if (precision < 1 || precision > 100) return Result("");
    auto value = Exact(number);
    Round(value, static_cast<int>(precision));
    auto result = value.point < -5 || value.point > static_cast<int>(precision) ? Scientific(value) : Plain(value);
    if (policy == 1) Truncate(result);
    if (number < 0) result.insert(result.begin(), '-');
    return Result(result);
}

String Fixed(double number, unsigned decimalPlaces) {
    if (!std::isfinite(number)) return Result(Special(number));
    if (decimalPlaces > 100 || std::fabs(number) >= 1e21) return Result("");
    auto value = Exact(number);
    Round(value, value.point + static_cast<int>(decimalPlaces));
    auto result = Plain(value);
    const auto dot = result.find('.');
    const auto places = dot == std::string::npos ? 0 : result.size() - dot - 1;
    if (places < decimalPlaces) {
        if (dot == std::string::npos) result += '.';
        result.append(decimalPlaces - places, '0');
    }
    if (number < 0) result.insert(result.begin(), '-');
    return Result(result);
}

}

extern "C" {

String APS5_VABI _ZN3WTF6String24numberToStringFixedWidthEdj(double number, unsigned decimalPlaces) {
    return Fixed(number, decimalPlaces);
}

String APS5_VABI _ZN3WTF6String28numberToStringFixedPrecisionEdjNS_29TrailingZerosTruncatingPolicyE(double number, unsigned precision, int policy) {
    return Precision(number, precision, policy);
}

String APS5_VABI _ZN3WTF6String28numberToStringFixedPrecisionEfjNS_29TrailingZerosTruncatingPolicyE(float number, unsigned precision, int policy) {
    return Precision(static_cast<double>(number), precision, policy);
}

String APS5_VABI _ZN3WTF6String6numberEd(double number) {
    return Shortest(number);
}

String APS5_VABI _ZN3WTF6String6numberEf(float number) {
    return Shortest(number);
}

}
