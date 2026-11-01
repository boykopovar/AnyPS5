#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"

extern "C" {
const void* APS5_VABI WKUserContentControllerCreate() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::UserContentController>());
}

const void* APS5_VABI WKUserContentControllerCopyUserScripts(const void* userContentController) {
    const auto* controller = NKWebKit::Cast<NKWebKit::UserContentController>(userContentController, __func__);
    auto array = NKWebKit::Make<NKWebKit::Array>();
    for (const auto& script : controller->userScripts) {
        array->elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(script.Get()));
    }
    return NKWebKit::ToRef(array);
}

void APS5_VABI WKUserContentControllerAddUserScript(const void* userContentController, const void* userScript) {
    auto* controller = NKWebKit::Cast<NKWebKit::UserContentController>(userContentController, __func__);
    auto* script = NKWebKit::Cast<NKWebKit::UserScript>(userScript, __func__);
    controller->userScripts.push_back(NKWebKit::Retained<NKWebKit::UserScript>::Share(script));
}

void APS5_VABI WKUserContentControllerRemoveAllUserScripts(const void* userContentController) {
    NKWebKit::Cast<NKWebKit::UserContentController>(userContentController, __func__)->userScripts.clear();
}

void APS5_VABI WKUserContentControllerAddUserContentFilter(const void* userContentController, const void* userContentFilter) {
    auto* controller = NKWebKit::Cast<NKWebKit::UserContentController>(userContentController, __func__);
    auto* filter = NKWebKit::Cast<NKWebKit::ContentRuleList>(userContentFilter, __func__);
    controller->contentRuleLists.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(filter));
}

void APS5_VABI WKUserContentControllerRemoveAllUserContentFilters(const void* userContentController) {
    NKWebKit::Cast<NKWebKit::UserContentController>(userContentController, __func__)->contentRuleLists.clear();
}

}
