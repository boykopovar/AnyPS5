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
        std::fprintf(stderr, "WKPage load data check failed at line %u\n", location.line());
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

    Require(!page->loadedData.has_value());
    Require(!page->loadedMIMEType.has_value());
    Require(!page->loadedEncoding.has_value());
    Require(!page->loadedBaseURL);
    Require(!page->provisionalURL);
    Require(!page->activeURL);
    Require(page->loadUserData == nullptr);

    const unsigned char sampleBytes[] = { 0x12, 0x34, 0x56, 0x78 };
    const void* dataRef = WKDataCreate(sampleBytes, sizeof(sampleBytes));
    auto mimeType = NKWebKit::String::FromUtf8("text/html");
    const void* mimeRef = NKWebKit::ToRef(mimeType);
    auto encoding = NKWebKit::String::FromUtf8("UTF-8");
    const void* encRef = NKWebKit::ToRef(encoding);
    auto base = NKWebKit::Make<NKWebKit::URL>("https://example.com");
    const void* baseRef = NKWebKit::ToRef(base);

    WKPageLoadData(pageRef, dataRef, mimeRef, encRef, baseRef);
    Require(page->loadedData.has_value());
    Require(page->loadedData->size() == sizeof(sampleBytes));
    Require((*page->loadedData)[0] == 0x12);
    Require((*page->loadedData)[3] == 0x78);
    Require(page->loadedMIMEType.has_value());
    Require(*page->loadedMIMEType == u"text/html");
    Require(page->loadedEncoding.has_value());
    Require(*page->loadedEncoding == u"UTF-8");
    Require(static_cast<bool>(page->loadedBaseURL));
    Require(page->loadedBaseURL->text == "https://example.com");
    Require(page->provisionalURL && page->provisionalURL->text == "https://example.com");
    Require(page->activeURL && page->activeURL->text == "https://example.com");
    Require(page->loadUserData == nullptr);

    WKPageLoadData(pageRef, dataRef, mimeRef, encRef, nullptr);
    Require(page->loadedData.has_value());
    Require(!page->loadedBaseURL);
    Require(!page->provisionalURL);
    Require(!page->activeURL);

    int userData = 456;
    WKPageLoadDataWithUserData(pageRef, dataRef, mimeRef, encRef, baseRef, &userData);
    Require(page->loadedData.has_value());
    Require(static_cast<bool>(page->loadedBaseURL));
    Require(page->loadUserData == &userData);

    page->isClosed = true;
    page->loadedData.reset();
    page->loadedMIMEType.reset();
    page->loadedEncoding.reset();
    page->loadedBaseURL = nullptr;
    page->loadUserData = nullptr;

    WKPageLoadData(pageRef, dataRef, mimeRef, encRef, baseRef);
    Require(!page->loadedData.has_value());
    Require(!page->loadedBaseURL);

    WKPageLoadDataWithUserData(pageRef, dataRef, mimeRef, encRef, baseRef, &userData);
    Require(!page->loadedData.has_value());
    Require(page->loadUserData == nullptr);

    auto prefs = NKWebKit::Make<NKWebKit::Preferences>();
    const void* prefsRef = NKWebKit::ToRef(prefs);

    RequireInvalidArgument([dataRef, mimeRef, encRef, baseRef] { WKPageLoadData(nullptr, dataRef, mimeRef, encRef, baseRef); });
    RequireInvalidArgument([prefsRef, dataRef, mimeRef, encRef, baseRef] { WKPageLoadData(prefsRef, dataRef, mimeRef, encRef, baseRef); });
    RequireInvalidArgument([pageRef, mimeRef, encRef, baseRef] { WKPageLoadData(pageRef, nullptr, mimeRef, encRef, baseRef); });
    RequireInvalidArgument([pageRef, prefsRef, mimeRef, encRef, baseRef] { WKPageLoadData(pageRef, prefsRef, mimeRef, encRef, baseRef); });
    RequireInvalidArgument([pageRef, dataRef, encRef, baseRef] { WKPageLoadData(pageRef, dataRef, nullptr, encRef, baseRef); });
    RequireInvalidArgument([pageRef, dataRef, prefsRef, encRef, baseRef] { WKPageLoadData(pageRef, dataRef, prefsRef, encRef, baseRef); });
    RequireInvalidArgument([pageRef, dataRef, mimeRef, baseRef] { WKPageLoadData(pageRef, dataRef, mimeRef, nullptr, baseRef); });
    RequireInvalidArgument([pageRef, dataRef, mimeRef, prefsRef, baseRef] { WKPageLoadData(pageRef, dataRef, mimeRef, prefsRef, baseRef); });
    RequireInvalidArgument([pageRef, dataRef, mimeRef, encRef, prefsRef] { WKPageLoadData(pageRef, dataRef, mimeRef, encRef, prefsRef); });

    RequireInvalidArgument([dataRef, mimeRef, encRef, baseRef, &userData] { WKPageLoadDataWithUserData(nullptr, dataRef, mimeRef, encRef, baseRef, &userData); });
    RequireInvalidArgument([prefsRef, dataRef, mimeRef, encRef, baseRef, &userData] { WKPageLoadDataWithUserData(prefsRef, dataRef, mimeRef, encRef, baseRef, &userData); });
    RequireInvalidArgument([pageRef, mimeRef, encRef, baseRef, &userData] { WKPageLoadDataWithUserData(pageRef, nullptr, mimeRef, encRef, baseRef, &userData); });
    RequireInvalidArgument([pageRef, prefsRef, mimeRef, encRef, baseRef, &userData] { WKPageLoadDataWithUserData(pageRef, prefsRef, mimeRef, encRef, baseRef, &userData); });
    RequireInvalidArgument([pageRef, dataRef, encRef, baseRef, &userData] { WKPageLoadDataWithUserData(pageRef, dataRef, nullptr, encRef, baseRef, &userData); });
    RequireInvalidArgument([pageRef, dataRef, prefsRef, encRef, baseRef, &userData] { WKPageLoadDataWithUserData(pageRef, dataRef, prefsRef, encRef, baseRef, &userData); });
    RequireInvalidArgument([pageRef, dataRef, mimeRef, baseRef, &userData] { WKPageLoadDataWithUserData(pageRef, dataRef, mimeRef, nullptr, baseRef, &userData); });
    RequireInvalidArgument([pageRef, dataRef, mimeRef, prefsRef, baseRef, &userData] { WKPageLoadDataWithUserData(pageRef, dataRef, mimeRef, prefsRef, baseRef, &userData); });
    RequireInvalidArgument([pageRef, dataRef, mimeRef, encRef, prefsRef, &userData] { WKPageLoadDataWithUserData(pageRef, dataRef, mimeRef, encRef, prefsRef, &userData); });

    WKRelease(prefsRef);
    WKRelease(baseRef);
    WKRelease(encRef);
    WKRelease(mimeRef);
    WKRelease(dataRef);
    WKRelease(pageRef);
    return 0;
}
