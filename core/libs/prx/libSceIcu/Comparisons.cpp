#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

std::int32_t Length(const char16_t* text) {
    if (text == nullptr) throw std::invalid_argument("ICU: null string");
    std::int32_t length = 0;
    while (text[length] != 0) {
        if (length == std::numeric_limits<std::int32_t>::max()) throw std::overflow_error("ICU: string length exceeds int32");
        ++length;
    }
    return length;
}

void RequireBuffers(const char16_t* first, const char16_t* second) {
    if (first == nullptr || second == nullptr) throw std::invalid_argument("ICU: null comparison buffer");
}

bool Lead(char16_t value) {
    return value >= 0xd800 && value <= 0xdbff;
}

bool Trail(char16_t value) {
    return value >= 0xdc00 && value <= 0xdfff;
}

bool Paired(const char16_t* text, std::size_t length, std::size_t index) {
    return (Lead(text[index]) && index + 1 < length && Trail(text[index + 1])) ||
           (Trail(text[index]) && index != 0 && Lead(text[index - 1]));
}

} // namespace

extern "C" {

std::int32_t APS5_VABI u_strlen_nid_postfix(const char16_t* text) {
    return Length(text);
}

std::int32_t APS5_VABI u_strcmp_nid_postfix(const char16_t* first, const char16_t* second) {
    RequireBuffers(first, second);
    for (std::size_t index = 0;; ++index) {
        const auto difference = static_cast<std::int32_t>(first[index]) - static_cast<std::int32_t>(second[index]);
        if (difference != 0 || first[index] == 0) return difference;
    }
}

std::int32_t APS5_VABI u_strncmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count <= 0) return 0;
    RequireBuffers(first, second);
    for (std::int32_t index = 0; index < count; ++index) {
        const auto difference = static_cast<std::int32_t>(first[index]) - static_cast<std::int32_t>(second[index]);
        if (difference != 0 || first[index] == 0) return difference;
    }
    return 0;
}

std::int32_t APS5_VABI u_memcmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count <= 0) return 0;
    RequireBuffers(first, second);
    for (std::int32_t index = 0; index < count; ++index) {
        const auto difference = static_cast<std::int32_t>(first[index]) - static_cast<std::int32_t>(second[index]);
        if (difference != 0) return difference;
    }
    return 0;
}

std::int32_t APS5_VABI u_memcmpCodePointOrder_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count == 0 || first == second) return 0;
    RequireBuffers(first, second);
    const auto firstLength = count < 0 ? static_cast<std::size_t>(Length(first)) + 1 : static_cast<std::size_t>(count);
    const auto secondLength = count < 0 ? static_cast<std::size_t>(Length(second)) + 1 : static_cast<std::size_t>(count);
    const auto common = std::min(firstLength, secondLength);
    for (std::size_t index = 0; index < common; ++index) {
        std::int32_t left = first[index];
        std::int32_t right = second[index];
        if (left == right) continue;
        if (left >= 0xd800 && right >= 0xd800) {
            if (!Paired(first, firstLength, index)) left -= 0x2800;
            if (!Paired(second, secondLength, index)) right -= 0x2800;
        }
        return left - right;
    }
    return 0;
}

}
