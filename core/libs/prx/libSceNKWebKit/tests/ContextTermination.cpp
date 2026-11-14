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
        std::fprintf(stderr, "ContextTermination check failed at line %u\n", location.line());
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

    RequireInvalidArgument([] { WKContextDisableProcessTermination(nullptr); });
    RequireInvalidArgument([] { WKContextEnableProcessTermination(nullptr); });
    RequireInvalidArgument([&] { WKContextAddVisitedLink(nullptr, linkRef); });
    RequireInvalidArgument([] { WKContextClearVisitedLinks(nullptr); });

    WKRelease(linkRef);
    WKRelease(poolRef);
    return 0;
}
