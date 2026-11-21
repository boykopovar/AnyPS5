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
        std::fprintf(stderr, "WKPage media check failed at line %u\n", location.line());
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

    Require(WKPageGetMediaState(pageRef) == 0);
    Require(page->mediaVolume == 1.0f);
    Require(page->mutedState == 0);
    Require(page->mayStartMediaWhenInWindow);

    WKPageSetMediaVolume(pageRef, 0.5f);
    Require(page->mediaVolume == 0.5f);

    WKPageSetMediaVolume(pageRef, 0.0f);
    Require(page->mediaVolume == 0.0f);

    WKPageSetMuted(pageRef, 1);
    Require(page->mutedState == 1);

    WKPageSetMuted(pageRef, 3);
    Require(page->mutedState == 3);

    WKPageSetMayStartMediaWhenInWindow(pageRef, false);
    Require(!page->mayStartMediaWhenInWindow);

    WKPageSetMayStartMediaWhenInWindow(pageRef, true);
    Require(page->mayStartMediaWhenInWindow);

    page->mediaState = 0x10;
    Require(WKPageGetMediaState(pageRef) == 0x10);

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([] { WKPageGetMediaState(nullptr); });
    RequireInvalidArgument([prefsRef] { WKPageGetMediaState(prefsRef); });

    RequireInvalidArgument([] { WKPageSetMayStartMediaWhenInWindow(nullptr, false); });
    RequireInvalidArgument([prefsRef] { WKPageSetMayStartMediaWhenInWindow(prefsRef, false); });

    RequireInvalidArgument([] { WKPageSetMediaVolume(nullptr, 0.5f); });
    RequireInvalidArgument([prefsRef] { WKPageSetMediaVolume(prefsRef, 0.5f); });

    RequireInvalidArgument([] { WKPageSetMuted(nullptr, 1); });
    RequireInvalidArgument([prefsRef] { WKPageSetMuted(prefsRef, 1); });

    WKRelease(prefsRef);
    WKRelease(pageRef);
    return 0;
}
