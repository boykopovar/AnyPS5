#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

struct Character {
    std::uint32_t value;
    std::size_t width;
};

bool Lead(char16_t value) { return value >= 0xd800 && value <= 0xdbff; }
bool Trail(char16_t value) { return value >= 0xdc00 && value <= 0xdfff; }

Character Next(const char16_t* text) {
    if (Lead(text[0]) && Trail(text[1]))
        return {0x10000u + ((text[0] - 0xd800u) << 10) + (text[1] - 0xdc00u), 2};
    return {text[0], 1};
}

bool Contains(const char16_t* set, std::size_t length, std::uint32_t value) {
    for (std::size_t position = 0; position < length;) {
        const auto character = Next(set + position);
        if (character.value == value) return true;
        position += character.width;
    }
    return false;
}

std::size_t FindSet(const char16_t* text, const char16_t* set, bool matches) {
    constexpr auto maximum = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    std::size_t length = 0;
    while (set[length] != 0) if (++length > maximum) return std::numeric_limits<std::size_t>::max();
    std::size_t position = 0;
    while (text[position] != 0) {
        const auto character = Next(text + position);
        if (Contains(set, length, character.value) == matches) break;
        position += character.width;
        if (position > maximum) return std::numeric_limits<std::size_t>::max();
    }
    return position;
}

} // namespace

extern "C" {

std::int32_t APS5_VABI u_strspn_nid_postfix(const char16_t* text, const char16_t* set) {
    if (!text || !set) NotImplemented_nid_no_patch(__func__);
    const auto position = FindSet(text, set, false);
    if (position > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    return static_cast<std::int32_t>(position);
}

std::int32_t APS5_VABI u_strcspn_nid_postfix(const char16_t* text, const char16_t* set) {
    if (!text || !set) NotImplemented_nid_no_patch(__func__);
    const auto position = FindSet(text, set, true);
    if (position > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    return static_cast<std::int32_t>(position);
}

char16_t* APS5_VABI u_strpbrk_nid_postfix(const char16_t* text, const char16_t* set) {
    if (!text || !set) NotImplemented_nid_no_patch(__func__);
    const auto position = FindSet(text, set, true);
    if (position > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    return text[position] != 0 ? const_cast<char16_t*>(text + position) : nullptr;
}

char16_t* APS5_VABI u_strtok_r_nid_postfix(char16_t* source, const char16_t* delimiters, char16_t** state) {
    if (!state) NotImplemented_nid_no_patch(__func__);
    auto* text = source ? source : *state;
    if (!text) return nullptr;
    if (!delimiters) NotImplemented_nid_no_patch(__func__);
    if (source) *state = source;
    const auto skipped = FindSet(text, delimiters, false);
    if (skipped > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    text += skipped;
    if (*text == 0) {
        *state = nullptr;
        return nullptr;
    }
    const auto length = FindSet(text, delimiters, true);
    if (length > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    auto* next = text + length;
    if (*next != 0) {
        *next++ = 0;
        *state = next;
    } else *state = nullptr;
    return text;
}

char16_t* APS5_VABI u_memrchr32_nid_postfix(const char16_t* text, std::int32_t value, std::int32_t count) {
    if (count <= 0 || static_cast<std::uint32_t>(value) > 0x10ffffu || (value > 0xffff && count < 2)) return nullptr;
    if (!text) {
        if (value >= 0xd800 && value <= 0xdfff) return nullptr;
        NotImplemented_nid_no_patch(__func__);
    }
    auto position = static_cast<std::size_t>(count);
    while (position != 0) {
        --position;
        std::uint32_t character = text[position];
        if (Trail(text[position]) && position != 0 && Lead(text[position - 1])) {
            --position;
            character = 0x10000u + ((text[position] - 0xd800u) << 10) + (text[position + 1] - 0xdc00u);
        }
        if (character == static_cast<std::uint32_t>(value)) return const_cast<char16_t*>(text + position);
    }
    return nullptr;
}

}
