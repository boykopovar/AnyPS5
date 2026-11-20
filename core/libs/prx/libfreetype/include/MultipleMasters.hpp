#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "SceTypes.hpp"

extern "C" {

std::int32_t APS5_VABI FT_Get_MM_Blend_Coordinates_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t numCoords, std::int64_t* coords);
std::int32_t APS5_VABI FT_Get_MM_WeightVector_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t* length, std::int64_t* weightVector);
std::int32_t APS5_VABI FT_Set_MM_WeightVector_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t length, std::int64_t* weightVector);
std::int32_t APS5_VABI FT_Set_Named_Instance_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t instanceIndex);

}
