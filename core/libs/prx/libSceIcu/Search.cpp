#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t Unbounded = static_cast<std::size_t>(-1);

bool Lead(char16_t unit) { return unit >= 0xd800 && unit <= 0xdbff; }
bool Trail(char16_t unit) { return unit >= 0xdc00 && unit <= 0xdfff; }

std::u16string ReadText(const char16_t* source, const char* caller, const char* name) {
    std::u16string text;
    for (;;) {
        char16_t chunk[0x800];
        const auto address = reinterpret_cast<std::uintptr_t>(source + text.size());
        const std::size_t count = (std::max)(std::size_t{1}, (0x1000 - address % 0x1000) / sizeof(char16_t));
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(chunk, source + text.size(), count * sizeof(char16_t));
        errno = savedErrno;
        if (!copied) throw std::invalid_argument(std::string(caller) + ": unreadable " + name);
        for (std::size_t index = 0; index < count; ++index) {
            if (chunk[index] == 0) return text;
            text.push_back(chunk[index]);
        }
    }
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
    const auto pattern = ReadText(sub, __func__, "substring");
    if (pattern.empty()) return const_cast<char16_t*>(text);
    const auto subject = ReadText(text, __func__, "string");
    const auto* host = subject.c_str();
    const auto subLength = pattern.size();
    for (std::size_t index = 0; host[index] != 0; ++index) {
        std::size_t offset = 0;
        while (offset < subLength && host[index + offset] == pattern[offset]) ++offset;
        if (offset == subLength && Matches(host, Unbounded, index, pattern.c_str(), subLength)) return const_cast<char16_t*>(text + index);
        if (offset < subLength && host[index + offset] == 0) return nullptr;
    }
    return nullptr;
}

char16_t* APS5_VABI u_strrstr_nid_postfix(const char16_t* text, const char16_t* sub) {
    if (!sub) return const_cast<char16_t*>(text);
    if (!text) return nullptr;
    const auto pattern = ReadText(sub, __func__, "substring");
    const auto subLength = pattern.size();
    if (subLength == 0) return const_cast<char16_t*>(text);
    const auto subject = ReadText(text, __func__, "string");
    const auto length = subject.size();
    for (std::size_t index = length < subLength ? 0 : length - subLength + 1; index-- > 0;) {
        if (Matches(subject.c_str(), length, index, pattern.c_str(), subLength)) return const_cast<char16_t*>(text + index);
    }
    return nullptr;
}

}
