#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    const void* contextRef = WKContextCreate();
    Require(contextRef != nullptr);
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);

    Require(!pool->allowsAnySSLCertificateForWebSocketTesting);
    Require(!pool->allowsAnySSLCertificateForServiceWorkerTesting);
    Require(!pool->serviceWorkerFetchTimeoutForTesting.has_value());
    Require(!pool->useTestingNetworkSession);

    WKContextSetAllowsAnySSLCertificateForWebSocketTesting(contextRef, true);
    Require(pool->allowsAnySSLCertificateForWebSocketTesting);
    WKContextSetAllowsAnySSLCertificateForWebSocketTesting(contextRef, false);
    Require(!pool->allowsAnySSLCertificateForWebSocketTesting);

    WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting(contextRef, true);
    Require(pool->allowsAnySSLCertificateForServiceWorkerTesting);
    WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting(contextRef, false);
    Require(!pool->allowsAnySSLCertificateForServiceWorkerTesting);

    WKContextSetServiceWorkerFetchTimeoutForTesting(contextRef, 15.5);
    Require(pool->serviceWorkerFetchTimeoutForTesting.has_value());
    Require(*pool->serviceWorkerFetchTimeoutForTesting == 15.5);

    WKContextResetServiceWorkerFetchTimeoutForTesting(contextRef);
    Require(!pool->serviceWorkerFetchTimeoutForTesting.has_value());

    WKContextUseTestingNetworkSession(contextRef);
    Require(pool->useTestingNetworkSession);

    const void* dummyString = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKContextSetAllowsAnySSLCertificateForWebSocketTesting(nullptr, true); });
    RequireThrows([&] { WKContextSetAllowsAnySSLCertificateForWebSocketTesting(dummyString, true); });

    RequireThrows([&] { WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting(nullptr, true); });
    RequireThrows([&] { WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting(dummyString, true); });

    RequireThrows([&] { WKContextSetServiceWorkerFetchTimeoutForTesting(nullptr, 10.0); });
    RequireThrows([&] { WKContextSetServiceWorkerFetchTimeoutForTesting(dummyString, 10.0); });

    RequireThrows([&] { WKContextResetServiceWorkerFetchTimeoutForTesting(nullptr); });
    RequireThrows([&] { WKContextResetServiceWorkerFetchTimeoutForTesting(dummyString); });

    RequireThrows([&] { WKContextUseTestingNetworkSession(nullptr); });
    RequireThrows([&] { WKContextUseTestingNetworkSession(dummyString); });

    WKRelease(dummyString);
    WKRelease(contextRef);
    return 0;
}
