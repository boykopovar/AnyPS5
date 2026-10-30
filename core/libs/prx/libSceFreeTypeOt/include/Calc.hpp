#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

std::int64_t APS5_VABI FT_MulDiv_No_Round_nid_postfix(std::int64_t a, std::int64_t b, std::int64_t c);
void APS5_VABI FT_Matrix_Multiply_Scaled_nid_postfix(const FreeType::GuestMatrix* a, FreeType::GuestMatrix* b, std::int64_t scaling);
void APS5_VABI FT_Vector_Transform_Scaled_nid_postfix(FreeType::GuestVector* vector, const FreeType::GuestMatrix* matrix, std::int64_t scaling);
std::uint32_t APS5_VABI ft_highpow2_nid_postfix(std::uint32_t value);

}
