#pragma once

#include "SceTypes.hpp"

extern "C" {

std::int32_t APS5_VABI uloc_countAvailable_nid_postfix();
const char* APS5_VABI uloc_getAvailable_nid_postfix(std::int32_t index);
std::int32_t APS5_VABI uloc_addLikelySubtags_nid_postfix(const char* locale, char* result, std::int32_t capacity,
                                                         std::int32_t* status);
std::int32_t APS5_VABI uloc_minimizeSubtags_nid_postfix(const char* locale, char* result, std::int32_t capacity,
                                                        std::int32_t* status);

void APS5_VABI udata_setCommonData_nid_postfix(const void* data, std::int32_t* status);
void APS5_VABI udata_setFileAccess_nid_postfix(std::int32_t access, std::int32_t* status);

}
