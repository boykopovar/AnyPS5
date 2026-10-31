#include "prx/libkernel/Module/ModuleArgs.hpp"

namespace {

alignas(16) constexpr SceKernelDefaultModuleArgs kDefaultModuleArgs{};
const PendingModuleArgs kEmptyPendingArgs{sizeof(kDefaultModuleArgs), &kDefaultModuleArgs};

thread_local PendingModuleArgs pendingModuleArgs{};
thread_local int pendingModuleInitResult = 0;
thread_local bool explicitLoadInProgress = false;

ModuleInitResultObserver s_initResultObserver = nullptr;

}

extern "C" {

const void* __aps5_get_pending_module_args_nid_no_patch() {
    if (explicitLoadInProgress) {
        return &pendingModuleArgs;
    }
    return &kEmptyPendingArgs;
}

void __aps5_set_module_init_result_nid_no_patch(int result) {
    pendingModuleInitResult = result;
    if (s_initResultObserver) {
        s_initResultObserver(result);
    }
}

void SetPendingModuleArgsForTest_nid_no_patch(std::size_t args, const void* argp, bool explicitLoad) {
    pendingModuleArgs = {args, argp};
    pendingModuleInitResult = 0;
    explicitLoadInProgress = explicitLoad;
}

int GetPendingModuleInitResultForTest_nid_no_patch() {
    return pendingModuleInitResult;
}

void SetModuleInitResultObserverForTest_nid_no_patch(ModuleInitResultObserver observer) {
    s_initResultObserver = observer;
}

}
