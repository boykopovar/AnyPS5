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
        std::fprintf(stderr, "WKPage load alternate HTML check failed at line %u\n", location.line());
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

    Require(!page->loadedAlternateHTML.has_value());
    Require(!page->loadedBaseURL);
    Require(!page->unreachableURL);
    Require(!page->provisionalURL);
    Require(!page->activeURL);
    Require(page->loadUserData == nullptr);

    auto html = NKWebKit::String::FromUtf8("<h1>Error</h1>");
    const void* htmlRef = NKWebKit::ToRef(html);
    auto base = NKWebKit::Make<NKWebKit::URL>("https://example.com/base");
    const void* baseRef = NKWebKit::ToRef(base);
    auto unreachable = NKWebKit::Make<NKWebKit::URL>("https://example.com/unreachable");
    const void* unreachableRef = NKWebKit::ToRef(unreachable);

    WKPageLoadAlternateHTMLString(pageRef, htmlRef, baseRef, unreachableRef);
    Require(page->loadedAlternateHTML.has_value());
    Require(*page->loadedAlternateHTML == u"<h1>Error</h1>");
    Require(static_cast<bool>(page->loadedBaseURL));
    Require(page->loadedBaseURL->text == "https://example.com/base");
    Require(static_cast<bool>(page->unreachableURL));
    Require(page->unreachableURL->text == "https://example.com/unreachable");
    Require(page->provisionalURL && page->provisionalURL->text == "https://example.com/unreachable");
    Require(page->activeURL && page->activeURL->text == "https://example.com/unreachable");
    Require(page->loadUserData == nullptr);

    WKPageLoadAlternateHTMLString(pageRef, htmlRef, baseRef, nullptr);
    Require(page->loadedAlternateHTML.has_value());
    Require(static_cast<bool>(page->loadedBaseURL));
    Require(!page->unreachableURL);
    Require(page->provisionalURL && page->provisionalURL->text == "https://example.com/base");
    Require(page->activeURL && page->activeURL->text == "https://example.com/base");

    WKPageLoadAlternateHTMLString(pageRef, htmlRef, nullptr, nullptr);
    Require(page->loadedAlternateHTML.has_value());
    Require(!page->loadedBaseURL);
    Require(!page->unreachableURL);
    Require(!page->provisionalURL);
    Require(!page->activeURL);

    int userData = 321;
    WKPageLoadAlternateHTMLStringWithUserData(pageRef, htmlRef, baseRef, unreachableRef, &userData);
    Require(page->loadedAlternateHTML.has_value());
    Require(static_cast<bool>(page->loadedBaseURL));
    Require(static_cast<bool>(page->unreachableURL));
    Require(page->loadUserData == &userData);

    page->isClosed = true;
    page->loadedAlternateHTML.reset();
    page->loadedBaseURL = nullptr;
    page->unreachableURL = nullptr;
    page->provisionalURL = nullptr;
    page->activeURL = nullptr;
    page->loadUserData = nullptr;

    WKPageLoadAlternateHTMLString(pageRef, htmlRef, baseRef, unreachableRef);
    Require(!page->loadedAlternateHTML.has_value());
    Require(!page->loadedBaseURL);
    Require(!page->unreachableURL);

    WKPageLoadAlternateHTMLStringWithUserData(pageRef, htmlRef, baseRef, unreachableRef, &userData);
    Require(!page->loadedAlternateHTML.has_value());
    Require(page->loadUserData == nullptr);

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([htmlRef, baseRef, unreachableRef] { WKPageLoadAlternateHTMLString(nullptr, htmlRef, baseRef, unreachableRef); });
    RequireInvalidArgument([prefsRef, htmlRef, baseRef, unreachableRef] { WKPageLoadAlternateHTMLString(prefsRef, htmlRef, baseRef, unreachableRef); });
    RequireInvalidArgument([pageRef, baseRef, unreachableRef] { WKPageLoadAlternateHTMLString(pageRef, nullptr, baseRef, unreachableRef); });
    RequireInvalidArgument([pageRef, prefsRef, baseRef, unreachableRef] { WKPageLoadAlternateHTMLString(pageRef, prefsRef, baseRef, unreachableRef); });
    RequireInvalidArgument([pageRef, htmlRef, prefsRef, unreachableRef] { WKPageLoadAlternateHTMLString(pageRef, htmlRef, prefsRef, unreachableRef); });
    RequireInvalidArgument([pageRef, htmlRef, baseRef, prefsRef] { WKPageLoadAlternateHTMLString(pageRef, htmlRef, baseRef, prefsRef); });

    RequireInvalidArgument([htmlRef, baseRef, unreachableRef, &userData] { WKPageLoadAlternateHTMLStringWithUserData(nullptr, htmlRef, baseRef, unreachableRef, &userData); });
    RequireInvalidArgument([prefsRef, htmlRef, baseRef, unreachableRef, &userData] { WKPageLoadAlternateHTMLStringWithUserData(prefsRef, htmlRef, baseRef, unreachableRef, &userData); });
    RequireInvalidArgument([pageRef, baseRef, unreachableRef, &userData] { WKPageLoadAlternateHTMLStringWithUserData(pageRef, nullptr, baseRef, unreachableRef, &userData); });
    RequireInvalidArgument([pageRef, prefsRef, baseRef, unreachableRef, &userData] { WKPageLoadAlternateHTMLStringWithUserData(pageRef, prefsRef, baseRef, unreachableRef, &userData); });
    RequireInvalidArgument([pageRef, htmlRef, prefsRef, unreachableRef, &userData] { WKPageLoadAlternateHTMLStringWithUserData(pageRef, htmlRef, prefsRef, unreachableRef, &userData); });
    RequireInvalidArgument([pageRef, htmlRef, baseRef, prefsRef, &userData] { WKPageLoadAlternateHTMLStringWithUserData(pageRef, htmlRef, baseRef, prefsRef, &userData); });

    WKRelease(prefsRef);
    WKRelease(unreachableRef);
    WKRelease(baseRef);
    WKRelease(htmlRef);
    WKRelease(pageRef);
    return 0;
}
