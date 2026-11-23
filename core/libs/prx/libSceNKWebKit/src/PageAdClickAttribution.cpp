#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <stdexcept>
#include <string>

void APS5_VABI WKPageClearAdClickAttribution(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->adClickAttributionCleared = true;
    page->adClickAttributionsExpiredForTesting = false;
    page->adClickAttributionConversionURL = nullptr;
    page->adClickAttributionOverrideTimer = false;
}

void APS5_VABI WKPageDumpAdClickAttribution(const void* pageRef, void* context, WKPageDumpAdClickAttributionFunction callback) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    if (!callback) {
        throw std::invalid_argument(std::string(__func__) + ": null callback");
    }
    if (!page->adClickAttributionDumpString) {
        page->adClickAttributionDumpString = NKWebKit::Make<NKWebKit::String>(std::u16string());
    }
    callback(NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(page->adClickAttributionDumpString.Get())), context);
}

void APS5_VABI WKPageMarkAdClickAttributionsAsExpiredForTesting(const void* pageRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->adClickAttributionsExpiredForTesting = true;
    page->adClickAttributionCleared = false;
}

void APS5_VABI WKPageSetAdClickAttributionConversionURLForTesting(const void* pageRef, const void* urlRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->adClickAttributionConversionURL = urlRef ? NKWebKit::Retained<NKWebKit::URL>::Share(NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__)) : nullptr;
}

void APS5_VABI WKPageSetAdClickAttributionOverrideTimerForTesting(const void* pageRef, bool hasOverrideTimer) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    page->adClickAttributionOverrideTimer = hasOverrideTimer;
}
