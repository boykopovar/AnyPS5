#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKUserMediaPermissionRequest check failed at line %u\n", location.line());
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
    auto request1 = NKWebKit::Make<NKWebKit::UserMediaPermissionRequest>();
    const void* ref1 = NKWebKit::ToRef(request1);
    Require(!request1->decided);
    Require(!request1->allowed);

    WKUserMediaPermissionRequestAllow(ref1);
    Require(request1->decided);
    Require(request1->allowed);

    auto request2 = NKWebKit::Make<NKWebKit::UserMediaPermissionRequest>();
    const void* ref2 = NKWebKit::ToRef(request2);
    WKUserMediaPermissionRequestDeny(ref2);
    Require(request2->decided);
    Require(!request2->allowed);

    RequireInvalidArgument([] { WKUserMediaPermissionRequestAllow(nullptr); });
    RequireInvalidArgument([] { WKUserMediaPermissionRequestDeny(nullptr); });

    WKRelease(ref1);
    WKRelease(ref2);
    return 0;
}
