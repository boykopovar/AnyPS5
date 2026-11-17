#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>


int main() {
    auto pool = NKWebKit::Make<NKWebKit::ProcessPool>();
    const void* poolRef = NKWebKit::ToRef(pool);

    Require(pool->processTerminationEnabled);
    WKContextDisableProcessTermination(poolRef);
    Require(!pool->processTerminationEnabled);
    WKContextEnableProcessTermination(poolRef);
    Require(pool->processTerminationEnabled);

    auto link = NKWebKit::String::FromUtf8("https://example.com");
    const void* linkRef = NKWebKit::ToRef(link);
    WKContextAddVisitedLink(poolRef, linkRef);
    Require(pool->visitedLinks.size() == 1);
    WKContextAddVisitedLink(poolRef, nullptr);
    Require(pool->visitedLinks.size() == 1);
    WKContextClearVisitedLinks(poolRef);
    Require(pool->visitedLinks.empty());

    ExpectErr<std::invalid_argument>([] { WKContextDisableProcessTermination(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKContextEnableProcessTermination(nullptr); });
    ExpectErr<std::invalid_argument>([&] { WKContextAddVisitedLink(nullptr, linkRef); });
    ExpectErr<std::invalid_argument>([] { WKContextClearVisitedLinks(nullptr); });

    WKRelease(linkRef);
    WKRelease(poolRef);
    return 0;
}
