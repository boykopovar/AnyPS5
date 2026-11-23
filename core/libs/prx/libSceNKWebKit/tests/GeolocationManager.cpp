#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "GeolocationManager check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

void DummyStartUpdating(const void*, const void*) {}
void DummyStopUpdating(const void*, const void*) {}
void DummySetEnableHighAccuracy(const void*, bool, const void*) {}

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

}

int main() {
    auto pool = NKWebKit::Make<NKWebKit::ProcessPool>();
    const void* contextRef = pool.Get();

    const void* managerRef = WKContextGetGeolocationManager(contextRef);
    Require(managerRef != nullptr);
    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(managerRef, "main");
    Require(manager != nullptr);

    const void* cachedManagerRef = WKContextGetGeolocationManager(contextRef);
    Require(cachedManagerRef == managerRef);

    int clientData = 42;
    WKGeolocationProviderV0 providerV0{
        {0, &clientData},
        DummyStartUpdating,
        DummyStopUpdating
    };

    WKGeolocationManagerSetProvider(managerRef, reinterpret_cast<const WKGeolocationProviderBase*>(&providerV0));
    Require(manager->providerConfigured);
    Require(manager->providerVersion == 0);
    Require(manager->providerClientInfo == &clientData);
    Require(manager->startUpdating == DummyStartUpdating);
    Require(manager->stopUpdating == DummyStopUpdating);
    Require(manager->setEnableHighAccuracy == nullptr);

    WKGeolocationProviderV1 providerV1{
        {1, &clientData},
        DummyStartUpdating,
        DummyStopUpdating,
        DummySetEnableHighAccuracy
    };

    WKGeolocationManagerSetProvider(managerRef, reinterpret_cast<const WKGeolocationProviderBase*>(&providerV1));
    Require(manager->providerConfigured);
    Require(manager->providerVersion == 1);
    Require(manager->providerClientInfo == &clientData);
    Require(manager->startUpdating == DummyStartUpdating);
    Require(manager->stopUpdating == DummyStopUpdating);
    Require(manager->setEnableHighAccuracy == DummySetEnableHighAccuracy);

    WKGeolocationManagerSetProvider(managerRef, nullptr);
    Require(!manager->providerConfigured);
    Require(manager->providerVersion == 0);
    Require(manager->providerClientInfo == nullptr);
    Require(manager->startUpdating == nullptr);
    Require(manager->stopUpdating == nullptr);
    Require(manager->setEnableHighAccuracy == nullptr);

    auto position = NKWebKit::Make<NKWebKit::GeolocationPosition>(
        100.0, 37.7749, -122.4194, 10.0,
        true, 5.0, true, 2.0, true, 180.0, true, 1.5, true, 2.0
    );

    Require(!manager->hasPosition);
    Require(!manager->failed);

    WKGeolocationManagerProviderDidChangePosition(managerRef, position.Get());
    Require(manager->hasPosition);
    Require(!manager->failed);
    Require(manager->lastPosition.Get() == position.Get());

    WKGeolocationManagerProviderDidFailToDeterminePosition(managerRef);
    Require(manager->failed);
    Require(!manager->errorMessage);

    auto errorMsg = NKWebKit::String::FromUtf8("User denied geolocation");
    WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(managerRef, errorMsg.Get());
    Require(manager->failed);
    Require(manager->errorMessage);
    Require(manager->errorMessage->Utf8() == "User denied geolocation");

    WKGeolocationManagerProviderDidChangePosition(managerRef, position.Get());
    Require(!manager->failed);
    Require(!manager->errorMessage);

    auto page = NKWebKit::Make<NKWebKit::Page>();

    RequireInvalidArgument([&] { WKContextGetGeolocationManager(nullptr); });
    RequireInvalidArgument([&] { WKContextGetGeolocationManager(page.Get()); });

    RequireInvalidArgument([&] { WKGeolocationManagerSetProvider(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKGeolocationManagerSetProvider(page.Get(), nullptr); });

    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidChangePosition(nullptr, position.Get()); });
    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidChangePosition(page.Get(), position.Get()); });
    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidChangePosition(managerRef, nullptr); });
    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidChangePosition(managerRef, page.Get()); });

    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidFailToDeterminePosition(nullptr); });
    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidFailToDeterminePosition(page.Get()); });

    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(nullptr, errorMsg.Get()); });
    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(page.Get(), errorMsg.Get()); });
    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(managerRef, nullptr); });
    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(managerRef, page.Get()); });

    return 0;
}
