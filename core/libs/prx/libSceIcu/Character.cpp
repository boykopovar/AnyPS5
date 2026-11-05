#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/Character.hpp"
#include <algorithm>
#include <array>
#include <cstdint>

namespace {

struct DecimalRange {
    std::int32_t first;
    std::int32_t last;
    std::int32_t value;
};

constexpr std::array<DecimalRange, 72> DecimalDigits{{
    {0x000030, 0x000039, 0}, {0x000660, 0x000669, 0}, {0x0006F0, 0x0006F9, 0}, {0x0007C0, 0x0007C9, 0},
    {0x000966, 0x00096F, 0}, {0x0009E6, 0x0009EF, 0}, {0x000A66, 0x000A6F, 0}, {0x000AE6, 0x000AEF, 0},
    {0x000B66, 0x000B6F, 0}, {0x000BE6, 0x000BEF, 0}, {0x000C66, 0x000C6F, 0}, {0x000CE6, 0x000CEF, 0},
    {0x000D66, 0x000D6F, 0}, {0x000DE6, 0x000DEF, 0}, {0x000E50, 0x000E59, 0}, {0x000ED0, 0x000ED9, 0},
    {0x000F20, 0x000F29, 0}, {0x001040, 0x001049, 0}, {0x001090, 0x001099, 0}, {0x0017E0, 0x0017E9, 0},
    {0x001810, 0x001819, 0}, {0x001946, 0x00194F, 0}, {0x0019D0, 0x0019D9, 0}, {0x001A80, 0x001A89, 0},
    {0x001A90, 0x001A99, 0}, {0x001B50, 0x001B59, 0}, {0x001BB0, 0x001BB9, 0}, {0x001C40, 0x001C49, 0},
    {0x001C50, 0x001C59, 0}, {0x00A620, 0x00A629, 0}, {0x00A8D0, 0x00A8D9, 0}, {0x00A900, 0x00A909, 0},
    {0x00A9D0, 0x00A9D9, 0}, {0x00A9F0, 0x00A9F9, 0}, {0x00AA50, 0x00AA59, 0}, {0x00ABF0, 0x00ABF9, 0},
    {0x00FF10, 0x00FF19, 0}, {0x0104A0, 0x0104A9, 0}, {0x010D30, 0x010D39, 0}, {0x010D40, 0x010D49, 0},
    {0x011066, 0x01106F, 0}, {0x0110F0, 0x0110F9, 0}, {0x011136, 0x01113F, 0}, {0x0111D0, 0x0111D9, 0},
    {0x0112F0, 0x0112F9, 0}, {0x011450, 0x011459, 0}, {0x0114D0, 0x0114D9, 0}, {0x011650, 0x011659, 0},
    {0x0116C0, 0x0116C9, 0}, {0x0116D0, 0x0116E3, 0}, {0x011730, 0x011739, 0}, {0x0118E0, 0x0118E9, 0},
    {0x011950, 0x011959, 0}, {0x011BF0, 0x011BF9, 0}, {0x011C50, 0x011C59, 0}, {0x011D50, 0x011D59, 0},
    {0x011DA0, 0x011DA9, 0}, {0x011DE0, 0x011DE9, 0}, {0x011F50, 0x011F59, 0}, {0x016130, 0x016139, 0},
    {0x016A60, 0x016A69, 0}, {0x016AC0, 0x016AC9, 0}, {0x016B50, 0x016B59, 0}, {0x016D70, 0x016D79, 0},
    {0x01CCF0, 0x01CCF9, 0}, {0x01D7CE, 0x01D7FF, 0}, {0x01E140, 0x01E149, 0}, {0x01E2F0, 0x01E2F9, 0},
    {0x01E4F0, 0x01E4F9, 0}, {0x01E5F1, 0x01E5FA, 0}, {0x01E950, 0x01E959, 0}, {0x01FBF0, 0x01FBF9, 0},
}};

bool IsDecimalDigit(std::int32_t character, std::int32_t& value) {
    const auto range = std::lower_bound(DecimalDigits.begin(), DecimalDigits.end(), character,
        [](const DecimalRange& entry, std::int32_t code) { return entry.last < code; });
    if (range == DecimalDigits.end() || character < range->first || character > range->last) return false;
    value = (range->value + (character - range->first)) % 10;
    return true;
}

} // namespace

extern "C" {

std::int32_t APS5_VABI u_charDigitValue_nid_postfix(std::int32_t character) {
    std::int32_t value = -1;
    return IsDecimalDigit(character, value) ? value : -1;
}

std::int32_t APS5_VABI u_digit_nid_postfix(std::int32_t character, std::int8_t radix) {
    std::int32_t value = -1;
    if (static_cast<std::uint8_t>(radix - 2) <= 34) {
        value = u_charDigitValue_nid_postfix(character);
        if (value < 0) {
            if (character >= 0x61 && character <= 0x7A) value = character - 0x57;
            else if (character >= 0x41 && character <= 0x5A) value = character - 0x37;
            else if (character >= 0xFF41 && character <= 0xFF5A) value = character - 0xFF37;
            else if (character >= 0xFF21 && character <= 0xFF3A) value = character - 0xFF17;
        }
    }
    return value < radix ? value : -1;
}

std::int8_t APS5_VABI u_isdigit_nid_postfix(std::int32_t character) {
    std::int32_t value = -1;
    return IsDecimalDigit(character, value) ? 1 : 0;
}

std::int8_t APS5_VABI u_isxdigit_nid_postfix(std::int32_t character) {
    if ((character <= 0x66 && character >= 0x41 && (character <= 0x46 || character >= 0x61)) ||
        (character >= 0xFF21 && character <= 0xFF46 && (character <= 0xFF26 || character >= 0xFF41))) {
        return 1;
    }
    std::int32_t value = -1;
    return IsDecimalDigit(character, value) ? 1 : 0;
}

}
