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

    Require(WKContextGetMaximumNumberOfProcesses(contextRef) == 0xffffffffu);
    Require(pool->maximumNumberOfProcesses == 0xffffffffu);
    Require(pool->processModel == 0);
    Require(pool->usesNetworkProcess);
    Require(!pool->diskCacheSpeculativeValidationEnabled);
    Require(pool->canHandleHTTPSServerTrustEvaluation);
    Require(!pool->customWebContentServiceBundleIdentifier);
    Require(pool->terminatedNetworkProcessesCount == 0);

    WKContextSetMaximumNumberOfProcesses(contextRef, 1);
    Require(WKContextGetMaximumNumberOfProcesses(contextRef) == 1);
    Require(pool->maximumNumberOfProcesses == 1);

    WKContextSetMaximumNumberOfProcesses(contextRef, 8);
    Require(WKContextGetMaximumNumberOfProcesses(contextRef) == 8);
    Require(pool->maximumNumberOfProcesses == 8);

    WKContextSetProcessModel(contextRef, 1);
    Require(pool->processModel == 1);

    WKContextSetProcessModel(contextRef, 0);
    Require(pool->processModel == 0);

    WKContextSetUsesNetworkProcess(contextRef, false);
    Require(!pool->usesNetworkProcess);

    WKContextSetUsesNetworkProcess(contextRef, true);
    Require(pool->usesNetworkProcess);

    WKContextSetDiskCacheSpeculativeValidationEnabled(contextRef, true);
    Require(pool->diskCacheSpeculativeValidationEnabled);

    WKContextSetDiskCacheSpeculativeValidationEnabled(contextRef, false);
    Require(!pool->diskCacheSpeculativeValidationEnabled);

    WKContextSetCanHandleHTTPSServerTrustEvaluation(contextRef, false);
    Require(!pool->canHandleHTTPSServerTrustEvaluation);

    WKContextSetCanHandleHTTPSServerTrustEvaluation(contextRef, true);
    Require(pool->canHandleHTTPSServerTrustEvaluation);

    const void* bundleId = WKStringCreateWithUTF8CString("com.playstation.webcontent");
    WKContextSetCustomWebContentServiceBundleIdentifier(contextRef, bundleId);
    Require(pool->customWebContentServiceBundleIdentifier.Get() != nullptr);
    Require(pool->customWebContentServiceBundleIdentifier->Utf8() == "com.playstation.webcontent");

    WKContextSetCustomWebContentServiceBundleIdentifier(contextRef, nullptr);
    Require(!pool->customWebContentServiceBundleIdentifier);

    WKContextTerminateNetworkProcess(contextRef);
    Require(pool->terminatedNetworkProcessesCount == 1);

    WKContextTerminateNetworkProcess(contextRef);
    Require(pool->terminatedNetworkProcessesCount == 2);

    const void* dummy = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKContextSetMaximumNumberOfProcesses(nullptr, 4); });
    RequireThrows([&] { WKContextSetMaximumNumberOfProcesses(dummy, 4); });

    RequireThrows([&] { WKContextGetMaximumNumberOfProcesses(nullptr); });
    RequireThrows([&] { WKContextGetMaximumNumberOfProcesses(dummy); });

    RequireThrows([&] { WKContextSetProcessModel(nullptr, 1); });
    RequireThrows([&] { WKContextSetProcessModel(dummy, 1); });

    RequireThrows([&] { WKContextSetUsesNetworkProcess(nullptr, true); });
    RequireThrows([&] { WKContextSetUsesNetworkProcess(dummy, true); });

    RequireThrows([&] { WKContextSetDiskCacheSpeculativeValidationEnabled(nullptr, true); });
    RequireThrows([&] { WKContextSetDiskCacheSpeculativeValidationEnabled(dummy, true); });

    RequireThrows([&] { WKContextSetCanHandleHTTPSServerTrustEvaluation(nullptr, true); });
    RequireThrows([&] { WKContextSetCanHandleHTTPSServerTrustEvaluation(dummy, true); });

    RequireThrows([&] { WKContextSetCustomWebContentServiceBundleIdentifier(nullptr, nullptr); });
    RequireThrows([&] { WKContextSetCustomWebContentServiceBundleIdentifier(dummy, nullptr); });
    RequireThrows([&] { WKContextSetCustomWebContentServiceBundleIdentifier(contextRef, contextRef); });

    RequireThrows([&] { WKContextTerminateNetworkProcess(nullptr); });
    RequireThrows([&] { WKContextTerminateNetworkProcess(dummy); });

    WKRelease(bundleId);
    WKRelease(dummy);
    WKRelease(contextRef);
    return 0;
}
