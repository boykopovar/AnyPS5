#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>


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

    ExpectErr<std::invalid_argument>([] { WKPageHorizontalRubberBandingIsEnabled(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageVerticalRubberBandingIsEnabled(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageRubberBandsAtBottom(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageRubberBandsAtLeft(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageRubberBandsAtRight(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageRubberBandsAtTop(nullptr); });

    ExpectErr<std::invalid_argument>([] { WKPageSetEnableHorizontalRubberBanding(nullptr, false); });
    ExpectErr<std::invalid_argument>([] { WKPageSetEnableVerticalRubberBanding(nullptr, false); });
    ExpectErr<std::invalid_argument>([] { WKPageSetRubberBandsAtBottom(nullptr, false); });
    ExpectErr<std::invalid_argument>([] { WKPageSetRubberBandsAtLeft(nullptr, false); });
    ExpectErr<std::invalid_argument>([] { WKPageSetRubberBandsAtRight(nullptr, false); });
    ExpectErr<std::invalid_argument>([] { WKPageSetRubberBandsAtTop(nullptr, false); });

    WKRelease(pageRef);
    return 0;
}
