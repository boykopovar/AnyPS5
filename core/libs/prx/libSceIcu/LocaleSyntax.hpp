#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI uloc_getVariant_nid_postfix(const char* locale, char* result,
                                                 std::int32_t capacity, std::int32_t* status);
std::int32_t APS5_VABI uloc_setKeywordValue_nid_postfix(const char* keyword, const char* value,
                                                      char* buffer, std::int32_t capacity,
                                                      std::int32_t* status);

}
