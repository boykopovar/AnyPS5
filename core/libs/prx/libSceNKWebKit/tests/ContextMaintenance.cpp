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
        std::fprintf(stderr, "ContextMaintenance check failed at line %u\n", location.line());
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
    auto pool = NKWebKit::Make<NKWebKit::ProcessPool>();
    const void* poolRef = NKWebKit::ToRef(pool);

    Require(pool->terminateNetworkProcessCount == 0);
    Require(pool->syncLocalStorageCount == 0);
    Require(pool->clearCachedCredentialsCount == 0);
    Require(pool->clearLegacyPrivateBrowsingLocalStorageCount == 0);
    Require(!pool->usesTestingNetworkSession);
    Require(pool->clearCurrentModifierStateCount == 0);

    WKContextTerminateNetworkProcess(poolRef);
    Require(pool->terminateNetworkProcessCount == 1);
    WKContextTerminateNetworkProcess(poolRef);
    Require(pool->terminateNetworkProcessCount == 2);

    WKContextSyncLocalStorage(poolRef);
    Require(pool->syncLocalStorageCount == 1);
    WKContextSyncLocalStorage(poolRef);
    Require(pool->syncLocalStorageCount == 2);

    WKContextClearCachedCredentials(poolRef);
    Require(pool->clearCachedCredentialsCount == 1);
    WKContextClearCachedCredentials(poolRef);
    Require(pool->clearCachedCredentialsCount == 2);

    WKContextClearLegacyPrivateBrowsingLocalStorage(poolRef);
    Require(pool->clearLegacyPrivateBrowsingLocalStorageCount == 1);
    WKContextClearLegacyPrivateBrowsingLocalStorage(poolRef);
    Require(pool->clearLegacyPrivateBrowsingLocalStorageCount == 2);

    WKContextUseTestingNetworkSession(poolRef);
    Require(pool->usesTestingNetworkSession);

    WKContextClearCurrentModifierStateForTesting(poolRef);
    Require(pool->clearCurrentModifierStateCount == 1);
    WKContextClearCurrentModifierStateForTesting(poolRef);
    Require(pool->clearCurrentModifierStateCount == 2);

    RequireInvalidArgument([] { WKContextTerminateNetworkProcess(nullptr); });
    RequireInvalidArgument([] { WKContextSyncLocalStorage(nullptr); });
    RequireInvalidArgument([] { WKContextClearCachedCredentials(nullptr); });
    RequireInvalidArgument([] { WKContextClearLegacyPrivateBrowsingLocalStorage(nullptr); });
    RequireInvalidArgument([] { WKContextUseTestingNetworkSession(nullptr); });
    RequireInvalidArgument([] { WKContextClearCurrentModifierStateForTesting(nullptr); });

    WKRelease(poolRef);
    std::puts("libSceNKWebKit ContextMaintenance tests passed");
    return 0;
}
