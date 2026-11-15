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
        std::fprintf(stderr, "WKContext process prewarming setting check failed at line %u\n", location.line());
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
    Require(!pool->prewarmsProcessesAutomatically);

    WKContextSetPrewarmsProcessesAutomatically(poolRef, true);
    Require(pool->prewarmsProcessesAutomatically);

    WKContextSetPrewarmsProcessesAutomatically(poolRef, false);
    Require(!pool->prewarmsProcessesAutomatically);

    RequireInvalidArgument([] { WKContextSetPrewarmsProcessesAutomatically(nullptr, true); });

    WKRelease(poolRef);
    return 0;
}
