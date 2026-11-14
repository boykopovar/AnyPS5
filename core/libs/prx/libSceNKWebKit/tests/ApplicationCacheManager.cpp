#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ApplicationCacheManager check failed at line %u\n", location.line());
        std::abort();
    }
}

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
