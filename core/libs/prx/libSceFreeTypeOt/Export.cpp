#include "prx/libSceFreeType/include/Api.hpp"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeTypeOt/include/Calc.hpp"
#include "prx/libc/include/General.hpp"

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

int APS5_VABI sceFreeTypeOtUnknown_73vg_nRH68g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("73vg+nRH68g", sceFreeTypeOtUnknown_73vg_nRH68g_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_wBKAYO7yEXw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wBKAYO7yEXw", sceFreeTypeOtUnknown_wBKAYO7yEXw_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_nSIux4OQyJM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("nSIux4OQyJM", sceFreeTypeOtUnknown_nSIux4OQyJM_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_Dr3F3qrSQ2c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Dr3F3qrSQ2c", sceFreeTypeOtUnknown_Dr3F3qrSQ2c_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_avLzN0tTxLI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("avLzN0tTxLI", sceFreeTypeOtUnknown_avLzN0tTxLI_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_0rKmI1ZbMAc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0rKmI1ZbMAc", sceFreeTypeOtUnknown_0rKmI1ZbMAc_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_BBRhYfYBDgU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BBRhYfYBDgU", sceFreeTypeOtUnknown_BBRhYfYBDgU_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_c1nbr8oSWlg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("c1nbr8oSWlg", sceFreeTypeOtUnknown_c1nbr8oSWlg_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_AVEEb57HhUw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AVEEb57HhUw", sceFreeTypeOtUnknown_AVEEb57HhUw_nid_no_patch);

int APS5_VABI sceFreeTypeOtHasPSGlyphNames_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("x8y0qzLiYOU", sceFreeTypeOtHasPSGlyphNames_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_VW4GD_TaIUc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VW4GD-TaIUc", sceFreeTypeOtUnknown_VW4GD_TaIUc_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_GiuNQjoj4fs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GiuNQjoj4fs", sceFreeTypeOtUnknown_GiuNQjoj4fs_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_5qqbXOGlHw4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5qqbXOGlHw4", sceFreeTypeOtUnknown_5qqbXOGlHw4_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_4LY5pyZ5IQk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4LY5pyZ5IQk", sceFreeTypeOtUnknown_4LY5pyZ5IQk_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_zE0V_0aouZ4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zE0V+0aouZ4", sceFreeTypeOtUnknown_zE0V_0aouZ4_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_WIvTHYs7ZQQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WIvTHYs7ZQQ", sceFreeTypeOtUnknown_WIvTHYs7ZQQ_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_yrtsLmTfa48_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yrtsLmTfa48", sceFreeTypeOtUnknown_yrtsLmTfa48_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_tCMhTh7h8D4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tCMhTh7h8D4", sceFreeTypeOtUnknown_tCMhTh7h8D4_nid_no_patch);

int APS5_VABI sceFreeTypeOtNewSize_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zyezaHweY3U", sceFreeTypeOtNewSize_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_C_PpdS5zris_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("C-PpdS5zris", sceFreeTypeOtUnknown_C_PpdS5zris_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_g8eLxL9VAhU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("g8eLxL9VAhU", sceFreeTypeOtUnknown_g8eLxL9VAhU_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_OeNH41MA2Vs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("OeNH41MA2Vs", sceFreeTypeOtUnknown_OeNH41MA2Vs_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineCopy_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Bhm8Injl0KA", sceFreeTypeOtOutlineCopy_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineDecompose_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("++lgV9pRsO4", sceFreeTypeOtOutlineDecompose_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_BqGJsJcFYaM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BqGJsJcFYaM", sceFreeTypeOtUnknown_BqGJsJcFYaM_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_umIqupy8UU0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("umIqupy8UU0", sceFreeTypeOtUnknown_umIqupy8UU0_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_l6Cf9rb9fZQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("l6Cf9rb9fZQ", sceFreeTypeOtUnknown_l6Cf9rb9fZQ_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_BgUMBGvhATU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BgUMBGvhATU", sceFreeTypeOtUnknown_BgUMBGvhATU_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineGetBBox_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Aal-TIC4so", sceFreeTypeOtOutlineGetBBox_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_4RRjK_Wd1lU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4RRjK-Wd1lU", sceFreeTypeOtUnknown_4RRjK_Wd1lU_nid_no_patch);

int APS5_VABI sceFreeTypeOtOutlineGetOrientation_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("weVsBCT+TTQ", sceFreeTypeOtOutlineGetOrientation_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_QH2emcgeFck_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QH2emcgeFck", sceFreeTypeOtUnknown_QH2emcgeFck_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_NG7OgYsgcF8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NG7OgYsgcF8", sceFreeTypeOtUnknown_NG7OgYsgcF8_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_9km02mPqPJE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9km02mPqPJE", sceFreeTypeOtUnknown_9km02mPqPJE_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_0Lv6K1d47jg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Lv6K1d47jg", sceFreeTypeOtUnknown_0Lv6K1d47jg_nid_no_patch);

int APS5_VABI sceFreeTypeOtPropertyGet_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8fAMdZCnewA", sceFreeTypeOtPropertyGet_nid_no_patch);

int APS5_VABI sceFreeTypeOtPropertySet_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LsB538+IPrE", sceFreeTypeOtPropertySet_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_hz_J3hohXW8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hz-J3hohXW8", sceFreeTypeOtUnknown_hz_J3hohXW8_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_KVbmKCGGowg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KVbmKCGGowg", sceFreeTypeOtUnknown_KVbmKCGGowg_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_hF3stKUWbWg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hF3stKUWbWg", sceFreeTypeOtUnknown_hF3stKUWbWg_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_p5GNJaXO2yE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("p5GNJaXO2yE", sceFreeTypeOtUnknown_p5GNJaXO2yE_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_8cnVx1yH9_I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8cnVx1yH9-I", sceFreeTypeOtUnknown_8cnVx1yH9_I_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_HVSaZbxYi2s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HVSaZbxYi2s", sceFreeTypeOtUnknown_HVSaZbxYi2s_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_qAPXM5DN62Q_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qAPXM5DN62Q", sceFreeTypeOtUnknown_qAPXM5DN62Q_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_5ipBcLYVc2k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("5ipBcLYVc2k", sceFreeTypeOtUnknown_5ipBcLYVc2k_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_8A0cAU4yBVY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8A0cAU4yBVY", sceFreeTypeOtUnknown_8A0cAU4yBVY_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_z9Wsaf34JA4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("z9Wsaf34JA4", sceFreeTypeOtUnknown_z9Wsaf34JA4_nid_no_patch);

int APS5_VABI sceFreeTypeOtRequestSize_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PaDk9bdmdhU", sceFreeTypeOtRequestSize_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_Z8KG6700sw4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Z8KG6700sw4", sceFreeTypeOtUnknown_Z8KG6700sw4_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_uK7vGxAQVxM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uK7vGxAQVxM", sceFreeTypeOtUnknown_uK7vGxAQVxM_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_zetC8Tp1S8w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zetC8Tp1S8w", sceFreeTypeOtUnknown_zetC8Tp1S8w_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_DAmwG6U7YIc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DAmwG6U7YIc", sceFreeTypeOtUnknown_DAmwG6U7YIc_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_Yu9oXRVmTTo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Yu9oXRVmTTo", sceFreeTypeOtUnknown_Yu9oXRVmTTo_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_ihIHD8BTNxI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ihIHD8BTNxI", sceFreeTypeOtUnknown_ihIHD8BTNxI_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_yCEJIsjIOO8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yCEJIsjIOO8", sceFreeTypeOtUnknown_yCEJIsjIOO8_nid_no_patch);

int APS5_VABI sceFreeTypeOtSin_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("L1uHkWTazkI", sceFreeTypeOtSin_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_C7N2GuwqTA0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("C7N2GuwqTA0", sceFreeTypeOtUnknown_C7N2GuwqTA0_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_JgtSc0g3G6E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("JgtSc0g3G6E", sceFreeTypeOtUnknown_JgtSc0g3G6E_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_uDXTnGbAz44_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uDXTnGbAz44", sceFreeTypeOtUnknown_uDXTnGbAz44_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_iruPsrXCN7U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("iruPsrXCN7U", sceFreeTypeOtUnknown_iruPsrXCN7U_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerBeginSubPath_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("xj3tNJptfLI", sceFreeTypeOtStrokerBeginSubPath_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerConicTo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("TPKhRt1QIeE", sceFreeTypeOtStrokerConicTo_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerCubicTo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6+yXRlVrjbc", sceFreeTypeOtStrokerCubicTo_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerDone_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+RLa-6fbshA", sceFreeTypeOtStrokerDone_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerEndSubPath_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pBqh-Uh-gns", sceFreeTypeOtStrokerEndSubPath_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_2rcvKZfm5rI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2rcvKZfm5rI", sceFreeTypeOtUnknown_2rcvKZfm5rI_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_7cIS2cFG3PE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("7cIS2cFG3PE", sceFreeTypeOtUnknown_7cIS2cFG3PE_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerGetBorderCounts_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LfyUi-LkdCk", sceFreeTypeOtStrokerGetBorderCounts_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerGetCounts_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rzIWh7FJnmU", sceFreeTypeOtStrokerGetCounts_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerLineTo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("cKndiZnt0EA", sceFreeTypeOtStrokerLineTo_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerNew_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("eqGCaeizvWM", sceFreeTypeOtStrokerNew_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_RvFx7dv9ThU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RvFx7dv9ThU", sceFreeTypeOtUnknown_RvFx7dv9ThU_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerRewind_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Po8kyd3DlIw", sceFreeTypeOtStrokerRewind_nid_no_patch);

int APS5_VABI sceFreeTypeOtStrokerSet_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("B3uU5aDZn94", sceFreeTypeOtStrokerSet_nid_no_patch);

int APS5_VABI sceFreeTypeOtTan_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dojyaLKzKN0", sceFreeTypeOtTan_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_08_mhD_AAds_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("08-mhD-AAds", sceFreeTypeOtUnknown_08_mhD_AAds_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_64XMq6sDxUo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("64XMq6sDxUo", sceFreeTypeOtUnknown_64XMq6sDxUo_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_4xG87W9dTSg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4xG87W9dTSg", sceFreeTypeOtUnknown_4xG87W9dTSg_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_4_SScYDT5Pg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4+SScYDT5Pg", sceFreeTypeOtUnknown_4_SScYDT5Pg_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_2rmvbvOCyjY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2rmvbvOCyjY", sceFreeTypeOtUnknown_2rmvbvOCyjY_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_1jmC2QcpRl0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1jmC2QcpRl0", sceFreeTypeOtUnknown_1jmC2QcpRl0_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_HMcNe70D9TI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HMcNe70D9TI", sceFreeTypeOtUnknown_HMcNe70D9TI_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_6wJwUIVwAxg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6wJwUIVwAxg", sceFreeTypeOtUnknown_6wJwUIVwAxg_nid_no_patch);

int APS5_VABI sceFreeTypeOtVectorUnit_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("3WpTQrvGUcU", sceFreeTypeOtVectorUnit_nid_no_patch);

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

std::uint32_t APS5_VABI ft_highpow2_nid_postfix(std::uint32_t value) {
    return std::bit_floor(value);
}

int APS5_VABI sceFreeTypeOtUnknown_Ln9I6YfmYJU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Ln9I6YfmYJU", sceFreeTypeOtUnknown_Ln9I6YfmYJU_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_7cVMwHmN308_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("7cVMwHmN308", sceFreeTypeOtUnknown_7cVMwHmN308_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_AEU2tkjdZ2w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AEU2tkjdZ2w", sceFreeTypeOtUnknown_AEU2tkjdZ2w_nid_no_patch);

int APS5_VABI sceFreeTypeOtUnknown_uSNqxrARpEQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("uSNqxrARpEQ", sceFreeTypeOtUnknown_uSNqxrARpEQ_nid_no_patch);

}
