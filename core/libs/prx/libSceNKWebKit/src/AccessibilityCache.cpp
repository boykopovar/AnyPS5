#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

#include <atomic>

namespace {

std::atomic<bool> g_accessibilityEnabled{false};
std::atomic<bool> g_enhancedAccessibilityEnabled{false};

}

extern "C" {

bool APS5_VABI WKAccessibilityEnhancedAccessibilityEnabled() {
    return g_enhancedAccessibilityEnabled.load(std::memory_order_relaxed);
}

void APS5_VABI _ZN7WebCore13AXObjectCache19enableAccessibilityEv() {
    g_accessibilityEnabled.store(true, std::memory_order_relaxed);
}

void APS5_VABI _ZN7WebCore13AXObjectCache20disableAccessibilityEv() {
    g_accessibilityEnabled.store(false, std::memory_order_relaxed);
}

void APS5_VABI _ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb(bool enabled) {
    g_enhancedAccessibilityEnabled.store(enabled, std::memory_order_relaxed);
}

}
