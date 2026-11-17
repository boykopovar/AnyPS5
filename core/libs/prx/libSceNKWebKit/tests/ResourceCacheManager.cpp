#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

bool callbackCalled = false;
void OnOrigins(const void* arrayRef, const void* errorRef, void* context) {
    (void)errorRef;
    (void)context;
    callbackCalled = (arrayRef != nullptr);
    if (arrayRef) {
        Require(WKArrayGetSize(arrayRef) == 1);
        const void* item = WKArrayGetItemAtIndex(arrayRef, 0);
        Require(WKGetTypeID(item) == WKSecurityOriginGetTypeID());
        const void* host = WKSecurityOriginCopyHost(item);
        Require(WKStringIsEqualToUTF8CString(host, "example.org"));
        WKRelease(host);
        WKRelease(arrayRef);
    }
}

}

int main() {
    auto mgr = NKWebKit::Make<NKWebKit::ResourceCacheManager>();
    const void* mgrRef = mgr.Get();
    mgr->origins.push_back("example.com");

    WKResourceCacheManagerClearCacheForAllOrigins(mgrRef, 1);
    Require(mgr->clearedAll);
    Require(mgr->lastCachesCleared == 1);
    Require(mgr->origins.empty());

    mgr->origins.push_back("example.org");
    auto protoStr = NKWebKit::String::FromUtf8("https");
    auto hostStr = NKWebKit::String::FromUtf8("example.org");
    const void* secOriginRef = WKSecurityOriginCreate(protoStr.Get(), hostStr.Get(), 443);
    WKResourceCacheManagerClearCacheForOrigin(mgrRef, secOriginRef, 2);
    Require(mgr->origins.empty());
    Require(mgr->lastCachesCleared == 2);
    WKRelease(secOriginRef);

    mgr->origins.push_back("example.net");
    auto originStr = NKWebKit::String::FromUtf8("example.net");
    const void* originRef = originStr.Get();
    WKResourceCacheManagerClearCacheForOrigin(mgrRef, originRef, 3);
    Require(mgr->origins.empty());
    Require(mgr->lastCachesCleared == 3);
    WKResourceCacheManagerClearCacheForOrigin(mgrRef, nullptr, 3);

    auto badObj = NKWebKit::Make<NKWebKit::Array>();
    ExpectErr<std::invalid_argument>([&] { WKResourceCacheManagerClearCacheForOrigin(mgrRef, badObj.Get(), 1); });

    mgr->origins.push_back("example.org");
    WKResourceCacheManagerGetCacheOrigins(mgrRef, nullptr, OnOrigins);
    Require(callbackCalled);
    WKResourceCacheManagerGetCacheOrigins(mgrRef, nullptr, nullptr);

    ExpectErr<std::invalid_argument>([] { WKResourceCacheManagerClearCacheForAllOrigins(nullptr, 0); });
    ExpectErr<std::invalid_argument>([&] { WKResourceCacheManagerClearCacheForOrigin(nullptr, originRef, 0); });
    ExpectErr<std::invalid_argument>([] { WKResourceCacheManagerGetCacheOrigins(nullptr, nullptr, OnOrigins); });

    return 0;
}
