#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/GuestArena.hpp"
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

FT_Vector HostVector(const GuestVector& vector, const char* function) {
    return {HostValue(vector.x, function), HostValue(vector.y, function)};
}

}

extern "C" {

std::int64_t APS5_VABI FT_Atan2_nid_postfix(std::int64_t x, std::int64_t y) {
    return FT_Atan2(HostValue(x, __func__), HostValue(y, __func__));
}

std::int64_t APS5_VABI FT_Vector_Length_nid_postfix(const GuestVector* vector) {
    if (!vector) return FT_Vector_Length(nullptr);
    FT_Vector native = HostVector(*vector, __func__);
    return FT_Vector_Length(&native);
}

void APS5_VABI FT_Vector_Rotate_nid_postfix(GuestVector* vector, std::int64_t angle) {
    if (!vector) return;
    FT_Vector native = HostVector(*vector, __func__);
    FT_Vector_Rotate(&native, HostValue(angle, __func__));
    GuestArena::HostWrite write(vector, sizeof(*vector));
    *vector = {native.x, native.y};
}

void APS5_VABI FT_Vector_Polarize_nid_postfix(const GuestVector* vector, std::int64_t* length, std::int64_t* angle) {
    if (!vector || !length || !angle || (vector->x == 0 && vector->y == 0)) return;
    FT_Vector native = HostVector(*vector, __func__);
    FT_Fixed nativeLength = 0;
    FT_Angle nativeAngle = 0;
    FT_Vector_Polarize(&native, &nativeLength, &nativeAngle);
    GuestArena::HostWrite lengthWrite(length, sizeof(*length));
    GuestArena::HostWrite angleWrite(angle, sizeof(*angle));
    *length = nativeLength;
    *angle = nativeAngle;
}

}
