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

    Require(pool->additionalPluginsDirectory.Get() == nullptr);
    Require(pool->refreshPlugInsCount == 0);
    Require(pool->supportedPlugins.empty());
    Require(pool->plugInAutoStartOrigins.Get() == nullptr);
    Require(pool->plugInAutoStartOriginsFilterTime == 0.0);
    Require(pool->plugInAutoStartOriginHashes.Get() == nullptr);

    const void* defaultHashes = WKContextCopyPlugInAutoStartOriginHashes(contextRef);
    Require(defaultHashes != nullptr);
    WKRelease(defaultHashes);

    const void* dir = WKStringCreateWithUTF8CString("/system/common/lib/plugins");
    WKContextSetAdditionalPluginsDirectory(contextRef, dir);
    Require(pool->additionalPluginsDirectory.Get() != nullptr);
    Require(WKStringIsEqualToUTF8CString(pool->additionalPluginsDirectory.Get(), "/system/common/lib/plugins"));
    WKContextSetAdditionalPluginsDirectory(contextRef, nullptr);
    Require(pool->additionalPluginsDirectory.Get() == nullptr);
    WKRelease(dir);

    WKContextRefreshPlugIns(contextRef);
    Require(pool->refreshPlugInsCount == 1);
    WKContextRefreshPlugIns(contextRef);
    Require(pool->refreshPlugInsCount == 2);

    const void* domain = WKStringCreateWithUTF8CString("example.com");
    const void* name = WKStringCreateWithUTF8CString("TestPlugin");
    const void* mime = WKStringCreateWithUTF8CString("application/x-test");
    const void* mimeValues[1] = { mime };
    const void* mimes = WKArrayCreate(mimeValues, 1);
    const void* ext = WKStringCreateWithUTF8CString("test");
    const void* extValues[1] = { ext };
    const void* exts = WKArrayCreate(extValues, 1);

    WKContextAddSupportedPlugin(contextRef, domain, name, mimes, exts);
    Require(pool->supportedPlugins.size() == 1);
    Require(pool->supportedPlugins[0].domain.Get() != nullptr);
    Require(pool->supportedPlugins[0].name.Get() != nullptr);
    Require(pool->supportedPlugins[0].mimeTypes.Get() != nullptr);
    Require(pool->supportedPlugins[0].extensions.Get() != nullptr);

    WKRelease(domain);
    WKRelease(name);
    WKRelease(mime);
    WKRelease(mimes);
    WKRelease(ext);
    WKRelease(exts);

    WKContextClearSupportedPlugins(contextRef);
    Require(pool->supportedPlugins.empty());

    const void* originArr = WKArrayCreate(nullptr, 0);
    WKContextSetPlugInAutoStartOrigins(contextRef, originArr);
    Require(pool->plugInAutoStartOrigins.Get() != nullptr);
    WKContextSetPlugInAutoStartOrigins(contextRef, nullptr);
    Require(pool->plugInAutoStartOrigins.Get() == nullptr);
    WKRelease(originArr);

    const void* originArr2 = WKArrayCreate(nullptr, 0);
    WKContextSetPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(contextRef, originArr2, 1234.5);
    Require(pool->plugInAutoStartOrigins.Get() != nullptr);
    Require(pool->plugInAutoStartOriginsFilterTime == 1234.5);
    WKContextSetPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(contextRef, nullptr, 9999.0);
    Require(pool->plugInAutoStartOrigins.Get() == nullptr);
    Require(pool->plugInAutoStartOriginsFilterTime == 9999.0);
    WKRelease(originArr2);

    const void* hashes = WKDictionaryCreate(nullptr, nullptr, 0);
    WKContextSetPlugInAutoStartOriginHashes(contextRef, hashes);
    Require(pool->plugInAutoStartOriginHashes.Get() != nullptr);

    const void* copiedHashes = WKContextCopyPlugInAutoStartOriginHashes(contextRef);
    Require(copiedHashes != nullptr);
    WKRelease(copiedHashes);

    WKContextSetPlugInAutoStartOriginHashes(contextRef, nullptr);
    Require(pool->plugInAutoStartOriginHashes.Get() == nullptr);
    WKRelease(hashes);

    const void* dummyArray = WKArrayCreate(nullptr, 0);
    RequireThrows([&] { WKContextSetAdditionalPluginsDirectory(nullptr, nullptr); });
    RequireThrows([&] { WKContextSetAdditionalPluginsDirectory(dummyArray, nullptr); });
    RequireThrows([&] { WKContextRefreshPlugIns(nullptr); });
    RequireThrows([&] { WKContextRefreshPlugIns(dummyArray); });
    RequireThrows([&] { WKContextClearSupportedPlugins(nullptr); });
    RequireThrows([&] { WKContextClearSupportedPlugins(dummyArray); });
    RequireThrows([&] { WKContextAddSupportedPlugin(nullptr, nullptr, nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKContextAddSupportedPlugin(dummyArray, nullptr, nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKContextSetPlugInAutoStartOriginHashes(nullptr, nullptr); });
    RequireThrows([&] { WKContextSetPlugInAutoStartOriginHashes(dummyArray, nullptr); });
    RequireThrows([&] { WKContextSetPlugInAutoStartOrigins(nullptr, nullptr); });
    RequireThrows([&] { WKContextSetPlugInAutoStartOrigins(dummyArray, nullptr); });
    RequireThrows([&] { WKContextSetPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(nullptr, nullptr, 0.0); });
    RequireThrows([&] { WKContextSetPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(dummyArray, nullptr, 0.0); });
    RequireThrows([&] { WKContextCopyPlugInAutoStartOriginHashes(nullptr); });
    RequireThrows([&] { WKContextCopyPlugInAutoStartOriginHashes(dummyArray); });
    WKRelease(dummyArray);

    WKRelease(contextRef);
    return 0;
}
