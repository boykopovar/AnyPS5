#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"

extern "C" {
const void* APS5_VABI WKWebsitePoliciesCreate() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::WebsitePolicies>());
}

bool APS5_VABI WKWebsitePoliciesGetContentBlockersEnabled(const void* websitePolicies) {
    return NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__)->contentBlockersEnabled;
}

void APS5_VABI WKWebsitePoliciesSetContentBlockersEnabled(const void* websitePolicies, bool enabled) {
    NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__)->contentBlockersEnabled = enabled;
}

const void* APS5_VABI WKWebsitePoliciesCopyCustomHeaderFields(const void* websitePolicies) {
    NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__);
    return nullptr;
}

void APS5_VABI WKWebsitePoliciesSetCustomHeaderFields(const void* websitePolicies, const void* customHeaderFields) {
    NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__);
    (void)customHeaderFields;
}

std::uint32_t APS5_VABI WKWebsitePoliciesGetAllowedAutoplayQuirks(const void* websitePolicies) {
    return NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__)->allowedAutoplayQuirks;
}

void APS5_VABI WKWebsitePoliciesSetAllowedAutoplayQuirks(const void* websitePolicies, std::uint32_t allowedQuirks) {
    NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__)->allowedAutoplayQuirks = allowedQuirks;
}

std::uint32_t APS5_VABI WKWebsitePoliciesGetAutoplayPolicy(const void* websitePolicies) {
    return NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__)->autoplayPolicy;
}

void APS5_VABI WKWebsitePoliciesSetAutoplayPolicy(const void* websitePolicies, std::uint32_t policy) {
    NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__)->autoplayPolicy = policy;
}

std::uint32_t APS5_VABI WKWebsitePoliciesGetPopUpPolicy(const void* websitePolicies) {
    return NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__)->popUpPolicy;
}

void APS5_VABI WKWebsitePoliciesSetPopUpPolicy(const void* websitePolicies, std::uint32_t policy) {
    NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__)->popUpPolicy = policy;
}

const void* APS5_VABI WKWebsitePoliciesGetDataStore(const void* websitePolicies) {
    const auto* policies = NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__);
    return static_cast<const void*>(policies->websiteDataStore.Get());
}

void APS5_VABI WKWebsitePoliciesSetDataStore(const void* websitePolicies, const void* websiteDataStore) {
    auto* policies = NKWebKit::Cast<NKWebKit::WebsitePolicies>(websitePolicies, __func__);
    policies->websiteDataStore = NKWebKit::Retained<NKWebKit::Object>::Share(
        NKWebKit::CastOrNull<NKWebKit::WebsiteDataStore>(websiteDataStore, __func__));
}

}
