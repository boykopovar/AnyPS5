#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

char16_t ReadUnit(const char16_t* base, std::size_t index) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    const auto offset = index * sizeof(char16_t);
    if (offset / sizeof(char16_t) != index || address > std::numeric_limits<std::uintptr_t>::max() - offset) {
        throw std::invalid_argument("ICU: unreadable string");
    }
    char16_t value = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const void*>(address + offset), sizeof(value));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument("ICU: unreadable string");
    return value;
}

std::int32_t Length(const char16_t* text) {
    if (text == nullptr) throw std::invalid_argument("ICU: null string");
    std::int32_t length = 0;
    while (ReadUnit(text, static_cast<std::size_t>(length)) != 0) {
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

bool Paired(const char16_t* text, std::size_t length, std::size_t index, char16_t value) {
    return (Lead(value) && index + 1 < length && Trail(ReadUnit(text, index + 1))) ||
           (Trail(value) && index != 0 && Lead(ReadUnit(text, index - 1)));
}

} // namespace

extern "C" {

std::int32_t APS5_VABI u_strlen_nid_postfix(const char16_t* text) {
    return Length(text);
}

std::int32_t APS5_VABI u_strcmp_nid_postfix(const char16_t* first, const char16_t* second) {
    RequireBuffers(first, second);
    for (std::size_t index = 0;; ++index) {
        const auto left = ReadUnit(first, index);
        const auto difference = static_cast<std::int32_t>(left) - static_cast<std::int32_t>(ReadUnit(second, index));
        if (difference != 0 || left == 0) return difference;
    }
}

std::int32_t APS5_VABI u_strncmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count <= 0) return 0;
    RequireBuffers(first, second);
    for (std::int32_t index = 0; index < count; ++index) {
        const auto left = ReadUnit(first, static_cast<std::size_t>(index));
        const auto difference = static_cast<std::int32_t>(left) - static_cast<std::int32_t>(ReadUnit(second, static_cast<std::size_t>(index)));
        if (difference != 0 || left == 0) return difference;
    }
    return 0;
}

std::int32_t APS5_VABI u_memcmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count <= 0) return 0;
    RequireBuffers(first, second);
    for (std::int32_t index = 0; index < count; ++index) {
        const auto difference = static_cast<std::int32_t>(ReadUnit(first, static_cast<std::size_t>(index))) -
                                static_cast<std::int32_t>(ReadUnit(second, static_cast<std::size_t>(index)));
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
        const auto leftUnit = ReadUnit(first, index);
        const auto rightUnit = ReadUnit(second, index);
        std::int32_t left = leftUnit;
        std::int32_t right = rightUnit;
        if (left == right) continue;
        if (left >= 0xd800 && right >= 0xd800) {
            if (!Paired(first, firstLength, index, leftUnit)) left -= 0x2800;
            if (!Paired(second, secondLength, index, rightUnit)) right -= 0x2800;
        }
        return left - right;
    }
    return 0;
}

}
