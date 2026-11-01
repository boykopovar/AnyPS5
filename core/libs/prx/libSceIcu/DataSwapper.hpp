// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2003-2014, International Business Machines Corporation and others. All Rights Reserved.
#pragma once
#include "Object.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdarg>

namespace Icu::Abi {

struct DataSwapper;

using DataReadUInt16 = std::uint16_t (APS5_VABI *)(std::uint16_t);
using DataReadUInt32 = std::uint32_t (APS5_VABI *)(std::uint32_t);
using DataWriteUInt16 = void (APS5_VABI *)(std::uint16_t*, std::uint16_t);
using DataWriteUInt32 = void (APS5_VABI *)(std::uint32_t*, std::uint32_t);
using DataCompareInvChars = std::int32_t (APS5_VABI *)(const DataSwapper*, const char*, std::int32_t, const char16_t*, std::int32_t);
using DataSwapFn = std::int32_t (APS5_VABI *)(const DataSwapper*, const void*, std::int32_t, void*, std::int32_t*);
using DataPrintError = void (APS5_VABI *)(void*, const char*, void*);

struct DataSwapper {
    std::int8_t inIsBigEndian;
    std::uint8_t inCharset;
    std::int8_t outIsBigEndian;
    std::uint8_t outCharset;
    DataReadUInt16 readUInt16;
    DataReadUInt32 readUInt32;
    DataCompareInvChars compareInvChars;
    DataWriteUInt16 writeUInt16;
    DataWriteUInt32 writeUInt32;
    DataSwapFn swapArray16;
    DataSwapFn swapArray32;
    DataSwapFn swapArray64;
    DataSwapFn swapInvChars;
    DataPrintError printError;
    void* printErrorContext;
};

static_assert(sizeof(DataSwapper) == 96 && alignof(DataSwapper) == 8);
static_assert(offsetof(DataSwapper, inIsBigEndian) == 0 && offsetof(DataSwapper, readUInt16) == 8);
static_assert(offsetof(DataSwapper, printErrorContext) == 88);

}

extern "C" {
Icu::Abi::DataSwapper* APS5_VABI udata_openSwapper_nid_postfix(std::int8_t inIsBigEndian, std::uint8_t inCharset, std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* errorCode);
Icu::Abi::DataSwapper* APS5_VABI udata_openSwapperForInputData_nid_postfix(const void* data, std::int32_t length, std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* errorCode);
void APS5_VABI udata_closeSwapper_nid_postfix(Icu::Abi::DataSwapper* swapper);
std::int32_t APS5_VABI udata_swapDataHeader_nid_postfix(const Icu::Abi::DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode);
std::int32_t APS5_VABI udata_swapInvStringBlock_nid_postfix(const Icu::Abi::DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode);
std::int16_t APS5_VABI udata_readInt16_nid_postfix(const Icu::Abi::DataSwapper* ds, std::int16_t x);
std::int32_t APS5_VABI udata_readInt32_nid_postfix(const Icu::Abi::DataSwapper* ds, std::int32_t x);
void APS5_VABI udata_printError_nid_postfix(const Icu::Abi::DataSwapper* ds, const char* fmt, ...);
std::int32_t APS5_VABI utrie_swapAnyVersion_nid_postfix(const Icu::Abi::DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode);
std::int32_t APS5_VABI utrie2_swap_nid_postfix(const Icu::Abi::DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode);
std::int32_t APS5_VABI ucptrie_swap_nid_postfix(const Icu::Abi::DataSwapper* ds, const void* inData, std::int32_t length, void* outData, std::int32_t* errorCode);
}
