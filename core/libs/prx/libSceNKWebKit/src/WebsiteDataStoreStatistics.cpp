#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

bool APS5_VABI WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(const void* websiteDataStore) {
    const auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(websiteDataStore, __func__);
    return store->resourceLoadStatisticsEnabled;
}

void APS5_VABI WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(const void* websiteDataStore, bool enabled) {
    auto* store = NKWebKit::Cast<NKWebKit::WebsiteDataStore>(websiteDataStore, __func__);
    store->resourceLoadStatisticsEnabled = enabled;
}

}
