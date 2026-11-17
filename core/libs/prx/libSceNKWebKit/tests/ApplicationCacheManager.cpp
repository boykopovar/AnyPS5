#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void OnGetApplicationCacheOrigins(const void*, const void*, void* context) {
    if (context) {
        ++(*static_cast<int*>(context));
    }
}

}

int main() {
    int dummyManager = 0;
    int dummyOrigin = 0;

    WKApplicationCacheManagerDeleteAllEntries(nullptr);
    WKApplicationCacheManagerDeleteAllEntries(&dummyManager);

    WKApplicationCacheManagerDeleteEntriesForOrigin(nullptr, nullptr);
    WKApplicationCacheManagerDeleteEntriesForOrigin(&dummyManager, nullptr);
    WKApplicationCacheManagerDeleteEntriesForOrigin(&dummyManager, &dummyOrigin);

    int callbackCount = 0;
    WKApplicationCacheManagerGetApplicationCacheOrigins(nullptr, nullptr, nullptr);
    WKApplicationCacheManagerGetApplicationCacheOrigins(nullptr, &callbackCount, OnGetApplicationCacheOrigins);
    WKApplicationCacheManagerGetApplicationCacheOrigins(&dummyManager, &callbackCount, OnGetApplicationCacheOrigins);
    Require(callbackCount == 0);

    return 0;
}
