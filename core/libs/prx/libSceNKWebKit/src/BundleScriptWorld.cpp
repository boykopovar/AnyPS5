#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKBundleScriptWorldCreateWorld() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::BundleScriptWorld>());
}

const void* APS5_VABI WKBundleScriptWorldNormalWorld() {
    return NKWebKit::BundleScriptWorld::NormalWorld();
}

const void* APS5_VABI WKBundleScriptWorldCopyName(const void* worldRef) {
    const auto* world = NKWebKit::Cast<NKWebKit::BundleScriptWorld>(worldRef, __func__);
    return world->name ? NKWebKit::ToRef(world->name) : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

void APS5_VABI WKBundleScriptWorldClearWrappers(const void* worldRef) {
    auto* world = NKWebKit::Cast<NKWebKit::BundleScriptWorld>(worldRef, __func__);
    ++world->clearedWrappersCount;
}

void APS5_VABI WKBundleScriptWorldMakeAllShadowRootsOpen(const void* worldRef) {
    auto* world = NKWebKit::Cast<NKWebKit::BundleScriptWorld>(worldRef, __func__);
    world->allShadowRootsOpen = true;
}

void APS5_VABI WKBundleScriptWorldDisableOverrideBuiltinsBehavior(const void* worldRef) {
    auto* world = NKWebKit::Cast<NKWebKit::BundleScriptWorld>(worldRef, __func__);
    world->overrideBuiltinsDisabled = true;
}

}
