#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <type_traits>

namespace {

template<class TValue>
std::int32_t FormatInteger(char* buffer, TValue value, std::uint32_t radix) {
    using TUnsigned = std::make_unsigned_t<TValue>;
    auto magnitude = static_cast<TUnsigned>(value);
    const bool negative = value < 0 && radix == 10;
    std::int32_t length = 0;
    if (negative) {
        magnitude = TUnsigned{0} - magnitude;
        buffer[length++] = '-';
    }
    const std::int32_t firstDigit = length;
    do {
        buffer[length++] = "0123456789ABCDEF"[magnitude % radix];
        magnitude /= radix;
    } while (magnitude);
    std::reverse(buffer + firstDigit, buffer + length);
    buffer[length] = 0;
    return length;
}

char* ConvertCase(char* text, char first, char last, int offset) {
    if (!text) return nullptr;
    for (char* cursor = text;; ++cursor) {
        const char value = *cursor;
        *cursor = value >= first && value <= last ? static_cast<char>(value + offset) : value;
        if (!value) return text;
    }
}

}

extern "C" {

std::int32_t APS5_VABI T_CString_integerToString_nid_postfix(char* buffer, std::int32_t value, std::int32_t radix) {
    if (!buffer || radix < 2 || radix > 16) APS5_INVALID_ARG_EX;
    return FormatInteger(buffer, value, static_cast<std::uint32_t>(radix));
}

std::int32_t APS5_VABI T_CString_int64ToString_nid_postfix(char* buffer, std::int64_t value, std::uint32_t radix) {
    if (!buffer || radix < 2 || radix > 16) APS5_INVALID_ARG_EX;
    return FormatInteger(buffer, value, radix);
}

std::int32_t APS5_VABI T_CString_stringToInteger_nid_postfix(const char* text, std::int32_t radix) {
    if (!text) APS5_INVALID_ARG_EX;
    const auto value = static_cast<std::uint32_t>(strtoul_nid_postfix(text, nullptr, radix));
    return std::bit_cast<std::int32_t>(value);
}

char* APS5_VABI T_CString_toLowerCase_nid_postfix(char* text) {
    return ConvertCase(text, 'A', 'Z', 'a' - 'A');
}

char* APS5_VABI T_CString_toUpperCase_nid_postfix(char* text) {
    return ConvertCase(text, 'a', 'z', 'A' - 'a');
}

}
