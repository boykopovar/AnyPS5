#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"

extern "C" {
const void* APS5_VABI WKPageConfigurationCreate() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::PageConfiguration>());
}

const void* APS5_VABI WKPageConfigurationGetContext(const void* configuration) {
    const auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    return static_cast<const void*>(config->context.Get());
}

void APS5_VABI WKPageConfigurationSetContext(const void* configuration, const void* context) {
    auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    config->context = NKWebKit::Retained<NKWebKit::Object>::Share(
        NKWebKit::CastOrNull<NKWebKit::ProcessPool>(context, __func__));
}

const void* APS5_VABI WKPageConfigurationGetPageGroup(const void* configuration) {
    const auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    return static_cast<const void*>(config->pageGroup.Get());
}

void APS5_VABI WKPageConfigurationSetPageGroup(const void* configuration, const void* pageGroup) {
    auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    config->pageGroup = NKWebKit::Retained<NKWebKit::Object>::Share(
        NKWebKit::CastOrNull<NKWebKit::PageGroup>(pageGroup, __func__));
}

const void* APS5_VABI WKPageConfigurationGetUserContentController(const void* configuration) {
    const auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    return static_cast<const void*>(config->userContentController.Get());
}

void APS5_VABI WKPageConfigurationSetUserContentController(const void* configuration, const void* userContentController) {
    auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    config->userContentController = NKWebKit::Retained<NKWebKit::Object>::Share(
        NKWebKit::CastOrNull<NKWebKit::UserContentController>(userContentController, __func__));
}

const void* APS5_VABI WKPageConfigurationGetPreferences(const void* configuration) {
    const auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    return static_cast<const void*>(config->preferences.Get());
}

void APS5_VABI WKPageConfigurationSetPreferences(const void* configuration, const void* preferences) {
    auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    config->preferences = NKWebKit::Retained<NKWebKit::Object>::Share(
        NKWebKit::CastOrNull<NKWebKit::Preferences>(preferences, __func__));
}

const void* APS5_VABI WKPageConfigurationGetRelatedPage(const void* configuration) {
    const auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    return static_cast<const void*>(config->relatedPage.Get());
}

void APS5_VABI WKPageConfigurationSetRelatedPage(const void* configuration, const void* relatedPage) {
    auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    config->relatedPage = NKWebKit::Retained<NKWebKit::Object>::Share(
        NKWebKit::CastOrNull<NKWebKit::Page>(relatedPage, __func__));
}

const void* APS5_VABI WKPageConfigurationGetWebsiteDataStore(const void* configuration) {
    const auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    return static_cast<const void*>(config->websiteDataStore.Get());
}

void APS5_VABI WKPageConfigurationSetWebsiteDataStore(const void* configuration, const void* websiteDataStore) {
    auto* config = NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__);
    config->websiteDataStore = NKWebKit::Retained<NKWebKit::Object>::Share(
        NKWebKit::CastOrNull<NKWebKit::WebsiteDataStore>(websiteDataStore, __func__));
}

void APS5_VABI WKPageConfigurationSetInitialCapitalizationEnabled(const void* configuration, bool enabled) {
    NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__)->initialCapitalizationEnabled = enabled;
}

void APS5_VABI WKPageConfigurationSetBackgroundCPULimit(const void* configuration, double cpuLimit) {
    NKWebKit::Cast<NKWebKit::PageConfiguration>(configuration, __func__)->cpuLimit = cpuLimit;
}

}
