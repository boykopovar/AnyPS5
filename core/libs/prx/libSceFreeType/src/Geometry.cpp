#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libc/include/General.hpp"

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
    if (!vector || !matrix) return;
    GuestVector input{};
    GuestMatrix transform{};
    if (!GuestCopyIn_nid_no_patch(&input, vector, sizeof(input)) || !GuestCopyIn_nid_no_patch(&transform, matrix, sizeof(transform))) return;
    const GuestVector result{Multiply(input.x, transform.xx) + Multiply(input.y, transform.xy),
                             Multiply(input.x, transform.yx) + Multiply(input.y, transform.yy)};
    GuestCopyOut_nid_no_patch(vector, &result, sizeof(result));
}

void APS5_VABI FT_Matrix_Multiply_nid_postfix(const GuestMatrix* a, GuestMatrix* b) {
    if (!a || !b) return;
    GuestMatrix left{};
    GuestMatrix right{};
    if (!GuestCopyIn_nid_no_patch(&left, a, sizeof(left)) || !GuestCopyIn_nid_no_patch(&right, b, sizeof(right))) return;
    const GuestMatrix result{Multiply(left.xx, right.xx) + Multiply(left.xy, right.yx),
                             Multiply(left.xx, right.xy) + Multiply(left.xy, right.yy),
                             Multiply(left.yx, right.xx) + Multiply(left.yy, right.yx),
                             Multiply(left.yx, right.xy) + Multiply(left.yy, right.yy)};
    GuestCopyOut_nid_no_patch(b, &result, sizeof(result));
}

int APS5_VABI FT_Matrix_Invert_nid_postfix(GuestMatrix* matrix) {
    if (!matrix) return FT_Err_Invalid_Argument;
    GuestMatrix input{};
    if (!GuestCopyIn_nid_no_patch(&input, matrix, sizeof(input))) return FT_Err_Invalid_Argument;
    const auto determinant = Multiply(input.xx, input.yy) - Multiply(input.xy, input.yx);
    if (determinant == 0) return FT_Err_Invalid_Argument;
    const auto divide = [determinant](std::int64_t value, bool negate) {
        const auto numerator = value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
        const auto denominator = determinant < 0 ? 0 - static_cast<std::uint64_t>(determinant) : static_cast<std::uint64_t>(determinant);
        const auto quotient = ((numerator << 16) + (denominator >> 1)) / denominator;
        return std::bit_cast<std::int64_t>(((value < 0) != (determinant < 0)) != negate ? 0 - quotient : quotient);
    };
    const GuestMatrix result{divide(input.yy, false), divide(input.xy, true),
                             divide(input.yx, true), divide(input.xx, false)};
    if (!GuestCopyOut_nid_no_patch(matrix, &result, sizeof(result))) return FT_Err_Invalid_Argument;
    return FT_Err_Ok;
}

}
