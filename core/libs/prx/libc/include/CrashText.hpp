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

inline const char* AmdOnlyInstruction(const std::uint8_t* bytes, std::size_t size) {
    bool operandSize = false;
    bool repne = false;
    bool rep = false;
    std::size_t index = 0;
    for (; index < size; ++index) {
        const std::uint8_t byte = bytes[index];
        if (byte == 0x66) operandSize = true;
        else if (byte == 0xf2) repne = true;
        else if (byte == 0xf3) rep = true;
        else if (byte != 0x67 && byte != 0xf0 && byte != 0x2e && byte != 0x36 && byte != 0x3e && byte != 0x26 && byte != 0x64 && byte != 0x65) break;
    }
    if (index < size && (bytes[index] & 0xf0) == 0x40) ++index;
    if (index + 2 > size || bytes[index] != 0x0f) return nullptr;
    const bool none = !operandSize && !repne && !rep;
    const bool onlyOperandSize = operandSize && !repne && !rep;
    const bool onlyRepne = !operandSize && repne && !rep;
    const bool onlyRep = !operandSize && !repne && rep;
    const std::uint8_t opcode = bytes[index + 1];
    if (opcode == 0x2b) return onlyRep ? "MOVNTSS" : onlyRepne ? "MOVNTSD" : nullptr;
    if (opcode == 0x78 || opcode == 0x79) return onlyOperandSize ? "EXTRQ" : onlyRepne ? "INSERTQ" : nullptr;
    if (opcode != 0x01 || index + 3 > size) return nullptr;
    switch (bytes[index + 2]) {
    case 0xfa: return none ? "MONITORX" : onlyRep ? "MCOMMIT" : nullptr;
    case 0xfb: return none ? "MWAITX" : nullptr;
    case 0xfc: return none ? "CLZERO" : nullptr;
    case 0xfd: return none ? "RDPRU" : nullptr;
    default: return nullptr;
    }
}

}

#endif
