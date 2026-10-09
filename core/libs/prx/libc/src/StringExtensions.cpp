#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
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

}
