#include "prx/libc/include/Api.hpp"

#include <cstdint>

namespace {

void Increment(std::int32_t& count) { count = (count < 0 ? 0 : count) + 1; }

}

extern "C" {

std::int32_t _ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix = -1;
std::int32_t _ZNSt8__sce_v28ios_base4Init9_Init_cntE_nid_postfix = -1;

void APS5_VABI _ZNSt8__sce_v26_WinitC1Ev_nid_postfix(void*) { Increment(_ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix); }
void APS5_VABI _ZNSt8__sce_v26_WinitC2Ev_nid_postfix(void*) { Increment(_ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix); }
void APS5_VABI _ZNSt8__sce_v26_WinitD1Ev_nid_postfix(void*) { --_ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix; }
void APS5_VABI _ZNSt8__sce_v26_WinitD2Ev_nid_postfix(void*) { --_ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix; }

void APS5_VABI _ZNSt8__sce_v28ios_base4Init10_Init_ctorEPS1__nid_postfix(void*) { Increment(_ZNSt8__sce_v28ios_base4Init9_Init_cntE_nid_postfix); }
void APS5_VABI _ZNSt8__sce_v28ios_base4Init10_Init_dtorEPS1__nid_postfix(void*) { --_ZNSt8__sce_v28ios_base4Init9_Init_cntE_nid_postfix; }
std::int32_t* APS5_VABI _ZNSt8__sce_v28ios_base4Init14_Init_cnt_funcEv_nid_postfix() { return &_ZNSt8__sce_v28ios_base4Init9_Init_cntE_nid_postfix; }

}
