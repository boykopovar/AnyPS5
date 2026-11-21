#pragma once

#include "prx/libSceFreeType/include/GuestList.hpp"
#include "SceTypes.hpp"

extern "C" {

std::int32_t APS5_VABI FT_Gzip_Uncompress_nid_postfix(FreeType::GuestMemoryRec* memory, std::uint8_t* output, std::uint64_t* outputLength, const std::uint8_t* input, std::uint64_t inputLength);

}
