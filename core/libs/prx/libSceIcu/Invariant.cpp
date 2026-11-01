#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstdint>

namespace {

constexpr std::array<std::uint32_t, 4> InvariantChars{0xfffffbff, 0xffffffe5, 0x87fffffe, 0x87fffffe};

bool Invariant(char16_t unit) {
    return unit <= 0x7f && (InvariantChars[unit >> 5] & (1u << (unit & 0x1f))) != 0;
}

}

extern "C" {

void APS5_VABI u_charsToUChars_nid_postfix(const char* chars, char16_t* units, std::int32_t length) {
    if (length > 0 && (!chars || !units)) APS5_INVALID_ARG_EX;
    for (std::int32_t index = 0; index < length; ++index) units[index] = static_cast<std::uint8_t>(chars[index]);
}

void APS5_VABI u_UCharsToChars_nid_postfix(const char16_t* units, char* chars, std::int32_t length) {
    if (length > 0 && (!units || !chars)) APS5_INVALID_ARG_EX;
    for (std::int32_t index = 0; index < length; ++index) {
        const char16_t unit = units[index];
        chars[index] = Invariant(unit) ? static_cast<char>(unit) : 0;
    }
}

char* APS5_VABI u_austrcpy_nid_postfix(char* chars, const char16_t* units) {
    if (!chars || !units) APS5_INVALID_ARG_EX;
    std::int32_t length = 0;
    while (units[length] != 0) {
        if (units[length] > 0x7f) NotImplemented_nid_no_patch(__func__);
        ++length;
    }
    for (std::int32_t index = 0; index <= length; ++index) chars[index] = static_cast<char>(units[index]);
    return chars;
}

char* APS5_VABI u_austrncpy_nid_postfix(char* chars, const char16_t* units, std::int32_t length) {
    if (length > 0 && (!chars || !units)) APS5_INVALID_ARG_EX;
    if (length <= 0) return chars;
    std::int32_t copied = 0;
    while (copied < length && units[copied] != 0) {
        if (units[copied] > 0x7f) NotImplemented_nid_no_patch(__func__);
        ++copied;
    }
    for (std::int32_t index = 0; index < copied; ++index) chars[index] = static_cast<char>(units[index]);
    if (copied < length) chars[copied] = 0;
    return chars;
}

}
