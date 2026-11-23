#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKBundlePageSetUIClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->uiClientConfigured = (client != nullptr);
}

void APS5_VABI WKBundlePageSetEditorClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->editorClientConfigured = (client != nullptr);
}

void APS5_VABI WKBundlePageSetFormClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->formClientConfigured = (client != nullptr);
}

void APS5_VABI WKBundlePageSetFullScreenClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->fullScreenClientConfigured = (client != nullptr);
}

void APS5_VABI WKBundlePageSetPageLoaderClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->pageLoaderClientConfigured = (client != nullptr);
}

void APS5_VABI WKBundlePageSetResourceLoadClient(const void* pageRef, const void* client) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->resourceLoadClientConfigured = (client != nullptr);
}

void APS5_VABI WKBundlePageAddUserScript(
    const void* pageRef, const void* sourceRef,
    std::uint32_t injectionTime, std::uint32_t injectedFrames) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    if (!sourceRef) {
        return;
    }
    const auto* src = NKWebKit::Cast<NKWebKit::String>(sourceRef, __func__);
    if (src->null || src->characters.empty()) {
        return;
    }
    auto script = NKWebKit::Make<NKWebKit::UserScript>(
        src->characters, injectionTime == 1 ? 1u : 0u, injectedFrames == 1);
    script->scriptWorld = NKWebKit::Retained<NKWebKit::BundleScriptWorld>::Share(
        NKWebKit::BundleScriptWorld::NormalWorld());
    page->userScripts.push_back(std::move(script));
}

void APS5_VABI WKBundlePageAddUserScriptInWorld(
    const void* pageRef, const void* sourceRef, const void* scriptWorldRef,
    std::uint32_t injectionTime, std::uint32_t injectedFrames) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    if (!sourceRef) {
        return;
    }
    const auto* src = NKWebKit::Cast<NKWebKit::String>(sourceRef, __func__);
    if (src->null || src->characters.empty()) {
        return;
    }
    auto* world = NKWebKit::CastOrNull<NKWebKit::BundleScriptWorld>(scriptWorldRef, __func__);
    auto script = NKWebKit::Make<NKWebKit::UserScript>(
        src->characters, injectionTime == 1 ? 1u : 0u, injectedFrames == 1);
    script->scriptWorld = world ? NKWebKit::Retained<NKWebKit::BundleScriptWorld>::Share(world)
                                : NKWebKit::Retained<NKWebKit::BundleScriptWorld>::Share(NKWebKit::BundleScriptWorld::NormalWorld());
    page->userScripts.push_back(std::move(script));
}

void APS5_VABI WKBundlePageAddUserStyleSheet(
    const void* pageRef, const void* sourceRef, std::uint32_t injectedFrames) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    if (!sourceRef) {
        return;
    }
    const auto* src = NKWebKit::Cast<NKWebKit::String>(sourceRef, __func__);
    if (src->null || src->characters.empty()) {
        return;
    }
    auto sheet = NKWebKit::Make<NKWebKit::UserStyleSheet>(src->characters);
    sheet->forMainFrameOnly = (injectedFrames == 1);
    page->userStyleSheets.push_back(std::move(sheet));
}

void APS5_VABI WKBundlePageRemoveAllUserContent(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->userScripts.clear();
    page->userStyleSheets.clear();
}

}
