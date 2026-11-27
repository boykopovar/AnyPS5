#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<std::u16string> Patterns(const void* array, const char* function) {
    std::vector<std::u16string> result;
    const auto* list = NKWebKit::CastOrNull<NKWebKit::Array>(array, function);
    if (!list) {
        return result;
    }
    for (const auto& element : list->elements) {
        const auto* string = NKWebKit::Cast<NKWebKit::String>(element.Get(), function);
        result.push_back(string->characters);
    }
    return result;
}

std::string BaseURL(const void* baseURL, const char* function) {
    const auto* url = NKWebKit::CastOrNull<NKWebKit::URL>(baseURL, function);
    if (!url || url->text.empty()) {
        return "about:blank";
    }
    return url->text;
}

}  // namespace

extern "C" {

void APS5_VABI WKBundleAddUserScript(
    const void* bundleRef, const void* pageGroupRef, const void* scriptWorldRef,
    const void* sourceRef, const void* urlRef, const void* whitelistRef,
    const void* blacklistRef, std::uint32_t injectionTimeRef,
    std::uint32_t injectedFramesRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    auto* pageGroup = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroupRef, __func__);
    if (!sourceRef) {
        return;
    }
    const auto* src = NKWebKit::Cast<NKWebKit::String>(sourceRef, __func__);
    if (src->null || src->characters.empty()) {
        return;
    }
    auto script = NKWebKit::Make<NKWebKit::UserScript>(
        src->characters, injectionTimeRef == 1 ? 1u : 0u, injectedFramesRef == 1);
    script->baseURL = BaseURL(urlRef, __func__);
    script->allowedURLPatterns = Patterns(whitelistRef, __func__);
    script->blockedURLPatterns = Patterns(blacklistRef, __func__);
    script->scriptWorld = NKWebKit::Retained<NKWebKit::BundleScriptWorld>::Share(
        NKWebKit::CastOrNull<NKWebKit::BundleScriptWorld>(scriptWorldRef, __func__));
    pageGroup->userContentController->userScripts.push_back(std::move(script));
}

void APS5_VABI WKBundleAddUserStyleSheet(
    const void* bundleRef, const void* pageGroupRef, const void* scriptWorldRef,
    const void* sourceRef, const void* urlRef, const void* whitelistRef,
    const void* blacklistRef, std::uint32_t injectedFramesRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    auto* pageGroup = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroupRef, __func__);
    if (!sourceRef) {
        return;
    }
    const auto* src = NKWebKit::Cast<NKWebKit::String>(sourceRef, __func__);
    if (src->null || src->characters.empty()) {
        return;
    }
    auto sheet = NKWebKit::Make<NKWebKit::UserStyleSheet>(src->characters);
    sheet->baseURL = BaseURL(urlRef, __func__);
    sheet->allowedURLPatterns = Patterns(whitelistRef, __func__);
    sheet->blockedURLPatterns = Patterns(blacklistRef, __func__);
    sheet->forMainFrameOnly = injectedFramesRef == 1;
    sheet->scriptWorld = NKWebKit::Retained<NKWebKit::BundleScriptWorld>::Share(
        NKWebKit::CastOrNull<NKWebKit::BundleScriptWorld>(scriptWorldRef, __func__));
    pageGroup->userStyleSheets.push_back(std::move(sheet));
}

void APS5_VABI WKBundleRemoveUserScript(
    const void* bundleRef, const void* pageGroupRef, const void* scriptWorldRef,
    const void* urlRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    auto* pageGroup = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroupRef, __func__);
    auto* world = NKWebKit::CastOrNull<NKWebKit::BundleScriptWorld>(scriptWorldRef, __func__);
    if (!world || world == NKWebKit::BundleScriptWorld::NormalWorld()) {
        world = NKWebKit::BundleScriptWorld::NormalWorld();
    }
    std::string targetUrl;
    if (urlRef) {
        const auto* urlObj = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
        targetUrl = urlObj->text;
    }
    auto& scripts = pageGroup->userContentController->userScripts;
    for (auto it = scripts.begin(); it != scripts.end(); ) {
        bool isNormal = (world == NKWebKit::BundleScriptWorld::NormalWorld());
        bool worldMatches = ((*it)->scriptWorld.Get() == world) ||
                            (isNormal && (*it)->scriptWorld.Get() == nullptr);
        bool urlMatches = targetUrl.empty() || (*it)->baseURL == targetUrl;
        if (worldMatches && urlMatches) {
            it = scripts.erase(it);
        } else {
            ++it;
        }
    }
}

void APS5_VABI WKBundleRemoveUserScripts(
    const void* bundleRef, const void* pageGroupRef, const void* scriptWorldRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    auto* pageGroup = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroupRef, __func__);
    auto* world = NKWebKit::CastOrNull<NKWebKit::BundleScriptWorld>(scriptWorldRef, __func__);
    if (!world || world == NKWebKit::BundleScriptWorld::NormalWorld()) {
        world = NKWebKit::BundleScriptWorld::NormalWorld();
    }
    auto& scripts = pageGroup->userContentController->userScripts;
    for (auto it = scripts.begin(); it != scripts.end(); ) {
        bool isNormal = (world == NKWebKit::BundleScriptWorld::NormalWorld());
        bool worldMatches = ((*it)->scriptWorld.Get() == world) ||
                            (isNormal && (*it)->scriptWorld.Get() == nullptr);
        if (worldMatches) {
            it = scripts.erase(it);
        } else {
            ++it;
        }
    }
}

void APS5_VABI WKBundleRemoveUserStyleSheet(
    const void* bundleRef, const void* pageGroupRef, const void* scriptWorldRef,
    const void* urlRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    auto* pageGroup = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroupRef, __func__);
    auto* world = NKWebKit::CastOrNull<NKWebKit::BundleScriptWorld>(scriptWorldRef, __func__);
    if (!world || world == NKWebKit::BundleScriptWorld::NormalWorld()) {
        world = NKWebKit::BundleScriptWorld::NormalWorld();
    }
    std::string targetUrl;
    if (urlRef) {
        const auto* urlObj = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
        targetUrl = urlObj->text;
    }
    auto& sheets = pageGroup->userStyleSheets;
    for (auto it = sheets.begin(); it != sheets.end(); ) {
        const auto* sheet = NKWebKit::Cast<NKWebKit::UserStyleSheet>(it->Get(), __func__);
        bool isNormal = (world == NKWebKit::BundleScriptWorld::NormalWorld());
        bool worldMatches = (sheet->scriptWorld.Get() == world) ||
                            (isNormal && sheet->scriptWorld.Get() == nullptr);
        bool urlMatches = targetUrl.empty() || sheet->baseURL == targetUrl;
        if (worldMatches && urlMatches) {
            it = sheets.erase(it);
        } else {
            ++it;
        }
    }
}

void APS5_VABI WKBundleRemoveUserStyleSheets(
    const void* bundleRef, const void* pageGroupRef, const void* scriptWorldRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    auto* pageGroup = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroupRef, __func__);
    auto* world = NKWebKit::CastOrNull<NKWebKit::BundleScriptWorld>(scriptWorldRef, __func__);
    if (!world || world == NKWebKit::BundleScriptWorld::NormalWorld()) {
        world = NKWebKit::BundleScriptWorld::NormalWorld();
    }
    auto& sheets = pageGroup->userStyleSheets;
    for (auto it = sheets.begin(); it != sheets.end(); ) {
        const auto* sheet = NKWebKit::Cast<NKWebKit::UserStyleSheet>(it->Get(), __func__);
        bool isNormal = (world == NKWebKit::BundleScriptWorld::NormalWorld());
        bool worldMatches = (sheet->scriptWorld.Get() == world) ||
                            (isNormal && sheet->scriptWorld.Get() == nullptr);
        if (worldMatches) {
            it = sheets.erase(it);
        } else {
            ++it;
        }
    }
}

void APS5_VABI WKBundleRemoveAllUserContent(
    const void* bundleRef, const void* pageGroupRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    auto* pageGroup = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroupRef, __func__);
    pageGroup->userContentController->userScripts.clear();
    pageGroup->userStyleSheets.clear();
}

void APS5_VABI WKBundleSetUserStyleSheetLocation(
    const void* bundleRef, const void* pageGroupRef, const void* locationRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    auto* pageGroup = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroupRef, __func__);
    if (!locationRef) {
        pageGroup->userStyleSheetLocation.clear();
        return;
    }
    const auto* str = NKWebKit::Cast<NKWebKit::String>(locationRef, __func__);
    pageGroup->userStyleSheetLocation = str->characters;
}

}
