#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

bool Lead(char16_t value) { return value >= 0xd800 && value <= 0xdbff; }
bool Trail(char16_t value) { return value >= 0xdc00 && value <= 0xdfff; }
bool Surrogate(char16_t value) { return value >= 0xd800 && value <= 0xdfff; }

std::size_t Length(const char16_t* text, const char* function) {
    std::size_t length = 0;
    while (text[length] != 0) {
        if (length == static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(function);
        ++length;
    }
    return length;
}

bool Boundary(const char16_t* text, std::size_t position, std::size_t width, std::size_t length) {
    if (position != 0 && Trail(text[position]) && Lead(text[position - 1])) return false;
    const auto end = position + width;
    return !(end < length && Lead(text[end - 1]) && Trail(text[end]));
}

char16_t* FindCharacter(const char16_t* text, char16_t value, std::int32_t count, bool last) {
    char16_t* result = nullptr;
    const bool terminated = count < 0;
    for (std::size_t position = 0; terminated || position < static_cast<std::size_t>(count); ++position) {
        const auto current = text[position];
        const bool startsPair = Lead(current) && (terminated || position + 1 < static_cast<std::size_t>(count)) && Trail(text[position + 1]);
        const bool endsPair = Trail(current) && position != 0 && Lead(text[position - 1]);
        if (current == value && !startsPair && !endsPair) {
            result = const_cast<char16_t*>(text + position);
            if (!last) return result;
        }
        if (terminated && current == 0) break;
    }
    return result;
}

char16_t* FindTerminated(const char16_t* text, const char16_t* needle, std::int32_t needleCount) {
    for (std::size_t position = 0; text[position] != 0; ++position) {
        if (text[position] != needle[0]) continue;
        for (std::size_t matched = 1;; ++matched) {
            if (needleCount < 0 ? needle[matched] == 0 : matched == static_cast<std::size_t>(needleCount)) {
                const auto end = position + matched;
                const bool beginsPair = position != 0 && Trail(text[position]) && Lead(text[position - 1]);
                const bool endsPair = Lead(text[end - 1]) && Trail(text[end]);
                if (!beginsPair && !endsPair) return const_cast<char16_t*>(text + position);
                break;
            }
            if (text[position + matched] == 0) return nullptr;
            if (text[position + matched] != needle[matched]) break;
        }
    }
    return nullptr;
}

char16_t* FindString(const char16_t* text, std::int32_t count, const char16_t* needle, std::int32_t needleCount, bool last, const char* function) {
    if (!needle || needleCount < -1) return const_cast<char16_t*>(text);
    if (!text || count < -1) return nullptr;
    if (!last && count < 0 && needleCount < 0) {
        if (needle[0] == 0) return const_cast<char16_t*>(text);
        if (needle[1] == 0 && !Surrogate(needle[0])) return FindCharacter(text, needle[0], -1, false);
        return FindTerminated(text, needle, -1);
    }
    const auto width = needleCount < 0 ? Length(needle, function) : static_cast<std::size_t>(needleCount);
    if (width == 0) return const_cast<char16_t*>(text);
    if (width == 1 && !Surrogate(needle[0])) return FindCharacter(text, needle[0], count, last);
    if (!last && count < 0) return FindTerminated(text, needle, needleCount);
    const auto length = count < 0 ? Length(text, function) : static_cast<std::size_t>(count);
    if (width > length) return nullptr;
    char16_t* result = nullptr;
    for (std::size_t position = 0; position <= length - width; ++position) {
        std::size_t matched = 0;
        while (matched < width && text[position + matched] == needle[matched]) ++matched;
        if (matched == width && Boundary(text, position, width, length)) {
            result = const_cast<char16_t*>(text + position);
            if (!last) return result;
        }
    }
    return result;
}

bool Paired(const char16_t* text, std::size_t position, std::int32_t count) {
    return (Lead(text[position]) && (count < 0 || position + 1 < static_cast<std::size_t>(count)) && Trail(text[position + 1])) ||
           (Trail(text[position]) && position != 0 && Lead(text[position - 1]));
}

std::int32_t Compare(const char16_t* first, const char16_t* second, std::int32_t count, const char* function) {
    if (first == second || count == 0) return 0;
    if (!first || !second) NotImplemented_nid_no_patch(function);
    for (std::size_t position = 0; count < 0 || position < static_cast<std::size_t>(count); ++position) {
        std::int32_t left = first[position];
        std::int32_t right = second[position];
        if (left != right) {
            if (left >= 0xd800 && right >= 0xd800) {
                if (!Paired(first, position, count)) left -= 0x2800;
                if (!Paired(second, position, count)) right -= 0x2800;
            }
            return left - right;
        }
        if (left == 0) return 0;
    }
    return 0;
}

} // namespace

extern "C" {

char16_t* APS5_VABI u_memmove_nid_postfix(char16_t* destination, const char16_t* source, std::int32_t count) {
    if (count <= 0) return destination;
    if (!destination || !source) NotImplemented_nid_no_patch(__func__);
    const auto bytes = static_cast<std::size_t>(count) * sizeof(char16_t);
    GuestArena::HostWrite access(destination, bytes);
    std::memmove(destination, source, bytes);
    return destination;
}

char16_t* APS5_VABI u_memset_nid_postfix(char16_t* destination, char16_t value, std::int32_t count) {
    if (count <= 0) return destination;
    if (!destination) NotImplemented_nid_no_patch(__func__);
    GuestArena::HostWrite access(destination, static_cast<std::size_t>(count) * sizeof(char16_t));
    for (std::int32_t position = 0; position < count; ++position) destination[position] = value;
    return destination;
}

char16_t* APS5_VABI u_memchr_nid_postfix(const char16_t* text, char16_t value, std::int32_t count) {
    if (count <= 0 || (!text && Surrogate(value))) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return FindCharacter(text, value, count, false);
}

char16_t* APS5_VABI u_memrchr_nid_postfix(const char16_t* text, char16_t value, std::int32_t count) {
    if (count <= 0 || (!text && Surrogate(value))) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return FindCharacter(text, value, count, true);
}

char16_t* APS5_VABI u_strchr_nid_postfix(const char16_t* text, char16_t value) {
    if (!text && Surrogate(value)) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return FindCharacter(text, value, -1, false);
}

char16_t* APS5_VABI u_strrchr_nid_postfix(const char16_t* text, char16_t value) {
    if (!text && Surrogate(value)) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return FindCharacter(text, value, -1, true);
}

char16_t* APS5_VABI u_strFindFirst_nid_postfix(const char16_t* text, std::int32_t count, const char16_t* needle, std::int32_t needleCount) {
    return FindString(text, count, needle, needleCount, false, __func__);
}

char16_t* APS5_VABI u_strFindLast_nid_postfix(const char16_t* text, std::int32_t count, const char16_t* needle, std::int32_t needleCount) {
    return FindString(text, count, needle, needleCount, true, __func__);
}

std::int32_t APS5_VABI u_strcmpCodePointOrder_nid_postfix(const char16_t* first, const char16_t* second) {
    return Compare(first, second, -1, __func__);
}

std::int32_t APS5_VABI u_strncmpCodePointOrder_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    return Compare(first, second, count, __func__);
}

}
