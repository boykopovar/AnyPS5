#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(cond) Check((cond), __LINE__)

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

namespace {

struct CallbackState {
    bool called = false;
    void* context = nullptr;
};

void APS5_VABI TestCompletionHandler(void* context) {
    auto* state = static_cast<CallbackState*>(context);
    state->called = true;
    state->context = context;
}

}

int main() {
    auto storeObj = NKWebKit::Make<NKWebKit::WebsiteDataStore>();
    const void* storeRef = NKWebKit::ToRef(storeObj);
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(storeRef, __func__);

    Require(store->networkProxyMode == NKWebKit::WebsiteDataStore::NetworkProxyMode::Default);
    Require(store->customProxyUrl.empty());
    Require(store->customProxyIgnoreHosts.empty());
    Require(store->networkProcessBundleIdentifier.empty());

    auto proxyUrl = NKWebKit::Make<NKWebKit::URL>("http://proxy.example.com:8080");
    const void* proxyUrlRef = NKWebKit::ToRef(proxyUrl);
    const void* ignoreHostsRef = WKStringCreateWithUTF8CString("*.local,127.0.0.1");

    WKWebsiteDataStoreEnableCustomNetworkProxySettings(storeRef, proxyUrlRef, ignoreHostsRef);
    Require(store->networkProxyMode == NKWebKit::WebsiteDataStore::NetworkProxyMode::Custom);
    Require(store->customProxyUrl == "http://proxy.example.com:8080");
    Require(store->customProxyIgnoreHosts == "*.local,127.0.0.1");

    WKWebsiteDataStoreEnableCustomNetworkProxySettings(storeRef, proxyUrlRef, nullptr);
    Require(store->networkProxyMode == NKWebKit::WebsiteDataStore::NetworkProxyMode::Custom);
    Require(store->customProxyUrl == "http://proxy.example.com:8080");
    Require(store->customProxyIgnoreHosts.empty());

    WKWebsiteDataStoreDisableNetworkProxySettings(storeRef);
    Require(store->networkProxyMode == NKWebKit::WebsiteDataStore::NetworkProxyMode::NoProxy);
    Require(store->customProxyUrl.empty());
    Require(store->customProxyIgnoreHosts.empty());

    WKWebsiteDataStoreEnableDefaultNetworkProxySettings(storeRef);
    Require(store->networkProxyMode == NKWebKit::WebsiteDataStore::NetworkProxyMode::Default);
    Require(store->customProxyUrl.empty());
    Require(store->customProxyIgnoreHosts.empty());

    const void* bundleIdRef = WKStringCreateWithUTF8CString("com.playstation.anyps5");
    CallbackState updateState;
    WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(storeRef, bundleIdRef, &updateState, TestCompletionHandler);
    Require(updateState.called);
    Require(updateState.context == &updateState);
    Require(store->networkProcessBundleIdentifier == "com.playstation.anyps5");

    CallbackState clearState;
    WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(storeRef, &clearState, TestCompletionHandler);
    Require(clearState.called);
    Require(clearState.context == &clearState);
    Require(store->networkProcessBundleIdentifier.empty());

    WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(storeRef, bundleIdRef, nullptr, nullptr);
    Require(store->networkProcessBundleIdentifier == "com.playstation.anyps5");

    WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(storeRef, nullptr, nullptr);
    Require(store->networkProcessBundleIdentifier.empty());

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireThrows([&] { WKWebsiteDataStoreEnableDefaultNetworkProxySettings(nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreEnableDefaultNetworkProxySettings(prefsRef); });

    RequireThrows([&] { WKWebsiteDataStoreEnableCustomNetworkProxySettings(nullptr, proxyUrlRef, ignoreHostsRef); });
    RequireThrows([&] { WKWebsiteDataStoreEnableCustomNetworkProxySettings(prefsRef, proxyUrlRef, ignoreHostsRef); });
    RequireThrows([&] { WKWebsiteDataStoreEnableCustomNetworkProxySettings(storeRef, nullptr, ignoreHostsRef); });
    RequireThrows([&] { WKWebsiteDataStoreEnableCustomNetworkProxySettings(storeRef, prefsRef, ignoreHostsRef); });
    RequireThrows([&] { WKWebsiteDataStoreEnableCustomNetworkProxySettings(storeRef, proxyUrlRef, prefsRef); });

    RequireThrows([&] { WKWebsiteDataStoreDisableNetworkProxySettings(nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreDisableNetworkProxySettings(prefsRef); });

    RequireThrows([&] { WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(nullptr, bundleIdRef, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(prefsRef, bundleIdRef, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(storeRef, nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(storeRef, prefsRef, nullptr, nullptr); });

    RequireThrows([&] { WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(nullptr, nullptr, nullptr); });
    RequireThrows([&] { WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(prefsRef, nullptr, nullptr); });

    WKRelease(prefsRef);
    WKRelease(bundleIdRef);
    WKRelease(ignoreHostsRef);
    WKRelease(proxyUrlRef);
    WKRelease(storeRef);

    return 0;
}
