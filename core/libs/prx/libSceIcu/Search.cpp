#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t Unbounded = static_cast<std::size_t>(-1);

bool Lead(char16_t unit) { return unit >= 0xd800 && unit <= 0xdbff; }
bool Trail(char16_t unit) { return unit >= 0xdc00 && unit <= 0xdfff; }

std::size_t Measure(const char16_t* text) {
    std::size_t length = 0;
    while (text[length] != 0) ++length;
    return length;
}

bool Matches(const char16_t* text, std::size_t textLength, std::size_t index, const char16_t* sub, std::size_t subLength) {
    for (std::size_t offset = 0; offset < subLength; ++offset) {
        if (text[index + offset] != sub[offset]) return false;
    }
    if (Trail(text[index]) && index > 0 && Lead(text[index - 1])) return false;
    const auto end = index + subLength;
    return !(Lead(text[end - 1]) && end < textLength && Trail(text[end]));
}

}

extern "C" {

char16_t* APS5_VABI u_strstr_nid_postfix(const char16_t* text, const char16_t* sub) {
    if (!sub) return const_cast<char16_t*>(text);
    if (!text) return nullptr;
    if (*sub == 0) return const_cast<char16_t*>(text);
    const auto subLength = Measure(sub);
    for (std::size_t index = 0; text[index] != 0; ++index) {
        std::size_t offset = 0;
        while (offset < subLength && text[index + offset] == sub[offset]) ++offset;
        if (offset == subLength && Matches(text, Unbounded, index, sub, subLength)) return const_cast<char16_t*>(text + index);
        if (offset < subLength && text[index + offset] == 0) return nullptr;
    }
    return nullptr;
}

char16_t* APS5_VABI u_strrstr_nid_postfix(const char16_t* text, const char16_t* sub) {
    if (!sub) return const_cast<char16_t*>(text);
    if (!text) return nullptr;
    const auto subLength = Measure(sub);
    if (subLength == 0) return const_cast<char16_t*>(text);
    const auto length = Measure(text);
    for (std::size_t index = length < subLength ? 0 : length - subLength + 1; index-- > 0;) {
        if (Matches(text, length, index, sub, subLength)) return const_cast<char16_t*>(text + index);
    }
    return nullptr;
}

}
