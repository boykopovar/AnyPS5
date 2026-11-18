#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI _Restore_state_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Rint_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Save_state_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Scanf_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Setgloballocale_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Sincos_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Skip_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Strcollx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

char* APS5_VABI _Strerror_nid_postfix(int error, char* buffer) {
    if (const char* text = StrerrorText_nid_no_patch(error)) return const_cast<char*>(text);
    char local[22];
    std::snprintf(local, sizeof(local), "error_code=0x%08x", static_cast<unsigned>(error));
    if (!buffer) {
        thread_local char ownBuffer[sizeof(local)];
        buffer = ownBuffer;
    }
    if (!GuestCopyOut_nid_no_patch(buffer, local, sizeof(local))) throw std::invalid_argument("_Strerror: an unwritable buffer");
    return buffer;
}

int APS5_VABI _Strxfrmx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Tan_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Tgamma_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Ttotm_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Tzoff_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Unlock_spin_lock_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WFrprep_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WFwprep_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WGenld_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WGetfld_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WGetfloat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WGetint_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WGetstr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WLdtob_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WLitob_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WPrintf_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WPutfld_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WPutstr_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WPuttxt_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _WScanf_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Wcscollx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Wcsftime_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Wcsxfrmx_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _Wctomb_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_yc_vi84_2aE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("yc-vi84-2aE", sceLibcInternalUnknown_yc_vi84_2aE_nid_no_patch);

int APS5_VABI _ZNKSt13bad_exception8_DoraiseEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt19istreambuf_iteratorIcSt11char_traitsIcEE5equalERKS2__nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibcInternalUnknown_jZmLD_ASDto_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("jZmLD-ASDto", sceLibcInternalUnknown_jZmLD_ASDto_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_n9_NJEULZ_0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("n9-NJEULZ-0", sceLibcInternalUnknown_n9_NJEULZ_0_nid_no_patch);

int APS5_VABI sceLibcInternalUnknown_pSQz254t3ug_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("pSQz254t3ug", sceLibcInternalUnknown_pSQz254t3ug_nid_no_patch);

int APS5_VABI _ZNKSt7_MpunctIcE11do_groupingEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE13do_neg_formatEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE13do_pos_formatEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE14do_curr_symbolEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE14do_frac_digitsEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNKSt7_MpunctIcE16do_decimal_pointEv_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
