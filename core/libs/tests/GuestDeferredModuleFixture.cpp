#include "GuestDeferredModule.hpp"
#include <atomic>
#include <cstddef>
#include <cstdlib>

static std::atomic<int> starts{0};
static std::atomic<int> constructors{0};

extern "C" int APS5_VABI DeferredStart_nid_no_patch(std::size_t bytes, const void* argument, void*) {
    ++starts;
    if (bytes != 0) {
        if (bytes != sizeof(GuestDeferredModuleArgs) || !argument) std::abort();
        const auto& args = *static_cast<const GuestDeferredModuleArgs*>(argument);
        if (args.Mode == 1) {
            if (!SetEvent(args.Entered) || WaitForSingleObject(args.Release, 10000) != WAIT_OBJECT_0)
                std::abort();
        } else if (args.Mode == 2) args.Reenter(args.Path);
    }
    return GuestDeferredModuleResult;
}

extern "C" void APS5_VABI DeferredConstructor_nid_no_patch(int, char**, char**) {
    ++constructors;
}

using Initializer = void (APS5_VABI*)(int, char**, char**);
extern "C" {
Initializer DeferredInitializerSlot_nid_no_patch = DeferredConstructor_nid_no_patch;
}

extern "C" int APS5_VABI DeferredStarts_nid_no_patch() { return starts.load(); }
extern "C" int APS5_VABI DeferredConstructors_nid_no_patch() { return constructors.load(); }
