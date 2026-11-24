#include "prx/libSceIcu/Utf16Search.hpp"
#include "unicode/ustring.h"
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

std::int32_t Length(const char16_t* text, const char* function) {
    std::size_t length = 0;
    while (text[length] != 0) {
        if (length == static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(function);
        ++length;
    }
    return static_cast<std::int32_t>(length);
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
    if (!access.Open()) NotImplemented_nid_no_patch(__func__);
    std::memmove(destination, source, bytes);
    return destination;
}

char16_t* APS5_VABI u_memset_nid_postfix(char16_t* destination, char16_t value, std::int32_t count) {
    if (count <= 0) return destination;
    if (!destination) NotImplemented_nid_no_patch(__func__);
    GuestArena::HostWrite access(destination, static_cast<std::size_t>(count) * sizeof(char16_t));
    if (!access.Open()) NotImplemented_nid_no_patch(__func__);
    for (std::int32_t position = 0; position < count; ++position) destination[position] = value;
    return destination;
}

char16_t* APS5_VABI u_memchr_nid_postfix(const char16_t* text, char16_t value, std::int32_t count) {
    if (count <= 0 || (!text && Surrogate(value))) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return u_memchr(text, value, count);
}

char16_t* APS5_VABI u_memrchr_nid_postfix(const char16_t* text, char16_t value, std::int32_t count) {
    if (count <= 0 || (!text && Surrogate(value))) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return u_memrchr(text, value, count);
}

char16_t* APS5_VABI u_strchr_nid_postfix(const char16_t* text, char16_t value) {
    if (!text && Surrogate(value)) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return u_strchr(text, value);
}

char16_t* APS5_VABI u_strrchr_nid_postfix(const char16_t* text, char16_t value) {
    if (!text && Surrogate(value)) return nullptr;
    if (!text) NotImplemented_nid_no_patch(__func__);
    return u_strrchr(text, value);
}

char16_t* APS5_VABI u_strFindFirst_nid_postfix(const char16_t* text, std::int32_t count, const char16_t* needle, std::int32_t needleCount) {
    if (text && count >= 0 && needle && needleCount == -1) needleCount = Length(needle, __func__);
    return u_strFindFirst(text, count, needle, needleCount);
}

char16_t* APS5_VABI u_strFindLast_nid_postfix(const char16_t* text, std::int32_t count, const char16_t* needle, std::int32_t needleCount) {
    if (!needle || needleCount < -1) return const_cast<char16_t*>(text);
    if (!text || count < -1) return nullptr;
    if (needleCount == -1) needleCount = Length(needle, __func__);
    if (count == -1 && (needleCount > 1 || (needleCount == 1 && Surrogate(needle[0])))) count = Length(text, __func__);
    return u_strFindLast(text, count, needle, needleCount);
}

std::int32_t APS5_VABI u_strcmpCodePointOrder_nid_postfix(const char16_t* first, const char16_t* second) {
    return Compare(first, second, -1, __func__);
}

std::int32_t APS5_VABI u_strncmpCodePointOrder_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    return Compare(first, second, count, __func__);
}

}
