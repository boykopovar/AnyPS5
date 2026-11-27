#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace NKWebKit {
WKContextInvalidMessageFunction GetInvalidMessageFunction();
}

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

static std::string g_lastInvalidMessage;
static void APS5_VABI HandleInvalidMessage(const void* messageNameRef) {
    if (messageNameRef) {
        const auto* str = NKWebKit::Cast<NKWebKit::String>(messageNameRef, "HandleInvalidMessage");
        g_lastInvalidMessage = str->Utf8();
    } else {
        g_lastInvalidMessage.clear();
    }
}

int main() {
    const void* contextRef = WKContextCreate();
    Require(contextRef != nullptr);
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);

    Require(!pool->memorySamplerRunning);
    Require(pool->memorySamplerInterval == 0.0);

    const void* validInterval = WKDoubleCreate(2.5);
    WKContextStartMemorySampler(contextRef, validInterval);
    Require(pool->memorySamplerRunning);
    Require(pool->memorySamplerInterval == 2.5);

    WKContextStopMemorySampler(contextRef);
    Require(!pool->memorySamplerRunning);
    Require(pool->memorySamplerInterval == 0.0);

    const void* zeroInterval = WKDoubleCreate(0.0);
    RequireThrows([&] { WKContextStartMemorySampler(contextRef, zeroInterval); });
    const void* negativeInterval = WKDoubleCreate(-1.0);
    RequireThrows([&] { WKContextStartMemorySampler(contextRef, negativeInterval); });
    const void* nanInterval = WKDoubleCreate(std::numeric_limits<double>::quiet_NaN());
    RequireThrows([&] { WKContextStartMemorySampler(contextRef, nanInterval); });
    const void* infInterval = WKDoubleCreate(std::numeric_limits<double>::infinity());
    RequireThrows([&] { WKContextStartMemorySampler(contextRef, infInterval); });

    const void* invalidTypeObj = WKContextConfigurationCreate();
    RequireThrows([&] { WKContextStartMemorySampler(contextRef, invalidTypeObj); });
    RequireThrows([&] { WKContextStartMemorySampler(contextRef, nullptr); });
    RequireThrows([&] { WKContextStartMemorySampler(nullptr, validInterval); });
    RequireThrows([&] { WKContextStopMemorySampler(nullptr); });

    Require(NKWebKit::GetInvalidMessageFunction() == nullptr);
    WKContextSetInvalidMessageFunction(&HandleInvalidMessage);
    Require(NKWebKit::GetInvalidMessageFunction() == &HandleInvalidMessage);

    const void* testMsg = WKStringCreateWithUTF8CString("TestInvalidMessageName");
    NKWebKit::GetInvalidMessageFunction()(testMsg);
    Require(g_lastInvalidMessage == "TestInvalidMessageName");

    WKContextSetInvalidMessageFunction(nullptr);
    Require(NKWebKit::GetInvalidMessageFunction() == nullptr);

    const void* schemeA = WKStringCreateWithUTF8CString("custom-relax-a");
    const void* schemeB = WKStringCreateWithUTF8CString("custom-relax-b");
    WKContextSetDomainRelaxationForbiddenForURLScheme(contextRef, schemeA);
    WKContextSetDomainRelaxationForbiddenForURLScheme(contextRef, schemeA);
    Require(pool->domainRelaxationForbiddenSchemes.size() == 1);
    Require(pool->domainRelaxationForbiddenSchemes[0] == "custom-relax-a");

    WKContextSetDomainRelaxationForbiddenForURLScheme(contextRef, schemeB);
    Require(pool->domainRelaxationForbiddenSchemes.size() == 2);
    Require(pool->domainRelaxationForbiddenSchemes[1] == "custom-relax-b");

    RequireThrows([&] { WKContextSetDomainRelaxationForbiddenForURLScheme(nullptr, schemeA); });
    RequireThrows([&] { WKContextSetDomainRelaxationForbiddenForURLScheme(contextRef, nullptr); });
    RequireThrows([&] { WKContextSetDomainRelaxationForbiddenForURLScheme(contextRef, validInterval); });

    Require(!pool->allowsAnySSLCertificateForServiceWorkerTesting);
    WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting(contextRef, true);
    Require(pool->allowsAnySSLCertificateForServiceWorkerTesting);
    WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting(contextRef, false);
    Require(!pool->allowsAnySSLCertificateForServiceWorkerTesting);
    RequireThrows([&] { WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting(nullptr, true); });

    Require(!pool->allowsAnySSLCertificateForWebSocketTesting);
    WKContextSetAllowsAnySSLCertificateForWebSocketTesting(contextRef, true);
    Require(pool->allowsAnySSLCertificateForWebSocketTesting);
    WKContextSetAllowsAnySSLCertificateForWebSocketTesting(contextRef, false);
    Require(!pool->allowsAnySSLCertificateForWebSocketTesting);
    RequireThrows([&] { WKContextSetAllowsAnySSLCertificateForWebSocketTesting(nullptr, true); });

    Require(!pool->serviceWorkerFetchTimeout.has_value());
    WKContextSetServiceWorkerFetchTimeoutForTesting(contextRef, 15.0);
    Require(pool->serviceWorkerFetchTimeout.has_value());
    Require(*pool->serviceWorkerFetchTimeout == 15.0);

    WKContextResetServiceWorkerFetchTimeoutForTesting(contextRef);
    Require(!pool->serviceWorkerFetchTimeout.has_value());

    RequireThrows([&] { WKContextSetServiceWorkerFetchTimeoutForTesting(contextRef, 0.0); });
    RequireThrows([&] { WKContextSetServiceWorkerFetchTimeoutForTesting(contextRef, -5.0); });
    RequireThrows([&] { WKContextSetServiceWorkerFetchTimeoutForTesting(contextRef, std::numeric_limits<double>::quiet_NaN()); });
    RequireThrows([&] { WKContextSetServiceWorkerFetchTimeoutForTesting(contextRef, std::numeric_limits<double>::infinity()); });
    RequireThrows([&] { WKContextSetServiceWorkerFetchTimeoutForTesting(nullptr, 10.0); });
    RequireThrows([&] { WKContextResetServiceWorkerFetchTimeoutForTesting(nullptr); });

    Require(!pool->usesTestingNetworkSession);
    WKContextUseTestingNetworkSession(contextRef);
    Require(pool->usesTestingNetworkSession);
    RequireThrows([&] { WKContextUseTestingNetworkSession(nullptr); });

    Require(pool->privilegedNetworkBandwidth == 0);
    WKContextSetPrivilegedNetworkBandwidth(contextRef, 1048576u);
    Require(pool->privilegedNetworkBandwidth == 1048576u);
    RequireThrows([&] { WKContextSetPrivilegedNetworkBandwidth(nullptr, 500u); });

    WKRelease(contextRef);
    WKRelease(validInterval);
    WKRelease(zeroInterval);
    WKRelease(negativeInterval);
    WKRelease(nanInterval);
    WKRelease(infInterval);
    WKRelease(invalidTypeObj);
    WKRelease(schemeA);
    WKRelease(schemeB);
    WKRelease(testMsg);

    return 0;
}
