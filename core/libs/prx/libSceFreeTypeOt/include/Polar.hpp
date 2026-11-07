#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <cstdint>

namespace FreeType {

std::int64_t OtVectorLength(const GuestVector& vector, const char* function);

}

extern "C" {

std::int64_t APS5_VABI sceFreeTypeOtVectorLength_nid_no_patch(FreeType::GuestVector* vector);
void APS5_VABI sceFreeTypeOtVectorPolarize_nid_no_patch(FreeType::GuestVector* vector, std::int64_t* length, std::int64_t* angle);
void APS5_VABI sceFreeTypeOtVectorFromPolar_nid_no_patch(FreeType::GuestVector* vector, std::int64_t length, std::int64_t angle);
void APS5_VABI sceFreeTypeOtVectorRotate_nid_no_patch(FreeType::GuestVector* vector, std::int64_t angle);

}
