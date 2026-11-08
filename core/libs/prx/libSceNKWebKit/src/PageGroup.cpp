#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"

namespace {

std::vector<std::u16string> Patterns(const void* array, const char* function) {
    std::vector<std::u16string> result;
    const auto* list = NKWebKit::CastOrNull<NKWebKit::Array>(array, function);
    if (!list) return result;
    for (const auto& element : list->elements) {
        const auto* string = NKWebKit::Cast<NKWebKit::String>(element.Get(), function);
        result.push_back(string->characters);
    }
    return result;
}

std::string BaseURL(const void* baseURL, const char* function) {
    const auto* url = NKWebKit::CastOrNull<NKWebKit::URL>(baseURL, function);
    if (!url || url->text.empty()) return "about:blank";
    return url->text;
}

}

extern "C" {
const void* APS5_VABI WKPageGroupCreateWithIdentifier(const void* identifier) {
    std::u16string idString;
    if (identifier) {
        idString = NKWebKit::Cast<NKWebKit::String>(identifier, __func__)->characters;
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::PageGroup>(std::move(idString)));
}

void APS5_VABI WKPageGroupSetPreferences(const void* pageGroup, const void* preferences) {
    auto* group = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroup, __func__);
    group->preferences = NKWebKit::Retained<NKWebKit::Object>::Share(
        NKWebKit::CastOrNull<NKWebKit::Preferences>(preferences, __func__));
}

const void* APS5_VABI WKPageGroupGetPreferences(const void* pageGroup) {
    const auto* group = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroup, __func__);
    return static_cast<const void*>(group->preferences.Get());
}

const void* APS5_VABI WKPageGroupGetUserContentController(const void* pageGroup) {
    const auto* group = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroup, __func__);
    return static_cast<const void*>(group->userContentController.Get());
}

void APS5_VABI WKPageGroupAddUserStyleSheet(
    const void* pageGroup, const void* source, const void* baseURL,
    const void* allowedURLPatterns, const void* blockedURLPatterns,
    std::uint32_t injectedFrames) {
    auto* group = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroup, __func__);
    if (!source) return;
    const auto* src = NKWebKit::Cast<NKWebKit::String>(source, __func__);
    if (src->null || src->characters.empty()) return;
    auto sheet = NKWebKit::Make<NKWebKit::UserStyleSheet>(src->characters);
    sheet->baseURL = BaseURL(baseURL, __func__);
    sheet->allowedURLPatterns = Patterns(allowedURLPatterns, __func__);
    sheet->blockedURLPatterns = Patterns(blockedURLPatterns, __func__);
    sheet->forMainFrameOnly = injectedFrames == 1;
    group->userStyleSheets.push_back(std::move(sheet));
}

void APS5_VABI WKPageGroupRemoveAllUserStyleSheets(const void* pageGroup) {
    NKWebKit::Cast<NKWebKit::PageGroup>(pageGroup, __func__)->userStyleSheets.clear();
}

void APS5_VABI WKPageGroupAddUserScript(
    const void* pageGroup, const void* source, const void* baseURL,
    const void* allowedURLPatterns, const void* blockedURLPatterns,
    std::uint32_t injectedFrames, std::uint32_t injectionTime) {
    auto* group = NKWebKit::Cast<NKWebKit::PageGroup>(pageGroup, __func__);
    if (!source) return;
    const auto* src = NKWebKit::Cast<NKWebKit::String>(source, __func__);
    if (src->null || src->characters.empty()) return;
    auto script = NKWebKit::Make<NKWebKit::UserScript>(src->characters, injectionTime, injectedFrames == 1);
    script->baseURL = BaseURL(baseURL, __func__);
    script->allowedURLPatterns = Patterns(allowedURLPatterns, __func__);
    script->blockedURLPatterns = Patterns(blockedURLPatterns, __func__);
    group->userContentController->userScripts.push_back(std::move(script));
}

void APS5_VABI WKPageGroupRemoveAllUserScripts(const void* pageGroup) {
    NKWebKit::Cast<NKWebKit::PageGroup>(pageGroup, __func__)->userContentController->userScripts.clear();
}

}
