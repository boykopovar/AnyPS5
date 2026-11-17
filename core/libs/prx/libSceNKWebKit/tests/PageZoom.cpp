#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "Testing.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>


int main() {
    auto page = NKWebKit::Make<NKWebKit::Page>();
    const void* pageRef = NKWebKit::ToRef(page);

    Require(WKPageGetPageZoomFactor(pageRef) == 1.0);
    Require(WKPageGetTextZoomFactor(pageRef) == 1.0);
    Require(WKPageSupportsTextZoom(pageRef));

    WKPageSetPageZoomFactor(pageRef, 1.5);
    Require(WKPageGetPageZoomFactor(pageRef) == 1.5);

    WKPageSetTextZoomFactor(pageRef, 2.0);
    Require(WKPageGetTextZoomFactor(pageRef) == 2.0);

    WKPageSetPageAndTextZoomFactors(pageRef, 1.25, 1.75);
    Require(WKPageGetPageZoomFactor(pageRef) == 1.25);
    Require(WKPageGetTextZoomFactor(pageRef) == 1.75);

    WKPageSetPageZoomFactor(pageRef, 0.0);
    Require(WKPageGetPageZoomFactor(pageRef) == 1.25);
    WKPageSetPageZoomFactor(pageRef, -0.5);
    Require(WKPageGetPageZoomFactor(pageRef) == 1.25);

    WKPageSetTextZoomFactor(pageRef, 0.0);
    Require(WKPageGetTextZoomFactor(pageRef) == 1.75);
    WKPageSetTextZoomFactor(pageRef, -1.0);
    Require(WKPageGetTextZoomFactor(pageRef) == 1.75);

    WKPageSetPageAndTextZoomFactors(pageRef, -1.0, 2.5);
    Require(WKPageGetPageZoomFactor(pageRef) == 1.25);
    Require(WKPageGetTextZoomFactor(pageRef) == 1.75);

    WKPageSetPageAndTextZoomFactors(pageRef, 2.5, 0.0);
    Require(WKPageGetPageZoomFactor(pageRef) == 1.25);
    Require(WKPageGetTextZoomFactor(pageRef) == 1.75);

    const double nanValue = std::numeric_limits<double>::quiet_NaN();
    const double infValue = std::numeric_limits<double>::infinity();

    WKPageSetPageZoomFactor(pageRef, nanValue);
    Require(WKPageGetPageZoomFactor(pageRef) == 1.25);
    WKPageSetPageZoomFactor(pageRef, infValue);
    Require(WKPageGetPageZoomFactor(pageRef) == 1.25);

    WKPageSetTextZoomFactor(pageRef, nanValue);
    Require(WKPageGetTextZoomFactor(pageRef) == 1.75);
    WKPageSetTextZoomFactor(pageRef, infValue);
    Require(WKPageGetTextZoomFactor(pageRef) == 1.75);

    WKPageSetPageAndTextZoomFactors(pageRef, nanValue, 2.5);
    Require(WKPageGetPageZoomFactor(pageRef) == 1.25);
    Require(WKPageGetTextZoomFactor(pageRef) == 1.75);

    WKPageSetPageAndTextZoomFactors(pageRef, 2.5, infValue);
    Require(WKPageGetPageZoomFactor(pageRef) == 1.25);
    Require(WKPageGetTextZoomFactor(pageRef) == 1.75);

    ExpectErr<std::invalid_argument>([] { WKPageGetPageZoomFactor(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageGetTextZoomFactor(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageSetPageZoomFactor(nullptr, 1.5); });
    ExpectErr<std::invalid_argument>([] { WKPageSetTextZoomFactor(nullptr, 1.5); });
    ExpectErr<std::invalid_argument>([] { WKPageSetPageAndTextZoomFactors(nullptr, 1.5, 1.5); });
    ExpectErr<std::invalid_argument>([] { WKPageSupportsTextZoom(nullptr); });

    WKRelease(pageRef);
    return 0;
}
