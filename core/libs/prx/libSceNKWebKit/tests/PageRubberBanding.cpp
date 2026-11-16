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
        std::fprintf(stderr, "WKPage rubber banding check failed at line %u\n", location.line());
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

    Require(WKPageHorizontalRubberBandingIsEnabled(pageRef));
    Require(WKPageVerticalRubberBandingIsEnabled(pageRef));
    Require(WKPageRubberBandsAtBottom(pageRef));
    Require(WKPageRubberBandsAtLeft(pageRef));
    Require(WKPageRubberBandsAtRight(pageRef));
    Require(WKPageRubberBandsAtTop(pageRef));

    WKPageSetEnableHorizontalRubberBanding(pageRef, false);
    WKPageSetEnableVerticalRubberBanding(pageRef, false);
    WKPageSetRubberBandsAtBottom(pageRef, false);
    WKPageSetRubberBandsAtLeft(pageRef, false);
    WKPageSetRubberBandsAtRight(pageRef, false);
    WKPageSetRubberBandsAtTop(pageRef, false);

    Require(!WKPageHorizontalRubberBandingIsEnabled(pageRef));
    Require(!WKPageVerticalRubberBandingIsEnabled(pageRef));
    Require(!WKPageRubberBandsAtBottom(pageRef));
    Require(!WKPageRubberBandsAtLeft(pageRef));
    Require(!WKPageRubberBandsAtRight(pageRef));
    Require(!WKPageRubberBandsAtTop(pageRef));

    WKPageSetEnableHorizontalRubberBanding(pageRef, true);
    WKPageSetEnableVerticalRubberBanding(pageRef, true);
    WKPageSetRubberBandsAtBottom(pageRef, true);
    WKPageSetRubberBandsAtLeft(pageRef, true);
    WKPageSetRubberBandsAtRight(pageRef, true);
    WKPageSetRubberBandsAtTop(pageRef, true);

    Require(WKPageHorizontalRubberBandingIsEnabled(pageRef));
    Require(WKPageVerticalRubberBandingIsEnabled(pageRef));
    Require(WKPageRubberBandsAtBottom(pageRef));
    Require(WKPageRubberBandsAtLeft(pageRef));
    Require(WKPageRubberBandsAtRight(pageRef));
    Require(WKPageRubberBandsAtTop(pageRef));

    RequireInvalidArgument([] { WKPageHorizontalRubberBandingIsEnabled(nullptr); });
    RequireInvalidArgument([] { WKPageVerticalRubberBandingIsEnabled(nullptr); });
    RequireInvalidArgument([] { WKPageRubberBandsAtBottom(nullptr); });
    RequireInvalidArgument([] { WKPageRubberBandsAtLeft(nullptr); });
    RequireInvalidArgument([] { WKPageRubberBandsAtRight(nullptr); });
    RequireInvalidArgument([] { WKPageRubberBandsAtTop(nullptr); });

    RequireInvalidArgument([] { WKPageSetEnableHorizontalRubberBanding(nullptr, false); });
    RequireInvalidArgument([] { WKPageSetEnableVerticalRubberBanding(nullptr, false); });
    RequireInvalidArgument([] { WKPageSetRubberBandsAtBottom(nullptr, false); });
    RequireInvalidArgument([] { WKPageSetRubberBandsAtLeft(nullptr, false); });
    RequireInvalidArgument([] { WKPageSetRubberBandsAtRight(nullptr, false); });
    RequireInvalidArgument([] { WKPageSetRubberBandsAtTop(nullptr, false); });

    WKRelease(pageRef);
    return 0;
}
