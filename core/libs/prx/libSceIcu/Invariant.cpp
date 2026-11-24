#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>

namespace {

constexpr std::array<std::uint32_t, 4> InvariantChars{0xfffffbff, 0xffffffe5, 0x87fffffe, 0x87fffffe};

bool Invariant(char16_t unit) {
    return unit <= 0x7f && (InvariantChars[unit >> 5] & (1u << (unit & 0x1f))) != 0;
}

std::string ReadEncodedText(const char* source, std::optional<std::size_t> count, const char* caller) {
    constexpr auto maxLength = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    const auto limit = count.value_or(maxLength);
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    std::array<char, 4096> chunk;
    std::string input;
    while (input.size() < limit) {
        if (input.size() > std::numeric_limits<std::uintptr_t>::max() - base) NotImplemented_nid_no_patch(caller);
        const auto address = base + input.size();
        const auto bytes = std::min(limit - input.size(), chunk.size() - address % chunk.size());
        if (!GuestCopyIn_nid_no_patch(chunk.data(), reinterpret_cast<const void*>(address), bytes))
            NotImplemented_nid_no_patch(caller);
        const auto end = std::find(chunk.begin(), chunk.begin() + bytes, '\0');
        input.append(chunk.data(), static_cast<std::size_t>(end - chunk.begin()));
        if (end != chunk.begin() + bytes) return input;
    }
    if (!count) NotImplemented_nid_no_patch(caller);
    return input;
}

std::u16string DecodeDefault(const std::string& input, std::optional<std::size_t> count, const char* caller) {
    if (input.empty()) return std::u16string(1, 0);
    const auto capacity = count.value_or(0x0fffffff);
    std::int32_t acquiredStatus = U_ZERO_ERROR;
    std::unique_ptr<UConverter, decltype(&u_releaseDefaultConverter_nid_postfix)> converter(
        u_getDefaultConverter_nid_postfix(&acquiredStatus), u_releaseDefaultConverter_nid_postfix);
    if (!converter) return std::u16string(1, 0);

    ucnv_resetToUnicode(converter.get());
    UErrorCode status;
    std::u16string output;
    std::array<char16_t, 2048> chunk;
    const char* current = input.data();
    const char* end = current + input.size();
    do {
        status = U_ZERO_ERROR;
        auto* next = chunk.data();
        const auto available = std::min(chunk.size(), capacity - output.size());
        ucnv_toUnicode(converter.get(), &next, chunk.data() + available, &current, end, nullptr, true, &status);
        output.append(chunk.data(), static_cast<std::size_t>(next - chunk.data()));
    } while (status == U_BUFFER_OVERFLOW_ERROR && output.size() < capacity);
    if (!count && output.size() == capacity) NotImplemented_nid_no_patch(caller);
    if (output.size() < capacity) output.push_back(0);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) output.front() = 0;
    return output;
}

char16_t* CopyDecodedText(char16_t* destination, const char* source, std::optional<std::size_t> count, const char* caller) {
    const auto output = DecodeDefault(ReadEncodedText(source, count, caller), count, caller);
    if (!GuestCopyOut_nid_no_patch(destination, output.data(), output.size() * sizeof(char16_t)))
        NotImplemented_nid_no_patch(caller);
    return destination;
}

}

extern "C" {

void APS5_VABI u_charsToUChars_nid_postfix(const char* chars, char16_t* units, std::int32_t length) {
    if (length > 0 && (!chars || !units)) APS5_INVALID_ARG_EX;
    for (std::int32_t index = 0; index < length; ++index) units[index] = static_cast<std::uint8_t>(chars[index]);
}

void APS5_VABI u_UCharsToChars_nid_postfix(const char16_t* units, char* chars, std::int32_t length) {
    if (length > 0 && (!units || !chars)) APS5_INVALID_ARG_EX;
    for (std::int32_t index = 0; index < length; ++index) {
        const char16_t unit = units[index];
        chars[index] = Invariant(unit) ? static_cast<char>(unit) : 0;
    }
}

char* APS5_VABI u_austrcpy_nid_postfix(char* chars, const char16_t* units) {
    if (!chars || !units) APS5_INVALID_ARG_EX;
    std::int32_t length = 0;
    while (units[length] != 0) {
        if (units[length] > 0x7f) NotImplemented_nid_no_patch(__func__);
        ++length;
    }
    for (std::int32_t index = 0; index <= length; ++index) chars[index] = static_cast<char>(units[index]);
    return chars;
}

char* APS5_VABI u_austrncpy_nid_postfix(char* chars, const char16_t* units, std::int32_t length) {
    if (length < 0) {
        if (!chars) APS5_INVALID_ARG_EX;
        chars[0] = 0;
        return chars;
    }
    if (length > 0 && (!chars || !units)) APS5_INVALID_ARG_EX;
    if (length <= 0) return chars;
    std::int32_t copied = 0;
    while (copied < length && units[copied] != 0) {
        if (units[copied] > 0x7f) NotImplemented_nid_no_patch(__func__);
        ++copied;
    }
    for (std::int32_t index = 0; index < copied; ++index) chars[index] = static_cast<char>(units[index]);
    if (copied < length) chars[copied] = 0;
    return chars;
}

char16_t* APS5_VABI u_uastrcpy_nid_postfix(char16_t* units, const char* chars) {
    if (!units || !chars) APS5_INVALID_ARG_EX;
    return CopyDecodedText(units, chars, std::nullopt, __func__);
}

char16_t* APS5_VABI u_uastrncpy_nid_postfix(char16_t* units, const char* chars, std::int32_t length) {
    if (length < 0) {
        if (!units) APS5_INVALID_ARG_EX;
        const char16_t terminator = 0;
        if (!GuestCopyOut_nid_no_patch(units, &terminator, sizeof(terminator))) NotImplemented_nid_no_patch(__func__);
        return units;
    }
    if (length > 0 && (!units || !chars)) APS5_INVALID_ARG_EX;
    if (length <= 0) return units;
    return CopyDecodedText(units, chars, static_cast<std::size_t>(length), __func__);
}

}
