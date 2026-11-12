#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>

extern "C" {

void APS5_VABI FT_Select_Metrics_nid_postfix(FreeType::GuestFaceRec* face, std::uint64_t strike_index);
std::int32_t APS5_VABI FT_Request_Metrics_nid_postfix(FreeType::GuestFaceRec* face, const FreeType::GuestSizeRequestRec* req);
std::int32_t APS5_VABI FT_Match_Size_nid_postfix(FreeType::GuestFaceRec* face, const FreeType::GuestSizeRequestRec* req, unsigned char ignore_width, std::uint64_t* size_index);

}
