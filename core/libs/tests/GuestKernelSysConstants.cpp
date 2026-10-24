#include "SceTypes.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI pthread_switch_add_np_nid_postfix(void);
int APS5_VABI pthread_switch_delete_np_nid_postfix(void);
int APS5_VABI sceKernelAddResource(void);
int APS5_VABI sceKernelEventLogInit(void);
int APS5_VABI sceKernelIsGenuineKratosCex(void);
int APS5_VABI sceKernelReleaseTraceDirectMemory(void);
int APS5_VABI sceKernelHasNeoMode(void);
int APS5_VABI sceKernelIsAuthenticNeo(void);
int APS5_VABI sceKernelIsGenuineN(void);
int APS5_VABI sceKernelCreateEport(void);
int APS5_VABI sceKernelDeleteEport(void);
int APS5_VABI sceKernelOpenEport(void);
int APS5_VABI sceKernelCloseEport(void);
int APS5_VABI sceKernelTriggerEport(void);
int APS5_VABI sceKernelQueryTraceMemory(void);
int APS5_VABI sceKernelIccGetCPMode(void);
int APS5_VABI sceKernelIccSetCPMode(void);
int APS5_VABI sceKernelClearGameDirectMemory(void);
int APS5_VABI sceKernelMapTraceMemory(void);
int APS5_VABI sceKernelTraceMemoryTypeProtect(void);
}

namespace {

constexpr int GUEST_ENOTSUP = 45;
constexpr int SCE_KERNEL_ERROR_EPERM = static_cast<int>(0x80020001);
constexpr int SCE_KERNEL_ERROR_ENOENT = static_cast<int>(0x80020002);
constexpr int SCE_KERNEL_ERROR_ENOTSUP = static_cast<int>(0x8002002d);
constexpr int SCE_KERNEL_ERROR_ENOSYS = static_cast<int>(0x8002004e);

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    Require(pthread_switch_add_np_nid_postfix() == GUEST_ENOTSUP);
    Require(pthread_switch_delete_np_nid_postfix() == GUEST_ENOTSUP);

    Require(sceKernelAddResource() == 0);
    Require(sceKernelEventLogInit() == 0);
    Require(sceKernelIsGenuineKratosCex() == 0);
    Require(sceKernelReleaseTraceDirectMemory() == 0);

    Require(sceKernelHasNeoMode() == 1);
    Require(sceKernelIsAuthenticNeo() == 1);
    Require(sceKernelIsGenuineN() == 1);

    Require(sceKernelCreateEport() == SCE_KERNEL_ERROR_EPERM);
    Require(sceKernelDeleteEport() == SCE_KERNEL_ERROR_EPERM);
    Require(sceKernelOpenEport() == SCE_KERNEL_ERROR_EPERM);
    Require(sceKernelCloseEport() == SCE_KERNEL_ERROR_EPERM);
    Require(sceKernelTriggerEport() == SCE_KERNEL_ERROR_EPERM);

    Require(sceKernelQueryTraceMemory() == SCE_KERNEL_ERROR_ENOENT);

    Require(sceKernelIccGetCPMode() == SCE_KERNEL_ERROR_ENOTSUP);
    Require(sceKernelIccSetCPMode() == SCE_KERNEL_ERROR_ENOTSUP);

    Require(sceKernelClearGameDirectMemory() == SCE_KERNEL_ERROR_ENOSYS);
    Require(sceKernelMapTraceMemory() == SCE_KERNEL_ERROR_ENOSYS);
    Require(sceKernelTraceMemoryTypeProtect() == SCE_KERNEL_ERROR_ENOSYS);
    return 0;
}
