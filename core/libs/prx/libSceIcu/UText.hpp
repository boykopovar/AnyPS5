// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2004-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  utext.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2004oct06
*   created by: Markus W. Scherer
*/

#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

struct UText;

struct UTextFuncs {
    std::int32_t tableSize;
    std::int32_t reserved1, reserved2, reserved3;
    UText* (APS5_VABI* clone)(UText*, const UText*, std::int8_t, std::int32_t*);
    std::int64_t (APS5_VABI* nativeLength)(UText*);
    std::int8_t (APS5_VABI* access)(UText*, std::int64_t, std::int8_t);
    std::int32_t (APS5_VABI* extract)(UText*, std::int64_t, std::int64_t, char16_t*, std::int32_t, std::int32_t*);
    std::int32_t (APS5_VABI* replace)(UText*, std::int64_t, std::int64_t, const char16_t*, std::int32_t, std::int32_t*);
    void (APS5_VABI* copy)(UText*, std::int64_t, std::int64_t, std::int64_t, std::int8_t, std::int32_t*);
    std::int64_t (APS5_VABI* mapOffsetToNative)(const UText*);
    std::int32_t (APS5_VABI* mapNativeIndexToUTF16)(const UText*, std::int64_t);
    void (APS5_VABI* close)(UText*);
    void (APS5_VABI* spare1)(UText*);
    void (APS5_VABI* spare2)(UText*);
    void (APS5_VABI* spare3)(UText*);
};

struct UText {
    std::uint32_t       magic;
    std::int32_t        flags;
    std::int32_t         providerProperties;
    std::int32_t         sizeOfStruct;
    std::int64_t         chunkNativeLimit;
    std::int32_t        extraSize;
    std::int32_t         nativeIndexingLimit;
    std::int64_t         chunkNativeStart;
    std::int32_t         chunkOffset;
    std::int32_t         chunkLength;
    const char16_t    *chunkContents;
    const UTextFuncs     *pFuncs;
    void          *pExtra;
    const void   *context;
    const void     *p;
    const void     *q;
    const void     *r;
    void           *privP;
    std::int64_t         a;
    std::int32_t         b;
    std::int32_t         c;
    std::int64_t         privA;
    std::int32_t         privB;
    std::int32_t         privC;
};

inline constexpr std::uint32_t UTEXT_MAGIC = 0x345ad82c;
inline constexpr std::int32_t UTEXT_HEAP_ALLOCATED = 1;
inline constexpr std::int32_t UTEXT_EXTRA_HEAP_ALLOCATED = 2;
inline constexpr std::int32_t UTEXT_OPEN = 4;

static_assert(sizeof(UText) == 144);
static_assert(alignof(UText) == 8);
static_assert(offsetof(UText, chunkNativeLimit) == 16);
static_assert(offsetof(UText, extraSize) == 24);
static_assert(offsetof(UText, chunkNativeStart) == 32);
static_assert(offsetof(UText, chunkContents) == 48);
static_assert(offsetof(UText, pFuncs) == 56);
static_assert(offsetof(UText, pExtra) == 64);
static_assert(offsetof(UText, context) == 72);
static_assert(offsetof(UText, a) == 112);
static_assert(offsetof(UText, privA) == 128);
static_assert(sizeof(UTextFuncs) == 112);
static_assert(offsetof(UTextFuncs, close) == 80);
