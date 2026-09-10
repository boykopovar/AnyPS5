#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include "src/General.cpp"
#include "src/specifics/gcc/StackGuard.cpp"
#include "src/specifics/linux/DlIteratePhdr.cpp"
#include "src/Strings.cpp"
#include "src/Math.cpp"
#include "src/Time.cpp"
#include "src/FileAndHeap.cpp"
#include "src/JumpBuffer.cpp"
#include "src/Formatting.cpp"
#include "src/RuntimeSupport.cpp"
#include "src/LocaleSupport.cpp"
#include "src/Process.cpp"
#include "src/CxxAbiSupport.cpp"
#include "src/exception/Exports.cpp"

uint32_t Need_sceLibc = 1;

extern "C" {

    void APS5_VABI init_env_nid_postfix(const InitEnvParams* params) {
        (void)params;
        NotImplemented_nid_no_patch(__func__);
    }

    void APS5_VABI catchReturnFromMain_nid_postfix(int status) {
        (void)status;
        NotImplemented_nid_no_patch(__func__);
    }

    int APS5_VABI cxa_atexit_nid_postfix(void (*func)(void*), void* arg, void* d) {
        (void)func;
        (void)arg;
        (void)d;
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }

    void APS5_VABI cxa_finalize_nid_postfix(void* d) {
        NotImplemented_nid_no_patch(__func__);
        (void)d;
    }

    int APS5_VABI std_execute_once_nid_postfix(int* flag, int (*func)(void*, void*, void**), void* arg) {
        (void)flag;
        (void)func;
        (void)arg;
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }

    void APS5_VABI LibcHeapGetTraceInfo_nid_postfix(LibcHeapInfo* info) {
        (void)info;
        NotImplemented_nid_no_patch(__func__);
    }

    int APS5_VABI LibcInternalExtCxaThreadAtexit_nid_postfix(void (*destructor)(void*), void* object, void* module_id) {
        (void)destructor;
        (void)object;
        (void)module_id;
        NotImplemented_nid_no_patch(__func__);
        return 0;
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

}
