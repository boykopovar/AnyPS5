#pragma once

#include "prx/libSceIcu/ConverterCallbacks.hpp"
#include <cstdint>

extern "C" {
void APS5_VABI ucnv_cbToUWriteUChars_nid_postfix(Icu::ConverterToUnicodeArgs* arguments,
    const char16_t* source, std::int32_t length, std::int32_t offsetIndex, std::int32_t* error);
void APS5_VABI ucnv_cbToUWriteSub_nid_postfix(Icu::ConverterToUnicodeArgs* arguments,
    std::int32_t offsetIndex, std::int32_t* error);
}
