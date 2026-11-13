#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libSceFreeTypeOt/include/Calc.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"

namespace {

std::uint64_t Magnitude(std::int64_t value) {
    return value < 0 ? 0 - static_cast<std::uint64_t>(value) : static_cast<std::uint64_t>(value);
}

std::int64_t CheckedMulDiv(std::int64_t a, std::int64_t b, std::int64_t c, bool rounded, const char* function) {
    const std::uint64_t x = Magnitude(a);
    const std::uint64_t y = Magnitude(b);
    const std::uint64_t z = Magnitude(c);
    if (x > 0x7FFFFFFF || y > 0x7FFFFFFF || z > 0x7FFFFFFF || (z == 0 && (x == 0 || y == 0))) {
        NotImplemented_nid_no_patch(function);
    }
    const std::uint64_t quotient = z == 0 ? 0x7FFFFFFF : (x * y + (rounded ? z >> 1 : 0)) / z;
    if (quotient > 0xFFFFFFFF) {
        NotImplemented_nid_no_patch(function);
    }
    const auto result = static_cast<std::int64_t>(quotient);
    return ((a < 0) != (b < 0)) != (c < 0) ? -result : result;
}

std::int64_t ScaledDivisor(std::int64_t scaling, const char* function) {
    if (scaling > std::numeric_limits<std::int64_t>::max() / 0x10000 || scaling < std::numeric_limits<std::int64_t>::min() / 0x10000) {
        NotImplemented_nid_no_patch(function);
    }
    return scaling * 0x10000;
}

std::int64_t ScaledSum(std::int64_t a, std::int64_t b, std::int64_t c, std::int64_t d, std::int64_t divisor, const char* function) {
    return CheckedMulDiv(a, b, divisor, true, function) + CheckedMulDiv(c, d, divisor, true, function);
}

std::int32_t IntegerSqrt(std::uint64_t value) {
    std::uint64_t low = 0;
    std::uint64_t high = 0xFFFFFFFF;
    while (low < high) {
        const std::uint64_t mid = (low + high + 1) >> 1;
        if (mid * mid <= value) {
            low = mid;
        } else {
            high = mid - 1;
        }
    }
    return static_cast<std::int32_t>(low);
}

}

extern "C" {


int APS5_VABI sceFreeTypeOtUnknown_UD7IFu7ML2U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("UD7IFu7ML2U", sceFreeTypeOtUnknown_UD7IFu7ML2U_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_2T7lbS_CSzA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2T7lbS+CSzA", sceFreeTypeOtUnknown_2T7lbS_CSzA_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_MGEIQsujKaY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MGEIQsujKaY", sceFreeTypeOtUnknown_MGEIQsujKaY_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_mkmgcRnAJog_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mkmgcRnAJog", sceFreeTypeOtUnknown_mkmgcRnAJog_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_m18bTUNF_0g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("m18bTUNF-0g", sceFreeTypeOtUnknown_m18bTUNF_0g_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_vTZ34rw1sfg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("vTZ34rw1sfg", sceFreeTypeOtUnknown_vTZ34rw1sfg_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_PvCNusrp9xQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PvCNusrp9xQ", sceFreeTypeOtUnknown_PvCNusrp9xQ_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_dlM4ASZdcjk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dlM4ASZdcjk", sceFreeTypeOtUnknown_dlM4ASZdcjk_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_Ev3BqJ_U1Fg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Ev3BqJ-U1Fg", sceFreeTypeOtUnknown_Ev3BqJ_U1Fg_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_ZhtAXOoLNyo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZhtAXOoLNyo", sceFreeTypeOtUnknown_ZhtAXOoLNyo_nid_no_patch);

int APS5_VABI sceFreeTypeOtGlyphCopy_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Dr3F3qrSQ2c", sceFreeTypeOtGlyphCopy_nid_no_patch);

int APS5_VABI sceFreeTypeOtGlyphGetCBox_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("avLzN0tTxLI", sceFreeTypeOtGlyphGetCBox_nid_no_patch);

int APS5_VABI FT_Glyph_Stroke_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFreeTypeOtGlyphStrokeBorder_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BBRhYfYBDgU", sceFreeTypeOtGlyphStrokeBorder_nid_no_patch);

int APS5_VABI sceFreeTypeOtGlyphToBitmap_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("c1nbr8oSWlg", sceFreeTypeOtGlyphToBitmap_nid_no_patch);

int APS5_VABI sceFreeTypeOtGlyphTransform_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AVEEb57HhUw", sceFreeTypeOtGlyphTransform_nid_no_patch);

int APS5_VABI sceFreeTypeOtHasPSGlyphNames_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("x8y0qzLiYOU", sceFreeTypeOtHasPSGlyphNames_nid_no_patch);

int APS5_VABI sceFreeTypeOtLibrarySetLcdFilter_nid_no_patch() {
    return 7;
}
APS5_EXPORT("VW4GD-TaIUc", sceFreeTypeOtLibrarySetLcdFilter_nid_no_patch);

int APS5_VABI sceFreeTypeOtLibrarySetLcdFilterWeights_nid_no_patch() {
    return 7;
}
APS5_EXPORT("GiuNQjoj4fs", sceFreeTypeOtLibrarySetLcdFilterWeights_nid_no_patch);

int APS5_VABI sceFreeTypeOtLibraryVersion_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5qqbXOGlHw4", sceFreeTypeOtLibraryVersion_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_4LY5pyZ5IQk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4LY5pyZ5IQk", sceFreeTypeOtUnknown_4LY5pyZ5IQk_nid_no_patch);

int APS5_VABI FT_New_GlyphSlot_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFreeTypeOtNewLibrary_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yrtsLmTfa48", sceFreeTypeOtNewLibrary_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_C_PpdS5zris_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("C-PpdS5zris", sceFreeTypeOtUnknown_C_PpdS5zris_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_g8eLxL9VAhU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("g8eLxL9VAhU", sceFreeTypeOtUnknown_g8eLxL9VAhU_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineDone_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BqGJsJcFYaM", sceFreeTypeOtOutlineDone_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineEmbolden_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("l6Cf9rb9fZQ", sceFreeTypeOtOutlineEmbolden_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineEmboldenXY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BgUMBGvhATU", sceFreeTypeOtOutlineEmboldenXY_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineGetBitmap_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4RRjK-Wd1lU", sceFreeTypeOtOutlineGetBitmap_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineNew_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QH2emcgeFck", sceFreeTypeOtOutlineNew_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineRender_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9km02mPqPJE", sceFreeTypeOtOutlineRender_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineTransform_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Lv6K1d47jg", sceFreeTypeOtOutlineTransform_nid_no_patch);

int APS5_VABI sceFreeTypeOtPropertyGet_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8fAMdZCnewA", sceFreeTypeOtPropertyGet_nid_no_patch);

int APS5_VABI sceFreeTypeOtPropertySet_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LsB538+IPrE", sceFreeTypeOtPropertySet_nid_no_patch);

int APS5_VABI sceFreeTypeOtQAlloc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hz-J3hohXW8", sceFreeTypeOtQAlloc_nid_no_patch);

int APS5_VABI sceFreeTypeOtQRealloc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KVbmKCGGowg", sceFreeTypeOtQRealloc_nid_no_patch);

int APS5_VABI sceFreeTypeOtRealloc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HVSaZbxYi2s", sceFreeTypeOtRealloc_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_qAPXM5DN62Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qAPXM5DN62Q", sceFreeTypeOtUnknown_qAPXM5DN62Q_nid_no_patch);

int APS5_VABI sceFreeTypeOtRemoveModule_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5ipBcLYVc2k", sceFreeTypeOtRemoveModule_nid_no_patch);

int APS5_VABI FT_Render_Glyph_Internal_nid_postfix(FreeType::GuestLibrary* library, FreeType::GuestGlyphSlotRec* slot, std::int32_t renderMode) {
    if (!library || !slot || !slot->face || slot->library != library) NotImplemented_nid_no_patch(__func__);
    bool owned = false;
    for (const auto& face : library->faces) owned = owned || &face->slot == slot;
    if (!owned) NotImplemented_nid_no_patch(__func__);
    return FT_Render_Glyph_nid_postfix(slot, renderMode);
}

int APS5_VABI FT_Set_Debug_Hook_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceFreeTypeOtSetMMBlendCoordinates_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zetC8Tp1S8w", sceFreeTypeOtSetMMBlendCoordinates_nid_no_patch);

int APS5_VABI sceFreeTypeOtSetMMDesignCoordinates_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DAmwG6U7YIc", sceFreeTypeOtSetMMDesignCoordinates_nid_no_patch);

int APS5_VABI sceFreeTypeOtSetRenderer_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Yu9oXRVmTTo", sceFreeTypeOtSetRenderer_nid_no_patch);

int APS5_VABI sceFreeTypeOtSetVarBlendCoordinates_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ihIHD8BTNxI", sceFreeTypeOtSetVarBlendCoordinates_nid_no_patch);

int APS5_VABI sceFreeTypeOtSetVarDesignCoordinates_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yCEJIsjIOO8", sceFreeTypeOtSetVarDesignCoordinates_nid_no_patch);

void APS5_VABI FT_Stroker_ExportForward_nid_no_patch(void* stroker, FreeType::GuestOutline* outline) {
    FT_Stroker_Export_nid_postfix(stroker, outline);
}
APS5_EXPORT("2rcvKZfm5rI", FT_Stroker_ExportForward_nid_no_patch);

int APS5_VABI FT_Trace_Get_Count_nid_no_patch() {
    return 0;
}
APS5_EXPORT("08-mhD-AAds", FT_Trace_Get_Count_nid_no_patch);

const char* APS5_VABI FT_Trace_Get_Name_nid_no_patch(int idx) {
    static_cast<void>(idx);
    return nullptr;
}
APS5_EXPORT("64XMq6sDxUo", FT_Trace_Get_Name_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_4xG87W9dTSg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4xG87W9dTSg", sceFreeTypeOtUnknown_4xG87W9dTSg_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_4_SScYDT5Pg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4+SScYDT5Pg", sceFreeTypeOtUnknown_4_SScYDT5Pg_nid_no_patch);

int APS5_VABI sceFreeTypeOtVectorFromPolar_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2rmvbvOCyjY", sceFreeTypeOtVectorFromPolar_nid_no_patch);

int APS5_VABI sceFreeTypeOtVectorLength_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1jmC2QcpRl0", sceFreeTypeOtVectorLength_nid_no_patch);

int APS5_VABI sceFreeTypeOtVectorPolarize_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HMcNe70D9TI", sceFreeTypeOtVectorPolarize_nid_no_patch);

int APS5_VABI sceFreeTypeOtVectorRotate_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6wJwUIVwAxg", sceFreeTypeOtVectorRotate_nid_no_patch);

int APS5_VABI ft_corner_is_flat_nid_postfix(std::int64_t in_x, std::int64_t in_y, std::int64_t out_x, std::int64_t out_y) {
    const __int128 ax = static_cast<__int128>(in_x) + static_cast<__int128>(out_x);
    const __int128 ay = static_cast<__int128>(in_y) + static_cast<__int128>(out_y);
    const auto hypot = [](__int128 x, __int128 y) -> unsigned __int128 {
        const auto ux = static_cast<unsigned __int128>(x < 0 ? -x : x);
        const auto uy = static_cast<unsigned __int128>(y < 0 ? -y : y);
        return ux > uy ? ux + ((3 * uy) >> 3) : uy + ((3 * ux) >> 3);
    };
    const auto inDist = hypot(in_x, in_y);
    const auto outDist = hypot(out_x, out_y);
    const auto hypotDist = hypot(ax, ay);
    const auto sumDist = inDist + outDist;
    if (sumDist < hypotDist) {
        return 1;
    }
    return (sumDist - hypotDist) < (hypotDist >> 4) ? 1 : 0;
}

int APS5_VABI ft_corner_orientation_nid_postfix(std::int64_t in_x, std::int64_t in_y, std::int64_t out_x, std::int64_t out_y) {
    const auto delta = static_cast<std::int64_t>(
        static_cast<std::uint64_t>(in_x) * static_cast<std::uint64_t>(out_y) -
        static_cast<std::uint64_t>(in_y) * static_cast<std::uint64_t>(out_x)
    );
    return (delta > 0) - (delta < 0);
}

std::int64_t APS5_VABI FT_MulDiv_No_Round_nid_postfix(std::int64_t a, std::int64_t b, std::int64_t c) {
    return CheckedMulDiv(a, b, c, false, __func__);
}

void APS5_VABI FT_Matrix_Multiply_Scaled_nid_postfix(const FreeType::GuestMatrix* a, FreeType::GuestMatrix* b, std::int64_t scaling) {
    const std::int64_t divisor = ScaledDivisor(scaling, __func__);
    if (!a || !b) {
        return;
    }
    const FreeType::GuestMatrix product{
        ScaledSum(a->xx, b->xx, a->xy, b->yx, divisor, __func__),
        ScaledSum(a->xx, b->xy, a->xy, b->yy, divisor, __func__),
        ScaledSum(a->yx, b->xx, a->yy, b->yx, divisor, __func__),
        ScaledSum(a->yx, b->xy, a->yy, b->yy, divisor, __func__),
    };
    *b = product;
}

void APS5_VABI FT_Vector_Transform_Scaled_nid_postfix(FreeType::GuestVector* vector, const FreeType::GuestMatrix* matrix, std::int64_t scaling) {
    const std::int64_t divisor = ScaledDivisor(scaling, __func__);
    if (!vector || !matrix) {
        return;
    }
    const FreeType::GuestVector transformed{
        ScaledSum(vector->x, matrix->xx, vector->y, matrix->xy, divisor, __func__),
        ScaledSum(vector->x, matrix->yx, vector->y, matrix->yy, divisor, __func__),
    };
    *vector = transformed;
}

std::int32_t APS5_VABI FT_SqrtFixed_nid_postfix(std::int32_t x) {
    return x > 0 ? IntegerSqrt(static_cast<std::uint64_t>(x) << 16) : 0;
}

std::int32_t APS5_VABI FT_Sqrt32_nid_postfix(std::int32_t x) {
    return IntegerSqrt(static_cast<std::uint32_t>(x));
}

std::uint32_t APS5_VABI ft_highpow2_nid_postfix(std::uint32_t value) {
    return std::bit_floor(value);
}

void APS5_VABI ft_synthesize_vertical_metrics_nid_postfix(FreeType::GuestGlyphMetrics* metrics, std::int64_t advance) {
    if (!metrics) {
        return;
    }
    std::int64_t height = metrics->height;
    if (metrics->horiBearingY < 0) {
        if (height < metrics->horiBearingY) {
            height = metrics->horiBearingY;
        }
    } else if (metrics->horiBearingY > 0) {
        height -= metrics->horiBearingY;
    }
    if (!advance) {
        advance = height * 12 / 10;
    }
    metrics->vertBearingX = metrics->horiBearingX - metrics->horiAdvance / 2;
    metrics->vertBearingY = (advance - height) / 2;
    metrics->vertAdvance = advance;
}

void* APS5_VABI ft_service_list_lookup_nid_postfix(const GuestServiceDesc* descriptors, const char* service_id) {
    if (descriptors && service_id) {
        for (; descriptors->serv_id; ++descriptors) {
            if (std::strcmp(descriptors->serv_id, service_id) == 0) {
                return descriptors->serv_data;
            }
        }
    }
    return nullptr;
}

int APS5_VABI sceFreeTypeOtUnknown_uSNqxrARpEQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uSNqxrARpEQ", sceFreeTypeOtUnknown_uSNqxrARpEQ_nid_no_patch);

}
