#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

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

    void init_env_nid_postfix(const InitEnvParams* params) {
        (void)params;
        NotImplemented_nid_no_patch(__func__);
    }

    void catchReturnFromMain_nid_postfix(int status) {
        (void)status;
        NotImplemented_nid_no_patch(__func__);
    }

    int cxa_atexit_nid_postfix(void (*func)(void*), void* arg, void* d) {
        (void)func;
        (void)arg;
        (void)d;
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }

    void cxa_finalize_nid_postfix(void* d) {
        NotImplemented_nid_no_patch(__func__);
        (void)d;
    }

    int std_execute_once_nid_postfix(int* flag, int (*func)(void*, void*, void**), void* arg) {
        (void)flag;
        (void)func;
        (void)arg;
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }

    void LibcHeapGetTraceInfo_nid_postfix(LibcHeapInfo* info) {
        (void)info;
        NotImplemented_nid_no_patch(__func__);
    }

    int LibcInternalExtCxaThreadAtexit_nid_postfix(void (*destructor)(void*), void* object, void* module_id) {
        (void)destructor;
        (void)object;
        (void)module_id;
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }

    int LibcHeapErrorReportForGame_nid_postfix(
        uint64_t msp, uint64_t ptr, uint64_t error,
        uint64_t arg3, uint64_t arg4, uint64_t arg5
    ) {
        (void)msp; (void)ptr; (void)error;
        (void)arg3; (void)arg4; (void)arg5;
        NotImplemented_nid_no_patch(__func__);
        return 0;
    }

}
