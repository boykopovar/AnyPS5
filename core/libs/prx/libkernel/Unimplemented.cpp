#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceKernelAioSetParam(void* param, int schedulingWindowSize, int delayedCountLimit, uint32_t enableSplit, uint32_t splitSize, uint32_t splitChunkSize) {
    (void)param;
    (void)schedulingWindowSize;
    (void)delayedCountLimit;
    (void)enableSplit;
    (void)splitSize;
    (void)splitChunkSize;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAddFileEvent(intptr_t queue, int descriptor, int watch, void* userData) {
    (void)queue;
    (void)descriptor;
    (void)watch;
    (void)userData;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelDeleteFileEvent(intptr_t queue, int descriptor) {
    (void)queue;
    (void)descriptor;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI fstat_nid_postfix(int descriptor, void* status) {
    (void)descriptor;
    (void)status;
    NotImplemented_nid_no_patch("fstat");
    return 0;
}

int64_t APS5_VABI pwrite_nid_postfix(int descriptor, void* buffer, size_t length, int64_t offset) {
    (void)descriptor;
    (void)buffer;
    (void)length;
    (void)offset;
    NotImplemented_nid_no_patch("pwrite");
    return 0;
}

int APS5_VABI sceKernelLwfsSetAttribute(int descriptor, int flags) {
    (void)descriptor;
    (void)flags;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelLwfsAllocateBlock(int descriptor, int64_t size) {
    (void)descriptor;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelLwfsTrimBlock(int descriptor, int64_t size) {
    (void)descriptor;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int64_t APS5_VABI sceKernelLwfsLseek(int descriptor, int64_t offset, int whence) {
    (void)descriptor;
    (void)offset;
    (void)whence;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int64_t APS5_VABI sceKernelLwfsWrite(int descriptor, void* buffer, size_t nbytes) {
    (void)descriptor;
    (void)buffer;
    (void)nbytes;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelAllocateDirectMemory2(int64_t searchStart, int64_t searchEnd, size_t length, size_t alignment, int memoryType, int flags, int64_t* physicalAddressOut) {
    (void)searchStart;
    (void)searchEnd;
    (void)length;
    (void)alignment;
    (void)memoryType;
    (void)flags;
    (void)physicalAddressOut;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI nmount_nid_postfix(void* iov, uint32_t niov, int flags) {
    (void)iov;
    (void)niov;
    (void)flags;
    NotImplemented_nid_no_patch("nmount");
    return 0;
}

int APS5_VABI unmount_nid_postfix(uint8_t* path, int flags) {
    (void)path;
    (void)flags;
    NotImplemented_nid_no_patch("unmount");
    return 0;
}

int APS5_VABI kill_nid_postfix(int pid, int signal) {
    (void)pid;
    (void)signal;
    NotImplemented_nid_no_patch("kill");
    return 0;
}

int APS5_VABI getdtablesize_nid_postfix(void) {
    NotImplemented_nid_no_patch("getdtablesize");
    return 0;
}

int APS5_VABI scePthreadGetcpuclockid(intptr_t thread, int* clockId) {
    (void)thread;
    (void)clockId;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadMutexattrGetprotocol(intptr_t* attr, int* protocol) {
    (void)attr;
    (void)protocol;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadMutexattrGetprioceiling(intptr_t* attr, int* prio) {
    (void)attr;
    (void)prio;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadMutexattrSetprioceiling(intptr_t* attr, int prio) {
    (void)attr;
    (void)prio;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePthreadRwlockattrGettype(intptr_t* attr, int* type) {
    (void)attr;
    (void)type;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelGetProsperoSystemSwVersion(void* version) {
    (void)version;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpConfigDumpMode(uint32_t mode) {
    (void)mode;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpGetStopInfoGpu(void* info, size_t size) {
    (void)info;
    (void)size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpGetThreadContextInfo(void* threadContextInfo, size_t threadContextInfoSize) {
    (void)threadContextInfo;
    (void)threadContextInfoSize;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceCoredumpWriteUserData() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI __tls_get_addr_nid_postfix(void) {
    NotImplemented_nid_no_patch("vNe1w4diLCs");
    return 0;
}

// Canonical lib is libScePosix (dead import of Cyberpunk 2077): 35 of its
// 36 sibling imports resolve to libkernel, and libScePosix is not in the
// game's NEEDED list so only a NEEDED module can satisfy the loader here.
int APS5_VABI pthread_cancel_nid_postfix(void) {
    NotImplemented_nid_no_patch("0D4-FVvEikw");
    return 0;
}
}
