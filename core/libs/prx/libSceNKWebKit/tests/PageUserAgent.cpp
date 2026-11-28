#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

constexpr char Standard[] = "Mozilla/5.0 (PlayStation; PlayStation/0.00) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/14.0 Safari/605.1.15";

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage user agent check failed at line %u\n", location.line());
        std::abort();
    }
}

bool Consume(const void* text, const char* expected) {
    const bool equal = text != nullptr && WKStringIsEqualToUTF8CString(text, expected);
    WKRelease(text);
    return equal;
}

void SetString(void (APS5_VABI* setter)(const void*, const void*), const void* page, const char* value) {
    const void* text = WKStringCreateWithUTF8CString(value);
    setter(page, text);
    WKRelease(text);
}

bool StandardWith(const char* applicationName, const char* expected) {
    const void* name = WKStringCreateWithUTF8CString(applicationName);
    const bool equal = Consume(WKPageCopyStandardUserAgentWithApplicationName(name), expected);
    WKRelease(name);
    return equal;
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
    const std::string standard = Standard;
    Require(StandardWith("", Standard));
    Require(StandardWith("Shell", (standard + " Shell/605.1.15").c_str()));
    Require(StandardWith("Shell/1.0", (standard + " Shell/1.0/605.1.15").c_str()));
    RequireInvalidArgument([] { WKPageCopyStandardUserAgentWithApplicationName(nullptr); });

    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = page.Get();
    Require(Consume(WKPageCopyUserAgent(pageRef), Standard));

    SetString(WKPageSetApplicationNameForUserAgent, pageRef, "Shell");
    Require(Consume(WKPageCopyUserAgent(pageRef), (standard + " Shell/605.1.15").c_str()));

    SetString(WKPageSetCustomUserAgent, pageRef, "Custom UA");
    Require(Consume(WKPageCopyUserAgent(pageRef), "Custom UA"));

    SetString(WKPageSetCustomUserAgent, pageRef, "");
    Require(Consume(WKPageCopyUserAgent(pageRef), (standard + " Shell/605.1.15").c_str()));

    WKPageSetApplicationNameForUserAgent(pageRef, nullptr);
    Require(Consume(WKPageCopyUserAgent(pageRef), Standard));

    RequireInvalidArgument([] { WKPageCopyUserAgent(nullptr); });
    return 0;
}
