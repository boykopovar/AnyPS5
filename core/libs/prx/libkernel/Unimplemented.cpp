#include "prx/libkernel/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/KernelErrors.hpp"

namespace {

int StreamWriteNoEntry() {
    *__error_nid_postfix() = ENOENT;
    return SCE_KERNEL_ERROR_ENOENT;
}

}

extern "C" {

int APS5_VABI physhm_open_nid_postfix(void) {
    NotImplemented_nid_no_patch("physhm_open");
    return 0;
}

int APS5_VABI physhm_unlink_nid_postfix(void) {
    NotImplemented_nid_no_patch("physhm_unlink");
    return 0;
}

int APS5_VABI rfork_thread_nid_postfix(int flags, void* stack, int (APS5_VABI* function)(void*), void* argument) {
    NotImplemented_nid_no_patch("rfork_thread");
    return 0;
}

int APS5_VABI sceKernelRdup(int pid, int fd) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

APS5_EXPORT("aAQZSjb2fho", sceKernelUnknown16);
int APS5_VABI sceKernelUnknown16(void) {
    NotImplemented_nid_no_patch("aAQZSjb2fho");
    return 0;
}

int APS5_VABI sceKernelPrintBacktraceWithModuleInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelProtectDirectMemoryForPID(int64_t offset, size_t len, int prot, int pid) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelStreamWriteActivate(void) {
    return StreamWriteNoEntry();
}

int APS5_VABI sceKernelStreamWriteCreate(void) {
    return StreamWriteNoEntry();
}

int APS5_VABI sceKernelStreamWriteDeactivate(void) {
    return StreamWriteNoEntry();
}

int APS5_VABI sceKernelStreamWriteDelete(void) {
    return StreamWriteNoEntry();
}

int APS5_VABI sceKernelStreamWriteRead(void) {
    return StreamWriteNoEntry();
}

int APS5_VABI sceKernelStreamWriteStat(void) {
    return StreamWriteNoEntry();
}

int APS5_VABI sceKernelStreamWriteWrite(void) {
    return StreamWriteNoEntry();
}

}

extern "C" {

int APS5_VABI __Ux86_64_setcontext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
