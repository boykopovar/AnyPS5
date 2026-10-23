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
 return 0;
}


APS5_EXPORT("H2e8t5ScQGc", libc___cxa_finalize);
void APS5_VABI libc___cxa_finalize(void* dsoHandle) {
    CxaFinalize_nid_no_patch(dsoHandle);
}

int APS5_VABI _ZSt4setwi_nid_postfix(int n) { return n; }
int APS5_VABI _ZSt12setprecisioni_nid_postfix(int n) { return n; }
uint64_t APS5_VABI _Thrd_id_nid_postfix() { return 1; }
[[noreturn]] void APS5_VABI _ZSt26__throw_bad_variant_accessv_nid_postfix() {
    throw std::runtime_error("bad_variant_access");
}

void APS5_VABI _ZNSt8__sce_v26_ThrowERKNS_12future_errorE_nid_postfix(void*) { throw std::runtime_error("future_error"); }
void APS5_VABI _ZNSt8__sce_v214_Cnd_broadcastEPP12pthread_cond_nid_postfix(void*) {}
void APS5_VABI _ZNSt8__sce_v228_Cnd_register_at_thread_exitEPP12pthread_condPP13pthread_mutexPi_nid_postfix(void*, void*, void*) {}
void APS5_VABI _ZNSt8__sce_v212_Thrd_detachEP7pthread_nid_postfix(void*) {}

void APS5_VABI _ZSt19_Throw_future_errorRKSt10error_code_nid_postfix(void*) { throw std::runtime_error("future_error"); }
void APS5_VABI _ZSt22_Throw_bad_node_accessPKc_nid_postfix(const char* msg) { throw std::runtime_error(msg ? msg : "bad_node_access"); }
void APS5_VABI _ZSt25_Rethrow_future_exceptionSt13exception_ptr_nid_postfix(void*) { throw std::runtime_error("future_exception"); }
void APS5_VABI _ZSt26_Throw_bad_optional_accessv_nid_postfix() { throw std::runtime_error("bad_optional_access"); }
void APS5_VABI _ZSt6_ThrowRKSt20bad_array_new_length_nid_postfix(void*) { throw std::runtime_error("bad_array_new_length"); }
void APS5_VABI _ZSt6_ThrowRKSt8bad_cast_nid_postfix(void*) { throw std::runtime_error("bad_cast"); }
void* APS5_VABI __std_find_trivial_1_nid_postfix(void*, void*, uint8_t) { return nullptr; }
void APS5_VABI __std_reverse_trivially_swappable_1_nid_postfix(void*, void*) {}
}
