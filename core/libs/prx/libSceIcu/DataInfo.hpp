#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UDataMemory;

namespace Icu {

struct DataMemory;

struct DataInfo {
    std::uint16_t size;
    std::uint16_t reservedWord;
    std::uint8_t isBigEndian;
    std::uint8_t charsetFamily;
    std::uint8_t sizeofUChar;
    std::uint8_t reservedByte;
    std::uint8_t dataFormat[4];
    std::uint8_t formatVersion[4];
    std::uint8_t dataVersion[4];
};

using DataMemoryIsAcceptable = std::int8_t (APS5_VABI*)(void*, const char*, const char*, const DataInfo*);

}

extern "C" {
void APS5_VABI udata_getInfo_nid_postfix(Icu::DataMemory* memory, Icu::DataInfo* info);
std::int32_t APS5_VABI udata_getLength_nid_postfix(Icu::DataMemory* memory);
const void* APS5_VABI udata_getRawMemory_nid_postfix(Icu::DataMemory* memory);
const void* APS5_VABI udata_getMemory_nid_postfix(Icu::DataMemory* memory);
UDataMemory* APS5_VABI udata_open_nid_postfix(const char* path, const char* type, const char* name, std::int32_t* status);
UDataMemory* APS5_VABI udata_openChoice_nid_postfix(const char* path, const char* type, const char* name,
                                                  Icu::DataMemoryIsAcceptable callback, void* context,
                                                  std::int32_t* status);
}
