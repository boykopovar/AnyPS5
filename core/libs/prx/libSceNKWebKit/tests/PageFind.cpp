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
        std::fprintf(stderr, "WKPage find check failed at line %u\n", location.line());
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

    Require(!page->activeFindString.has_value());
    Require(!page->activeFindMatches.has_value());
    Require(!page->activeCountMatches.has_value());
    Require(!page->selectedFindMatch.has_value());
    Require(!page->lastImageFindMatch.has_value());
    Require(!page->findUIHidden);

    auto target = NKWebKit::String::FromUtf8("hello world");
    const void* targetRef = NKWebKit::ToRef(target);

    WKPageHideFindUI(pageRef);
    Require(page->findUIHidden);

    WKPageFindString(pageRef, targetRef, 1, 10);
    Require(page->activeFindString.has_value());
    Require(page->activeFindString->target == u"hello world");
    Require(page->activeFindString->options == 1);
    Require(page->activeFindString->maxMatchCount == 10);
    Require(!page->findUIHidden);

    WKPageFindStringMatches(pageRef, targetRef, 3, 50);
    Require(page->activeFindMatches.has_value());
    Require(page->activeFindMatches->target == u"hello world");
    Require(page->activeFindMatches->options == 3);
    Require(page->activeFindMatches->maxMatchCount == 50);

    WKPageCountStringMatches(pageRef, targetRef, 7, 100);
    Require(page->activeCountMatches.has_value());
    Require(page->activeCountMatches->target == u"hello world");
    Require(page->activeCountMatches->options == 7);
    Require(page->activeCountMatches->maxMatchCount == 100);

    WKPageSelectFindMatch(pageRef, 4);
    Require(page->selectedFindMatch.has_value());
    Require(*page->selectedFindMatch == 4);

    WKPageGetImageForFindMatch(pageRef, 2);
    Require(page->lastImageFindMatch.has_value());
    Require(*page->lastImageFindMatch == 2);

    page->title = u"hello world hello again hello";
    auto searchTarget = NKWebKit::String::FromUtf8("hello");
    const void* searchRef = NKWebKit::ToRef(searchTarget);

    WKPageCountStringMatches(pageRef, searchRef, 0, 100);
    Require(page->activeCountMatches.has_value());
    Require(page->activeCountMatches->matchCount == 3);

    WKPageFindString(pageRef, searchRef, 0, 100);
    Require(page->activeFindString.has_value());
    Require(page->activeFindString->matchCount == 3);
    Require(page->selectedFindMatch.has_value());
    Require(*page->selectedFindMatch == 0);

    WKPageFindStringMatches(pageRef, searchRef, 0, 2);
    Require(page->activeFindMatches.has_value());
    Require(page->activeFindMatches->matchCount == 2);

    WKRelease(searchRef);

    page->isClosed = true;
    page->activeFindString.reset();
    page->activeFindMatches.reset();
    page->activeCountMatches.reset();
    page->selectedFindMatch.reset();
    page->lastImageFindMatch.reset();
    page->findUIHidden = false;

    WKPageFindString(pageRef, targetRef, 1, 10);
    Require(!page->activeFindString.has_value());

    WKPageFindStringMatches(pageRef, targetRef, 3, 50);
    Require(!page->activeFindMatches.has_value());

    WKPageCountStringMatches(pageRef, targetRef, 7, 100);
    Require(!page->activeCountMatches.has_value());

    WKPageSelectFindMatch(pageRef, 4);
    Require(!page->selectedFindMatch.has_value());

    WKPageGetImageForFindMatch(pageRef, 2);
    Require(!page->lastImageFindMatch.has_value());

    WKPageHideFindUI(pageRef);
    Require(!page->findUIHidden);

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([targetRef] { WKPageFindString(nullptr, targetRef, 0, 0); });
    RequireInvalidArgument([prefsRef, targetRef] { WKPageFindString(prefsRef, targetRef, 0, 0); });
    RequireInvalidArgument([pageRef] { WKPageFindString(pageRef, nullptr, 0, 0); });
    RequireInvalidArgument([pageRef, prefsRef] { WKPageFindString(pageRef, prefsRef, 0, 0); });

    RequireInvalidArgument([targetRef] { WKPageFindStringMatches(nullptr, targetRef, 0, 0); });
    RequireInvalidArgument([prefsRef, targetRef] { WKPageFindStringMatches(prefsRef, targetRef, 0, 0); });
    RequireInvalidArgument([pageRef] { WKPageFindStringMatches(pageRef, nullptr, 0, 0); });
    RequireInvalidArgument([pageRef, prefsRef] { WKPageFindStringMatches(pageRef, prefsRef, 0, 0); });

    RequireInvalidArgument([targetRef] { WKPageCountStringMatches(nullptr, targetRef, 0, 0); });
    RequireInvalidArgument([prefsRef, targetRef] { WKPageCountStringMatches(prefsRef, targetRef, 0, 0); });
    RequireInvalidArgument([pageRef] { WKPageCountStringMatches(pageRef, nullptr, 0, 0); });
    RequireInvalidArgument([pageRef, prefsRef] { WKPageCountStringMatches(pageRef, prefsRef, 0, 0); });

    RequireInvalidArgument([] { WKPageSelectFindMatch(nullptr, 0); });
    RequireInvalidArgument([prefsRef] { WKPageSelectFindMatch(prefsRef, 0); });

    RequireInvalidArgument([] { WKPageGetImageForFindMatch(nullptr, 0); });
    RequireInvalidArgument([prefsRef] { WKPageGetImageForFindMatch(prefsRef, 0); });

    RequireInvalidArgument([] { WKPageHideFindUI(nullptr); });
    RequireInvalidArgument([prefsRef] { WKPageHideFindUI(prefsRef); });

    WKRelease(prefsRef);
    WKRelease(targetRef);
    WKRelease(pageRef);
    return 0;
}
