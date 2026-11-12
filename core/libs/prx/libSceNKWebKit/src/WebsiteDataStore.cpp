#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"
#include <mutex>

namespace {

NKWebKit::WebsiteDataStore* DefaultStore() {
    static NKWebKit::WebsiteDataStore* store = [] {
        auto config = NKWebKit::Make<NKWebKit::WebsiteDataStoreConfiguration>();
        return NKWebKit::Make<NKWebKit::WebsiteDataStore>(std::move(config)).Leak();
    }();
    return store;
}

}

extern "C" {

const void* APS5_VABI WKWebsiteDataStoreGetDefaultDataStore() {
    return static_cast<const void*>(DefaultStore());
}

const void* APS5_VABI WKWebsiteDataStoreCreateNonPersistentDataStore() {
    auto config = NKWebKit::Make<NKWebKit::WebsiteDataStoreConfiguration>();
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::WebsiteDataStore>(std::move(config)));
}

const void* APS5_VABI WKWebsiteDataStoreCreateWithConfiguration(const void* configuration) {
    auto* config = NKWebKit::Cast<NKWebKit::WebsiteDataStoreConfiguration>(configuration, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::WebsiteDataStore>(
        NKWebKit::Retained<NKWebKit::WebsiteDataStoreConfiguration>::Share(config)));
}

const void* APS5_VABI WKWebsiteDataStoreGetHTTPCookieStore(const void* websiteDataStore) {
    const auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(websiteDataStore, __func__);
    return static_cast<const void*>(store->cookieStore.Get());
}

}
