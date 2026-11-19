#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI _WPutfld_nid_postfix();
int APS5_VABI _Save_state_nid_postfix();
char* APS5_VABI _Strerror_nid_postfix(int error, char* buffer);
void* APS5_VABI _Ttotm_nid_postfix(void* time, std::int64_t seconds, int isdst);
int APS5_VABI _Restore_state_nid_postfix();
int APS5_VABI _WGenld_nid_postfix();
int APS5_VABI _ZNKSt7_MpunctIcE14do_frac_digitsEv_nid_postfix();
int APS5_VABI _Strxfrmx_nid_postfix();
int APS5_VABI _Tzoff_nid_postfix();
int APS5_VABI _ZNKSt7_MpunctIcE13do_pos_formatEv_nid_postfix();
int APS5_VABI _WPutstr_nid_postfix();
int APS5_VABI _Skip_nid_postfix();
int APS5_VABI _WPuttxt_nid_postfix();
int APS5_VABI _WGetfloat_nid_postfix();
int APS5_VABI _Wcsftime_nid_postfix();
int APS5_VABI _Rint_nid_postfix();
int APS5_VABI _WFrprep_nid_postfix();
int APS5_VABI _ZNKSt7_MpunctIcE14do_curr_symbolEv_nid_postfix();
int APS5_VABI _WLitob_nid_postfix();
int APS5_VABI _WGetfld_nid_postfix();
int APS5_VABI _ZNKSbIwSt11char_traitsIwESaIwEE5_XlenEv_nid_postfix();
void APS5_VABI _Unlock_spin_lock_nid_postfix(volatile char* lock, int mode = 0);
int APS5_VABI _ZNKSt7_MpunctIcE11do_groupingEv_nid_postfix();
int APS5_VABI _Setgloballocale_nid_postfix();
int APS5_VABI _Wctomb_nid_postfix();
int APS5_VABI _Wcsxfrmx_nid_postfix();
int APS5_VABI _WGetint_nid_postfix();
int APS5_VABI _WScanf_nid_postfix();
int APS5_VABI _WFwprep_nid_postfix();
int APS5_VABI _Sincos_nid_postfix();
int APS5_VABI _ZNKSt13bad_exception8_DoraiseEv_nid_postfix();
int APS5_VABI _WLdtob_nid_postfix();
int APS5_VABI _WPrintf_nid_postfix();
int APS5_VABI _WGetstr_nid_postfix();
int APS5_VABI _Scanf_nid_postfix();
int APS5_VABI _Tan_nid_postfix();
int APS5_VABI _ZNKSt7_MpunctIcE16do_decimal_pointEv_nid_postfix();
int APS5_VABI _ZNKSt19istreambuf_iteratorIcSt11char_traitsIcEE5equalERKS2__nid_postfix();
int APS5_VABI _ZNKSt7_MpunctIcE13do_neg_formatEv_nid_postfix();
int APS5_VABI _Strcollx_nid_postfix();
int APS5_VABI _Wcscollx_nid_postfix();
int APS5_VABI _Tgamma_nid_postfix();
int APS5_VABI _ZNKSbIwSt11char_traitsIwESaIwEE5_XranEv_nid_postfix();

}
