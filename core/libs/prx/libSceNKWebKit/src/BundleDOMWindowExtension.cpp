#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKBundleDOMWindowExtensionCreate(const void* frameRef, const void* scriptWorldRef) {
    auto* frame = NKWebKit::CastOrNull<NKWebKit::BundleFrame>(frameRef, __func__);
    auto* world = NKWebKit::CastOrNull<NKWebKit::BundleScriptWorld>(scriptWorldRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::BundleDOMWindowExtension>(
        frame ? NKWebKit::Retained<NKWebKit::BundleFrame>::Share(frame) : nullptr,
        world ? NKWebKit::Retained<NKWebKit::BundleScriptWorld>::Share(world) : nullptr));
}

const void* APS5_VABI WKBundleDOMWindowExtensionGetFrame(const void* extensionRef) {
    const auto* extension = NKWebKit::Cast<NKWebKit::BundleDOMWindowExtension>(extensionRef, __func__);
    return extension->frame ? extension->frame.Get() : nullptr;
}

const void* APS5_VABI WKBundleDOMWindowExtensionGetScriptWorld(const void* extensionRef) {
    const auto* extension = NKWebKit::Cast<NKWebKit::BundleDOMWindowExtension>(extensionRef, __func__);
    return extension->scriptWorld ? extension->scriptWorld.Get() : nullptr;
}

}
