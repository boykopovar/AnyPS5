#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HeapDiagnostics.hpp"

uint32_t Need_sceLibc = 1;

extern "C" {

    int APS5_VABI _ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_postfix(int*, int (APS5_VABI *)(void*, void*, void**), void*);

    int APS5_VABI std_execute_once_nid_postfix(int* flag, int (APS5_VABI *func)(void*, void*, void**), void* arg) {
        return _ZSt13_Execute_onceRSt9once_flagPFiPvS1_PS1_ES1__nid_postfix(flag, func, arg);
    }

    void APS5_VABI LibcHeapGetTraceInfo_nid_postfix(LibcHeapInfo* info) {
        LibcHeapTraceInfo_nid_no_patch(info);
    }

// Dead import of Cyberpunk 2077 (PPSA04029): no call sites, but the
// Windows loader resolves imports strictly, so it must be present.
int APS5_VABI _ZSt14_Atomic_assertPKcS0__nid_postfix() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

// Live Cyberpunk 2077 import used in an fopen/fseek/ftell-like file-size
// idiom as (handle, 0, 2); returning 0 reports success.
int APS5_VABI vsnprintf_s_nid_postfix() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

// Live Cyberpunk 2077 import used as (handle, 0, 0) in the same file-size
// idiom; returning 0 reports success.
int APS5_VABI vsscanf_s_nid_postfix() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


APS5_EXPORT("Ye20uNnlglA", libcCyberUnknown02);
std::uint64_t APS5_VABI libcCyberUnknown02(void) {
    NotImplemented_nid_no_patch("Ye20uNnlglA");
    return 0;
}

std::uint64_t APS5_VABI _Mtx_destroy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI _Cnd_destroy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI _Iswctype_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("H+8UBOwfScI", libcCyberUnknown08);
std::uint64_t APS5_VABI libcCyberUnknown08(void) {
    NotImplemented_nid_no_patch("H+8UBOwfScI");
    return 0;
}

std::uint64_t APS5_VABI _WStoul_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI _Cnd_init_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI _Cnd_broadcast_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI _Mtx_init_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI _Mtx_unlock_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI _Mtx_lock_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI _Cnd_wait_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI swscanf_s_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
