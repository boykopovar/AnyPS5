#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UIDNA;
struct UIDNAInfo;

extern "C" {
std::int32_t APS5_VABI uidna_nameToUnicode_nid_postfix(const UIDNA* idna, const char16_t* name, std::int32_t length, char16_t* dest,
                             std::int32_t capacity, UIDNAInfo* pInfo, std::int32_t* pErrorCode);
}
