#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

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

}

extern "C" {
void APS5_VABI udata_getInfo_nid_postfix(Icu::DataMemory* memory, Icu::DataInfo* info);
}
