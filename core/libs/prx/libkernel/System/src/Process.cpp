#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"

extern "C" {

// unknown signature
char* __progname_nid_postfix = nullptr;

int APS5_VABI getargc_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

const char** APS5_VABI getargv_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI getpagesize_nid_postfix(void) {
 return PS5_PAGE_SIZE;
}

int APS5_VABI getpid_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI exit_nid_postfix(int code) {
 (void)code;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetCurrentCpu(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t APS5_VABI sceKernelGetGPI(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI sceKernelSetGPO(uint32_t bits) {
 (void)bits;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetOpenPsId(void* open_ps_id) {
 (void)open_ps_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* APS5_VABI sceKernelGetProcParam(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI sceKernelUuidCreate(uint32_t* uuid) {
 (void)uuid;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI sceKernelSync(void) {
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sched_get_priority_max_nid_postfix(int policy) {
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sched_get_priority_min_nid_postfix(int policy) {
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
