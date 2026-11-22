#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKPage testing check failed at line %u\n", location.line());
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

}  // namespace

int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = page.Get();

    Require(page->loadedSubresourceDomains.empty());
    Require(!page->userMediaStateCleared);
    Require(!page->wheelEventTestMonitorCleared);

    page->loadedSubresourceDomains.push_back("example.com");
    page->loadedSubresourceDomains.push_back("cdn.example.org");
    Require(page->loadedSubresourceDomains.size() == 2);

    WKPageClearLoadedSubresourceDomains(pageRef);
    Require(page->loadedSubresourceDomains.empty());

    WKPageClearUserMediaState(pageRef);
    Require(page->userMediaStateCleared);

    WKPageClearWheelEventTestMonitor(pageRef);
    Require(page->wheelEventTestMonitorCleared);

    RequireInvalidArgument([] { WKPageClearLoadedSubresourceDomains(nullptr); });
    RequireInvalidArgument([] { WKPageClearUserMediaState(nullptr); });
    RequireInvalidArgument([] { WKPageClearWheelEventTestMonitor(nullptr); });

    auto dummyString = NKWebKit::Make<NKWebKit::String>();
    const void* dummyRef = dummyString.Get();
    RequireInvalidArgument([&] { WKPageClearLoadedSubresourceDomains(dummyRef); });
    RequireInvalidArgument([&] { WKPageClearUserMediaState(dummyRef); });
    RequireInvalidArgument([&] { WKPageClearWheelEventTestMonitor(dummyRef); });

    return 0;
}
