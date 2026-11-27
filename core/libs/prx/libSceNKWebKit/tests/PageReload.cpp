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
        std::fprintf(stderr, "WKPage reload check failed at line %u\n", location.line());
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

    Require(page->reloadCount == 0);
    Require(!page->lastReloadMode.has_value());
    Require(!page->provisionalURL);

    WKPageReload(pageRef);
    Require(page->reloadCount == 1);
    Require(page->lastReloadMode == NKWebKit::ReloadMode::Normal);
    Require(!page->provisionalURL);

    auto committedURL = NKWebKit::Make<NKWebKit::URL>("https://example.com");
    page->committedURL = committedURL;

    WKPageReload(pageRef);
    Require(page->reloadCount == 2);
    Require(page->lastReloadMode == NKWebKit::ReloadMode::Normal);
    Require(page->provisionalURL && page->provisionalURL->text == "https://example.com");

    WKPageStopLoading(pageRef);
    Require(!page->provisionalURL);

    WKPageReloadFromOrigin(pageRef);
    Require(page->reloadCount == 3);
    Require(page->lastReloadMode == NKWebKit::ReloadMode::FromOrigin);
    Require(page->provisionalURL && page->provisionalURL->text == "https://example.com");

    WKPageReloadExpiredOnly(pageRef);
    Require(page->reloadCount == 4);
    Require(page->lastReloadMode == NKWebKit::ReloadMode::ExpiredOnly);
    Require(page->provisionalURL && page->provisionalURL->text == "https://example.com");

    WKPageReloadWithoutContentBlockers(pageRef);
    Require(page->reloadCount == 5);
    Require(page->lastReloadMode == NKWebKit::ReloadMode::WithoutContentBlockers);
    Require(page->provisionalURL && page->provisionalURL->text == "https://example.com");

    page->committedURL = nullptr;
    page->provisionalURL = nullptr;
    auto activeURL = NKWebKit::Make<NKWebKit::URL>("https://example.com/active");
    page->activeURL = activeURL;

    WKPageReload(pageRef);
    Require(page->reloadCount == 6);
    Require(page->provisionalURL && page->provisionalURL->text == "https://example.com/active");

    page->isClosed = true;
    WKPageReload(pageRef);
    Require(page->reloadCount == 6);
    WKPageReloadFromOrigin(pageRef);
    Require(page->reloadCount == 6);
    WKPageReloadExpiredOnly(pageRef);
    Require(page->reloadCount == 6);
    WKPageReloadWithoutContentBlockers(pageRef);
    Require(page->reloadCount == 6);

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([] { WKPageReload(nullptr); });
    RequireInvalidArgument([prefsRef] { WKPageReload(prefsRef); });

    RequireInvalidArgument([] { WKPageReloadExpiredOnly(nullptr); });
    RequireInvalidArgument([prefsRef] { WKPageReloadExpiredOnly(prefsRef); });

    RequireInvalidArgument([] { WKPageReloadFromOrigin(nullptr); });
    RequireInvalidArgument([prefsRef] { WKPageReloadFromOrigin(prefsRef); });

    RequireInvalidArgument([] { WKPageReloadWithoutContentBlockers(nullptr); });
    RequireInvalidArgument([prefsRef] { WKPageReloadWithoutContentBlockers(prefsRef); });

    RequireInvalidArgument([] { WKPageStopLoading(nullptr); });
    RequireInvalidArgument([prefsRef] { WKPageStopLoading(prefsRef); });

    WKRelease(prefsRef);
    WKRelease(pageRef);
    return 0;
}
