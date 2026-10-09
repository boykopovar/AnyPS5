#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <string_view>

extern "C" {

char* APS5_VABI basename_nid_postfix(const char* path) {
    thread_local char buffer[1024];
    std::string_view name = path && *path ? path : ".";
    while (name.size() > 1 && name.back() == '/') name.remove_suffix(1);
    if (name != "/") {
        const auto separator = name.find_last_of('/');
        if (separator != std::string_view::npos) name.remove_prefix(separator + 1);
    }
    if (name.size() >= sizeof(buffer)) { errno = 63; return nullptr; }
    std::memcpy(buffer, name.data(), name.size());
    buffer[name.size()] = '\0';
    return buffer;
}

std::size_t APS5_VABI strnlen_nid_postfix(const char* text, std::size_t limit) {
    std::size_t length = 0;
    while (length < limit && text[length] != '\0') ++length;
    return length;
}

std::size_t APS5_VABI strnlen_s_nid_postfix(const char* text, std::size_t limit) {
    return text ? strnlen_nid_postfix(text, limit) : 0;
}

char* APS5_VABI strncat_nid_postfix(char* destination, const char* source, std::size_t limit) {
    return std::strncat(destination, source, limit);
}

char* APS5_VABI strpbrk_nid_postfix(const char* text, const char* accept) {
    return const_cast<char*>(std::strpbrk(text, accept));
}

std::size_t APS5_VABI strcspn_nid_postfix(const char* text, const char* reject) {
    return std::strcspn(text, reject);
}

std::size_t APS5_VABI strlcat_nid_postfix(char* destination, const char* source, std::size_t capacity) {
    const auto destinationLength = strnlen_nid_postfix(destination, capacity);
    const auto sourceLength = std::strlen(source);
    if (destinationLength < capacity) {
        const auto remaining = capacity - destinationLength - 1;
        const auto count = sourceLength < remaining ? sourceLength : remaining;
        std::memcpy(destination + destinationLength, source, count);
        destination[destinationLength + count] = '\0';
    }
    return destinationLength + sourceLength;
}

char* APS5_VABI stpcpy_nid_postfix(char* destination, const char* source) {
    const auto length = std::strlen(source);
    std::memcpy(destination, source, length + 1);
    return destination + length;
}

char* APS5_VABI strtok_r_nid_postfix(char* text, const char* delimiters, char** state) {
    if (text == nullptr) text = *state;
    if (text == nullptr) return nullptr;
    text += std::strspn(text, delimiters);
    if (*text == '\0') {
        *state = text;
        return nullptr;
    }
    char* end = text + std::strcspn(text, delimiters);
    if (*end != '\0') *end++ = '\0';
    *state = end;
    return text;
}

char* APS5_VABI strtok_nid_postfix(char* text, const char* delimiters) {
    thread_local char* state = nullptr;
    return strtok_r_nid_postfix(text, delimiters, &state);
}


int APS5_VABI __inet_aton_nid_postfix(const char* text, void* address) {
    std::uint8_t parts[3];
    std::size_t count = 0;
    std::uint64_t value = 0;
    bool digit = false;
    char c = *text;
    for (;;) {
        if (c < '0' || c > '9') return 0;
        value = 0;
        unsigned base = 10;
        digit = false;
        if (c == '0') {
            c = *++text;
            if (c == 'x' || c == 'X') {
                base = 16;
                c = *++text;
            } else {
                base = 8;
                digit = true;
            }
        }
        for (;; c = *++text, digit = true) {
            const char lower = static_cast<char>(c | 0x20);
            if (c >= '0' && c <= '9') {
                if (base == 8 && c >= '8') return 0;
                value = value * base + static_cast<unsigned>(c - '0');
            } else if (base == 16 && lower >= 'a' && lower <= 'f') {
                value = (value << 4) | static_cast<unsigned>(lower - 'a' + 10);
            } else {
                break;
            }
        }
        if (c != '.') break;
        if (count == 3 || value > 0xff) return 0;
        parts[count++] = static_cast<std::uint8_t>(value);
        c = *++text;
    }
    if (c != '\0' && c != ' ' && (c < '\t' || c > '\r')) return 0;
    if (!digit) return 0;
    constexpr std::uint64_t limits[] = {UINT64_MAX, 0xffffff, 0xffff, 0xff};
    if (value > limits[count]) return 0;
    for (std::size_t index = 0; index < count; ++index) value |= std::uint64_t{parts[index]} << (24 - 8 * index);
    if (address) {
        const std::uint8_t bytes[4] = {static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16),
            static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value)};
        std::memcpy(address, bytes, sizeof(bytes));
    }
    return 1;
}

}
