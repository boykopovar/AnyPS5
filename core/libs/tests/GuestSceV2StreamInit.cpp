#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* label) {
    if (condition) return;
    std::fprintf(stderr, "sce v2 stream init: %s\n", label);
    std::abort();
}

}

int main() {
    std::int32_t& wide = _ZNSt8__sce_v26_Winit9_Init_cntE_nid_postfix;
    std::int32_t& narrow = _ZNSt8__sce_v28ios_base4Init9_Init_cntE_nid_postfix;
    std::uint64_t self[2] {};
    Require(wide == -1 && narrow == -1, "both counters start at -1");

    _ZNSt8__sce_v26_WinitC1Ev_nid_postfix(self);
    Require(wide == 1, "the first _Winit raises -1 to 1");
    _ZNSt8__sce_v26_WinitC2Ev_nid_postfix(self);
    Require(wide == 2, "a second _Winit counts up");
    _ZNSt8__sce_v26_WinitD1Ev_nid_postfix(self);
    _ZNSt8__sce_v26_WinitD2Ev_nid_postfix(self);
    Require(wide == 0, "the destructors count down to 0");
    _ZNSt8__sce_v26_WinitD1Ev_nid_postfix(self);
    Require(wide == -1, "a destructor below 0 keeps counting down");
    wide = -5;
    _ZNSt8__sce_v26_WinitC1Ev_nid_postfix(self);
    Require(wide == 1, "a negative count restarts at 1");
    Require(narrow == -1, "_Winit leaves ios_base::Init alone");

    Require(_ZNSt8__sce_v28ios_base4Init14_Init_cnt_funcEv_nid_postfix() == &narrow, "_Init_cnt_func returns the counter");
    _ZNSt8__sce_v28ios_base4Init10_Init_ctorEPS1__nid_postfix(self);
    Require(narrow == 1, "the first _Init_ctor raises -1 to 1");
    _ZNSt8__sce_v28ios_base4Init10_Init_ctorEPS1__nid_postfix(self);
    _ZNSt8__sce_v28ios_base4Init10_Init_dtorEPS1__nid_postfix(self);
    _ZNSt8__sce_v28ios_base4Init10_Init_dtorEPS1__nid_postfix(self);
    Require(narrow == 0, "_Init_dtor counts down to 0");
    _ZNSt8__sce_v28ios_base4Init10_Init_dtorEPS1__nid_postfix(self);
    Require(narrow == -1, "_Init_dtor below 0 keeps counting down");
    Require(wide == 1, "ios_base::Init leaves _Winit alone");
    return 0;
}
