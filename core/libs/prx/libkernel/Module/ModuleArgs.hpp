#ifndef CORE_LIBS_PRX_LIBKERNEL_MODULE_MODULEARGS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_MODULE_MODULEARGS_HPP

#include <cstddef>

struct PendingModuleArgs {
    std::size_t args = 0;
    const void* argp = nullptr;
};

extern "C" {
const void* __aps5_get_pending_module_args_nid_no_patch();
void __aps5_set_module_init_result_nid_no_patch(int result);
}

#endif
