#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>

namespace {
constexpr std::uint32_t NoDevice = 0xFFFFFFFFu;
constexpr std::uint16_t TypeMask = 0170000;
constexpr std::uint16_t CharacterType = 0020000;
constexpr std::uint16_t BlockType = 0060000;
constexpr int ErrorOverflow = 84;
constexpr std::size_t SpecialNameLength = 63;

void CopyString(char* destination, std::size_t size, const char* text, std::size_t length) {
    if (size == 0) return;
    const std::size_t count = std::min(length, size - 1);
    std::copy_n(text, count, destination);
    destination[count] = '\0';
}
}

extern "C" char* APS5_VABI devname_r_nid_postfix(std::uint32_t device, std::uint16_t type, char* buffer, int length) {
    const auto size = static_cast<std::size_t>(length);
    const bool character = (type & TypeMask) == CharacterType;
    if (device == NoDevice || !(character || (device & TypeMask) == BlockType)) {
        CopyString(buffer, size, "#NODEV", 6);
        return buffer;
    }
    char text[4 + 2 + sizeof(device) * 2 + 1];
    char* end = text;
    *end++ = '#';
    *end++ = character ? 'C' : 'B';
    *end++ = ':';
    if (device != 0) {
        *end++ = '0';
        *end++ = 'x';
    }
    int digits = 1;
    for (std::uint32_t rest = device >> 4; rest != 0; rest >>= 4) ++digits;
    for (int index = digits - 1; index >= 0; --index) *end++ = "0123456789abcdef"[(device >> (index * 4)) & 0xF];
    if (size > static_cast<std::size_t>(INT_MAX) + 1) {
        *__error_nid_postfix() = ErrorOverflow;
        *buffer = '\0';
        return buffer;
    }
    CopyString(buffer, size, text, static_cast<std::size_t>(end - text));
    return buffer;
}

extern "C" char* APS5_VABI devname_nid_postfix(std::uint32_t device, std::uint16_t type) {
    static char buffer[SpecialNameLength + 1];
    return devname_r_nid_postfix(device, type, buffer, sizeof(buffer));
}
