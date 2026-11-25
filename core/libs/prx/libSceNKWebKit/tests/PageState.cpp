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
        std::fprintf(stderr, "WKPage state check failed at line %u\n", location.line());
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

template<class Copy>
bool CopiedStringEquals(Copy copy, const void* page, const char* expected) {
    const void* text = copy(page);
    const bool equal = text != nullptr && WKStringIsEqualToUTF8CString(text, expected);
    WKRelease(text);
    return equal;
}

template<class Copy>
bool CopiedURLEquals(Copy copy, const void* page, const char* expected) {
    const void* url = copy(page);
    if (url == nullptr) return false;
    const void* text = WKURLCopyString(url);
    const bool equal = WKStringIsEqualToUTF8CString(text, expected);
    WKRelease(text);
    WKRelease(url);
    return equal;
}

}

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = page.Get();

    Require(!WKPageIsClosed(pageRef));
    Require(WKPageGetEstimatedProgress(pageRef) == 0.0);
    Require(!WKPageBackgroundExtendsBeyondPage(pageRef));
    Require(!WKPageGetAllowsRemoteInspection(pageRef));
    Require(WKPageCopyPendingAPIRequestURL(pageRef) == nullptr);
    Require(WKPageCopyPageConfiguration(pageRef) == nullptr);
    Require(WKPageCopyActiveURL(pageRef) == nullptr);
    Require(WKPageCopyProvisionalURL(pageRef) == nullptr);
    Require(WKPageCopyCommittedURL(pageRef) == nullptr);
    Require(CopiedStringEquals(WKPageCopyTitle, pageRef, ""));
    Require(CopiedStringEquals(WKPageCopyApplicationNameForUserAgent, pageRef, ""));
    Require(CopiedStringEquals(WKPageCopyCustomUserAgent, pageRef, ""));
    Require(CopiedStringEquals(WKPageCopyCustomTextEncodingName, pageRef, ""));

    page->isClosed = true;
    page->title = std::u16string(u"Home");
    page->activeURL = NKWebKit::Make<NKWebKit::URL>("https://example.test/active");
    page->provisionalURL = NKWebKit::Make<NKWebKit::URL>("https://example.test/provisional");
    page->committedURL = NKWebKit::Make<NKWebKit::URL>("https://example.test/committed");
    page->estimatedProgress = 0.75;

    Require(WKPageIsClosed(pageRef));
    Require(WKPageGetEstimatedProgress(pageRef) == 0.75);
    Require(CopiedStringEquals(WKPageCopyTitle, pageRef, "Home"));
    Require(CopiedURLEquals(WKPageCopyActiveURL, pageRef, "https://example.test/active"));
    Require(CopiedURLEquals(WKPageCopyProvisionalURL, pageRef, "https://example.test/provisional"));
    Require(CopiedURLEquals(WKPageCopyCommittedURL, pageRef, "https://example.test/committed"));

    page->pendingAPIRequestURL = NKWebKit::Make<NKWebKit::URL>("https://example.test/pending");
    Require(CopiedURLEquals(WKPageCopyPendingAPIRequestURL, pageRef, "https://example.test/pending"));

    auto config = NKWebKit::Make<NKWebKit::PageConfiguration>();
    config->initialCapitalizationEnabled = true;
    config->cpuLimit = 42.5;
    page->configuration = config;
    const void* copiedConfig = WKPageCopyPageConfiguration(pageRef);
    Require(copiedConfig != nullptr);
    Require(copiedConfig != config.Get());
    const auto* copiedImpl = NKWebKit::Cast<NKWebKit::PageConfiguration>(copiedConfig, __func__);
    Require(copiedImpl->initialCapitalizationEnabled == true);
    Require(copiedImpl->cpuLimit == 42.5);
    config->initialCapitalizationEnabled = false;
    Require(copiedImpl->initialCapitalizationEnabled == true);
    WKRelease(copiedConfig);

    const void* applicationName = WKStringCreateWithUTF8CString("Shell/1.0");
    WKPageSetApplicationNameForUserAgent(pageRef, applicationName);
    WKRelease(applicationName);
    Require(CopiedStringEquals(WKPageCopyApplicationNameForUserAgent, pageRef, "Shell/1.0"));

    const void* userAgent = WKStringCreateWithUTF8CString("Custom UA");
    WKPageSetCustomUserAgent(pageRef, userAgent);
    WKRelease(userAgent);
    Require(CopiedStringEquals(WKPageCopyCustomUserAgent, pageRef, "Custom UA"));

    const void* encoding = WKStringCreateWithUTF8CString("UTF-16");
    WKPageSetCustomTextEncodingName(pageRef, encoding);
    WKRelease(encoding);
    Require(CopiedStringEquals(WKPageCopyCustomTextEncodingName, pageRef, "UTF-16"));

    const void* empty = WKStringCreateWithUTF8CString("");
    WKPageSetCustomUserAgent(pageRef, empty);
    WKRelease(empty);
    Require(CopiedStringEquals(WKPageCopyCustomUserAgent, pageRef, ""));
    Require(CopiedStringEquals(WKPageCopyApplicationNameForUserAgent, pageRef, "Shell/1.0"));

    WKPageSetCustomUserAgent(pageRef, nullptr);
    WKPageSetApplicationNameForUserAgent(pageRef, nullptr);
    WKPageSetCustomTextEncodingName(pageRef, nullptr);
    Require(CopiedStringEquals(WKPageCopyCustomUserAgent, pageRef, ""));
    Require(CopiedStringEquals(WKPageCopyApplicationNameForUserAgent, pageRef, ""));
    Require(CopiedStringEquals(WKPageCopyCustomTextEncodingName, pageRef, ""));

    WKPageSetBackgroundExtendsBeyondPage(pageRef, true);
    Require(WKPageBackgroundExtendsBeyondPage(pageRef));
    WKPageSetBackgroundExtendsBeyondPage(pageRef, false);
    Require(!WKPageBackgroundExtendsBeyondPage(pageRef));

    WKPageSetAllowsRemoteInspection(pageRef, true);
    Require(WKPageGetAllowsRemoteInspection(pageRef));
    WKPageSetAllowsRemoteInspection(pageRef, false);
    Require(!WKPageGetAllowsRemoteInspection(pageRef));

    RequireInvalidArgument([] { WKPageIsClosed(nullptr); });
    RequireInvalidArgument([] { WKPageCopyTitle(nullptr); });
    RequireInvalidArgument([] { WKPageCopyActiveURL(nullptr); });
    RequireInvalidArgument([] { WKPageCopyProvisionalURL(nullptr); });
    RequireInvalidArgument([] { WKPageCopyCommittedURL(nullptr); });
    RequireInvalidArgument([] { WKPageGetEstimatedProgress(nullptr); });
    RequireInvalidArgument([] { WKPageCopyApplicationNameForUserAgent(nullptr); });
    RequireInvalidArgument([] { WKPageCopyCustomUserAgent(nullptr); });
    RequireInvalidArgument([] { WKPageCopyCustomTextEncodingName(nullptr); });
    RequireInvalidArgument([] { WKPageBackgroundExtendsBeyondPage(nullptr); });
    RequireInvalidArgument([] { WKPageSetBackgroundExtendsBeyondPage(nullptr, true); });
    RequireInvalidArgument([] { WKPageGetAllowsRemoteInspection(nullptr); });
    RequireInvalidArgument([] { WKPageSetAllowsRemoteInspection(nullptr, true); });
    RequireInvalidArgument([] { WKPageCopyPendingAPIRequestURL(nullptr); });
    RequireInvalidArgument([] { WKPageCopyPageConfiguration(nullptr); });
    const void* probe = WKStringCreateWithUTF8CString("x");
    RequireInvalidArgument([probe] { WKPageSetCustomUserAgent(nullptr, probe); });
    WKRelease(probe);
    RequireInvalidArgument([] { WKPageSetCustomUserAgent(NKWebKit::Make<NKWebKit::Page>().Get(), NKWebKit::Make<NKWebKit::URL>("x").Get()); });
    RequireInvalidArgument([probe] { WKPageSetApplicationNameForUserAgent(nullptr, probe); });
    RequireInvalidArgument([probe] { WKPageSetCustomTextEncodingName(nullptr, probe); });
    RequireInvalidArgument([] { WKPageIsClosed(NKWebKit::Make<NKWebKit::BundleScriptWorld>().Get()); });
    RequireInvalidArgument([] { WKPageGetAllowsRemoteInspection(NKWebKit::Make<NKWebKit::BundleScriptWorld>().Get()); });
    RequireInvalidArgument([] { WKPageSetAllowsRemoteInspection(NKWebKit::Make<NKWebKit::BundleScriptWorld>().Get(), true); });
    RequireInvalidArgument([] { WKPageCopyPendingAPIRequestURL(NKWebKit::Make<NKWebKit::BundleScriptWorld>().Get()); });
    RequireInvalidArgument([] { WKPageCopyPageConfiguration(NKWebKit::Make<NKWebKit::BundleScriptWorld>().Get()); });

    return 0;
}
