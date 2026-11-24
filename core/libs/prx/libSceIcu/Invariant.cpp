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
    if (length < 0) {
        if (!chars) APS5_INVALID_ARG_EX;
        chars[0] = 0;
        return chars;
    }
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

char16_t* APS5_VABI u_uastrcpy_nid_postfix(char16_t* units, const char* chars) {
    if (!units || !chars) APS5_INVALID_ARG_EX;
    std::int32_t length = 0;
    while (chars[length] != 0) {
        if (static_cast<std::uint8_t>(chars[length]) > 0x7f) NotImplemented_nid_no_patch(__func__);
        ++length;
    }
    for (std::int32_t index = 0; index <= length; ++index) units[index] = static_cast<char16_t>(chars[index]);
    return units;
}

char16_t* APS5_VABI u_uastrncpy_nid_postfix(char16_t* units, const char* chars, std::int32_t length) {
    if (length < 0) {
        if (!units) APS5_INVALID_ARG_EX;
        units[0] = 0;
        return units;
    }
    if (length > 0 && (!units || !chars)) APS5_INVALID_ARG_EX;
    if (length <= 0) return units;
    std::int32_t copied = 0;
    while (copied < length && chars[copied] != 0) {
        if (static_cast<std::uint8_t>(chars[copied]) > 0x7f) NotImplemented_nid_no_patch(__func__);
        ++copied;
    }
    for (std::int32_t index = 0; index < copied; ++index) units[index] = static_cast<char16_t>(chars[index]);
    if (copied < length) units[copied] = 0;
    return units;
}

}
