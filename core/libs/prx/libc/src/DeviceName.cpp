#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {
constexpr std::uint32_t NoDevice = 0xFFFFFFFFu;
constexpr std::uint16_t TypeMask = 0170000;
constexpr std::uint16_t CharacterType = 0020000;
constexpr std::uint16_t BlockType = 0060000;
constexpr int ErrorNoEntry = 2;
constexpr int ErrorFault = 14;
constexpr std::size_t SpecialNameLength = 63;

void CopyString(char* destination, std::size_t size, const char* text, std::size_t length) {
    if (size == 0) return;
    const std::size_t count = std::min(length, size - 1);
    char copy[32] = {};
    std::copy_n(text, count, copy);
    GuestCopyOut_nid_no_patch(destination, copy, count + 1);
}
}

extern "C" char* APS5_VABI devname_r_nid_postfix(std::uint32_t device, std::uint16_t type, char* buffer, int length) {
    const auto size = static_cast<std::size_t>(length);
    const bool character = (type & TypeMask) == CharacterType;
    if (device == NoDevice || !(character || (device & TypeMask) == BlockType)) {
        CopyString(buffer, size, "#NODEV", 6);
        return buffer;
    }
    char text[32];
    const int major = static_cast<int>((device >> 8) & 0xFF);
    const unsigned minor = device & 0xFFFF00FFu;
    const int written = std::snprintf(text, sizeof(text), "#%c:%d:0x%x", character ? 'C' : 'B', major, minor);
    CopyString(buffer, size, text, static_cast<std::size_t>(written));
    if (character) *__error_nid_postfix() = length < 0 ? ErrorFault : ErrorNoEntry;
    return buffer;
}

extern "C" char* APS5_VABI devname_nid_postfix(std::uint32_t device, std::uint16_t type) {
    static char buffer[SpecialNameLength + 1];
    return devname_r_nid_postfix(device, type, buffer, sizeof(buffer));
}
