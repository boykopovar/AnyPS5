#include "prx/libSceIcu/Utf16Search.hpp"
#include "unicode/ustring.h"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool Lead(char16_t value) { return value >= 0xd800 && value <= 0xdbff; }
bool Trail(char16_t value) { return value >= 0xdc00 && value <= 0xdfff; }
bool Surrogate(char16_t value) { return value >= 0xd800 && value <= 0xdfff; }

std::vector<char16_t> ReadOperand(const char16_t* operand, std::int32_t count, const char* caller) {
    constexpr std::uintptr_t Chunk = 0x1000;
    const auto address = reinterpret_cast<std::uintptr_t>(operand);
    const std::size_t limit = count >= 0 ? static_cast<std::size_t>(count) * sizeof(char16_t) : 0;
    std::vector<unsigned char> bytes;
    bool done = false;
    while (!done) {
        const std::size_t offset = bytes.size();
        std::size_t amount = Chunk - (address + offset) % Chunk;
        if (count >= 0) {
            amount = (std::min)(amount, limit - offset);
            if (!amount) break;
        }
        bytes.resize(offset + amount);
        const int saved = errno;
        const bool copied = GuestCopyIn_nid_no_patch(bytes.data() + offset, reinterpret_cast<const void*>(address + offset), amount);
        errno = saved;
        if (!copied) throw std::invalid_argument(std::string(caller) + ": unreadable operand");
        if (count >= 0) {
            done = bytes.size() == limit;
            continue;
        }
        for (std::size_t scanned = offset & ~std::size_t{1}; scanned + sizeof(char16_t) <= bytes.size(); scanned += sizeof(char16_t)) {
            char16_t unit = 0;
            std::memcpy(&unit, bytes.data() + scanned, sizeof(unit));
            if (unit != 0) continue;
            bytes.resize(scanned + sizeof(char16_t));
            done = true;
            break;
        }
    }
    std::vector<char16_t> units(bytes.size() / sizeof(char16_t) + (bytes.empty() ? 1 : 0));
    if (!bytes.empty()) std::memcpy(units.data(), bytes.data(), bytes.size());
    return units;
}

char16_t* Find(const char16_t* text, std::int32_t count, const char16_t* needle, std::int32_t needleCount,
               bool last, const char* caller) {
    const auto sub = ReadOperand(needle, needleCount, caller);
    const auto subLength = needleCount >= 0 ? needleCount : static_cast<std::int32_t>(sub.size() - 1);
    if (subLength == 0) return const_cast<char16_t*>(text);
    const auto units = ReadOperand(text, count, caller);
    const char16_t* found = last ? u_strFindLast(units.data(), count, sub.data(), needleCount)
                                 : u_strFindFirst(units.data(), count, sub.data(), needleCount);
    return found ? const_cast<char16_t*>(text) + (found - units.data()) : nullptr;
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
    if (!needle || needleCount < -1) return const_cast<char16_t*>(text);
    if (!text || count < -1) return nullptr;
    return Find(text, count, needle, needleCount, false, __func__);
}

char16_t* APS5_VABI u_strFindLast_nid_postfix(const char16_t* text, std::int32_t count, const char16_t* needle, std::int32_t needleCount) {
    if (!needle || needleCount < -1) return const_cast<char16_t*>(text);
    if (!text || count < -1) return nullptr;
    return Find(text, count, needle, needleCount, true, __func__);
}

std::int32_t APS5_VABI u_strcmpCodePointOrder_nid_postfix(const char16_t* first, const char16_t* second) {
    return Compare(first, second, -1, __func__);
}

std::int32_t APS5_VABI u_strncmpCodePointOrder_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    return Compare(first, second, count, __func__);
}

}
