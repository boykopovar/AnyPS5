#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace {

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
    if (!errorCode || *errorCode > 0) return nullptr;
    if (!source || sourceLength < -1) {
        *errorCode = 1;
        return nullptr;
    }
    std::array<unsigned char, 5> prefix{};
    std::int32_t available = 0;
    while (available < static_cast<std::int32_t>(prefix.size()) && (sourceLength < 0 || available < sourceLength)) {
        const auto value = static_cast<unsigned char>(source[available]);
        if (sourceLength < 0 && !value) break;
        prefix[available++] = value;
    }
    for (const auto& signature : Signatures) {
        if (available < signature.length) continue;
        bool matches = true;
        for (std::int32_t i = 0; i < signature.length; ++i)
            if (prefix[i] != signature.bytes[i]) matches = false;
        if (matches) {
            if (signatureLength) *signatureLength = signature.length;
            return signature.name;
        }
    }
    if (signatureLength) *signatureLength = 0;
    return nullptr;
}

}
