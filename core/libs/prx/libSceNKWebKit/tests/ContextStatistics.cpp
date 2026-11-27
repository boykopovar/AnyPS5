#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Context statistics check failed at line %u\n", location.line());
        std::abort();
    }
}

void RequireCounts(unsigned views, unsigned pages, unsigned frames, std::source_location location = std::source_location::current()) {
    NKWebKit::ContextStatistics statistics{99, 99, 99};
    WKContextGetGlobalStatistics(&statistics);
    Require(statistics.viewCount == views && statistics.pageCount == pages && statistics.frameCount == frames, location);
}

}

int main() {
    RequireCounts(0, 0, 0);
    {
        auto view = NKWebKit::Make<NKWebKit::View>();
        auto page = NKWebKit::Make<NKWebKit::Page>();
        auto frame = NKWebKit::Make<NKWebKit::Frame>();
        RequireCounts(1, 1, 1);
        {
            auto other = NKWebKit::Make<NKWebKit::Frame>();
            auto shared = page;
            RequireCounts(1, 1, 2);
        }
        RequireCounts(1, 1, 1);
    }
    RequireCounts(0, 0, 0);
    bool rejected = false;
    try {
        WKContextGetGlobalStatistics(nullptr);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);
    std::puts("Context statistics tests passed.");
}
