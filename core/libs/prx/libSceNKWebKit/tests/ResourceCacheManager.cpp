#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ResourceCacheManager check failed at line %u\n", location.line());
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

bool callbackCalled = false;
void OnOrigins(const void* arrayRef, const void* errorRef, void* context) {
    (void)errorRef;
    (void)context;
    callbackCalled = (arrayRef != nullptr);
    if (arrayRef) {
        WKRelease(arrayRef);
    }
}

}

int main() {
    auto mgr = NKWebKit::Make<NKWebKit::ResourceCacheManager>();
    const void* mgrRef = NKWebKit::ToRef(mgr);
    mgr->origins.push_back("example.com");

    WKResourceCacheManagerClearCacheForAllOrigins(mgrRef, 1);
    Require(mgr->clearedAll);
    Require(mgr->lastCachesCleared == 1);
    Require(mgr->origins.empty());

    mgr->origins.push_back("example.org");
    auto originStr = NKWebKit::String::FromUtf8("example.org");
    const void* originRef = NKWebKit::ToRef(originStr);
    WKResourceCacheManagerClearCacheForOrigin(mgrRef, originRef, 2);
    Require(mgr->origins.empty());
    Require(mgr->lastCachesCleared == 2);
    WKResourceCacheManagerClearCacheForOrigin(mgrRef, nullptr, 2);

    WKResourceCacheManagerGetCacheOrigins(mgrRef, nullptr, OnOrigins);
    Require(callbackCalled);
    WKResourceCacheManagerGetCacheOrigins(mgrRef, nullptr, nullptr);

    RequireInvalidArgument([] { WKResourceCacheManagerClearCacheForAllOrigins(nullptr, 0); });
    RequireInvalidArgument([&] { WKResourceCacheManagerClearCacheForOrigin(nullptr, originRef, 0); });
    RequireInvalidArgument([] { WKResourceCacheManagerGetCacheOrigins(nullptr, nullptr, OnOrigins); });

    WKRelease(originRef);
    WKRelease(mgrRef);
    return 0;
}
