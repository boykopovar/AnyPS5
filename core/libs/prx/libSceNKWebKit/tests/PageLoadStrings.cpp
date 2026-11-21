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
        std::fprintf(stderr, "WKPage load strings check failed at line %u\n", location.line());
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

    Require(!page->loadedHTML.has_value());
    Require(!page->loadedPlainText.has_value());
    Require(!page->loadedBaseURL);
    Require(!page->provisionalURL);
    Require(!page->activeURL);
    Require(page->loadUserData == nullptr);

    auto html = NKWebKit::String::FromUtf8("<h1>Hello</h1>");
    const void* htmlRef = NKWebKit::ToRef(html);
    auto base = NKWebKit::Make<NKWebKit::URL>("https://example.com");
    const void* baseRef = NKWebKit::ToRef(base);

    WKPageLoadHTMLString(pageRef, htmlRef, baseRef);
    Require(page->loadedHTML.has_value());
    Require(*page->loadedHTML == u"<h1>Hello</h1>");
    Require(!page->loadedPlainText.has_value());
    Require(static_cast<bool>(page->loadedBaseURL));
    Require(page->loadedBaseURL->text == "https://example.com");
    Require(page->provisionalURL && page->provisionalURL->text == "https://example.com");
    Require(page->activeURL && page->activeURL->text == "https://example.com");
    Require(page->loadUserData == nullptr);

    WKPageLoadHTMLString(pageRef, htmlRef, nullptr);
    Require(page->loadedHTML.has_value());
    Require(*page->loadedHTML == u"<h1>Hello</h1>");
    Require(!page->loadedBaseURL);
    Require(!page->provisionalURL);
    Require(!page->activeURL);

    int userData = 789;
    WKPageLoadHTMLStringWithUserData(pageRef, htmlRef, baseRef, &userData);
    Require(page->loadedHTML.has_value());
    Require(static_cast<bool>(page->loadedBaseURL));
    Require(page->loadUserData == &userData);

    auto plain = NKWebKit::String::FromUtf8("Just plain text");
    const void* plainRef = NKWebKit::ToRef(plain);

    WKPageLoadPlainTextString(pageRef, plainRef);
    Require(!page->loadedHTML.has_value());
    Require(page->loadedPlainText.has_value());
    Require(*page->loadedPlainText == u"Just plain text");
    Require(!page->loadedBaseURL);
    Require(!page->provisionalURL);
    Require(!page->activeURL);
    Require(page->loadUserData == nullptr);

    WKPageLoadPlainTextStringWithUserData(pageRef, plainRef, &userData);
    Require(page->loadedPlainText.has_value());
    Require(page->loadUserData == &userData);

    page->isClosed = true;
    page->loadedHTML.reset();
    page->loadedPlainText.reset();
    page->loadedBaseURL = nullptr;
    page->loadUserData = nullptr;

    WKPageLoadHTMLString(pageRef, htmlRef, baseRef);
    Require(!page->loadedHTML.has_value());
    Require(!page->loadedBaseURL);

    WKPageLoadHTMLStringWithUserData(pageRef, htmlRef, baseRef, &userData);
    Require(!page->loadedHTML.has_value());
    Require(page->loadUserData == nullptr);

    WKPageLoadPlainTextString(pageRef, plainRef);
    Require(!page->loadedPlainText.has_value());

    WKPageLoadPlainTextStringWithUserData(pageRef, plainRef, &userData);
    Require(!page->loadedPlainText.has_value());
    Require(page->loadUserData == nullptr);

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([htmlRef, baseRef] { WKPageLoadHTMLString(nullptr, htmlRef, baseRef); });
    RequireInvalidArgument([prefsRef, htmlRef, baseRef] { WKPageLoadHTMLString(prefsRef, htmlRef, baseRef); });
    RequireInvalidArgument([pageRef, baseRef] { WKPageLoadHTMLString(pageRef, nullptr, baseRef); });
    RequireInvalidArgument([pageRef, prefsRef, baseRef] { WKPageLoadHTMLString(pageRef, prefsRef, baseRef); });
    RequireInvalidArgument([pageRef, htmlRef, prefsRef] { WKPageLoadHTMLString(pageRef, htmlRef, prefsRef); });

    RequireInvalidArgument([htmlRef, baseRef, &userData] { WKPageLoadHTMLStringWithUserData(nullptr, htmlRef, baseRef, &userData); });
    RequireInvalidArgument([prefsRef, htmlRef, baseRef, &userData] { WKPageLoadHTMLStringWithUserData(prefsRef, htmlRef, baseRef, &userData); });
    RequireInvalidArgument([pageRef, baseRef, &userData] { WKPageLoadHTMLStringWithUserData(pageRef, nullptr, baseRef, &userData); });
    RequireInvalidArgument([pageRef, prefsRef, baseRef, &userData] { WKPageLoadHTMLStringWithUserData(pageRef, prefsRef, baseRef, &userData); });
    RequireInvalidArgument([pageRef, htmlRef, prefsRef, &userData] { WKPageLoadHTMLStringWithUserData(pageRef, htmlRef, prefsRef, &userData); });

    RequireInvalidArgument([plainRef] { WKPageLoadPlainTextString(nullptr, plainRef); });
    RequireInvalidArgument([prefsRef, plainRef] { WKPageLoadPlainTextString(prefsRef, plainRef); });
    RequireInvalidArgument([pageRef] { WKPageLoadPlainTextString(pageRef, nullptr); });
    RequireInvalidArgument([pageRef, prefsRef] { WKPageLoadPlainTextString(pageRef, prefsRef); });

    RequireInvalidArgument([plainRef, &userData] { WKPageLoadPlainTextStringWithUserData(nullptr, plainRef, &userData); });
    RequireInvalidArgument([prefsRef, plainRef, &userData] { WKPageLoadPlainTextStringWithUserData(prefsRef, plainRef, &userData); });
    RequireInvalidArgument([pageRef, &userData] { WKPageLoadPlainTextStringWithUserData(pageRef, nullptr, &userData); });
    RequireInvalidArgument([pageRef, prefsRef, &userData] { WKPageLoadPlainTextStringWithUserData(pageRef, prefsRef, &userData); });

    WKRelease(prefsRef);
    WKRelease(plainRef);
    WKRelease(baseRef);
    WKRelease(htmlRef);
    WKRelease(pageRef);
    return 0;
}
