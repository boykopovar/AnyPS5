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
        std::fprintf(stderr, "WKPage load URL check failed at line %u\n", location.line());
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

    Require(!page->provisionalURL);
    Require(!page->activeURL);
    Require(page->loadUserData == nullptr);
    Require(!page->shouldOpenExternalURLsPolicy);

    auto url = NKWebKit::Make<NKWebKit::URL>("https://example.com");
    const void* urlRef = NKWebKit::ToRef(url);

    WKPageLoadURL(pageRef, urlRef);
    Require(static_cast<bool>(page->provisionalURL));
    Require(page->provisionalURL->text == "https://example.com");
    Require(page->activeURL && page->activeURL->text == "https://example.com");
    Require(page->loadUserData == nullptr);

    int userData = 12345;
    WKPageLoadURLWithUserData(pageRef, urlRef, &userData);
    Require(static_cast<bool>(page->provisionalURL));
    Require(page->provisionalURL->text == "https://example.com");
    Require(page->loadUserData == &userData);

    WKPageLoadURLWithShouldOpenExternalURLsPolicy(pageRef, urlRef, true);
    Require(page->shouldOpenExternalURLsPolicy);
    Require(page->loadUserData == nullptr);

    WKPageLoadURLWithShouldOpenExternalURLsPolicy(pageRef, urlRef, false);
    Require(!page->shouldOpenExternalURLsPolicy);

    auto reqUrl = NKWebKit::Make<NKWebKit::URL>("https://request.example.com");
    auto request = NKWebKit::Make<NKWebKit::URLRequest>(reqUrl);
    const void* requestRef = NKWebKit::ToRef(request);

    WKPageLoadURLRequest(pageRef, requestRef);
    Require(static_cast<bool>(page->provisionalURL));
    Require(page->provisionalURL->text == "https://request.example.com");
    Require(page->loadUserData == nullptr);

    WKPageLoadURLRequestWithUserData(pageRef, requestRef, &userData);
    Require(static_cast<bool>(page->provisionalURL));
    Require(page->provisionalURL->text == "https://request.example.com");
    Require(page->loadUserData == &userData);

    page->isClosed = true;
    page->provisionalURL = nullptr;
    page->activeURL = nullptr;
    page->loadUserData = nullptr;

    WKPageLoadURL(pageRef, urlRef);
    Require(!page->provisionalURL);
    Require(!page->activeURL);

    WKPageLoadURLRequest(pageRef, requestRef);
    Require(!page->provisionalURL);
    Require(!page->activeURL);

    WKPageLoadURLWithUserData(pageRef, urlRef, &userData);
    Require(!page->provisionalURL);
    Require(page->loadUserData == nullptr);

    WKPageLoadURLRequestWithUserData(pageRef, requestRef, &userData);
    Require(!page->provisionalURL);
    Require(page->loadUserData == nullptr);

    WKPageLoadURLWithShouldOpenExternalURLsPolicy(pageRef, urlRef, true);
    Require(!page->provisionalURL);

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([urlRef] { WKPageLoadURL(nullptr, urlRef); });
    RequireInvalidArgument([prefsRef, urlRef] { WKPageLoadURL(prefsRef, urlRef); });
    RequireInvalidArgument([pageRef] { WKPageLoadURL(pageRef, nullptr); });
    RequireInvalidArgument([pageRef, prefsRef] { WKPageLoadURL(pageRef, prefsRef); });

    RequireInvalidArgument([requestRef] { WKPageLoadURLRequest(nullptr, requestRef); });
    RequireInvalidArgument([prefsRef, requestRef] { WKPageLoadURLRequest(prefsRef, requestRef); });
    RequireInvalidArgument([pageRef] { WKPageLoadURLRequest(pageRef, nullptr); });
    RequireInvalidArgument([pageRef, prefsRef] { WKPageLoadURLRequest(pageRef, prefsRef); });

    RequireInvalidArgument([urlRef, &userData] { WKPageLoadURLWithUserData(nullptr, urlRef, &userData); });
    RequireInvalidArgument([prefsRef, urlRef, &userData] { WKPageLoadURLWithUserData(prefsRef, urlRef, &userData); });
    RequireInvalidArgument([pageRef, &userData] { WKPageLoadURLWithUserData(pageRef, nullptr, &userData); });
    RequireInvalidArgument([pageRef, prefsRef, &userData] { WKPageLoadURLWithUserData(pageRef, prefsRef, &userData); });

    RequireInvalidArgument([requestRef, &userData] { WKPageLoadURLRequestWithUserData(nullptr, requestRef, &userData); });
    RequireInvalidArgument([prefsRef, requestRef, &userData] { WKPageLoadURLRequestWithUserData(prefsRef, requestRef, &userData); });
    RequireInvalidArgument([pageRef, &userData] { WKPageLoadURLRequestWithUserData(pageRef, nullptr, &userData); });
    RequireInvalidArgument([pageRef, prefsRef, &userData] { WKPageLoadURLRequestWithUserData(pageRef, prefsRef, &userData); });

    RequireInvalidArgument([urlRef] { WKPageLoadURLWithShouldOpenExternalURLsPolicy(nullptr, urlRef, true); });
    RequireInvalidArgument([prefsRef, urlRef] { WKPageLoadURLWithShouldOpenExternalURLsPolicy(prefsRef, urlRef, true); });
    RequireInvalidArgument([pageRef] { WKPageLoadURLWithShouldOpenExternalURLsPolicy(pageRef, nullptr, true); });
    RequireInvalidArgument([pageRef, prefsRef] { WKPageLoadURLWithShouldOpenExternalURLsPolicy(pageRef, prefsRef, true); });

    WKRelease(prefsRef);
    WKRelease(requestRef);
    WKRelease(urlRef);
    WKRelease(pageRef);
    return 0;
}
