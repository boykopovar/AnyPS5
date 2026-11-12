#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

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

}

int main() {
    const void* contextRef = WKContextCreate();
    Require(contextRef != nullptr);

    const void* mgrRef = WKContextGetGeolocationManager(contextRef);
    Require(mgrRef != nullptr);
    Require(WKContextGetGeolocationManager(contextRef) == mgrRef);
    RequireInvalidArgument([] { WKContextGetGeolocationManager(nullptr); });

    const auto* mgr = NKWebKit::Cast<NKWebKit::GeolocationManager>(mgrRef, "test");

    WKGeolocationProviderBase provider{1, nullptr};
    WKGeolocationManagerSetProvider(mgrRef, &provider);
    Require(mgr->hasProvider);

    auto pos = NKWebKit::Make<NKWebKit::GeolocationPosition>(
        0.0, 37.7749, -122.4194, 10.0,
        false, 0.0, false, 0.0, false, 0.0, false, 0.0, false, 0.0);
    const void* posRef = NKWebKit::ToRef(pos);

    WKGeolocationManagerProviderDidChangePosition(mgrRef, posRef);
    Require(mgr->lastPosition.Get() != nullptr);
    Require(!mgr->hasError);

    WKGeolocationManagerProviderDidChangePosition(mgrRef, nullptr);

    WKGeolocationManagerProviderDidFailToDeterminePosition(mgrRef);
    Require(mgr->hasError);

    auto errStr = NKWebKit::String::FromUtf8("Position unavailable");
    const void* errRef = NKWebKit::ToRef(errStr);
    WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(mgrRef, errRef);
    Require(mgr->hasError);
    Require(!mgr->lastErrorMessage.empty());

    WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(mgrRef, nullptr);

    WKGeolocationManagerSetProvider(mgrRef, nullptr);
    Require(!mgr->hasProvider);

    RequireInvalidArgument([] { WKGeolocationManagerSetProvider(nullptr, nullptr); });
    RequireInvalidArgument([&] { WKGeolocationManagerProviderDidChangePosition(nullptr, posRef); });
    RequireInvalidArgument([] { WKGeolocationManagerProviderDidFailToDeterminePosition(nullptr); });
    RequireInvalidArgument([] { WKGeolocationManagerProviderDidFailToDeterminePositionWithErrorMessage(nullptr, nullptr); });

    WKRelease(errRef);
    WKRelease(posRef);
    WKRelease(contextRef);
    return 0;
}
