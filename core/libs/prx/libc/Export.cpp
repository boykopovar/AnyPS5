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


APS5_EXPORT("DiGVep5yB5w", libcCyberUnknown01);
std::uint64_t APS5_VABI libcCyberUnknown01(void) {
    NotImplemented_nid_no_patch("DiGVep5yB5w");
    return 0;
}

APS5_EXPORT("Ye20uNnlglA", libcCyberUnknown02);
std::uint64_t APS5_VABI libcCyberUnknown02(void) {
    NotImplemented_nid_no_patch("Ye20uNnlglA");
    return 0;
}

APS5_EXPORT("5Lf51jvohTQ", libcCyberUnknown03);
std::uint64_t APS5_VABI libcCyberUnknown03(void) {
    NotImplemented_nid_no_patch("5Lf51jvohTQ");
    return 0;
}

APS5_EXPORT("7yMFgcS8EPA", libcCyberUnknown05);
std::uint64_t APS5_VABI libcCyberUnknown05(void) {
    NotImplemented_nid_no_patch("7yMFgcS8EPA");
    return 0;
}

APS5_EXPORT("CyXs2l-1kNA", libcCyberUnknown07);
std::uint64_t APS5_VABI libcCyberUnknown07(void) {
    NotImplemented_nid_no_patch("CyXs2l-1kNA");
    return 0;
}

APS5_EXPORT("H+8UBOwfScI", libcCyberUnknown08);
std::uint64_t APS5_VABI libcCyberUnknown08(void) {
    NotImplemented_nid_no_patch("H+8UBOwfScI");
    return 0;
}

APS5_EXPORT("JhVR7D4Ax6Y", libcCyberUnknown09);
std::uint64_t APS5_VABI libcCyberUnknown09(void) {
    NotImplemented_nid_no_patch("JhVR7D4Ax6Y");
    return 0;
}

APS5_EXPORT("SreZybSRWpU", libcCyberUnknown10);
std::uint64_t APS5_VABI libcCyberUnknown10(void) {
    NotImplemented_nid_no_patch("SreZybSRWpU");
    return 0;
}

APS5_EXPORT("VsP3daJgmVA", libcCyberUnknown11);
std::uint64_t APS5_VABI libcCyberUnknown11(void) {
    NotImplemented_nid_no_patch("VsP3daJgmVA");
    return 0;
}

APS5_EXPORT("YaHc3GS7y7g", libcCyberUnknown12);
std::uint64_t APS5_VABI libcCyberUnknown12(void) {
    NotImplemented_nid_no_patch("YaHc3GS7y7g");
    return 0;
}

APS5_EXPORT("gTuXQwP9rrs", libcCyberUnknown13);
std::uint64_t APS5_VABI libcCyberUnknown13(void) {
    NotImplemented_nid_no_patch("gTuXQwP9rrs");
    return 0;
}

APS5_EXPORT("iS4aWbUonl0", libcCyberUnknown14);
std::uint64_t APS5_VABI libcCyberUnknown14(void) {
    NotImplemented_nid_no_patch("iS4aWbUonl0");
    return 0;
}

APS5_EXPORT("vEaqE-7IZYc", libcCyberUnknown16);
std::uint64_t APS5_VABI libcCyberUnknown16(void) {
    NotImplemented_nid_no_patch("vEaqE-7IZYc");
    return 0;
}
}
