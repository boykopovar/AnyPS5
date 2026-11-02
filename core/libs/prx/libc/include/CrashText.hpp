#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_CRASHTEXT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_CRASHTEXT_HPP

#include <cstddef>
#include <cstdint>

namespace crash_text {

constexpr std::size_t MaximumCharacters = 512;
constexpr std::size_t MinimumCharacters = 4;
constexpr std::size_t MaximumBytes = 2 * (MaximumCharacters + 1);

inline std::uint16_t Unit(const std::uint8_t* bytes, std::size_t index, std::size_t width) {
    if (width == 1) return bytes[index];
    return static_cast<std::uint16_t>(bytes[2 * index] | bytes[2 * index + 1] << 8);
}

inline bool IsText(std::uint16_t unit) {
    return (unit >= 0x20 && unit < 0x7f) || unit == '\t' || unit == '\n' || unit == '\r';
}

inline bool StartsWithText(const std::uint8_t* bytes, std::size_t size, std::size_t width) {
    if (size < MinimumCharacters * width) return false;
    for (std::size_t index = 0; index < MinimumCharacters; ++index) {
        if (!IsText(Unit(bytes, index, width))) return false;
    }
    return true;
}

inline bool Format(const std::uint8_t* bytes, std::size_t size, char* out, std::size_t capacity) {
    const std::size_t width = StartsWithText(bytes, size, 2) ? 2 : StartsWithText(bytes, size, 1) ? 1 : 0;
    if (width == 0 || capacity < 8) return false;
    std::size_t length = 0;
    if (width == 2) out[length++] = 'u';
    out[length++] = '"';
    bool terminated = false;
    for (std::size_t index = 0; (index + 1) * width <= size; ++index) {
        const std::uint16_t unit = Unit(bytes, index, width);
        if (unit == 0) {
            terminated = true;
            break;
        }
        if (index == MaximumCharacters || !IsText(unit)) break;
        const char escape = unit == '\n' ? 'n' : unit == '\r' ? 'r' : unit == '\t' ? 't' : unit == '"' || unit == '\\' ? static_cast<char>(unit) : '\0';
        if (length + (escape != '\0' ? 2 : 1) + 5 > capacity) break;
        if (escape != '\0') out[length++] = '\\';
        out[length++] = escape != '\0' ? escape : static_cast<char>(unit);
    }
    out[length++] = '"';
    if (!terminated) {
        for (int dot = 0; dot < 3; ++dot) out[length++] = '.';
    }
    out[length] = '\0';
    return true;
}

}

#endif
