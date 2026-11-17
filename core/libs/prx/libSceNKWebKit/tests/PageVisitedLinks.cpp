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
    Require(WKPageGetAddsVisitedLinks(pageRef));

    WKPageSetAddsVisitedLinks(pageRef, false);
    Require(!WKPageGetAddsVisitedLinks(pageRef));

    WKPageSetAddsVisitedLinks(pageRef, true);
    Require(WKPageGetAddsVisitedLinks(pageRef));

    ExpectErr<std::invalid_argument>([] { WKPageGetAddsVisitedLinks(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKPageSetAddsVisitedLinks(nullptr, false); });

    WKRelease(pageRef);
    return 0;
}
