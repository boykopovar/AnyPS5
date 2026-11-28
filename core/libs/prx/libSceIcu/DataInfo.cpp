#include "prx/libSceIcu/DataInfo.hpp"
#include "udatamem.h"
#include <cstddef>

static_assert(sizeof(Icu::DataInfo) == sizeof(UDataInfo));
static_assert(alignof(Icu::DataInfo) == alignof(UDataInfo));
static_assert(offsetof(Icu::DataInfo, size) == offsetof(UDataInfo, size));
static_assert(offsetof(Icu::DataInfo, reservedWord) == offsetof(UDataInfo, reservedWord));
static_assert(offsetof(Icu::DataInfo, isBigEndian) == offsetof(UDataInfo, isBigEndian));
static_assert(offsetof(Icu::DataInfo, charsetFamily) == offsetof(UDataInfo, charsetFamily));
static_assert(offsetof(Icu::DataInfo, sizeofUChar) == offsetof(UDataInfo, sizeofUChar));
static_assert(offsetof(Icu::DataInfo, reservedByte) == offsetof(UDataInfo, reservedByte));
static_assert(offsetof(Icu::DataInfo, dataFormat) == offsetof(UDataInfo, dataFormat));
static_assert(offsetof(Icu::DataInfo, formatVersion) == offsetof(UDataInfo, formatVersion));
static_assert(offsetof(Icu::DataInfo, dataVersion) == offsetof(UDataInfo, dataVersion));
static_assert(sizeof(UDataMemory) == 56);
static_assert(alignof(UDataMemory) == 8);
static_assert(offsetof(UDataMemory, pHeader) == 8);
static_assert(sizeof(DataHeader) == 24);
static_assert(offsetof(DataHeader, info) == 4);

extern "C" void APS5_VABI udata_getInfo_nid_postfix(Icu::DataMemory* memory, Icu::DataInfo* info) {
    udata_getInfo(reinterpret_cast<UDataMemory*>(memory), reinterpret_cast<UDataInfo*>(info));
}

extern "C" std::int32_t APS5_VABI udata_getLength_nid_postfix(Icu::DataMemory* memory) {
    return udata_getLength(reinterpret_cast<UDataMemory*>(memory));
}

extern "C" const void* APS5_VABI udata_getRawMemory_nid_postfix(Icu::DataMemory* memory) {
    return udata_getRawMemory(reinterpret_cast<UDataMemory*>(memory));
}

extern "C" const void* APS5_VABI udata_getMemory_nid_postfix(Icu::DataMemory* memory) {
    return udata_getMemory(reinterpret_cast<UDataMemory*>(memory));
}
