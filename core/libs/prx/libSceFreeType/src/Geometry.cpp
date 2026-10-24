#include "prx/libSceFreeType/include/GuestFreeType.hpp"

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
    const GuestVector result{Multiply(vector->x, matrix->xx) + Multiply(vector->y, matrix->xy),
                             Multiply(vector->x, matrix->yx) + Multiply(vector->y, matrix->yy)};
    *vector = result;
}

void APS5_VABI FT_Matrix_Multiply_nid_postfix(const GuestMatrix* a, GuestMatrix* b) {
    if (!a || !b) return;
    const GuestMatrix result{Multiply(a->xx, b->xx) + Multiply(a->xy, b->yx),
                             Multiply(a->xx, b->xy) + Multiply(a->xy, b->yy),
                             Multiply(a->yx, b->xx) + Multiply(a->yy, b->yx),
                             Multiply(a->yx, b->xy) + Multiply(a->yy, b->yy)};
    *b = result;
}

int APS5_VABI FT_Matrix_Invert_nid_postfix(GuestMatrix* matrix) {
    if (!matrix) return FT_Err_Invalid_Argument;
    const auto determinant = Multiply(matrix->xx, matrix->yy) - Multiply(matrix->xy, matrix->yx);
    if (determinant == 0) return FT_Err_Invalid_Argument;
    const auto divide = [determinant](std::int64_t value, bool negate) {
        const auto numerator = value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
        const auto denominator = determinant < 0 ? 0 - static_cast<std::uint64_t>(determinant) : static_cast<std::uint64_t>(determinant);
        const auto quotient = ((numerator << 16) + (denominator >> 1)) / denominator;
        return std::bit_cast<std::int64_t>(((value < 0) != (determinant < 0)) != negate ? 0 - quotient : quotient);
    };
    const GuestMatrix result{divide(matrix->yy, false), divide(matrix->xy, true),
                             divide(matrix->yx, true), divide(matrix->xx, false)};
    *matrix = result;
    return FT_Err_Ok;
}

}
