#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRIGONOMETRY_H

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using namespace FreeType;

namespace {

FT_Fixed HostValue(std::int64_t value, const char* function) {
    if (value < std::numeric_limits<FT_Fixed>::min() || value > std::numeric_limits<FT_Fixed>::max()) {
        throw std::runtime_error(std::string(function) + ": a value outside the host FT_Fixed range is not implemented");
    }
    return static_cast<FT_Fixed>(value);
}

FT_Angle HostAngle(std::int64_t angle) {
    const auto normalized = NormalizeAngle_nid_no_patch(angle);
    return normalized == 0 && angle != 0 ? FT_ANGLE_2PI : normalized;
}

FT_Vector LoadVector(const GuestVector* vector, const char* function) {
    GuestVector value{};
    if (!GuestCopyIn_nid_no_patch(&value, vector, sizeof(value))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return {HostValue(value.x, function), HostValue(value.y, function)};
}

void StoreVector(GuestVector* vector, const FT_Vector& value, const char* function) {
    const GuestVector guest{value.x, value.y};
    if (!GuestCopyOut_nid_no_patch(vector, &guest, sizeof(guest))) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

}

extern "C" {

std::int64_t APS5_VABI FT_Atan2_nid_postfix(std::int64_t x, std::int64_t y) {
    return FT_Atan2(HostValue(x, __func__), HostValue(y, __func__));
}

std::int64_t APS5_VABI FT_Vector_Length_nid_postfix(const GuestVector* vector) {
    if (!vector) return FT_Vector_Length(nullptr);
    FT_Vector native = LoadVector(vector, __func__);
    return FT_Vector_Length(&native);
}

void APS5_VABI FT_Vector_Rotate_nid_postfix(GuestVector* vector, std::int64_t angle) {
    if (!vector) return;
    FT_Vector native = LoadVector(vector, __func__);
    FT_Vector_Rotate(&native, HostAngle(angle));
    StoreVector(vector, native, __func__);
}

void APS5_VABI FT_Vector_Polarize_nid_postfix(const GuestVector* vector, std::int64_t* length, std::int64_t* angle) {
    if (!vector || !length || !angle) return;
    FT_Vector native = LoadVector(vector, __func__);
    if (native.x == 0 && native.y == 0) return;
    FT_Fixed nativeLength = 0;
    FT_Angle nativeAngle = 0;
    FT_Vector_Polarize(&native, &nativeLength, &nativeAngle);
    const std::int64_t guestLength = nativeLength;
    const std::int64_t guestAngle = nativeAngle;
    if (!GuestWritable_nid_no_patch(length, sizeof(guestLength)) || !GuestWritable_nid_no_patch(angle, sizeof(guestAngle))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!GuestCopyOut_nid_no_patch(length, &guestLength, sizeof(guestLength)) || !GuestCopyOut_nid_no_patch(angle, &guestAngle, sizeof(guestAngle))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI FT_Vector_From_Polar_nid_postfix(GuestVector* vector, std::int64_t length, std::int64_t angle) {
    if (!vector) return;
    FT_Vector native{};
    FT_Vector_From_Polar(&native, HostValue(length, __func__), HostAngle(angle));
    StoreVector(vector, native, __func__);
}

}
