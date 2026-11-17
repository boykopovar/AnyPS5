#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestPtr.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace {

struct Character {
    std::uint32_t value;
    std::size_t width;
};

bool Lead(char16_t value) { return value >= 0xd800 && value <= 0xdbff; }
bool Trail(char16_t value) { return value >= 0xdc00 && value <= 0xdfff; }

class GuestText {
public:
    GuestText(const char16_t* source, const char* function) : source(source), function(function) {}

    char16_t operator[](std::size_t index) {
        while (index >= units.size()) Load();
        return units[index];
    }

private:
    void Load() {
        char16_t chunk[0x800];
        const auto* address = source + units.size();
        const auto offset = reinterpret_cast<std::uintptr_t>(address) % 0x1000;
        const std::size_t count = (std::max)(std::size_t{1}, (0x1000 - offset) / sizeof(char16_t));
        if (!GuestCopyIn_nid_no_patch(chunk, address, count * sizeof(char16_t))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        const auto used = static_cast<std::size_t>(std::find(chunk, chunk + count, char16_t{0}) - chunk);
        units.append(chunk, (std::min)(count, used + 1));
    }

    const char16_t* source;
    const char* function;
    std::u16string units;
};

Character Next(GuestText& text, std::size_t position) {
    if (Lead(text[position]) && Trail(text[position + 1]))
        return {0x10000u + ((text[position] - 0xd800u) << 10) + (text[position + 1] - 0xdc00u), 2};
    return {text[position], 1};
}

bool Contains(GuestText& set, std::size_t length, std::uint32_t value) {
    for (std::size_t position = 0; position < length;) {
        const auto character = Next(set, position);
        if (character.value == value) return true;
        position += character.width;
    }
    return false;
}

std::size_t FindSet(GuestText& text, std::size_t start, GuestText& set, bool matches) {
    constexpr auto maximum = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    std::size_t length = 0;
    while (set[length] != 0) if (++length > maximum) return std::numeric_limits<std::size_t>::max();
    std::size_t position = 0;
    while (text[start + position] != 0) {
        const auto character = Next(text, start + position);
        if (Contains(set, length, character.value) == matches) break;
        position += character.width;
        if (position > maximum) return std::numeric_limits<std::size_t>::max();
    }
    return position;
}

std::size_t FindSet(const char16_t* text, const char16_t* set, bool matches, const char* function) {
    GuestText setText(set, function);
    GuestText guestText(text, function);
    return FindSet(guestText, 0, setText, matches);
}

} // namespace

extern "C" {

std::int32_t APS5_VABI u_strspn_nid_postfix(const char16_t* text, const char16_t* set) {
    if (!text || !set) NotImplemented_nid_no_patch(__func__);
    const auto position = FindSet(text, set, false, __func__);
    if (position > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    return static_cast<std::int32_t>(position);
}

std::int32_t APS5_VABI u_strcspn_nid_postfix(const char16_t* text, const char16_t* set) {
    if (!text || !set) NotImplemented_nid_no_patch(__func__);
    const auto position = FindSet(text, set, true, __func__);
    if (position > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    return static_cast<std::int32_t>(position);
}

char16_t* APS5_VABI u_strpbrk_nid_postfix(const char16_t* text, const char16_t* set) {
    if (!text || !set) NotImplemented_nid_no_patch(__func__);
    GuestText setText(set, __func__);
    GuestText guestText(text, __func__);
    const auto position = FindSet(guestText, 0, setText, true);
    if (position > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    return guestText[position] != 0 ? const_cast<char16_t*>(text + position) : nullptr;
}

char16_t* APS5_VABI u_strtok_r_nid_postfix(char16_t* source, const char16_t* delimiters, char16_t** state) {
    if (!state) NotImplemented_nid_no_patch(__func__);
    const GuestPtr<char16_t*> continuation(state);
    auto* text = source;
    if (!text && !continuation.CopyIn(text)) NotImplemented_nid_no_patch(__func__);
    if (!text) return nullptr;
    if (!delimiters) NotImplemented_nid_no_patch(__func__);
    if (!continuation.CopyOut(text)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    GuestText delimiterText(delimiters, __func__);
    GuestText guestText(text, __func__);
    const auto skipped = FindSet(guestText, 0, delimiterText, false);
    if (skipped > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    if (guestText[skipped] == 0) {
        if (!continuation.CopyOut(nullptr)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        return nullptr;
    }
    const auto length = FindSet(guestText, skipped, delimiterText, true);
    if (length > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    auto* token = text + skipped;
    char16_t* next = nullptr;
    if (guestText[skipped + length] != 0) {
        constexpr char16_t terminator = 0;
        if (!GuestCopyOut_nid_no_patch(token + length, &terminator, sizeof(terminator))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        next = token + length + 1;
    }
    if (!continuation.CopyOut(next)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return token;
}

char16_t* APS5_VABI u_memrchr32_nid_postfix(const char16_t* text, std::int32_t value, std::int32_t count) {
    if (count <= 0 || static_cast<std::uint32_t>(value) > 0x10ffffu || (value > 0xffff && count < 2)) return nullptr;
    if (!text) {
        if (value >= 0xd800 && value <= 0xdfff) return nullptr;
        NotImplemented_nid_no_patch(__func__);
    }
    std::u16string units(static_cast<std::size_t>(count), 0);
    if (!GuestCopyIn_nid_no_patch(units.data(), text, units.size() * sizeof(char16_t))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    auto position = units.size();
    while (position != 0) {
        --position;
        std::uint32_t character = units[position];
        if (Trail(units[position]) && position != 0 && Lead(units[position - 1])) {
            --position;
            character = 0x10000u + ((units[position] - 0xd800u) << 10) + (units[position + 1] - 0xdc00u);
        }
        if (character == static_cast<std::uint32_t>(value)) return const_cast<char16_t*>(text + position);
    }
    return nullptr;
}

}
