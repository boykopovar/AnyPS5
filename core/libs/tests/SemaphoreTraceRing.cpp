#include "prx/libkernel/Semaphore/include/Semaphore.hpp"
#include "prx/libc/include/SyncTrace.hpp"
#include <cstring>

extern "C" void KernelTraceWait_nid_postfix(const char*, const void*, std::uint64_t, bool) {}
extern "C" Pthread APS5_VABI scePthreadSelf() { return nullptr; }
extern "C" int APS5_VABI scePthreadGetprio(Pthread, int*) { return -1; }

int main(int argc, char** argv) {
    KernelSema sem = nullptr;
    if (sceKernelCreateSema(&sem, "ring-trace", 0, 1, 2, nullptr) != KERNEL_SEMA_OK) return 1;
    if (sceKernelWaitSema(sem, 1, nullptr) != KERNEL_SEMA_OK) return 2;
    if (sceKernelSignalSema(sem, 1) != KERNEL_SEMA_OK) return 3;
    if (sceKernelPollSema(sem, 1) != KERNEL_SEMA_OK) return 4;
    if (sceKernelSignalSema(sem, 1) != KERNEL_SEMA_OK) return 5;
    if (sceKernelCancelSema(sem, 0, nullptr) != KERNEL_SEMA_OK) return 6;
    if (sceKernelDeleteSema(sem) != KERNEL_SEMA_OK) return 7;
    if (argc > 1 && std::strcmp(argv[1], "crash") == 0) __builtin_trap();
    SyncTraceReportRing_nid_no_patch();
    return 0;
}
