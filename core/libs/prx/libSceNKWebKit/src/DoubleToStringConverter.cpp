#include "prx/libSceNKWebKit/include/DoubleConversionExports.hpp"

#include <charconv>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

constexpr int kEmitPositiveExponentSign = 1;
constexpr int kUniqueZero = 8;
constexpr int kMaxExponentialDigits = 100;

using NKWebKit::WtfAbi::DoubleConversionStringBuilder;
using NKWebKit::WtfAbi::DoubleToStringConverter;

void ValidateBuilder(const DoubleConversionStringBuilder& builder) {
    if (!builder.buffer.start || builder.buffer.length <= 0 || builder.position < 0)
        throw std::invalid_argument("WTF::double_conversion::StringBuilder: invalid builder state");
}

void AppendCharacter(DoubleConversionStringBuilder& builder, char ch) {
    ValidateBuilder(builder);
    if (ch == '\0')
        throw std::invalid_argument("WTF::double_conversion::StringBuilder: NUL character");
    if (builder.position + 1 >= builder.buffer.length)
        throw std::out_of_range("WTF::double_conversion::StringBuilder: buffer overflow");
    builder.buffer.start[builder.position++] = ch;
}

void AppendSubstring(DoubleConversionStringBuilder& builder, const char* text, int count) {
    ValidateBuilder(builder);
    if (count < 0)
        throw std::invalid_argument("WTF::double_conversion::StringBuilder: negative substring length");
    if (count == 0)
        return;
    if (!text)
        throw std::invalid_argument("WTF::double_conversion::StringBuilder: null substring");
    if (builder.position + count >= builder.buffer.length)
        throw std::out_of_range("WTF::double_conversion::StringBuilder: buffer overflow");
    std::memcpy(builder.buffer.start + builder.position, text, static_cast<std::size_t>(count));
    builder.position += count;
}

void AppendString(DoubleConversionStringBuilder& builder, const char* text) {
    AppendSubstring(builder, text, static_cast<int>(std::strlen(text)));
}

void CreateExponentialRepresentation(
    const DoubleToStringConverter& converter,
    const std::string& digits,
    int exponent,
    DoubleConversionStringBuilder& builder) {
    AppendCharacter(builder, digits[0]);
    if (digits.size() != 1) {
        AppendCharacter(builder, '.');
        AppendSubstring(builder, digits.data() + 1, static_cast<int>(digits.size() - 1));
    }
    AppendCharacter(builder, converter.exponentCharacter);
    if (exponent < 0) {
        AppendCharacter(builder, '-');
        exponent = -exponent;
    } else if ((converter.flags & kEmitPositiveExponentSign) != 0) {
        AppendCharacter(builder, '+');
    }
    if (exponent == 0) {
        AppendCharacter(builder, '0');
        return;
    }
    std::string expDigits = std::to_string(exponent);
    AppendSubstring(builder, expDigits.data(), static_cast<int>(expDigits.size()));
}

bool HandleSpecialValues(
    const DoubleToStringConverter& converter,
    double value,
    DoubleConversionStringBuilder& builder) {
    if (std::isinf(value)) {
        if (!converter.infinitySymbol)
            return false;
        if (value < 0.0)
            AppendCharacter(builder, '-');
        AppendString(builder, converter.infinitySymbol);
        return true;
    }
    if (std::isnan(value)) {
        if (!converter.nanSymbol)
            return false;
        AppendString(builder, converter.nanSymbol);
        return true;
    }
    return false;
}

}  // namespace

extern "C" {

const DoubleToStringConverter* APS5_VABI
_ZN3WTF17double_conversion23DoubleToStringConverter19EcmaScriptConverterEv() {
    static constexpr DoubleToStringConverter converter{
        kUniqueZero | kEmitPositiveExponentSign,
        "Infinity",
        "NaN",
        'e',
        -6,
        21,
        6,
        0,
    };
    return &converter;
}

bool APS5_VABI
_ZNK3WTF17double_conversion23DoubleToStringConverter13ToExponentialEdiPNS0_13StringBuilderE(
    const DoubleToStringConverter* self,
    double value,
    int requestedDigits,
    DoubleConversionStringBuilder* resultBuilder) {
    if (!self || !resultBuilder)
        throw std::invalid_argument("WTF::double_conversion::DoubleToStringConverter::ToExponential: null pointer");
    if (!std::isfinite(value))
        return HandleSpecialValues(*self, value, *resultBuilder);
    if (requestedDigits < -1 || requestedDigits > kMaxExponentialDigits)
        return false;

    std::string digits;
    int exponent = 0;
    bool sign = std::signbit(value);

    if (value == 0.0) {
        int totalDigits = requestedDigits == -1 ? 1 : requestedDigits + 1;
        digits.assign(static_cast<std::size_t>(totalDigits), '0');
        exponent = 0;
    } else {
        char buffer[1024];
        double absValue = std::fabs(value);
        constexpr int kExactPrecision = 800;
        auto converted = requestedDigits == -1
            ? std::to_chars(buffer, buffer + sizeof(buffer), absValue, std::chars_format::scientific)
            : std::to_chars(buffer, buffer + sizeof(buffer), absValue, std::chars_format::scientific, kExactPrecision);
        if (converted.ec != std::errc{})
            throw std::runtime_error("WTF::double_conversion::DoubleToStringConverter::ToExponential: conversion failed");

        const char* ePtr = buffer;
        while (ePtr != converted.ptr && *ePtr != 'e')
            ++ePtr;
        digits.reserve(static_cast<std::size_t>(ePtr - buffer));
        for (const char* it = buffer; it != ePtr; ++it) {
            if (*it != '.')
                digits.push_back(*it);
        }
        const char* expStart = ePtr + 1;
        if (expStart < converted.ptr && *expStart == '+')
            ++expStart;
        if (ePtr == converted.ptr || std::from_chars(expStart, converted.ptr, exponent).ec != std::errc{})
            throw std::runtime_error("WTF::double_conversion::DoubleToStringConverter::ToExponential: exponent conversion failed");

        if (requestedDigits >= 0) {
            std::size_t keepCount = static_cast<std::size_t>(requestedDigits + 1);
            bool roundUp = digits.size() > keepCount && digits[keepCount] >= '5';
            digits.resize(keepCount, '0');
            if (roundUp) {
                bool carry = true;
                for (std::size_t i = keepCount; i > 0; --i) {
                    if (digits[i - 1] < '9') {
                        ++digits[i - 1];
                        carry = false;
                        break;
                    }
                    digits[i - 1] = '0';
                }
                if (carry) {
                    digits[0] = '1';
                    ++exponent;
                }
            }
        }
    }

    bool uniqueZero = (self->flags & kUniqueZero) != 0;
    if (sign && (value != 0.0 || !uniqueZero))
        AppendCharacter(*resultBuilder, '-');

    CreateExponentialRepresentation(*self, digits, exponent, *resultBuilder);
    return true;
}

}
