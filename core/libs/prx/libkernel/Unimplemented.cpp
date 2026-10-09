#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

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

int32_t APS5_VABI sceKernelSettimeofday(const KernelTimeval* tv, const KernelTimezone* tz) {
    (void)tv;
    (void)tz;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelError(int32_t posixError) {
    (void)posixError;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelGetAllowedSdkVersionOnSystem(int32_t* ver) {
    (void)ver;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelGetSystemSwVersion(void* version) {
    (void)version;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI get_authinfo(int32_t pid, void* info) {
    (void)pid;
    (void)info;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelGetAppInfo(int32_t pid, void* appInfo) {
    (void)pid;
    (void)appInfo;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelTitleWorkaroundIsEnabled(void* tw, int32_t bit, int32_t* result) {
    (void)tw;
    (void)bit;
    (void)result;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelGetProcessType(int32_t pid) {
    (void)pid;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI __sys_regmgr_call(uint32_t op, uint32_t key, void* result, void* value, uint64_t len) {
    (void)op;
    (void)key;
    (void)result;
    (void)value;
    (void)len;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelMmap(void* addr, uint64_t len, int32_t prot, int32_t flags, int32_t fd, int64_t offset, void** res) {
    (void)addr;
    (void)len;
    (void)prot;
    (void)flags;
    (void)fd;
    (void)offset;
    (void)res;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelMapNamedSystemFlexibleMemory(void** addrInOut, uint64_t len, int32_t prot, int32_t flags, const char* name) {
    (void)addrInOut;
    (void)len;
    (void)prot;
    (void)flags;
    (void)name;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelMunlock(void* addr, uint64_t len) {
    (void)addr;
    (void)len;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelGetCompiledSdkVersion(int32_t* ver) {
    (void)ver;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelGetModuleInfo2(KernelModule handle, void* info) {
    (void)handle;
    (void)info;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelGetModuleInfoInternal(KernelModule handle, void* info) {
    (void)handle;
    (void)info;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelGetModuleList2(KernelModule* handles, uint64_t numArray, uint64_t* outCount) {
    (void)handles;
    (void)numArray;
    (void)outCount;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI __pthread_cleanup_push_imp(void (*routine)(void*), void* arg, void* info) {
    (void)routine;
    (void)arg;
    (void)info;
    NotImplemented_nid_no_patch(__func__);
}

int32_t APS5_VABI sceKernelCloseEventFlag(KernelEventFlag ef) {
    (void)ef;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int32_t APS5_VABI sceKernelOpenEventFlag(KernelEventFlag* ef, const char* name) {
    (void)ef;
    (void)name;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
}
