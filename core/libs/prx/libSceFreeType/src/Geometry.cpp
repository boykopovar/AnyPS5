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
    GuestVector localVector{};
    GuestMatrix localMatrix{};
    if (!GuestCopyIn_nid_no_patch(&localVector, vector, sizeof(localVector)) ||
        !GuestCopyIn_nid_no_patch(&localMatrix, matrix, sizeof(localMatrix))) {
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if (!GuestWritable_nid_no_patch(vector, sizeof(*vector))) {
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    const GuestVector result{Multiply(localVector.x, localMatrix.xx) + Multiply(localVector.y, localMatrix.xy),
                             Multiply(localVector.x, localMatrix.yx) + Multiply(localVector.y, localMatrix.yy)};
    if (!GuestCopyOut_nid_no_patch(vector, &result, sizeof(result))) {
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
}

void APS5_VABI FT_Matrix_Multiply_nid_postfix(const GuestMatrix* a, GuestMatrix* b) {
    if (!a || !b) return;
    GuestMatrix localA{};
    GuestMatrix localB{};
    if (!GuestCopyIn_nid_no_patch(&localA, a, sizeof(localA)) ||
        !GuestCopyIn_nid_no_patch(&localB, b, sizeof(localB))) {
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if (!GuestWritable_nid_no_patch(b, sizeof(*b))) {
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    const GuestMatrix result{Multiply(localA.xx, localB.xx) + Multiply(localA.xy, localB.yx),
                             Multiply(localA.xx, localB.xy) + Multiply(localA.xy, localB.yy),
                             Multiply(localA.yx, localB.xx) + Multiply(localA.yy, localB.yx),
                             Multiply(localA.yx, localB.xy) + Multiply(localA.yy, localB.yy)};
    if (!GuestCopyOut_nid_no_patch(b, &result, sizeof(result))) {
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
}

int APS5_VABI FT_Matrix_Invert_nid_postfix(GuestMatrix* matrix) {
    if (!matrix) return FT_Err_Invalid_Argument;
    GuestMatrix localMatrix{};
    if (!GuestCopyIn_nid_no_patch(&localMatrix, matrix, sizeof(localMatrix)) ||
        !GuestWritable_nid_no_patch(matrix, sizeof(*matrix))) {
        return FT_Err_Invalid_Argument;
    }
    const auto determinant = Multiply(localMatrix.xx, localMatrix.yy) - Multiply(localMatrix.xy, localMatrix.yx);
    if (determinant == 0) return FT_Err_Invalid_Argument;
    const auto divide = [determinant](std::int64_t value, bool negate) {
        const auto numerator = value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
        const auto denominator = determinant < 0 ? 0 - static_cast<std::uint64_t>(determinant) : static_cast<std::uint64_t>(determinant);
        const auto quotient = ((numerator << 16) + (denominator >> 1)) / denominator;
        return std::bit_cast<std::int64_t>(((value < 0) != (determinant < 0)) != negate ? 0 - quotient : quotient);
    };
    const GuestMatrix result{divide(localMatrix.yy, false), divide(localMatrix.xy, true),
                             divide(localMatrix.yx, true), divide(localMatrix.xx, false)};
    if (!GuestCopyOut_nid_no_patch(matrix, &result, sizeof(result))) {
        return FT_Err_Invalid_Argument;
    }
    return FT_Err_Ok;
}

}
