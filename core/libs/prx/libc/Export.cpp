#include <cstdint>
#include <cstddef>
#include <stdexcept>
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
    throw std::runtime_error("_ZSt14_Atomic_assert: unknown signature");
}

int APS5_VABI vsnprintf_s_nid_postfix() {
    throw std::runtime_error("vsnprintf_s: unknown signature");
}

int APS5_VABI vsscanf_s_nid_postfix() {
    throw std::runtime_error("vsscanf_s: unknown signature");
}

APS5_EXPORT("Ye20uNnlglA", libcCyberUnknown02);
std::uint64_t APS5_VABI libcCyberUnknown02(void) {
    throw std::runtime_error("libcCyberUnknown02 (Ye20uNnlglA): unknown signature");
}

std::uint64_t APS5_VABI _Mtx_destroy_nid_postfix() {
    throw std::runtime_error("_Mtx_destroy: unknown signature");
}

std::uint64_t APS5_VABI _Cnd_destroy_nid_postfix() {
    throw std::runtime_error("_Cnd_destroy: unknown signature");
}

std::uint64_t APS5_VABI _Iswctype_nid_postfix() {
    throw std::runtime_error("_Iswctype: unknown signature");
}

APS5_EXPORT("H+8UBOwfScI", libcCyberUnknown08);
std::uint64_t APS5_VABI libcCyberUnknown08(void) {
    throw std::runtime_error("libcCyberUnknown08 (H+8UBOwfScI): unknown signature");
}

std::uint64_t APS5_VABI _WStoul_nid_postfix() {
    throw std::runtime_error("_WStoul: unknown signature");
}

std::uint64_t APS5_VABI _Cnd_init_nid_postfix() {
    throw std::runtime_error("_Cnd_init: unknown signature");
}

std::uint64_t APS5_VABI _Cnd_broadcast_nid_postfix() {
    throw std::runtime_error("_Cnd_broadcast: unknown signature");
}

std::uint64_t APS5_VABI _Mtx_init_nid_postfix() {
    throw std::runtime_error("_Mtx_init: unknown signature");
}

std::uint64_t APS5_VABI _Mtx_unlock_nid_postfix() {
    throw std::runtime_error("_Mtx_unlock: unknown signature");
}

std::uint64_t APS5_VABI _Mtx_lock_nid_postfix() {
    throw std::runtime_error("_Mtx_lock: unknown signature");
}

std::uint64_t APS5_VABI _Cnd_wait_nid_postfix() {
    throw std::runtime_error("_Cnd_wait: unknown signature");
}
}
