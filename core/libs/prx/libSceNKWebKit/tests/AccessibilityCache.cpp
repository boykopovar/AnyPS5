#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    Require(!WKAccessibilityEnhancedAccessibilityEnabled());

    _ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb(true);
    Require(WKAccessibilityEnhancedAccessibilityEnabled());

    _ZN7WebCore13AXObjectCache37setEnhancedUserInterfaceAccessibilityEb(false);
    Require(!WKAccessibilityEnhancedAccessibilityEnabled());

    _ZN7WebCore13AXObjectCache19enableAccessibilityEv();
    _ZN7WebCore13AXObjectCache20disableAccessibilityEv();

    return 0;
}
