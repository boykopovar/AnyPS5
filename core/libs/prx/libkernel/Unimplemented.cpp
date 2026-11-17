#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI physhm_open_nid_postfix(void) {
    return SCE_KERNEL_ERROR_ENOSYS;
}

int APS5_VABI physhm_unlink_nid_postfix(void) {
    return SCE_KERNEL_ERROR_ENOSYS;
}

int APS5_VABI rfork_thread_nid_postfix(int flags, void* stack, int (APS5_VABI* function)(void*), void* argument) {
    return SCE_KERNEL_ERROR_ENOSYS;
}

int APS5_VABI sceKernelRdup(int pid, int fd) {
    return SCE_KERNEL_ERROR_ENOSYS;
}

APS5_EXPORT("aAQZSjb2fho", sceKernelUnknown16);
int APS5_VABI sceKernelUnknown16(void) {
    return 0;
}

int APS5_VABI sceKernelPrintBacktraceWithModuleInfo(void) {
    return 0;
}

int APS5_VABI sceKernelProtectDirectMemoryForPID(int64_t offset, size_t len, int prot, int pid) {
    return SCE_KERNEL_ERROR_ENOSYS;
}

}

extern "C" {

int APS5_VABI __Ux86_64_setcontext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
