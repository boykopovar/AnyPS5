#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libc/include/GuestPtr.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <bit>
#include <cstdint>

using namespace FreeType;

namespace {

std::int64_t Multiply(std::int64_t a, std::int64_t b) {
    const auto left = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a));
    const auto right = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(b));
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(FT_MulFix(left, right)));
}

}

extern "C" {

void APS5_VABI FT_Vector_Transform_nid_postfix(GuestVector* vector, const GuestMatrix* matrix) {
    GuestVector value;
    GuestMatrix transform;
    if (!GuestPtr<GuestVector>(vector).CopyIn(value) || !GuestPtr<const GuestMatrix>(matrix).CopyIn(transform)) return;
    const GuestVector result{Multiply(value.x, transform.xx) + Multiply(value.y, transform.xy),
                             Multiply(value.x, transform.yx) + Multiply(value.y, transform.yy)};
    GuestPtr<GuestVector>(vector).CopyOut(result);
}

void APS5_VABI FT_Matrix_Multiply_nid_postfix(const GuestMatrix* a, GuestMatrix* b) {
    GuestMatrix left;
    GuestMatrix right;
    if (!GuestPtr<const GuestMatrix>(a).CopyIn(left) || !GuestPtr<GuestMatrix>(b).CopyIn(right)) return;
    const GuestMatrix result{Multiply(left.xx, right.xx) + Multiply(left.xy, right.yx),
                             Multiply(left.xx, right.xy) + Multiply(left.xy, right.yy),
                             Multiply(left.yx, right.xx) + Multiply(left.yy, right.yx),
                             Multiply(left.yx, right.xy) + Multiply(left.yy, right.yy)};
    GuestPtr<GuestMatrix>(b).CopyOut(result);
}

int APS5_VABI FT_Matrix_Invert_nid_postfix(GuestMatrix* matrix) {
    GuestMatrix value;
    if (!GuestPtr<GuestMatrix>(matrix).CopyIn(value)) return FT_Err_Invalid_Argument;
    const auto determinant = Multiply(value.xx, value.yy) - Multiply(value.xy, value.yx);
    if (determinant == 0) return FT_Err_Invalid_Argument;
    const auto divide = [determinant](std::int64_t value, bool negate) {
        const auto numerator = value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
        const auto denominator = determinant < 0 ? 0 - static_cast<std::uint64_t>(determinant) : static_cast<std::uint64_t>(determinant);
        const auto quotient = ((numerator << 16) + (denominator >> 1)) / denominator;
        return std::bit_cast<std::int64_t>(((value < 0) != (determinant < 0)) != negate ? 0 - quotient : quotient);
    };
    const GuestMatrix result{divide(value.yy, false), divide(value.xy, true),
                             divide(value.yx, true), divide(value.xx, false)};
    if (!GuestPtr<GuestMatrix>(matrix).CopyOut(result)) return FT_Err_Invalid_Argument;
    return FT_Err_Ok;
}

}
