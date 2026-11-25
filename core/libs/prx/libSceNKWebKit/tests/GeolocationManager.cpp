#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

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

struct TestGeolocationProvider {
    int version = 1;
    const void* clientInfo = nullptr;
    void* startUpdating = nullptr;
    void* stopUpdating = nullptr;
};

}  // namespace

int main() {
    auto pool = NKWebKit::Make<NKWebKit::ProcessPool>();
    const void* contextRef = pool.Get();

    const void* managerRef = WKContextGetGeolocationManager(contextRef);
    Require(managerRef != nullptr);
    Require(WKGetTypeID(managerRef) == WKGeolocationManagerGetTypeID());
    Require(WKContextGetGeolocationManager(contextRef) == managerRef);

    auto* manager = NKWebKit::Cast<NKWebKit::GeolocationManager>(managerRef, "main");
    Require(!manager->provider.has_value());
    Require(!manager->lastPosition);
    Require(!manager->positionFailed);
    Require(!manager->errorMessage.has_value());

    int clientToken = 42;
    TestGeolocationProvider provider{1, &clientToken, nullptr, nullptr};
    WKGeolocationManagerSetProvider(managerRef, &provider);
    Require(manager->provider.has_value());
    Require(manager->provider->version == 1);
    Require(manager->provider->clientInfo == &clientToken);
    Require(manager->provider->raw == &provider);

    WKGeolocationManagerSetProvider(managerRef, nullptr);
    Require(!manager->provider.has_value());

    const void* pos1 = WKGeolocationPositionCreate(1234.5, 37.7749, -122.4194, 15.0);
    Require(pos1 != nullptr);
    WKGeolocationManagerProviderDidChangePosition(managerRef, pos1);
    Require(manager->lastPosition != nullptr);
    Require(manager->lastPosition->timestamp == 1234.5);
    Require(manager->lastPosition->latitude == 37.7749);
    Require(manager->lastPosition->longitude == -122.4194);
    Require(manager->lastPosition->accuracy == 15.0);
    Require(!manager->positionFailed);
    Require(!manager->errorMessage.has_value());

    WKGeolocationManagerProviderDidFailToDeterminePosition(managerRef);
    Require(manager->positionFailed);
    Require(!manager->errorMessage.has_value());

    const void* errStr = WKStringCreateWithUTF8CString("Location service unavailable");
    Require(errStr != nullptr);
    WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(managerRef, errStr);
    Require(manager->positionFailed);
    Require(manager->errorMessage.has_value());
    Require(manager->errorMessage.value() == u"Location service unavailable");

    const void* pos2 = WKGeolocationPositionCreate_b(2345.6, 51.5074, -0.1278, 8.0, true, 25.0, true, 3.0, true, 90.0, true, 5.0);
    Require(pos2 != nullptr);
    WKGeolocationManagerProviderDidChangePosition(managerRef, pos2);
    Require(manager->lastPosition != nullptr);
    Require(manager->lastPosition->timestamp == 2345.6);
    Require(manager->lastPosition->latitude == 51.5074);
    Require(manager->lastPosition->longitude == -0.1278);
    Require(manager->lastPosition->accuracy == 8.0);
    Require(manager->lastPosition->providesAltitude);
    Require(manager->lastPosition->altitude == 25.0);
    Require(manager->lastPosition->providesAltitudeAccuracy);
    Require(manager->lastPosition->altitudeAccuracy == 3.0);
    Require(manager->lastPosition->providesHeading);
    Require(manager->lastPosition->heading == 90.0);
    Require(manager->lastPosition->providesSpeed);
    Require(manager->lastPosition->speed == 5.0);
    Require(!manager->positionFailed);
    Require(!manager->errorMessage.has_value());

    auto dummyObject = NKWebKit::Make<NKWebKit::Array>();
    const void* badRef = dummyObject.Get();

    RequireInvalidArgument([] { WKContextGetGeolocationManager(nullptr); });
    RequireInvalidArgument([badRef] { WKContextGetGeolocationManager(badRef); });

    RequireInvalidArgument([&provider] { WKGeolocationManagerSetProvider(nullptr, &provider); });
    RequireInvalidArgument([badRef, &provider] { WKGeolocationManagerSetProvider(badRef, &provider); });

    RequireInvalidArgument([pos1] { WKGeolocationManagerProviderDidChangePosition(nullptr, pos1); });
    RequireInvalidArgument([badRef, pos1] { WKGeolocationManagerProviderDidChangePosition(badRef, pos1); });
    RequireInvalidArgument([managerRef] { WKGeolocationManagerProviderDidChangePosition(managerRef, nullptr); });
    RequireInvalidArgument([managerRef, badRef] { WKGeolocationManagerProviderDidChangePosition(managerRef, badRef); });

    RequireInvalidArgument([] { WKGeolocationManagerProviderDidFailToDeterminePosition(nullptr); });
    RequireInvalidArgument([badRef] { WKGeolocationManagerProviderDidFailToDeterminePosition(badRef); });

    RequireInvalidArgument([errStr] { WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(nullptr, errStr); });
    RequireInvalidArgument([badRef, errStr] { WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(badRef, errStr); });
    RequireInvalidArgument([managerRef] { WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(managerRef, nullptr); });
    RequireInvalidArgument([managerRef, badRef] { WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(managerRef, badRef); });

    WKRelease(pos1);
    WKRelease(pos2);
    WKRelease(errStr);

    return 0;
}
