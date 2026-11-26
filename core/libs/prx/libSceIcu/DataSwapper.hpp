#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

namespace Icu {

struct DataSwapper;

using DataSwapFunction = std::int32_t (APS5_VABI*)(const DataSwapper*, const void*, std::int32_t, void*, std::int32_t*);
using DataReadUInt16 = std::uint16_t (APS5_VABI*)(std::uint16_t);
using DataReadUInt32 = std::uint32_t (APS5_VABI*)(std::uint32_t);
using DataWriteUInt16 = void (APS5_VABI*)(std::uint16_t*, std::uint16_t);
using DataWriteUInt32 = void (APS5_VABI*)(std::uint32_t*, std::uint32_t);
using DataCompareInvariantChars = std::int32_t (APS5_VABI*)(const DataSwapper*, const char*, std::int32_t,
                                                            const char16_t*, std::int32_t);
using DataPrintError = void (APS5_VABI*)(void*, const char*, void*);

struct DataSwapper {
    std::int8_t inIsBigEndian;
    std::uint8_t inCharset;
    std::int8_t outIsBigEndian;
    std::uint8_t outCharset;
    DataReadUInt16 readUInt16;
    DataReadUInt32 readUInt32;
    DataCompareInvariantChars compareInvChars;
    DataWriteUInt16 writeUInt16;
    DataWriteUInt32 writeUInt32;
    DataSwapFunction swapArray16;
    DataSwapFunction swapArray32;
    DataSwapFunction swapArray64;
    DataSwapFunction swapInvChars;
    DataPrintError printError;
    void* printErrorContext;
};

}

extern "C" {
Icu::DataSwapper* APS5_VABI udata_openSwapper_nid_postfix(std::int8_t inIsBigEndian, std::uint8_t inCharset,
                                                         std::int8_t outIsBigEndian, std::uint8_t outCharset,
                                                         std::int32_t* status);
Icu::DataSwapper* APS5_VABI udata_openSwapperForInputData_nid_postfix(const void* data, std::int32_t length,
                                                                     std::int8_t outIsBigEndian,
                                                                     std::uint8_t outCharset, std::int32_t* status);
void APS5_VABI udata_closeSwapper_nid_postfix(Icu::DataSwapper* swapper);
std::int16_t APS5_VABI udata_readInt16_nid_postfix(const Icu::DataSwapper* swapper, std::int16_t value);
std::int32_t APS5_VABI udata_readInt32_nid_postfix(const Icu::DataSwapper* swapper, std::int32_t value);
std::int32_t APS5_VABI udata_swapDataHeader_nid_postfix(const Icu::DataSwapper* swapper, const void* inData,
                                                       std::int32_t length, void* outData, std::int32_t* status);
std::int32_t APS5_VABI udata_swapInvStringBlock_nid_postfix(const Icu::DataSwapper* swapper, const void* inData,
                                                           std::int32_t length, void* outData, std::int32_t* status);
void APS5_VABI udata_printError_nid_postfix(const Icu::DataSwapper* swapper, const char* format, ...);
}
