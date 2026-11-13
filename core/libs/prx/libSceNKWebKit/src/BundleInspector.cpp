#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKBundleInspectorShow(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::BundleInspector>(inspectorRef, __func__);
    inspector->isVisible = true;
}

void APS5_VABI WKBundleInspectorClose(const void* inspectorRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::BundleInspector>(inspectorRef, __func__);
    inspector->isVisible = false;
}

void APS5_VABI WKBundleInspectorEvaluateScriptForTest(const void* inspectorRef, const void* scriptRef) {
    auto* inspector = NKWebKit::Cast<NKWebKit::BundleInspector>(inspectorRef, __func__);
    const auto* script = NKWebKit::Cast<NKWebKit::String>(scriptRef, __func__);
    inspector->evaluatedScriptsForTest.push_back(script->Utf8());
}

void APS5_VABI WKBundleInspectorSetPageProfilingEnabled(const void* inspectorRef, bool enabled) {
    auto* inspector = NKWebKit::Cast<NKWebKit::BundleInspector>(inspectorRef, __func__);
    inspector->isProfilingPage = enabled;
}

}
