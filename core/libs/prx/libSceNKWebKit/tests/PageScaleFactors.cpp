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
        std::fprintf(stderr, "WKPage scale factor check failed at line %u\n", location.line());
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
    const NKWebKit::PointValue origin{0.0, 0.0};

    Require(WKPageGetBackingScaleFactor(pageRef) == 1.0);
    Require(WKPageGetScaleFactor(pageRef) == 1.0);

    WKPageSetCustomBackingScaleFactor(pageRef, 2.0);
    Require(WKPageGetBackingScaleFactor(pageRef) == 2.0);
    WKPageSetCustomBackingScaleFactor(pageRef, 0.0);
    Require(WKPageGetBackingScaleFactor(pageRef) == 1.0);

    page->intrinsicBackingScaleFactor = 1.5f;
    Require(WKPageGetBackingScaleFactor(pageRef) == 1.5);
    WKPageSetCustomBackingScaleFactor(pageRef, 3.0);
    Require(WKPageGetBackingScaleFactor(pageRef) == 3.0);
    WKPageSetCustomBackingScaleFactor(pageRef, 0.0);
    Require(WKPageGetBackingScaleFactor(pageRef) == 1.5);

    WKPageSetScaleFactor(pageRef, 2.5, origin);
    Require(WKPageGetScaleFactor(pageRef) == 2.5);
    WKPageSetScaleFactor(pageRef, 0.25, NKWebKit::PointValue{10.0, 20.0});
    Require(WKPageGetScaleFactor(pageRef) == 0.25);

    page->isSuspended = true;
    RequireThrows<std::logic_error>([&] { WKPageSetCustomBackingScaleFactor(pageRef, 2.0); });
    RequireThrows<std::logic_error>([&] { WKPageSetScaleFactor(pageRef, 4.0, origin); });
    Require(WKPageGetBackingScaleFactor(pageRef) == 1.5);
    Require(WKPageGetScaleFactor(pageRef) == 0.25);
    page->isSuspended = false;

    RequireThrows<std::invalid_argument>([] { WKPageGetBackingScaleFactor(nullptr); });
    RequireThrows<std::invalid_argument>([&] { WKPageSetCustomBackingScaleFactor(nullptr, 2.0); });
    RequireThrows<std::invalid_argument>([] { WKPageGetScaleFactor(nullptr); });
    RequireThrows<std::invalid_argument>([&] { WKPageSetScaleFactor(nullptr, 2.0, origin); });
    auto frame = NKWebKit::Make<NKWebKit::Frame>();
    RequireThrows<std::invalid_argument>([&] { WKPageGetScaleFactor(frame.Get()); });
    RequireThrows<std::invalid_argument>([&] { WKPageSetScaleFactor(frame.Get(), 2.0, origin); });

    return 0;
}
