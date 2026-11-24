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

    const void* uniqueName = WKBundleScriptWorldCopyName(worldRef);
    Require(WKStringIsEqualToUTF8CString(uniqueName, "UniqueWorld_0"));

    const void* secondRef = WKBundleScriptWorldCreateWorld();
    const void* secondName = WKBundleScriptWorldCopyName(secondRef);
    Require(WKStringIsEqualToUTF8CString(secondName, "UniqueWorld_1"));
    WKRelease(secondName);
    WKRelease(secondRef);

    const void* thirdRef = WKBundleScriptWorldCreateWorld();
    const void* thirdName = WKBundleScriptWorldCopyName(thirdRef);
    Require(WKStringIsEqualToUTF8CString(thirdName, "UniqueWorld_2"));
    WKRelease(thirdName);
    WKRelease(thirdRef);

    const void* sameName = WKBundleScriptWorldCopyName(worldRef);
    Require(WKStringIsEqualToUTF8CString(sameName, "UniqueWorld_0"));
    WKRelease(sameName);

    const void* normalName = WKBundleScriptWorldCopyName(WKBundleScriptWorldNormalWorld());
    Require(WKStringIsEqualToUTF8CString(normalName, ""));
    WKRelease(normalName);

    RequireThrows([&] { WKBundleScriptWorldClearWrappers(nullptr); });
    RequireThrows([&] { WKBundleScriptWorldMakeAllShadowRootsOpen(uniqueName); });
    RequireThrows([&] { WKBundleScriptWorldDisableOverrideBuiltinsBehavior(nullptr); });

    WKRelease(uniqueName);
    WKRelease(worldRef);
    std::printf("All libSceNKWebKit BundleScriptWorld tests passed!\n");
    return 0;
}
