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

struct SubresourceCallbackState {
    bool called = false;
    std::size_t count = 0;
};

void APS5_VABI SubresourceDomainsCallback(const void* domainsRef, void* context) {
    auto* state = static_cast<SubresourceCallbackState*>(context);
    state->called = true;
    if (domainsRef) {
        state->count = WKArrayGetSize(domainsRef);
    }
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);

    auto url = NKWebKit::Make<NKWebKit::URL>("https://example.com/test");
    const void* urlRef = NKWebKit::ToRef(url);

    WKPageLoadURL(pageRef, urlRef);
    Require(static_cast<bool>(page->pendingAPIRequestURL));
    Require(page->pendingAPIRequestURL->text == "https://example.com/test");
    Require(static_cast<bool>(page->provisionalURL));
    Require(page->provisionalURL->text == "https://example.com/test");
    Require(static_cast<bool>(page->activeURL));
    Require(page->activeURL->text == "https://example.com/test");
    Require(page->loadUserData == nullptr);
    Require(!page->shouldOpenExternalURLs);

    auto urlWithUser = NKWebKit::Make<NKWebKit::URL>("https://example.com/user");
    const void* urlWithUserRef = NKWebKit::ToRef(urlWithUser);
    int userData = 123;
    WKPageLoadURLWithUserData(pageRef, urlWithUserRef, &userData);
    Require(page->pendingAPIRequestURL->text == "https://example.com/user");
    Require(page->loadUserData == &userData);

    auto urlExternal = NKWebKit::Make<NKWebKit::URL>("https://example.com/external");
    const void* urlExternalRef = NKWebKit::ToRef(urlExternal);
    WKPageLoadURLWithShouldOpenExternalURLsPolicy(pageRef, urlExternalRef, true);
    Require(page->pendingAPIRequestURL->text == "https://example.com/external");
    Require(page->shouldOpenExternalURLs);
    Require(page->loadUserData == nullptr);

    WKPageLoadURLWithShouldOpenExternalURLsPolicy(pageRef, urlExternalRef, false);
    Require(!page->shouldOpenExternalURLs);

    const void* reqRef = WKURLRequestCreateWithWKURL(urlRef);
    Require(reqRef != nullptr);

    WKPageLoadURLRequest(pageRef, reqRef);
    Require(page->activeURLRequest.Get() != nullptr);
    Require(page->pendingAPIRequestURL->text == "https://example.com/test");
    Require(page->loadUserData == nullptr);

    int reqUserData = 456;
    WKPageLoadURLRequestWithUserData(pageRef, reqRef, &reqUserData);
    Require(page->activeURLRequest.Get() != nullptr);
    Require(page->loadUserData == &reqUserData);

    SubresourceCallbackState cbState;
    WKPageLoadedSubresourceDomains(pageRef, SubresourceDomainsCallback, &cbState);
    Require(cbState.called);
    Require(cbState.count == 0);

    page->loadedSubresourceDomains.push_back("cdn.example.com");
    page->loadedSubresourceDomains.push_back("api.example.com");

    cbState = {};
    WKPageLoadedSubresourceDomains(pageRef, SubresourceDomainsCallback, &cbState);
    Require(cbState.called);
    Require(cbState.count == 2);

    page->isClosed = true;
    auto closedUrl = NKWebKit::Make<NKWebKit::URL>("https://example.com/closed");
    const void* closedUrlRef = NKWebKit::ToRef(closedUrl);
    WKPageLoadURL(pageRef, closedUrlRef);
    Require(page->pendingAPIRequestURL->text == "https://example.com/test");

    WKPageLoadURLWithUserData(pageRef, closedUrlRef, &userData);
    Require(page->loadUserData == &reqUserData);

    WKPageLoadURLWithShouldOpenExternalURLsPolicy(pageRef, closedUrlRef, true);
    Require(!page->shouldOpenExternalURLs);

    WKPageLoadURLRequest(pageRef, reqRef);
    WKPageLoadURLRequestWithUserData(pageRef, reqRef, &userData);
    Require(page->loadUserData == &reqUserData);

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([urlRef] { WKPageLoadURL(nullptr, urlRef); });
    RequireInvalidArgument([prefsRef, urlRef] { WKPageLoadURL(prefsRef, urlRef); });
    RequireInvalidArgument([pageRef] { WKPageLoadURL(pageRef, nullptr); });
    RequireInvalidArgument([pageRef, prefsRef] { WKPageLoadURL(pageRef, prefsRef); });

    RequireInvalidArgument([urlRef] { WKPageLoadURLWithUserData(nullptr, urlRef, nullptr); });
    RequireInvalidArgument([pageRef] { WKPageLoadURLWithUserData(pageRef, nullptr, nullptr); });

    RequireInvalidArgument([urlRef] { WKPageLoadURLWithShouldOpenExternalURLsPolicy(nullptr, urlRef, true); });
    RequireInvalidArgument([pageRef] { WKPageLoadURLWithShouldOpenExternalURLsPolicy(pageRef, nullptr, true); });

    RequireInvalidArgument([reqRef] { WKPageLoadURLRequest(nullptr, reqRef); });
    RequireInvalidArgument([pageRef] { WKPageLoadURLRequest(pageRef, nullptr); });
    RequireInvalidArgument([pageRef, prefsRef] { WKPageLoadURLRequest(pageRef, prefsRef); });

    RequireInvalidArgument([reqRef] { WKPageLoadURLRequestWithUserData(nullptr, reqRef, nullptr); });
    RequireInvalidArgument([pageRef] { WKPageLoadURLRequestWithUserData(pageRef, nullptr, nullptr); });

    RequireInvalidArgument([&cbState] { WKPageLoadedSubresourceDomains(nullptr, SubresourceDomainsCallback, &cbState); });
    RequireInvalidArgument([prefsRef, &cbState] { WKPageLoadedSubresourceDomains(prefsRef, SubresourceDomainsCallback, &cbState); });
    RequireInvalidArgument([pageRef, &cbState] { WKPageLoadedSubresourceDomains(pageRef, nullptr, &cbState); });

    WKRelease(reqRef);
    WKRelease(prefsRef);
    WKRelease(closedUrlRef);
    WKRelease(urlExternalRef);
    WKRelease(urlWithUserRef);
    WKRelease(urlRef);
    WKRelease(pageRef);

    return 0;
}
