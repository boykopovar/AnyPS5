#pragma once
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceIcu/DataInfo.hpp"

namespace Icu {

struct ResourceData {
    DataMemory* data;
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

}

extern "C" {
const char* APS5_VABI locale_getKeywordsStart_nid_postfix(const char* localeID);
std::int32_t APS5_VABI res_countArrayItems_nid_postfix(const Icu::ResourceData* resourceData, std::uint32_t resource);
const char16_t* APS5_VABI res_getAlias_nid_postfix(const Icu::ResourceData* resourceData, std::uint32_t resource,
                                                   std::int32_t* length);
std::uint32_t APS5_VABI res_getArrayItem_nid_postfix(const Icu::ResourceData* resourceData, std::uint32_t array,
                                                     std::int32_t index);
const std::uint8_t* APS5_VABI res_getBinaryNoTrace_nid_postfix(const Icu::ResourceData* resourceData,
                                                               std::uint32_t resource, std::int32_t* length);
const std::int32_t* APS5_VABI res_getIntVectorNoTrace_nid_postfix(const Icu::ResourceData* resourceData,
                                                                  std::uint32_t resource, std::int32_t* length);
std::int32_t APS5_VABI res_getPublicType_nid_postfix(std::uint32_t resource);
std::uint32_t APS5_VABI res_getResource_nid_postfix(const Icu::ResourceData* resourceData, const char* key);
const char16_t* APS5_VABI res_getStringNoTrace_nid_postfix(const Icu::ResourceData* resourceData,
                                                           std::uint32_t resource, std::int32_t* length);
std::uint32_t APS5_VABI res_getTableItemByIndex_nid_postfix(const Icu::ResourceData* resourceData, std::uint32_t table,
                                                            std::int32_t index, const char** key);
std::uint32_t APS5_VABI res_getTableItemByKey_nid_postfix(const Icu::ResourceData* resourceData, std::uint32_t table,
                                                          std::int32_t* index, const char** key);
void APS5_VABI res_read_nid_postfix(Icu::ResourceData* resourceData, const Icu::DataInfo* info, const void* inBytes,
                                    std::int32_t length, std::int32_t* errorCode);
int APS5_VABI u_charDigitValue_nid_postfix(std::int32_t character);
int APS5_VABI u_charFromName_nid_postfix();
int APS5_VABI u_charName_nid_postfix();
int APS5_VABI u_digit_nid_postfix(std::int32_t character, std::int8_t radix);
int APS5_VABI u_enumCharNames_nid_postfix();
}
