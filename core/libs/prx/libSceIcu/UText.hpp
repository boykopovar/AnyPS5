#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

struct UText;

struct UTextFuncs {
    std::int32_t tableSize;
    std::int32_t reserved1;
    std::int32_t reserved2;
    std::int32_t reserved3;
    UText* (APS5_VABI* clone)(UText* dest, const UText* src, std::int8_t deep, std::int32_t* status);
    std::int64_t (APS5_VABI* nativeLength)(UText* ut);
    std::int8_t (APS5_VABI* access)(UText* ut, std::int64_t nativeIndex, std::int8_t forward);
    std::int32_t (APS5_VABI* extract)(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit, char16_t* dest,
                                      std::int32_t destCapacity, std::int32_t* status);
    std::int32_t (APS5_VABI* replace)(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit,
                                      const char16_t* replacementText, std::int32_t replacementLength, std::int32_t* status);
    void (APS5_VABI* copy)(UText* ut, std::int64_t nativeStart, std::int64_t nativeLimit, std::int64_t nativeDest,
                           std::int8_t move, std::int32_t* status);
    std::int64_t (APS5_VABI* mapOffsetToNative)(const UText* ut);
    std::int32_t (APS5_VABI* mapNativeIndexToUTF16)(const UText* ut, std::int64_t nativeIndex);
    void (APS5_VABI* close)(UText* ut);
    void (APS5_VABI* spare1)(UText* ut);
    void (APS5_VABI* spare2)(UText* ut);
    void (APS5_VABI* spare3)(UText* ut);
};

struct UText {
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
    const UTextFuncs* pFuncs;
    void* pExtra;
    const void* context;
    const void* p;
    const void* q;
    const void* r;
    void* privP;
    std::int64_t a;
    std::int32_t b;
    std::int32_t c;
    std::int64_t privA;
    std::int32_t privB;
    std::int32_t privC;
};

static_assert(sizeof(UTextFuncs) == 112 && alignof(UTextFuncs) == 8);
static_assert(offsetof(UTextFuncs, clone) == 16 && offsetof(UTextFuncs, nativeLength) == 24);
static_assert(offsetof(UTextFuncs, access) == 32 && offsetof(UTextFuncs, extract) == 40);
static_assert(offsetof(UTextFuncs, replace) == 48 && offsetof(UTextFuncs, copy) == 56);
static_assert(offsetof(UTextFuncs, mapOffsetToNative) == 64 && offsetof(UTextFuncs, mapNativeIndexToUTF16) == 72);
static_assert(offsetof(UTextFuncs, close) == 80 && offsetof(UTextFuncs, spare3) == 104);
static_assert(sizeof(UText) == 144 && alignof(UText) == 8);
static_assert(offsetof(UText, chunkNativeLimit) == 16 && offsetof(UText, extraSize) == 24);
static_assert(offsetof(UText, nativeIndexingLimit) == 28 && offsetof(UText, chunkNativeStart) == 32);
static_assert(offsetof(UText, chunkOffset) == 40 && offsetof(UText, chunkLength) == 44);
static_assert(offsetof(UText, chunkContents) == 48 && offsetof(UText, pFuncs) == 56);
static_assert(offsetof(UText, pExtra) == 64 && offsetof(UText, context) == 72);
static_assert(offsetof(UText, p) == 80 && offsetof(UText, privP) == 104);
static_assert(offsetof(UText, a) == 112 && offsetof(UText, b) == 120 && offsetof(UText, c) == 124);
static_assert(offsetof(UText, privA) == 128 && offsetof(UText, privB) == 136 && offsetof(UText, privC) == 140);

extern "C" {
UText* APS5_VABI utext_setup(UText* ut, std::int32_t extraSpace, std::int32_t* status);
UText* APS5_VABI utext_openUChars(UText* ut, const char16_t* s, std::int64_t length, std::int32_t* status);
}
