#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"

uint32_t Need_sceLibc = 1;

extern "C" {

    int APS5_VABI std_execute_once_nid_postfix(int* flag, int (*func)(void*, void*, void**), void* arg) {
        (void)flag;
        (void)func;
        (void)arg;
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }

    void APS5_VABI LibcHeapGetTraceInfo_nid_postfix(LibcHeapInfo* info) {
        LibcHeapTraceInfo_nid_no_patch(info);
    }

    int APS5_VABI LibcHeapErrorReportForGame_nid_postfix(
        uint64_t msp, uint64_t ptr, uint64_t error,
        uint64_t arg3, uint64_t arg4, uint64_t arg5
    ) {
        (void)msp; (void)ptr; (void)error;
        (void)arg3; (void)arg4; (void)arg5;
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }

APS5_EXPORT("Pu0Ecyk-7FU", libcUnknown_Pu0Ecyk_M7FU);
int APS5_VABI libcUnknown_Pu0Ecyk_M7FU() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

// Dead import of Cyberpunk 2077 (PPSA04029): no call sites, but the
// Windows loader resolves imports strictly, so it must be present.
APS5_EXPORT("u2tMGOLaqnE", libcUnknown_u2tMGOLaqnE);
int APS5_VABI libcUnknown_u2tMGOLaqnE() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

// Live float->float import of Cyberpunk 2077 (unnamed); callers pass one
// float in xmm0 and consume the result, so return 0.0f instead of integer 0.
APS5_EXPORT("DQbtGaBKlaw", libcCyberUnknown17);
float APS5_VABI libcCyberUnknown17(float x) {
 (void)x;
 NotImplemented_nid_no_patch("DQbtGaBKlaw");
 return 0.0f;
}

// Live Cyberpunk 2077 import used in an fopen/fseek/ftell-like file-size
// idiom as (handle, 0, 2); returning 0 reports success.
APS5_EXPORT("rWSuTWY2JN0", libcCyberUnknown18);
int APS5_VABI libcCyberUnknown18(void) {
 NotImplemented_nid_no_patch("rWSuTWY2JN0");
 return 0;
}

// Live Cyberpunk 2077 import used as (handle, 0, 0) in the same file-size
// idiom; returning 0 reports success.
APS5_EXPORT("tfNbpqL3D0M", libcCyberUnknown19);
int APS5_VABI libcCyberUnknown19(void) {
 NotImplemented_nid_no_patch("tfNbpqL3D0M");
 return 0;
}


APS5_EXPORT("2gbcltk3swE", libcCyberUnknown00);
std::uint64_t APS5_VABI libcCyberUnknown00(void) {
    NotImplemented_nid_no_patch("2gbcltk3swE");
    return 0;
}

APS5_EXPORT("DiGVep5yB5w", libcCyberUnknown01);
std::uint64_t APS5_VABI libcCyberUnknown01(void) {
    NotImplemented_nid_no_patch("DiGVep5yB5w");
    return 0;
}

APS5_EXPORT("79s2tnYQI6I", libcCyberUnknown04);
std::uint64_t APS5_VABI libcCyberUnknown04(void) {
    NotImplemented_nid_no_patch("79s2tnYQI6I");
    return 0;
}

APS5_EXPORT("Cj+Fw5q1tUo", libcCyberUnknown06);
std::uint64_t APS5_VABI libcCyberUnknown06(void) {
    NotImplemented_nid_no_patch("Cj+Fw5q1tUo");
    return 0;
}

APS5_EXPORT("qdGFBoLVNKI", libcCyberUnknown15);
std::uint64_t APS5_VABI libcCyberUnknown15(void) {
    NotImplemented_nid_no_patch("qdGFBoLVNKI");
    return 0;
}
}
