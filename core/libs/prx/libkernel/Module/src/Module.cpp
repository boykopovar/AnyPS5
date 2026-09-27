#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceKernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
 (void)addr;
 (void)flags;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetModuleInfoFromAddr(uint64_t addr, int n, ModuleInfo* r) {
 (void)addr;
 (void)n;
 (void)r;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

KernelModule APS5_VABI sceKernelLoadStartModule(const char* module_file_name, size_t args, const void* argp, uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
 (void)module_file_name;
 (void)args;
 (void)argp;
 (void)flags;
 (void)opt;
 (void)res;
 NotImplemented_nid_no_patch(__func__);
 return {};
}

int APS5_VABI sceKernelStopUnloadModule(KernelModule handle, size_t args, const void* argp, uint32_t flags, const KernelUnloadModuleOpt* opt, int* res) {
 (void)handle;
 (void)args;
 (void)argp;
 (void)flags;
 (void)opt;
 (void)res;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
