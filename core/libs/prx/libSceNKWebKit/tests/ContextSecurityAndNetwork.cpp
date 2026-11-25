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
        std::fprintf(stderr, "ContextSecurityAndNetwork check failed at line %u\n", location.line());
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

    Require(WKContextGetNetworkProcessIdentifier(poolRef) == 1001);
    Require(pool->privilegedNetworkBandwidth == 0);
    Require(WKContextGetWebSecurityFilter(poolRef) == nullptr);
    Require(!pool->downloadClientConfigured);

    WKContextSetPrivilegedNetworkBandwidth(poolRef, 1000000);
    Require(pool->privilegedNetworkBandwidth == 1000000);
    WKContextSetPrivilegedNetworkBandwidth(poolRef, 0);
    Require(pool->privilegedNetworkBandwidth == 0);

    const void* filterObj = NKWebKit::ToRef(NKWebKit::String::FromUtf8("WebSecurityFilterInstance"));
    WKContextSetWebSecurityFilter(poolRef, filterObj);
    Require(WKContextGetWebSecurityFilter(poolRef) == filterObj);
    WKContextSetWebSecurityFilter(poolRef, nullptr);
    Require(WKContextGetWebSecurityFilter(poolRef) == nullptr);

    WKContextSetDownloadClient(poolRef, filterObj);
    Require(pool->downloadClientConfigured);
    WKContextSetDownloadClient(poolRef, nullptr);
    Require(!pool->downloadClientConfigured);

    auto wrongType = NKWebKit::Make<NKWebKit::Page>();
    const void* wrongRef = NKWebKit::ToRef(wrongType);

    RequireInvalidArgument([&] { WKContextGetNetworkProcessIdentifier(nullptr); });
    RequireInvalidArgument([&] { WKContextGetNetworkProcessIdentifier(wrongRef); });

    RequireInvalidArgument([&] { WKContextSetPrivilegedNetworkBandwidth(nullptr, 100); });
    RequireInvalidArgument([&] { WKContextSetPrivilegedNetworkBandwidth(wrongRef, 100); });

    RequireInvalidArgument([&] { WKContextSetWebSecurityFilter(nullptr, filterObj); });
    RequireInvalidArgument([&] { WKContextSetWebSecurityFilter(wrongRef, filterObj); });

    RequireInvalidArgument([&] { WKContextGetWebSecurityFilter(nullptr); });
    RequireInvalidArgument([&] { WKContextGetWebSecurityFilter(wrongRef); });

    RequireInvalidArgument([&] { WKContextSetDownloadClient(nullptr, filterObj); });
    RequireInvalidArgument([&] { WKContextSetDownloadClient(wrongRef, filterObj); });

    return 0;
}
