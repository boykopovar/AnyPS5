#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKBundleScriptWorldNormalWorld() {
    return NKWebKit::BundleScriptWorld::NormalWorld();
}

const void* APS5_VABI WKBundleScriptWorldCopyName(const void* worldRef) {
    const auto* world = NKWebKit::Cast<NKWebKit::BundleScriptWorld>(worldRef, __func__);
    return world->name ? NKWebKit::ToRef(world->name) : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

}
