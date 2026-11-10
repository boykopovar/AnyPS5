#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <atomic>

namespace {

std::atomic<bool> s_accessibilityEnabled{false};
std::atomic<bool> s_accessibilityEnhancedUserInterfaceEnabled{false};

}

extern "C" {

void APS5_VABI _ZN7WebCore13AXObjectCache19enableAccessibilityEv() {
    s_accessibilityEnabled.store(true, std::memory_order_relaxed);
}

void APS5_VABI _ZN7WebCore13AXObjectCache20disableAccessibilityEv() {
    s_accessibilityEnabled.store(false, std::memory_order_relaxed);
}

void APS5_VABI _ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb(bool flag) {
    s_accessibilityEnhancedUserInterfaceEnabled.store(flag, std::memory_order_relaxed);
}

bool APS5_VABI WKAccessibilityEnhancedAccessibilityEnabled() {
    return s_accessibilityEnhancedUserInterfaceEnabled.load(std::memory_order_relaxed);
}

}
