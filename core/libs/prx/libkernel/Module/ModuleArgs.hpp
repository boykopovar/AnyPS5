#ifndef CORE_LIBS_PRX_LIBKERNEL_MODULE_MODULEARGS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_MODULE_MODULEARGS_HPP

#include <cstddef>
#include <cstdint>

struct SceKernelDefaultModuleArgs {
    std::uint32_t size = 0x10;
    std::uint32_t version = 0x200;
    const void* context = nullptr;
};

struct PendingModuleArgs {
    std::size_t args = 0;
    const void* argp = nullptr;
};

using ModuleInitResultObserver = void (*)(int);

extern "C" {
const void* __aps5_get_pending_module_args_nid_no_patch();
void __aps5_set_module_init_result_nid_no_patch(int result);
void SetPendingModuleArgs_nid_no_patch(std::size_t args, const void* argp, bool explicitLoad);
int GetPendingModuleInitResult_nid_no_patch();
void SetModuleInitResultObserverForTest_nid_no_patch(ModuleInitResultObserver observer);
}

#endif
