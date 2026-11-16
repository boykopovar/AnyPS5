#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace {

constexpr std::array<std::uint32_t, 4> InvariantChars{0xfffffbff, 0xffffffe5, 0x87fffffe, 0x87fffffe};
constexpr std::size_t PageBytes = 4096;

bool Invariant(char16_t unit) {
    return unit <= 0x7f && (InvariantChars[unit >> 5] & (1u << (unit & 0x1f))) != 0;
}

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void RequireWritable(void* destination, std::size_t bytes, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(destination, bytes);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

std::u16string ReadUnits(const char16_t* source, std::size_t maximum, const char* caller) {
    std::u16string units;
    auto address = reinterpret_cast<std::uintptr_t>(source);
    std::array<char16_t, PageBytes / sizeof(char16_t)> chunk;
    while (units.size() < maximum) {
        const auto available = std::max<std::size_t>(1, (PageBytes - address % PageBytes) / sizeof(char16_t));
        const auto count = std::min(available, maximum - units.size());
        CopyIn(chunk.data(), reinterpret_cast<const void*>(address), count * sizeof(char16_t), caller);
        const char16_t* end = std::find(chunk.data(), chunk.data() + count, u'\0');
        units.append(chunk.data(), static_cast<std::size_t>(end - chunk.data()));
        if (end != chunk.data() + count) break;
        address += count * sizeof(char16_t);
    }
    return units;
}

std::string Narrow(const std::u16string& units, const char* caller) {
    std::string chars;
    chars.reserve(units.size() + 1);
    for (const char16_t unit : units) {
        if (unit > 0x7f) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
        chars.push_back(static_cast<char>(unit));
    }
    return chars;
}

}

extern "C" {

void APS5_VABI u_charsToUChars_nid_postfix(const char* chars, char16_t* units, std::int32_t length) {
    if (length > 0 && (!chars || !units)) APS5_INVALID_ARG_EX;
    if (length <= 0) return;
    const auto total = static_cast<std::size_t>(length);
    RequireWritable(units, total * sizeof(char16_t), __func__);
    std::array<char, PageBytes> in;
    std::array<char16_t, PageBytes> out;
    for (std::size_t offset = 0; offset < total; offset += in.size()) {
        const auto count = std::min(in.size(), total - offset);
        CopyIn(in.data(), chars + offset, count, __func__);
        for (std::size_t index = 0; index < count; ++index) out[index] = static_cast<std::uint8_t>(in[index]);
        CopyOut(units + offset, out.data(), count * sizeof(char16_t), __func__);
    }
}

void APS5_VABI u_UCharsToChars_nid_postfix(const char16_t* units, char* chars, std::int32_t length) {
    if (length > 0 && (!units || !chars)) APS5_INVALID_ARG_EX;
    if (length <= 0) return;
    const auto total = static_cast<std::size_t>(length);
    RequireWritable(chars, total, __func__);
    std::array<char16_t, PageBytes> in;
    std::array<char, PageBytes> out;
    for (std::size_t offset = 0; offset < total; offset += in.size()) {
        const auto count = std::min(in.size(), total - offset);
        CopyIn(in.data(), units + offset, count * sizeof(char16_t), __func__);
        for (std::size_t index = 0; index < count; ++index) out[index] = Invariant(in[index]) ? static_cast<char>(in[index]) : 0;
        CopyOut(chars + offset, out.data(), count, __func__);
    }
}

char* APS5_VABI u_austrcpy_nid_postfix(char* chars, const char16_t* units) {
    if (!chars || !units) APS5_INVALID_ARG_EX;
    const auto copy = Narrow(ReadUnits(units, std::numeric_limits<std::size_t>::max(), __func__), __func__);
    RequireWritable(chars, copy.size() + 1, __func__);
    CopyOut(chars, copy.c_str(), copy.size() + 1, __func__);
    return chars;
}

char* APS5_VABI u_austrncpy_nid_postfix(char* chars, const char16_t* units, std::int32_t length) {
    if (length > 0 && (!chars || !units)) APS5_INVALID_ARG_EX;
    if (length <= 0) return chars;
    const auto copy = Narrow(ReadUnits(units, static_cast<std::size_t>(length), __func__), __func__);
    const auto bytes = copy.size() < static_cast<std::size_t>(length) ? copy.size() + 1 : copy.size();
    RequireWritable(chars, bytes, __func__);
    CopyOut(chars, copy.c_str(), bytes, __func__);
    return chars;
}

}
