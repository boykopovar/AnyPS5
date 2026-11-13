// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "Trace.hpp"
#include <cstddef>

namespace Icu {

using ArrayComparator = std::int32_t (APS5_VABI*)(const void*, const void*, const void*);

struct UDataSwapper {
    std::int8_t inIsBigEndian;
    std::uint8_t inCharset;
    std::int8_t outIsBigEndian;
    std::uint8_t outCharset;
    std::uint16_t (APS5_VABI* readUInt16)(std::uint16_t);
    std::uint32_t (APS5_VABI* readUInt32)(std::uint32_t);
    std::int32_t (APS5_VABI* compareInvChars)(const UDataSwapper*, const char*, std::int32_t, const char16_t*, std::int32_t);
    void (APS5_VABI* writeUInt16)(std::uint16_t*, std::uint16_t);
    void (APS5_VABI* writeUInt32)(std::uint32_t*, std::uint32_t);
    std::int32_t (APS5_VABI* swapArray16)(const UDataSwapper*, const void*, std::int32_t, void*, std::int32_t*);
    std::int32_t (APS5_VABI* swapArray32)(const UDataSwapper*, const void*, std::int32_t, void*, std::int32_t*);
    std::int32_t (APS5_VABI* swapArray64)(const UDataSwapper*, const void*, std::int32_t, void*, std::int32_t*);
    std::int32_t (APS5_VABI* swapInvChars)(const UDataSwapper*, const void*, std::int32_t, void*, std::int32_t*);
    void (APS5_VABI* printError)(void*, const char*, TraceArguments);
    void* printErrorContext;
};

static_assert(sizeof(UDataSwapper) == 96 && alignof(UDataSwapper) == 8);
static_assert(offsetof(UDataSwapper, inIsBigEndian) == 0 && offsetof(UDataSwapper, inCharset) == 1);
static_assert(offsetof(UDataSwapper, outIsBigEndian) == 2 && offsetof(UDataSwapper, outCharset) == 3);
static_assert(offsetof(UDataSwapper, readUInt16) == 8 && offsetof(UDataSwapper, readUInt32) == 16);
static_assert(offsetof(UDataSwapper, compareInvChars) == 24 && offsetof(UDataSwapper, writeUInt16) == 32);
static_assert(offsetof(UDataSwapper, writeUInt32) == 40 && offsetof(UDataSwapper, swapArray16) == 48);
static_assert(offsetof(UDataSwapper, swapArray32) == 56 && offsetof(UDataSwapper, swapArray64) == 64);
static_assert(offsetof(UDataSwapper, swapInvChars) == 72 && offsetof(UDataSwapper, printError) == 80);
static_assert(offsetof(UDataSwapper, printErrorContext) == 88);

}

extern "C" {
std::int32_t APS5_VABI uprv_int32Comparator_nid_postfix(const void*, const void*, const void*);
std::int32_t APS5_VABI uprv_uint32Comparator_nid_postfix(const void*, const void*, const void*);
std::int32_t APS5_VABI uprv_uint16Comparator_nid_postfix(const void*, const void*, const void*);
std::int32_t APS5_VABI uprv_stableBinarySearch_nid_postfix(char*, std::int32_t, void*, std::int32_t, Icu::ArrayComparator, const void*);
void APS5_VABI uprv_sortArray_nid_postfix(void*, std::int32_t, std::int32_t, Icu::ArrayComparator, const void*, std::int8_t, std::int32_t*);
char APS5_VABI uprv_toupper_nid_postfix(char);
char APS5_VABI uprv_asciitolower_nid_postfix(char);
char APS5_VABI uprv_ebcdictolower_nid_postfix(char);
std::int8_t APS5_VABI uprv_isASCIILetter_nid_postfix(char);
std::int32_t APS5_VABI uprv_stricmp_nid_postfix(const char*, const char*);
std::int32_t APS5_VABI uprv_strnicmp_nid_postfix(const char*, const char*, std::uint32_t);
std::int8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char*, std::int32_t);
std::int8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t*, std::int32_t);
std::int32_t APS5_VABI uprv_compareASCIIPropertyNames_nid_postfix(const char*, const char*);
std::int32_t APS5_VABI uprv_compareEBCDICPropertyNames_nid_postfix(const char*, const char*);
std::int32_t APS5_VABI uprv_compareInvEbcdicAsAscii_nid_postfix(const char*, const char*);
char APS5_VABI uprv_ebcdicToAscii_nid_postfix(char);
char APS5_VABI uprv_ebcdicToLowercaseAscii_nid_postfix(char);
std::uint8_t* APS5_VABI uprv_aestrncpy_nid_postfix(std::uint8_t*, const std::uint8_t*, std::int32_t);
std::uint8_t* APS5_VABI uprv_eastrncpy_nid_postfix(std::uint8_t*, const std::uint8_t*, std::int32_t);
std::int32_t APS5_VABI uprv_ebcdicFromAscii_nid_postfix(const Icu::UDataSwapper*, const void*, std::int32_t, void*, std::int32_t*);
std::int32_t APS5_VABI uprv_itou_nid_postfix(char16_t*, std::int32_t, std::uint32_t, std::uint32_t, std::int32_t);
std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char*);
double APS5_VABI uprv_getUTCtime_nid_postfix();
double APS5_VABI uprv_getRawUTCtime_nid_postfix();
}
