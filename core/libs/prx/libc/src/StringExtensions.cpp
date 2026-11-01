#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

extern "C" {

long long APS5_VABI strtonum_nid_postfix(const char* text, long long minimum, long long maximum, const char** error) {
    const int savedError = errno;
    const auto fail = [error](const char* message, int number) -> long long {
        if (error) *error = message;
        errno = number;
        return 0;
    };
    if (minimum > maximum) return fail("invalid", 22);
    errno = 0;
    char* end = nullptr;
    const long long value = std::strtoll(text, &end, 10);
    if (errno == EINVAL || end == text || *end != '\0') return fail("invalid", 22);
    if ((value == LLONG_MIN && errno == ERANGE) || value < minimum) return fail("too small", 34);
    if ((value == LLONG_MAX && errno == ERANGE) || value > maximum) return fail("too large", 34);
    if (error) *error = nullptr;
    errno = savedError;
    return value;
}

char* APS5_VABI basename_r_nid_postfix(const char* path, char* buffer) {
    if (buffer == nullptr) throw std::invalid_argument("basename_r: null buffer");
    std::string_view name = path && *path ? path : ".";
    while (name.size() > 1 && name.back() == '/') name.remove_suffix(1);
    if (name != "/") {
        const auto separator = name.find_last_of('/');
        if (separator != std::string_view::npos) name.remove_prefix(separator + 1);
    }
    if (name.size() >= 1024) { errno = 63; return nullptr; }
    std::memcpy(buffer, name.data(), name.size());
    buffer[name.size()] = '\0';
    return buffer;
}

char* APS5_VABI basename_nid_postfix(const char* path) {
    thread_local char buffer[1024];
    return basename_r_nid_postfix(path, buffer);
}

char* APS5_VABI __inet_ntoa_nid_postfix(unsigned int address) {
    thread_local char buffer[sizeof("255.255.255.255")];
    unsigned char bytes[sizeof(address)];
    std::memcpy(bytes, &address, sizeof(bytes));
    char* end = buffer;
    for (std::size_t index = 0; index < sizeof(bytes); ++index) {
        if (index != 0) *end++ = '.';
        const unsigned value = bytes[index];
        if (value >= 100) *end++ = static_cast<char>('0' + value / 100);
        if (value >= 10) *end++ = static_cast<char>('0' + value / 10 % 10);
        *end++ = static_cast<char>('0' + value % 10);
    }
    *end = 0;
    return buffer;
}

char* APS5_VABI dirname_nid_postfix(const char* path) {
    thread_local char buffer[1024];
    if (path == nullptr || *path == '\0') {
        buffer[0] = '.';
        buffer[1] = '\0';
        return buffer;
    }

    auto length = std::strlen(path);
    while (length > 0 && path[length - 1] == '/') --length;
    if (length == 0) {
        buffer[0] = '/';
        buffer[1] = '\0';
        return buffer;
    }
    auto separator = length;
    while (separator > 0 && path[separator - 1] != '/') --separator;
    if (separator == 0) {
        buffer[0] = '.';
        buffer[1] = '\0';
        return buffer;
    }
    while (separator > 1 && path[separator - 1] == '/') --separator;
    if (separator >= sizeof(buffer)) {
        errno = 63;
        return nullptr;
    }
    std::memcpy(buffer, path, separator);
    buffer[separator] = '\0';
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

char* APS5_VABI strsep_nid_postfix(char** text, const char* delimiters) {
    char* token = *text;
    if (token == nullptr) return nullptr;
    char* end = token + std::strcspn(token, delimiters);
    if (*end == '\0') {
        *text = nullptr;
    } else {
        *end = '\0';
        *text = end + 1;
    }
    return token;
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
    static const char exhausted[] = "";
    if (text == nullptr) text = *state;
    if (text == nullptr) return nullptr;
    text += std::strspn(text, delimiters);
    if (*text == '\0') {
        *state = const_cast<char*>(exhausted);
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

std::uint32_t APS5_VABI __inet_addr_nid_postfix(const char* text) {
    std::uint32_t address;
    return __inet_aton_nid_postfix(text, &address) ? address : 0xffffffff;
}

}
