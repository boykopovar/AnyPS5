#pragma once

#include "SceTypes.hpp"

struct UDataSwapper;

extern "C" {

std::int32_t APS5_VABI ucol_swap_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status);
std::int32_t APS5_VABI ucol_swapInverseUCA_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status);
std::int8_t APS5_VABI ucol_looksLikeCollationBinary_nid_postfix(const UDataSwapper* swapper, const void* inData,
    std::int32_t length);
std::int32_t APS5_VABI unorm2_swap_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status);
std::int32_t APS5_VABI ucnv_swapAliases_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status);
std::int32_t APS5_VABI uchar_swapNames_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status);
std::int32_t APS5_VABI ubrk_swap_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status);
std::int32_t APS5_VABI udict_swap_nid_postfix(const UDataSwapper* swapper, const void* inData, std::int32_t length,
    void* outData, std::int32_t* status);

}
