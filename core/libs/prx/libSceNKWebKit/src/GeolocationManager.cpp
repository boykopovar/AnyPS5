#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <stdexcept>

typedef void (*WKGeolocationProviderStartUpdatingCallback)(const void* geolocationManager, const void* clientInfo);
typedef void (*WKGeolocationProviderStopUpdatingCallback)(const void* geolocationManager, const void* clientInfo);
typedef void (*WKGeolocationProviderSetEnableHighAccuracyCallback)(const void* geolocationManager, bool enabled, const void* clientInfo);

struct WKGeolocationProviderV0 {
    WKGeolocationProviderBase base;
    WKGeolocationProviderStartUpdatingCallback startUpdating;
    WKGeolocationProviderStopUpdatingCallback stopUpdating;
};

struct WKGeolocationProviderV1 {
    WKGeolocationProviderBase base;
    WKGeolocationProviderStartUpdatingCallback startUpdating;
    WKGeolocationProviderStopUpdatingCallback stopUpdating;
    WKGeolocationProviderSetEnableHighAccuracyCallback setEnableHighAccuracy;
};

extern "C" {

const void* APS5_VABI WKContextGetGeolocationManager(const void* contextRef) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    if (!pool->geolocationManager) {
        pool->geolocationManager = NKWebKit::Make<NKWebKit::GeolocationManager>();
    }
    return NKWebKit::ToRef(pool->geolocationManager);
}

void APS5_VABI WKGeolocationManagerSetProvider(const void* geolocationManagerRef, const WKGeolocationProviderBase* provider) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(geolocationManagerRef, __func__);
    if (!provider) {
        manager->providerConfigured = false;
        manager->providerVersion = 0;
        manager->providerClientInfo = nullptr;
        manager->startUpdating = nullptr;
        manager->stopUpdating = nullptr;
        manager->setEnableHighAccuracy = nullptr;
        return;
    }
    manager->providerConfigured = true;
    manager->providerVersion = provider->version;
    manager->providerClientInfo = provider->clientInfo;
    if (provider->version == 0) {
        const auto* v0 = reinterpret_cast<const WKGeolocationProviderV0*>(provider);
        manager->startUpdating = v0->startUpdating;
        manager->stopUpdating = v0->stopUpdating;
        manager->setEnableHighAccuracy = nullptr;
    } else {
        const auto* v1 = reinterpret_cast<const WKGeolocationProviderV1*>(provider);
        manager->startUpdating = v1->startUpdating;
        manager->stopUpdating = v1->stopUpdating;
        manager->setEnableHighAccuracy = v1->setEnableHighAccuracy;
    }
}

void APS5_VABI WKGeolocationManagerProviderDidChangePosition(const void* geolocationManagerRef, const void* positionRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(geolocationManagerRef, __func__);
    auto* position = NKWebKit::Cast<NKWebKit::GeolocationPosition>(positionRef, __func__);
    manager->lastPosition = NKWebKit::Retained<NKWebKit::GeolocationPosition>::Share(position);
    manager->hasPosition = true;
    manager->failed = false;
    manager->errorMessage.Reset();
}

void APS5_VABI WKGeolocationManagerProviderDidFailToDeterminePosition(const void* geolocationManagerRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(geolocationManagerRef, __func__);
    manager->failed = true;
    manager->errorMessage.Reset();
}

void APS5_VABI WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(const void* geolocationManagerRef, const void* errorMessageRef) {
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(geolocationManagerRef, __func__);
    auto* message = NKWebKit::Cast<NKWebKit::String>(errorMessageRef, __func__);
    manager->failed = true;
    manager->errorMessage = NKWebKit::Retained<NKWebKit::String>::Share(message);
}

}
