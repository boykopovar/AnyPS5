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
        std::fprintf(stderr, "WKPage editing and media state check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Exception, class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const Exception&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = page.Get();

    Require(!WKPageGetIsControlledByAutomation(pageRef));
    Require(WKPageGetMediaCaptureEnabled(pageRef));
    Require(!WKPageIsContentEditable(pageRef));
    Require(!WKPageHasSelectedRange(pageRef));
    Require(!WKPageCanDelete(pageRef));
    Require(!WKPageIsPlayingAudio(pageRef));
    Require(!WKPageSupportsTextEncoding(pageRef));

    WKPageSetControlledByAutomation(pageRef, true);
    Require(WKPageGetIsControlledByAutomation(pageRef));
    WKPageSetControlledByAutomation(pageRef, false);
    Require(!WKPageGetIsControlledByAutomation(pageRef));
    WKPageSetMediaCaptureEnabled(pageRef, false);
    Require(!WKPageGetMediaCaptureEnabled(pageRef));
    WKPageSetMediaCaptureEnabled(pageRef, true);
    Require(WKPageGetMediaCaptureEnabled(pageRef));

    page->isContentEditable = true;
    Require(WKPageIsContentEditable(pageRef));
    Require(!WKPageHasSelectedRange(pageRef));
    Require(!WKPageCanDelete(pageRef));
    page->selectionIsRange = true;
    Require(WKPageHasSelectedRange(pageRef));
    Require(WKPageCanDelete(pageRef));
    page->isContentEditable = false;
    Require(!WKPageCanDelete(pageRef));

    page->isPlayingAudio = true;
    Require(WKPageIsPlayingAudio(pageRef));

    page->mainFrame = NKWebKit::Make<NKWebKit::Frame>();
    Require(WKPageSupportsTextEncoding(pageRef));
    page->mainFrame->isDisplayingStandaloneImageDocument = true;
    Require(!WKPageSupportsTextEncoding(pageRef));

    const void* historyType = WKPageGetSessionHistoryURLValueType();
    const void* itemType = WKPageGetSessionBackForwardListItemValueType();
    Require(historyType != nullptr && WKStringIsEqualToUTF8CString(historyType, "SessionHistoryURL"));
    Require(itemType != nullptr && WKStringIsEqualToUTF8CString(itemType, "SessionBackForwardListItem"));
    Require(WKPageGetSessionHistoryURLValueType() == historyType);
    Require(WKPageGetSessionBackForwardListItemValueType() == itemType);

    page->isSuspended = true;
    RequireThrows<std::logic_error>([&] { WKPageSetControlledByAutomation(pageRef, true); });
    RequireThrows<std::logic_error>([&] { WKPageSetMediaCaptureEnabled(pageRef, false); });
    Require(!WKPageGetIsControlledByAutomation(pageRef));
    Require(WKPageGetMediaCaptureEnabled(pageRef));
    const void* suspendedText = WKStringCreateWithUTF8CString("x");
    RequireThrows<std::logic_error>([&] { WKPageSetApplicationNameForUserAgent(pageRef, suspendedText); });
    RequireThrows<std::logic_error>([&] { WKPageSetCustomUserAgent(pageRef, suspendedText); });
    RequireThrows<std::logic_error>([&] { WKPageSetCustomTextEncodingName(pageRef, suspendedText); });
    RequireThrows<std::logic_error>([&] { WKPageSetBackgroundExtendsBeyondPage(pageRef, true); });
    WKRelease(suspendedText);
    Require(!WKPageBackgroundExtendsBeyondPage(pageRef));
    Require(WKPageIsClosed(pageRef) == false);
    page->isSuspended = false;

    RequireThrows<std::invalid_argument>([] { WKPageGetIsControlledByAutomation(nullptr); });
    RequireThrows<std::invalid_argument>([] { WKPageSetControlledByAutomation(nullptr, true); });
    RequireThrows<std::invalid_argument>([] { WKPageGetMediaCaptureEnabled(nullptr); });
    RequireThrows<std::invalid_argument>([] { WKPageSetMediaCaptureEnabled(nullptr, true); });
    RequireThrows<std::invalid_argument>([] { WKPageIsContentEditable(nullptr); });
    RequireThrows<std::invalid_argument>([] { WKPageHasSelectedRange(nullptr); });
    RequireThrows<std::invalid_argument>([] { WKPageCanDelete(nullptr); });
    RequireThrows<std::invalid_argument>([] { WKPageIsPlayingAudio(nullptr); });
    RequireThrows<std::invalid_argument>([] { WKPageSupportsTextEncoding(nullptr); });
    auto frame = NKWebKit::Make<NKWebKit::Frame>();
    RequireThrows<std::invalid_argument>([&] { WKPageIsPlayingAudio(frame.Get()); });
    RequireThrows<std::invalid_argument>([&] { WKPageSetMediaCaptureEnabled(frame.Get(), true); });

    return 0;
}
