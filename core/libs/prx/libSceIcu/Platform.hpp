#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

using UComparator = std::int32_t (APS5_VABI*)(const void* context, const void* left, const void* right);

std::int32_t APS5_VABI uprv_itou_nid_postfix(char16_t* buffer, std::int32_t capacity, std::uint32_t value,
                                             std::uint32_t radix, std::int32_t minwidth);
std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char* path);
std::int32_t APS5_VABI uprv_compareASCIIPropertyNames_nid_postfix(const char* first, const char* second);
std::int32_t APS5_VABI uprv_stableBinarySearch_nid_postfix(char* array, std::int32_t limit, void* item,
                                                           std::int32_t itemSize, UComparator comparator,
                                                           const void* context);
void APS5_VABI uprv_sortArray_nid_postfix(void* array, std::int32_t length, std::int32_t itemSize,
                                          UComparator comparator, const void* context, std::int8_t sortStable,
                                          std::int32_t* status);

}
