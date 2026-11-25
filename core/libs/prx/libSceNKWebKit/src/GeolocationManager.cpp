#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

const void* APS5_VABI WKContextGetGeolocationManager(const void* contextRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!pool->geolocationManager) {
        pool->geolocationManager = NKWebKit::Make<NKWebKit::GeolocationManager>();
    }
    return pool->geolocationManager.Get();
}

void APS5_VABI WKGeolocationManagerSetProvider(const void* managerRef, const void* provider) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(managerRef, __func__);
    if (!provider) {
        manager->provider.reset();
        return;
    }
    const auto* base = static_cast<const NKWebKit::GeolocationProviderBase*>(provider);
    manager->provider = NKWebKit::GeolocationProviderRegistration{base->version, base->clientInfo, provider};
}

void APS5_VABI WKGeolocationManagerProviderDidChangePosition(const void* managerRef, const void* positionRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(managerRef, __func__);
    auto* position = NKWebKit::Cast<NKWebKit::GeolocationPosition>(positionRef, __func__);
    manager->lastPosition = NKWebKit::Retained<NKWebKit::GeolocationPosition>::Share(position);
    manager->positionFailed = false;
    manager->errorMessage.reset();
}

void APS5_VABI WKGeolocationManagerProviderDidFailToDeterminePosition(const void* managerRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(managerRef, __func__);
    manager->positionFailed = true;
    manager->errorMessage.reset();
}

void APS5_VABI WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(const void* managerRef, const void* errorMessageRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(managerRef, __func__);
    const auto* message = NKWebKit::Cast<NKWebKit::String>(errorMessageRef, __func__);
    manager->positionFailed = true;
    manager->errorMessage = message->characters;
}

}
