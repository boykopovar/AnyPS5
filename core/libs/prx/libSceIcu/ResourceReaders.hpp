#pragma once
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

struct IcuResourceData {
    void* data;
    const std::int32_t* pRoot;
    const std::uint16_t* p16BitUnits;
    const char* poolBundleKeys;
    std::uint32_t rootRes;
    std::int32_t localKeyLimit;
    const std::uint16_t* poolBundleStrings;
    std::int32_t poolStringIndexLimit;
    std::int32_t poolStringIndex16Limit;
    std::int8_t noFallback;
    std::int8_t isPoolBundle;
    std::int8_t usesPoolBundle;
    std::int8_t useNativeStrcmp;
};

struct IcuDataInfo {
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

extern "C" {
void APS5_VABI res_read_nid_postfix(IcuResourceData* data, const IcuDataInfo* info, const void* bytes, std::int32_t length, std::int32_t* error);
std::int32_t APS5_VABI res_countArrayItems_nid_postfix(const IcuResourceData* data, std::uint32_t resource);
const char16_t* APS5_VABI res_getAlias_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t* length);
std::uint32_t APS5_VABI res_getArrayItem_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t index);
const std::uint8_t* APS5_VABI res_getBinaryNoTrace_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t* length);
const std::int32_t* APS5_VABI res_getIntVectorNoTrace_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t* length);
std::uint32_t APS5_VABI res_getResource_nid_postfix(const IcuResourceData* data, const char* key);
const char16_t* APS5_VABI res_getStringNoTrace_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t* length);
std::uint32_t APS5_VABI res_getTableItemByIndex_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t index, const char** key);
std::uint32_t APS5_VABI res_getTableItemByKey_nid_postfix(const IcuResourceData* data, std::uint32_t resource, std::int32_t* index, const char** key);
}
