#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

std::int32_t APS5_VABI WKBundleNavigationActionGetNavigationType(const void* navigationActionRef) {
    const auto* action = NKWebKit::Cast<NKWebKit::BundleNavigationAction>(navigationActionRef, __func__);
    return action->navigationType;
}

std::uint32_t APS5_VABI WKBundleNavigationActionGetEventModifiers(const void* navigationActionRef) {
    const auto* action = NKWebKit::Cast<NKWebKit::BundleNavigationAction>(navigationActionRef, __func__);
    return action->eventModifiers;
}

std::int32_t APS5_VABI WKBundleNavigationActionGetEventMouseButton(const void* navigationActionRef) {
    const auto* action = NKWebKit::Cast<NKWebKit::BundleNavigationAction>(navigationActionRef, __func__);
    return action->eventMouseButton;
}

const void* APS5_VABI WKBundleNavigationActionCopyHitTestResult(const void* navigationActionRef) {
    const auto* action = NKWebKit::Cast<NKWebKit::BundleNavigationAction>(navigationActionRef, __func__);
    return action->hitTestResult ? NKWebKit::ToRef(action->hitTestResult) : nullptr;
}

const void* APS5_VABI WKBundleNavigationActionCopyFormElement(const void* navigationActionRef) {
    const auto* action = NKWebKit::Cast<NKWebKit::BundleNavigationAction>(navigationActionRef, __func__);
    return action->formElement ? NKWebKit::ToRef(action->formElement) : nullptr;
}

const void* APS5_VABI WKBundleNavigationActionCopyDownloadAttribute(const void* navigationActionRef) {
    const auto* action = NKWebKit::Cast<NKWebKit::BundleNavigationAction>(navigationActionRef, __func__);
    return action->downloadAttribute ? NKWebKit::ToRef(action->downloadAttribute) : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

bool APS5_VABI WKBundleNavigationActionGetShouldOpenExternalURLs(const void* navigationActionRef) {
    const auto* action = NKWebKit::Cast<NKWebKit::BundleNavigationAction>(navigationActionRef, __func__);
    return action->shouldOpenExternalURLs;
}

bool APS5_VABI WKBundleNavigationActionGetShouldTryAppLinks(const void* navigationActionRef) {
    const auto* action = NKWebKit::Cast<NKWebKit::BundleNavigationAction>(navigationActionRef, __func__);
    return action->shouldTryAppLinks;
}

}
