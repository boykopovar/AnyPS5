#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>

namespace {
int startCalls = 0;
std::size_t startArgs = 0;
const void* startArgp = nullptr;
int initializerCalls = 0;
}

extern "C" int APS5_VABI DeferredModuleStart(std::size_t args, const void* argp, void*) {
    ++startCalls;
    startArgs = args;
    startArgp = argp;
    return 7;
}

extern "C" void APS5_VABI DeferredModuleInitializer(int, char**, char**) {
    ++initializerCalls;
}

using Initializer = void (APS5_VABI*)(int, char**, char**);

extern "C" {
extern const Initializer DeferredModuleSlot;
const Initializer DeferredModuleSlot = DeferredModuleInitializer;
std::uint32_t __aps5_guest_initialize[3] = {};
}

namespace {
std::uint32_t RelativeAddress(const void* address) {
    Dl_info info{};
    if (!::dladdr(address, &info)) return 0;
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address) - reinterpret_cast<std::uintptr_t>(info.dli_fbase));
}

struct Table {
    Table() {
        __aps5_guest_initialize[0] = RelativeAddress(reinterpret_cast<const void*>(&DeferredModuleStart));
        __aps5_guest_initialize[1] = 1;
        __aps5_guest_initialize[2] = RelativeAddress(&DeferredModuleSlot);
    }
} table;
}

extern "C" void DeferredModuleState(int* calls, std::size_t* args, const void** argp, int* initializers) {
    *calls = startCalls;
    *args = startArgs;
    *argp = startArgp;
    *initializers = initializerCalls;
}
