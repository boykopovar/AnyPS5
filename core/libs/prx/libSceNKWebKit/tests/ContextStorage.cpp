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

    Require(!pool->cachedCredentialsCleared);
    Require(!pool->legacyPrivateBrowsingLocalStorageCleared);
    Require(!pool->localStorageSynced);

    WKContextClearCachedCredentials(contextRef);
    Require(pool->cachedCredentialsCleared);

    WKContextClearLegacyPrivateBrowsingLocalStorage(contextRef);
    Require(pool->legacyPrivateBrowsingLocalStorageCleared);

    WKContextSyncLocalStorage(contextRef);
    Require(pool->localStorageSynced);

    const void* dummyString = WKStringCreateWithUTF8CString("dummy");

    RequireThrows([&] { WKContextClearCachedCredentials(nullptr); });
    RequireThrows([&] { WKContextClearCachedCredentials(dummyString); });

    RequireThrows([&] { WKContextClearLegacyPrivateBrowsingLocalStorage(nullptr); });
    RequireThrows([&] { WKContextClearLegacyPrivateBrowsingLocalStorage(dummyString); });

    RequireThrows([&] { WKContextSyncLocalStorage(nullptr); });
    RequireThrows([&] { WKContextSyncLocalStorage(dummyString); });

    WKRelease(dummyString);
    WKRelease(contextRef);
    return 0;
}
