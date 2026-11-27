#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

void APS5_VABI WKWebsiteDataStoreEnableDefaultNetworkProxySettings(const void* dataStoreRef) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    store->networkProxyMode = NKWebKit::WebsiteDataStore::NetworkProxyMode::Default;
    store->customProxyUrl.clear();
    store->customProxyIgnoreHosts.clear();
}

void APS5_VABI WKWebsiteDataStoreEnableCustomNetworkProxySettings(const void* dataStoreRef, const void* proxyUrlRef, const void* ignoreHostsRef) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(proxyUrlRef, __func__);
    const auto* ignoreHosts = ignoreHostsRef ? NKWebKit::Cast<NKWebKit::String>(ignoreHostsRef, __func__) : nullptr;
    store->networkProxyMode = NKWebKit::WebsiteDataStore::NetworkProxyMode::Custom;
    store->customProxyUrl = url->text;
    store->customProxyIgnoreHosts = ignoreHosts ? ignoreHosts->Utf8() : std::string();
}

void APS5_VABI WKWebsiteDataStoreDisableNetworkProxySettings(const void* dataStoreRef) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    store->networkProxyMode = NKWebKit::WebsiteDataStore::NetworkProxyMode::NoProxy;
    store->customProxyUrl.clear();
    store->customProxyIgnoreHosts.clear();
}

void APS5_VABI WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcess(const void* dataStoreRef, const void* bundleIdentifierRef, void* context, WKWebsiteDataStoreUpdateBundleIdentifierInNetworkProcessFunction completionHandler) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    const auto* bundleId = NKWebKit::Cast<NKWebKit::String>(bundleIdentifierRef, __func__);
    store->networkProcessBundleIdentifier = bundleId->Utf8();
    if (completionHandler) {
        completionHandler(context);
    }
}

void APS5_VABI WKWebsiteDataStoreClearBundleIdentifierInNetworkProcess(const void* dataStoreRef, void* context, WKWebsiteDataStoreClearBundleIdentifierInNetworkProcessFunction completionHandler) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(dataStoreRef, __func__);
    store->networkProcessBundleIdentifier.clear();
    if (completionHandler) {
        completionHandler(context);
    }
}

}
