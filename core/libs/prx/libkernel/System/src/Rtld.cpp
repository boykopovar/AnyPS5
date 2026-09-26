#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/Pthread/include/ThreadLifecycle.hpp"

extern "C" {

void APS5_VABI sceKernelRtldSetApplicationHeapAPI(void* api[]) {
    ApplicationHeapRegister_nid_no_patch(api);
}

int APS5_VABI sceKernelRtldThreadAtexitDecrement(uint64_t* c) {
 (void)c;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelRtldThreadAtexitIncrement(uint64_t* c) {
 (void)c;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI sceKernelSetThreadAtexitCount(get_thread_atexit_count_func_t func) {
    ThreadLifecycle::SetThreadAtexitCount(func);
}

void APS5_VABI sceKernelSetThreadAtexitReport(thread_atexit_report_func_t func) {
    ThreadLifecycle::SetThreadAtexitReport(func);
}

void APS5_VABI sceKernelSetThreadDtors(thread_dtors_func_t dtors) {
    ThreadLifecycle::SetThreadDtors(dtors);
}

}

extern "C" {

void APS5_VABI _sceKernelRtldSetApplicationHeapAPI_nid_postfix(void* api[]) {
    ApplicationHeapRegister_nid_no_patch(api);
}

int APS5_VABI _sceKernelRtldThreadAtexitDecrement_nid_postfix(std::uint64_t* counter) {
    (void)counter;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _sceKernelRtldThreadAtexitIncrement_nid_postfix(std::uint64_t* counter) {
    (void)counter;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI _sceKernelSetThreadAtexitCount_nid_postfix(get_thread_atexit_count_func_t callback) {
    ThreadLifecycle::SetThreadAtexitCount(callback);
}

void APS5_VABI _sceKernelSetThreadAtexitReport_nid_postfix(thread_atexit_report_func_t callback) {
    ThreadLifecycle::SetThreadAtexitReport(callback);
}

void APS5_VABI _sceKernelSetThreadDtors_nid_postfix(thread_dtors_func_t callback) {
    ThreadLifecycle::SetThreadDtors(callback);
}

}
