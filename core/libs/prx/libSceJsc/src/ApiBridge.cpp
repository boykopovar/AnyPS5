#include "prx/libSceJsc/include/ApiBridgeExports.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>

namespace {

std::atomic<const Jsc::ApiBridge*> registeredBridge{nullptr};

const Jsc::ApiBridge& Bridge(const char* function) {
    const auto* bridge = registeredBridge.load(std::memory_order_acquire);
    if (!bridge) NotImplemented_nid_no_patch(function);
    return *bridge;
}

}

extern "C" {

void RegisterApiBridge_nid_no_patch(const Jsc::ApiBridge* bridge) {
    registeredBridge.store(bridge, std::memory_order_release);
}

const void* APS5_VABI JSValueMakeString(const void* context, const GuestJSString* string) {
    if (!context) return nullptr;
    return Bridge(__func__).makeString(context, string);
}

bool APS5_VABI JSObjectHasPropertyForKey(const void* context, const void* object, const void* key, const void** exception) {
    if (!context) return false;
    return Bridge(__func__).hasPropertyForKey(context, object, key, exception);
}

}
