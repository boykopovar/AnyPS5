#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKContextGetGeolocationManager(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!pool->geolocationManager) {
        pool->geolocationManager = NKWebKit::Make<NKWebKit::GeolocationManager>();
    }
    return pool->geolocationManager.Get();
}

void APS5_VABI WKGeolocationManagerSetProvider(const void* managerRef, const WKGeolocationProviderBase* provider) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(managerRef, __func__);
    if (provider) {
        manager->provider = *provider;
        manager->hasProvider = true;
    } else {
        manager->provider = {0, nullptr};
        manager->hasProvider = false;
    }
}

void APS5_VABI WKGeolocationManagerProviderDidChangePosition(const void* managerRef, const void* positionRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(managerRef, __func__);
    if (!positionRef) {
        return;
    }
    auto* pos = NKWebKit::Cast<NKWebKit::GeolocationPosition>(positionRef, __func__);
    manager->lastPosition = NKWebKit::Retained<NKWebKit::Object>::Share(pos);
    manager->hasError = false;
    manager->lastErrorMessage.clear();
}

void APS5_VABI WKGeolocationManagerProviderDidFailToDeterminePosition(const void* managerRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(managerRef, __func__);
    manager->hasError = true;
    manager->lastErrorMessage.clear();
}

void APS5_VABI WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(const void* managerRef, const void* errorMessageRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(managerRef, __func__);
    manager->hasError = true;
    if (errorMessageRef) {
        const auto* str = NKWebKit::Cast<NKWebKit::String>(errorMessageRef, __func__);
        manager->lastErrorMessage = str->characters;
    } else {
        manager->lastErrorMessage.clear();
    }
}

}
