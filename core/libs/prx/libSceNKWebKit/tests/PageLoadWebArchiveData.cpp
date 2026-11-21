#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage load web archive data check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);

    Require(!page->loadedWebArchiveData.has_value());
    Require(page->loadUserData == nullptr);

    const unsigned char sampleArchive[] = { 0x62, 0x70, 0x6c, 0x69, 0x73, 0x74, 0x30, 0x30 };
    const void* dataRef = WKDataCreate(sampleArchive, sizeof(sampleArchive));

    WKPageLoadWebArchiveData(pageRef, dataRef);
    Require(page->loadedWebArchiveData.has_value());
    Require(page->loadedWebArchiveData->size() == sizeof(sampleArchive));
    Require((*page->loadedWebArchiveData)[0] == 0x62);
    Require((*page->loadedWebArchiveData)[7] == 0x30);
    Require(page->loadUserData == nullptr);

    int userData = 888;
    WKPageLoadWebArchiveDataWithUserData(pageRef, dataRef, &userData);
    Require(page->loadedWebArchiveData.has_value());
    Require(page->loadUserData == &userData);

    page->isClosed = true;
    page->loadedWebArchiveData.reset();
    page->loadUserData = nullptr;

    WKPageLoadWebArchiveData(pageRef, dataRef);
    Require(!page->loadedWebArchiveData.has_value());

    WKPageLoadWebArchiveDataWithUserData(pageRef, dataRef, &userData);
    Require(!page->loadedWebArchiveData.has_value());
    Require(page->loadUserData == nullptr);

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([dataRef] { WKPageLoadWebArchiveData(nullptr, dataRef); });
    RequireInvalidArgument([prefsRef, dataRef] { WKPageLoadWebArchiveData(prefsRef, dataRef); });
    RequireInvalidArgument([pageRef] { WKPageLoadWebArchiveData(pageRef, nullptr); });
    RequireInvalidArgument([pageRef, prefsRef] { WKPageLoadWebArchiveData(pageRef, prefsRef); });

    RequireInvalidArgument([dataRef, &userData] { WKPageLoadWebArchiveDataWithUserData(nullptr, dataRef, &userData); });
    RequireInvalidArgument([prefsRef, dataRef, &userData] { WKPageLoadWebArchiveDataWithUserData(prefsRef, dataRef, &userData); });
    RequireInvalidArgument([pageRef, &userData] { WKPageLoadWebArchiveDataWithUserData(pageRef, nullptr, &userData); });
    RequireInvalidArgument([pageRef, prefsRef, &userData] { WKPageLoadWebArchiveDataWithUserData(pageRef, prefsRef, &userData); });

    WKRelease(prefsRef);
    WKRelease(dataRef);
    WKRelease(pageRef);
    return 0;
}
