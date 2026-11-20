#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UConverter;

extern "C" {
UConverter* APS5_VABI ucnv_open_nid_postfix(const char* name, std::int32_t* status);
void APS5_VABI ucnv_close_nid_postfix(UConverter* converter);
void APS5_VABI ucnv_fromUnicode_nid_postfix(UConverter* converter, char** target, const char* targetLimit,
    const char16_t** source, const char16_t* sourceLimit, std::int32_t* offsets, std::int8_t flush,
    std::int32_t* status);
void APS5_VABI ucnv_toUnicode_nid_postfix(UConverter* converter, char16_t** target, const char16_t* targetLimit,
    const char** source, const char* sourceLimit, std::int32_t* offsets, std::int8_t flush,
    std::int32_t* status);
}
