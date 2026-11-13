// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace Icu {

struct Text;

struct TextFunctions {
    std::int32_t tableSize;
    std::int32_t reserved1;
    std::int32_t reserved2;
    std::int32_t reserved3;
    Text* (APS5_VABI* clone)(Text*, const Text*, std::int8_t, std::int32_t*);
    std::int64_t (APS5_VABI* nativeLength)(Text*);
    std::int8_t (APS5_VABI* access)(Text*, std::int64_t, std::int8_t);
    std::int32_t (APS5_VABI* extract)(Text*, std::int64_t, std::int64_t, char16_t*, std::int32_t, std::int32_t*);
    std::int32_t (APS5_VABI* replace)(Text*, std::int64_t, std::int64_t, const char16_t*, std::int32_t, std::int32_t*);
    void (APS5_VABI* copy)(Text*, std::int64_t, std::int64_t, std::int64_t, std::int8_t, std::int32_t*);
    std::int64_t (APS5_VABI* mapOffsetToNative)(const Text*);
    std::int32_t (APS5_VABI* mapNativeIndexToUTF16)(const Text*, std::int64_t);
    void (APS5_VABI* close)(Text*);
    void (APS5_VABI* spare1)(Text*);
    void (APS5_VABI* spare2)(Text*);
    void (APS5_VABI* spare3)(Text*);
};

struct Text {
    std::uint32_t magic;
    std::int32_t flags;
    std::int32_t providerProperties;
    std::int32_t sizeOfStruct;
    std::int64_t chunkNativeLimit;
    std::int32_t extraSize;
    std::int32_t nativeIndexingLimit;
    std::int64_t chunkNativeStart;
    std::int32_t chunkOffset;
    std::int32_t chunkLength;
    const char16_t* chunkContents;
    const TextFunctions* functions;
    void* extra;
    const void* context;
    const void* p;
    const void* q;
    const void* r;
    void* privatePointer;
    std::int64_t a;
    std::int32_t b;
    std::int32_t c;
    std::int64_t privateA;
    std::int32_t privateB;
    std::int32_t privateC;
};

static_assert(sizeof(TextFunctions) == 112 && alignof(TextFunctions) == 8);
static_assert(offsetof(TextFunctions, tableSize) == 0 && offsetof(TextFunctions, reserved3) == 12);
static_assert(offsetof(TextFunctions, clone) == 16 && offsetof(TextFunctions, nativeLength) == 24);
static_assert(offsetof(TextFunctions, access) == 32 && offsetof(TextFunctions, extract) == 40);
static_assert(offsetof(TextFunctions, replace) == 48 && offsetof(TextFunctions, copy) == 56);
static_assert(offsetof(TextFunctions, mapOffsetToNative) == 64 && offsetof(TextFunctions, mapNativeIndexToUTF16) == 72);
static_assert(offsetof(TextFunctions, close) == 80 && offsetof(TextFunctions, spare1) == 88);
static_assert(offsetof(TextFunctions, spare2) == 96 && offsetof(TextFunctions, spare3) == 104);
static_assert(sizeof(Text) == 144 && alignof(Text) == 8);
static_assert(offsetof(Text, magic) == 0 && offsetof(Text, flags) == 4);
static_assert(offsetof(Text, providerProperties) == 8 && offsetof(Text, sizeOfStruct) == 12);
static_assert(offsetof(Text, chunkNativeLimit) == 16 && offsetof(Text, extraSize) == 24);
static_assert(offsetof(Text, nativeIndexingLimit) == 28 && offsetof(Text, chunkNativeStart) == 32);
static_assert(offsetof(Text, chunkOffset) == 40 && offsetof(Text, chunkLength) == 44);
static_assert(offsetof(Text, chunkContents) == 48 && offsetof(Text, functions) == 56);
static_assert(offsetof(Text, extra) == 64 && offsetof(Text, context) == 72);
static_assert(offsetof(Text, p) == 80 && offsetof(Text, q) == 88 && offsetof(Text, r) == 96);
static_assert(offsetof(Text, privatePointer) == 104 && offsetof(Text, a) == 112);
static_assert(offsetof(Text, b) == 120 && offsetof(Text, c) == 124);
static_assert(offsetof(Text, privateA) == 128 && offsetof(Text, privateB) == 136 && offsetof(Text, privateC) == 140);

}

extern "C" {
std::int8_t APS5_VABI utext_equals_nid_postfix(const Icu::Text*, const Icu::Text*);
std::int8_t APS5_VABI utext_isLengthExpensive_nid_postfix(const Icu::Text*);
std::int8_t APS5_VABI utext_isWritable_nid_postfix(const Icu::Text*);
std::int8_t APS5_VABI utext_hasMetaData_nid_postfix(const Icu::Text*);
void APS5_VABI utext_freeze_nid_postfix(Icu::Text*);
std::int64_t APS5_VABI utext_nativeLength_nid_postfix(Icu::Text*);
std::int64_t APS5_VABI utext_getNativeIndex_nid_postfix(const Icu::Text*);
void APS5_VABI utext_setNativeIndex_nid_postfix(Icu::Text*, std::int64_t);
std::int64_t APS5_VABI utext_getPreviousNativeIndex_nid_postfix(Icu::Text*);
std::int32_t APS5_VABI utext_current32_nid_postfix(Icu::Text*);
std::int32_t APS5_VABI utext_char32At_nid_postfix(Icu::Text*, std::int64_t);
std::int32_t APS5_VABI utext_next32_nid_postfix(Icu::Text*);
std::int32_t APS5_VABI utext_previous32_nid_postfix(Icu::Text*);
std::int32_t APS5_VABI utext_next32From_nid_postfix(Icu::Text*, std::int64_t);
std::int32_t APS5_VABI utext_previous32From_nid_postfix(Icu::Text*, std::int64_t);
std::int8_t APS5_VABI utext_moveIndex32_nid_postfix(Icu::Text*, std::int32_t);
std::int32_t APS5_VABI utext_extract_nid_postfix(Icu::Text*, std::int64_t, std::int64_t, char16_t*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI utext_replace_nid_postfix(Icu::Text*, std::int64_t, std::int64_t, const char16_t*, std::int32_t, std::int32_t*);
void APS5_VABI utext_copy_nid_postfix(Icu::Text*, std::int64_t, std::int64_t, std::int64_t, std::int8_t, std::int32_t*);
Icu::Text* APS5_VABI utext_clone_nid_postfix(Icu::Text*, const Icu::Text*, std::int8_t, std::int8_t, std::int32_t*);
}
