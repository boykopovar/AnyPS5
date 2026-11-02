#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace {

struct Range {
    std::int32_t first;
    std::int32_t last;
};

constexpr std::array<Range, 7> SpaceSeparators{{
    {0x20, 0x20}, {0xa0, 0xa0}, {0x1680, 0x1680}, {0x2000, 0x200a}, {0x202f, 0x202f}, {0x205f, 0x205f}, {0x3000, 0x3000}}};
constexpr std::array<Range, 10> WhiteSpace{{
    {0x09, 0x0d}, {0x20, 0x20}, {0x85, 0x85}, {0xa0, 0xa0}, {0x1680, 0x1680},
    {0x2000, 0x200a}, {0x2028, 0x2029}, {0x202f, 0x202f}, {0x205f, 0x205f}, {0x3000, 0x3000}}};

bool Contains(std::span<const Range> ranges, std::int32_t character) {
    return std::any_of(ranges.begin(), ranges.end(), [character](const Range& range) {
        return character >= range.first && character <= range.last;
    });
}

bool IsSpaceSeparator(std::int32_t character) {
    return Contains(SpaceSeparators, character);
}

bool IsSeparator(std::int32_t character) {
    return IsSpaceSeparator(character) || character == 0x2028 || character == 0x2029;
}

bool IsAsciiControlSpace(std::int32_t character) {
    return (character >= 0x09 && character <= 0x0d) || (character >= 0x1c && character <= 0x1f);
}

bool IsNoBreakSpace(std::int32_t character) {
    return character == 0xa0 || character == 0x2007 || character == 0x202f;
}

}

extern "C" {

std::int8_t APS5_VABI u_isspace_nid_postfix(std::int32_t character) {
    return IsSeparator(character) || IsAsciiControlSpace(character) || character == 0x85;
}

std::int8_t APS5_VABI u_isblank_nid_postfix(std::int32_t character) {
    if (static_cast<std::uint32_t>(character) <= 0x9f) return character == 0x09 || character == 0x20;
    return IsSpaceSeparator(character);
}

std::int8_t APS5_VABI u_isWhitespace_nid_postfix(std::int32_t character) {
    return (IsSeparator(character) && !IsNoBreakSpace(character)) || IsAsciiControlSpace(character);
}

std::int8_t APS5_VABI u_isJavaSpaceChar_nid_postfix(std::int32_t character) {
    return IsSeparator(character);
}

std::int8_t APS5_VABI u_isUWhiteSpace_nid_postfix(std::int32_t character) {
    return Contains(WhiteSpace, character);
}

}
