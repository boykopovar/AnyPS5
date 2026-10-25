#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_TRIGONOMETRY_H

#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>

using namespace FreeType;

namespace {

FT_Angle NormalizeAngle(std::int64_t angle) {
    return static_cast<FT_Angle>(angle % (360ll << 16));
}

GuestVector TangentVector(std::int64_t angle) {
    constexpr std::array<std::int64_t, 22> angles{
        1740967, 919879, 466945, 234379, 117304, 58666, 29335, 14668, 7334, 3667, 1833,
        917, 458, 229, 115, 57, 29, 14, 7, 4, 2, 1};
    std::int64_t remaining = NormalizeAngle(angle);
    GuestVector vector{1ll << 24, 0};
    while (remaining < -(45ll << 16)) {
        vector = {vector.y, -vector.x};
        remaining += 90ll << 16;
    }
    while (remaining > (45ll << 16)) {
        vector = {-vector.y, vector.x};
        remaining -= 90ll << 16;
    }
    std::int64_t rounding = 1;
    for (std::size_t iteration = 1; iteration <= angles.size(); ++iteration) {
        const std::int64_t direction = remaining < 0 ? 1 : -1;
        const GuestVector step{(vector.y + rounding) >> iteration, (vector.x + rounding) >> iteration};
        vector.x += direction * step.x;
        vector.y -= direction * step.y;
        remaining += direction * angles[iteration - 1];
        rounding <<= 1;
    }
    return vector;
}

}

extern "C" {

std::int64_t APS5_VABI FT_Cos_nid_postfix(std::int64_t angle) {
    return FT_Cos(NormalizeAngle(angle));
}

std::int64_t APS5_VABI FT_Sin_nid_postfix(std::int64_t angle) {
    return FT_Sin(NormalizeAngle(angle));
}

std::int64_t APS5_VABI FT_Tan_nid_postfix(std::int64_t angle) {
    const GuestVector vector = TangentVector(angle);
    const std::uint64_t x = static_cast<std::uint64_t>(vector.x < 0 ? -vector.x : vector.x);
    const std::uint64_t y = static_cast<std::uint64_t>(vector.y < 0 ? -vector.y : vector.y);
    const auto quotient = static_cast<std::int64_t>(x == 0 ? 0x7FFFFFFFull : ((y << 16) + (x >> 1)) / x);
    return (vector.x < 0) != (vector.y < 0) ? -quotient : quotient;
}

void APS5_VABI FT_Vector_Unit_nid_postfix(GuestVector* vector, std::int64_t angle) {
    if (!vector) return;
    FT_Vector native{};
    FT_Vector_Unit(&native, NormalizeAngle(angle));
    vector->x = native.x;
    vector->y = native.y;
}

std::int64_t APS5_VABI FT_Angle_Diff_nid_postfix(std::int64_t first, std::int64_t second) {
    if ((first > 0 && second < std::numeric_limits<std::int64_t>::min() + first) ||
        (first < 0 && second > std::numeric_limits<std::int64_t>::max() + first)) {
        throw std::runtime_error("FT_Angle_Diff: overflowing signed angle subtraction is not implemented");
    }
    return FT_Angle_Diff(0, NormalizeAngle(second - first));
}

}
