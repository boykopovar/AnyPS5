#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    const void* worldRef = WKBundleScriptWorldCreateWorld();
    Require(worldRef != nullptr);
    Require(worldRef != WKBundleScriptWorldNormalWorld());
    Require(WKGetTypeID(worldRef) == WKBundleScriptWorldGetTypeID());

    const auto* world = NKWebKit::Cast<NKWebKit::BundleScriptWorld>(worldRef, __func__);
    Require(world->clearedWrappersCount == 0);
    Require(!world->allShadowRootsOpen);
    Require(!world->overrideBuiltinsDisabled);

    WKBundleScriptWorldClearWrappers(worldRef);
    Require(world->clearedWrappersCount == 1);

    WKBundleScriptWorldMakeAllShadowRootsOpen(worldRef);
    Require(world->allShadowRootsOpen);

    WKBundleScriptWorldDisableOverrideBuiltinsBehavior(worldRef);
    Require(world->overrideBuiltinsDisabled);

    const void* emptyName = WKBundleScriptWorldCopyName(worldRef);
    Require(WKStringIsEqualToUTF8CString(emptyName, ""));

    RequireThrows([&] { WKBundleScriptWorldClearWrappers(nullptr); });
    RequireThrows([&] { WKBundleScriptWorldMakeAllShadowRootsOpen(emptyName); });
    RequireThrows([&] { WKBundleScriptWorldDisableOverrideBuiltinsBehavior(nullptr); });

    WKRelease(emptyName);
    WKRelease(worldRef);
    std::printf("All libSceNKWebKit BundleScriptWorld tests passed!\n");
    return 0;
}
