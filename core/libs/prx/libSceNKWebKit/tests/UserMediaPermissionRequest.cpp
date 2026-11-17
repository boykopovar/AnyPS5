#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "Testing.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>


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

    ExpectErr<std::invalid_argument>([] { WKUserMediaPermissionRequestAllow(nullptr); });
    ExpectErr<std::invalid_argument>([] { WKUserMediaPermissionRequestDeny(nullptr); });

    WKRelease(ref1);
    WKRelease(ref2);
    return 0;
}
