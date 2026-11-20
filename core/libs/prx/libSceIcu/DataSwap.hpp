#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdarg>
#include <cstdint>

extern "C" {

struct UDataSwapper;

using DataSwapFunction = std::int32_t (APS5_VABI*)(const UDataSwapper*, const void*, std::int32_t, void*,
                                                   std::int32_t*);
using DataReadUInt16 = std::uint16_t (APS5_VABI*)(std::uint16_t);
using DataReadUInt32 = std::uint32_t (APS5_VABI*)(std::uint32_t);
using DataWriteUInt16 = void (APS5_VABI*)(std::uint16_t*, std::uint16_t);
using DataWriteUInt32 = void (APS5_VABI*)(std::uint32_t*, std::uint32_t);
using DataCompareInvChars = std::int8_t (APS5_VABI*)(const UDataSwapper*, const char*, std::int32_t, const char16_t*,
                                                     std::int32_t);
using DataPrintError = void (APS5_VABI*)(const void* context, const char* format, va_list arguments);

struct UDataSwapper {
    std::int8_t inIsBigEndian;
    std::uint8_t inCharset;
    std::int8_t outIsBigEndian;
    std::uint8_t outCharset;
    DataReadUInt16 readUInt16;
    DataReadUInt32 readUInt32;
    DataCompareInvChars compareInvChars;
    DataWriteUInt16 writeUInt16;
    DataWriteUInt32 writeUInt32;
    DataSwapFunction swapArray16;
    DataSwapFunction swapArray32;
    DataSwapFunction swapArray64;
    DataSwapFunction swapInvChars;
    DataPrintError printError;
    void* printErrorContext;
};

UDataSwapper* APS5_VABI udata_openSwapper_nid_postfix(std::int8_t inIsBigEndian, std::uint8_t inCharset,
                                                     std::int8_t outIsBigEndian, std::uint8_t outCharset,
                                                     std::int32_t* status);
void APS5_VABI udata_closeSwapper_nid_postfix(UDataSwapper* swapper);
std::int16_t APS5_VABI udata_readInt16_nid_postfix(const UDataSwapper* swapper, std::int16_t value);
std::int32_t APS5_VABI udata_readInt32_nid_postfix(const UDataSwapper* swapper, std::int32_t value);
void APS5_VABI udata_printError_nid_postfix(const UDataSwapper* swapper, const char* format, ...);
UDataSwapper* APS5_VABI udata_openSwapperForInputData_nid_postfix(const void* data, std::int32_t length, std::int8_t outIsBigEndian, std::uint8_t outCharset, std::int32_t* status);
std::int32_t APS5_VABI udata_swapDataHeader_nid_postfix(const UDataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);
std::int32_t APS5_VABI udata_swapInvStringBlock_nid_postfix(const UDataSwapper* swapper, const void* input, std::int32_t length, void* output, std::int32_t* status);

}
