#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/ucnv.h"
#include "ustr_cnv.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>

namespace {

std::mutex DefaultConverterMutex;
std::unordered_set<UConverter*> CheckedOutDefaultConverters;

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    if (!ok) NotImplemented_nid_no_patch(caller);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* caller) {
    const bool ok = GuestCopyOut_nid_no_patch(destination, source, bytes);
    if (!ok) NotImplemented_nid_no_patch(caller);
}

std::string CopyConverterName(const char* source, const char* caller) {
    constexpr int GuestNameTooLong = 63;
    std::string name;
    for (std::size_t capacity = UCNV_MAX_CONVERTER_NAME_LENGTH; capacity <= name.max_size() / 2; capacity *= 2) {
        const int savedErrno = errno;
        const int error = CopyGuestString_nid_no_patch(source, capacity, name);
        errno = savedErrno;
        if (error == 0) return name;
        if (error != GuestNameTooLong) break;
    }
    NotImplemented_nid_no_patch(caller);
}

std::u16string CopyConverterInput(const char16_t* source, std::size_t maxUnits, const char* caller) {
    constexpr std::size_t pageBytes = 4096;
    std::array<char16_t, pageBytes / sizeof(char16_t)> chunk;
    std::u16string input;
    const auto base = reinterpret_cast<std::uintptr_t>(source);
    while (input.size() < maxUnits) {
        if (input.size() > (std::numeric_limits<std::uintptr_t>::max() - base) / sizeof(char16_t))
            NotImplemented_nid_no_patch(caller);
        const auto address = base + input.size() * sizeof(char16_t);
        const auto pageUnits = std::max<std::size_t>(1, (pageBytes - address % pageBytes) / sizeof(char16_t));
        const auto units = std::min(maxUnits - input.size(), pageUnits);
        CopyIn(chunk.data(), reinterpret_cast<const void*>(address), units * sizeof(char16_t), caller);
        const auto end = std::find(chunk.begin(), chunk.begin() + units, char16_t{});
        input.append(chunk.data(), static_cast<std::size_t>(end - chunk.begin()));
        if (end != chunk.begin() + units) break;
    }
    return input;
}

std::string EncodeDefault(const std::u16string& input, std::optional<std::size_t> count, const char* caller) {
    if (input.empty()) return std::string(1, '\0');
    const auto capacity = count.value_or(0x0fffffff);

    const std::lock_guard lock(DefaultConverterMutex);
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<UConverter, decltype(&u_releaseDefaultConverter)> converter(
        u_getDefaultConverter(&status), u_releaseDefaultConverter);
    if (U_FAILURE(status) || !converter) return std::string(1, '\0');
    ucnv_resetFromUnicode(converter.get());

    std::string output;
    std::array<char, 4096> chunk;
    const char16_t* current = input.data();
    const char16_t* end = current + input.size();
    do {
        status = U_ZERO_ERROR;
        auto* next = chunk.data();
        const auto available = std::min(chunk.size(), capacity - output.size());
        ucnv_fromUnicode(converter.get(), &next, chunk.data() + available, &current, end, nullptr, true, &status);
        output.append(chunk.data(), static_cast<std::size_t>(next - chunk.data()));
    } while (status == U_BUFFER_OVERFLOW_ERROR && output.size() < capacity);
    if (!count && output.size() == capacity) NotImplemented_nid_no_patch(caller);
    if (output.size() < capacity) output.push_back('\0');
    if (count && U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) output.front() = '\0';
    return output;
}

bool IsDigit(unsigned char value, bool ebcdic) {
    const unsigned char zero = ebcdic ? 0xf0 : 0x30;
    return value >= zero && value <= zero + 9;
}

unsigned char LowerLetter(unsigned char value, bool ebcdic) {
    if (!ebcdic) {
        if (value >= 0x41 && value <= 0x5a) return value + 0x20;
        return value >= 0x61 && value <= 0x7a ? value : 0;
    }
    if ((value >= 0xc1 && value <= 0xc9) || (value >= 0xd1 && value <= 0xd9) || (value >= 0xe2 && value <= 0xe9))
        return value - 0x40;
    return ((value >= 0x81 && value <= 0x89) || (value >= 0x91 && value <= 0x99) || (value >= 0xa2 && value <= 0xa9)) ? value : 0;
}

unsigned char NextNameByte(const unsigned char*& cursor, bool& afterDigit, bool ebcdic) {
    const unsigned char zero = ebcdic ? 0xf0 : 0x30;
    while (const auto value = *cursor++) {
        if (IsDigit(value, ebcdic)) {
            if (value != zero) afterDigit = true;
            else if (!afterDigit && IsDigit(*cursor, ebcdic)) continue;
            return value;
        }
        afterDigit = false;
        if (const auto letter = LowerLetter(value, ebcdic)) return letter;
    }
    return 0;
}

char* StripName(char* destination, const char* name, bool ebcdic) {
    if (!destination || !name) APS5_INVALID_ARG_EX;
    auto cursor = reinterpret_cast<const unsigned char*>(name);
    char* output = destination;
    bool afterDigit = false;
    while (const auto value = NextNameByte(cursor, afterDigit, ebcdic)) *output++ = static_cast<char>(value);
    *output = 0;
    return destination;
}

struct Signature {
    const char* name;
    std::array<unsigned char, 5> bytes;
    std::int32_t length;
};

constexpr Signature Signatures[] = {
    {"UTF-32LE", {0xff, 0xfe, 0, 0}, 4},
    {"UTF-16BE", {0xfe, 0xff}, 2},
    {"UTF-16LE", {0xff, 0xfe}, 2},
    {"UTF-8", {0xef, 0xbb, 0xbf}, 3},
    {"UTF-32BE", {0, 0, 0xfe, 0xff}, 4},
    {"SCSU", {0x0e, 0xfe, 0xff}, 3},
    {"BOCU-1", {0xfb, 0xee, 0x28}, 3},
    {"UTF-7", {0x2b, 0x2f, 0x76, 0x38, 0x2d}, 5},
    {"UTF-7", {0x2b, 0x2f, 0x76, 0x38}, 4},
    {"UTF-7", {0x2b, 0x2f, 0x76, 0x39}, 4},
    {"UTF-7", {0x2b, 0x2f, 0x76, 0x2b}, 4},
    {"UTF-7", {0x2b, 0x2f, 0x76, 0x2f}, 4},
    {"UTF-EBCDIC", {0xdd, 0x73, 0x66, 0x73}, 4},
};

}

extern "C" {

void APS5_VABI u_flushDefaultConverter_nid_postfix() {
    const std::lock_guard lock(DefaultConverterMutex);
    u_flushDefaultConverter();
}

UConverter* APS5_VABI u_getDefaultConverter_nid_postfix(std::int32_t* errorCode) {
    if (!errorCode) APS5_INVALID_ARG_EX;
    std::int32_t incoming = 0;
    CopyIn(&incoming, errorCode, sizeof(incoming), __func__);
    const std::int32_t entered = incoming < U_ERROR_WARNING_START ? 0 : incoming;
    auto status = static_cast<UErrorCode>(entered);

    const std::lock_guard lock(DefaultConverterMutex);
    std::unique_ptr<UConverter, decltype(&u_releaseDefaultConverter)> converter(
        u_getDefaultConverter(&status), u_releaseDefaultConverter);
    const auto outgoing = static_cast<std::int32_t>(status);
    if (outgoing != entered) CopyOut(errorCode, &outgoing, sizeof(outgoing), __func__);
    if (converter) CheckedOutDefaultConverters.insert(converter.get());
    return converter.release();
}

void APS5_VABI u_releaseDefaultConverter_nid_postfix(UConverter* converter) {
    if (!converter) return;
    const std::lock_guard lock(DefaultConverterMutex);
    if (CheckedOutDefaultConverters.erase(converter) == 0) APS5_INVALID_ARG_EX;
    if constexpr (!U_CHARSET_IS_UTF8) {
        UErrorCode status = U_ZERO_ERROR;
        const char* name = ucnv_getName(converter, &status);
        if (U_FAILURE(status) || !name || std::strcmp(name, ucnv_getDefaultName()) != 0) {
            ucnv_close(converter);
            return;
        }
    }
    u_releaseDefaultConverter(converter);
}

const char* APS5_VABI ucnv_getDefaultName_nid_postfix() {
    const std::lock_guard lock(DefaultConverterMutex);
    return ucnv_getDefaultName();
}

void APS5_VABI ucnv_setDefaultName_nid_postfix(const char* name) {
    if constexpr (U_CHARSET_IS_UTF8) return;
    const std::string copied = name ? CopyConverterName(name, __func__) : std::string{};
    const std::lock_guard lock(DefaultConverterMutex);
    ucnv_setDefaultName(name ? copied.c_str() : nullptr);
    if (!name) u_flushDefaultConverter();
}

char* APS5_VABI u_austrcpy_nid_postfix(char* destination, const char16_t* source) {
    if (!destination || !source) APS5_INVALID_ARG_EX;
    constexpr auto maxUnits = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    const auto input = CopyConverterInput(source, maxUnits, __func__);
    if (input.size() == maxUnits) NotImplemented_nid_no_patch(__func__);
    const auto output = EncodeDefault(input, std::nullopt, __func__);
    CopyOut(destination, output.data(), output.size(), __func__);
    return destination;
}

char* APS5_VABI u_austrncpy_nid_postfix(char* destination, const char16_t* source, std::int32_t count) {
    if (count == 0) return destination;
    if (!destination || (count > 0 && !source)) APS5_INVALID_ARG_EX;
    if (count < 0) {
        const char terminator = '\0';
        CopyOut(destination, &terminator, sizeof(terminator), __func__);
        return destination;
    }
    const auto units = static_cast<std::size_t>(count);
    const auto output = EncodeDefault(CopyConverterInput(source, units, __func__), units, __func__);
    CopyOut(destination, output.data(), output.size(), __func__);
    return destination;
}

int APS5_VABI ucnv_compareNames_nid_postfix(const char* left, const char* right) {
    if (!left || !right) APS5_INVALID_ARG_EX;
    auto first = reinterpret_cast<const unsigned char*>(left);
    auto second = reinterpret_cast<const unsigned char*>(right);
    bool firstAfterDigit = false;
    bool secondAfterDigit = false;
    for (;;) {
        const auto a = NextNameByte(first, firstAfterDigit, false);
        const auto b = NextNameByte(second, secondAfterDigit, false);
        if (a != b || !a) return static_cast<int>(a) - static_cast<int>(b);
    }
}

char* APS5_VABI ucnv_io_stripASCIIForCompare_nid_postfix(char* destination, const char* name) {
    return StripName(destination, name, false);
}

char* APS5_VABI ucnv_io_stripEBCDICForCompare_nid_postfix(char* destination, const char* name) {
    return StripName(destination, name, true);
}

const char* APS5_VABI ucnv_detectUnicodeSignature_nid_postfix(const char* source, std::int32_t sourceLength,
                                                         std::int32_t* signatureLength, std::int32_t* errorCode) {
    if (!errorCode) return nullptr;
    std::int32_t status = 0;
    CopyIn(&status, errorCode, sizeof(status), __func__);
    if (status > 0) return nullptr;
    if (!source || sourceLength < -1) {
        const std::int32_t illegalArgument = 1;
        CopyOut(errorCode, &illegalArgument, sizeof(illegalArgument), __func__);
        return nullptr;
    }
    std::array<unsigned char, 5> prefix{};
    std::int32_t available = 0;
    if (sourceLength >= 0) {
        available = sourceLength < static_cast<std::int32_t>(prefix.size()) ? sourceLength : static_cast<std::int32_t>(prefix.size());
        if (available > 0) CopyIn(prefix.data(), source, static_cast<std::size_t>(available), __func__);
    } else {
        while (available < static_cast<std::int32_t>(prefix.size())) {
            unsigned char value = 0;
            CopyIn(&value, source + available, 1, __func__);
            if (!value) break;
            prefix[available++] = value;
        }
    }
    const char* name = nullptr;
    std::int32_t length = 0;
    for (const auto& signature : Signatures) {
        if (available < signature.length) continue;
        bool matches = true;
        for (std::int32_t i = 0; i < signature.length; ++i)
            if (prefix[i] != signature.bytes[i]) matches = false;
        if (matches) {
            name = signature.name;
            length = signature.length;
            break;
        }
    }
    if (signatureLength) CopyOut(signatureLength, &length, sizeof(length), __func__);
    return name;
}

}
