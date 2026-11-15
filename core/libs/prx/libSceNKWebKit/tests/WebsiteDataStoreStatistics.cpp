#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WebsiteDataStoreStatistics check failed at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    auto config = NKWebKit::Make<NKWebKit::WebsiteDataStoreConfiguration>();
    auto store = NKWebKit::Make<NKWebKit::WebsiteDataStore>(std::move(config));
    const void* storeRef = store.Get();

    Require(!WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(storeRef));

    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(storeRef, true);
    Require(WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(storeRef) == true);

    WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(storeRef, false);
    Require(WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(storeRef) == false);

    bool threw = false;
    try {
        WKWebsiteDataStoreGetResourceLoadStatisticsEnabled(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKWebsiteDataStoreSetResourceLoadStatisticsEnabled(nullptr, true);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    return 0;
}
