#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKBundle origin access check failed at line %u\n", location.line());
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

}  // namespace

int main() {
    auto bundle = NKWebKit::Make<NKWebKit::Bundle>();
    const void* bundleRef = bundle.Get();

    Require(WKGetTypeID(bundleRef) == WKBundleGetTypeID());
    Require(bundle->originAccessAllowList.empty());

    const void* origin = WKStringCreateWithUTF8CString("https://example.com");
    const void* proto = WKStringCreateWithUTF8CString("https");
    const void* host = WKStringCreateWithUTF8CString("api.example.com");

    WKBundleAddOriginAccessAllowListEntry(bundleRef, origin, proto, host, true);
    Require(bundle->originAccessAllowList.size() == 1);
    Require(bundle->originAccessAllowList[0].sourceOrigin == "https://example.com");
    Require(bundle->originAccessAllowList[0].destinationProtocol == "https");
    Require(bundle->originAccessAllowList[0].destinationHost == "api.example.com");
    Require(bundle->originAccessAllowList[0].allowDestinationSubdomains);

    WKBundleAddOriginAccessAllowListEntry(bundleRef, origin, proto, host, true);
    Require(bundle->originAccessAllowList.size() == 1);

    const void* proto2 = WKStringCreateWithUTF8CString("wss");
    const void* host2 = WKStringCreateWithUTF8CString("ws.example.com");

    WKBundleAddOriginAccessAllowListEntry(bundleRef, origin, proto2, host2, false);
    Require(bundle->originAccessAllowList.size() == 2);
    Require(bundle->originAccessAllowList[1].sourceOrigin == "https://example.com");
    Require(bundle->originAccessAllowList[1].destinationProtocol == "wss");
    Require(bundle->originAccessAllowList[1].destinationHost == "ws.example.com");
    Require(!bundle->originAccessAllowList[1].allowDestinationSubdomains);

    WKBundleAddOriginAccessAllowListEntry(bundleRef, nullptr, nullptr, nullptr, false);
    Require(bundle->originAccessAllowList.size() == 3);
    Require(bundle->originAccessAllowList[2].sourceOrigin.empty());
    Require(bundle->originAccessAllowList[2].destinationProtocol.empty());
    Require(bundle->originAccessAllowList[2].destinationHost.empty());
    Require(!bundle->originAccessAllowList[2].allowDestinationSubdomains);

    WKBundleRemoveOriginAccessAllowListEntry(bundleRef, nullptr, nullptr, nullptr, false);
    Require(bundle->originAccessAllowList.size() == 2);

    WKBundleRemoveOriginAccessAllowListEntry(bundleRef, origin, proto, host, true);
    Require(bundle->originAccessAllowList.size() == 1);
    Require(bundle->originAccessAllowList[0].destinationProtocol == "wss");

    WKBundleRemoveOriginAccessAllowListEntry(bundleRef, origin, proto, host, true);
    Require(bundle->originAccessAllowList.size() == 1);

    WKBundleResetOriginAccessAllowLists(bundleRef);
    Require(bundle->originAccessAllowList.empty());

    RequireInvalidArgument([] { WKBundleAddOriginAccessAllowListEntry(nullptr, nullptr, nullptr, nullptr, false); });
    RequireInvalidArgument([] { WKBundleRemoveOriginAccessAllowListEntry(nullptr, nullptr, nullptr, nullptr, false); });
    RequireInvalidArgument([] { WKBundleResetOriginAccessAllowLists(nullptr); });

    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    RequireInvalidArgument([&] { WKBundleAddOriginAccessAllowListEntry(frame.Get(), origin, proto, host, true); });
    RequireInvalidArgument([&] { WKBundleRemoveOriginAccessAllowListEntry(frame.Get(), origin, proto, host, true); });
    RequireInvalidArgument([&] { WKBundleResetOriginAccessAllowLists(frame.Get()); });

    RequireInvalidArgument([&] { WKBundleAddOriginAccessAllowListEntry(bundleRef, frame.Get(), proto, host, true); });
    RequireInvalidArgument([&] { WKBundleAddOriginAccessAllowListEntry(bundleRef, origin, frame.Get(), host, true); });
    RequireInvalidArgument([&] { WKBundleAddOriginAccessAllowListEntry(bundleRef, origin, proto, frame.Get(), true); });
    RequireInvalidArgument([&] { WKBundleRemoveOriginAccessAllowListEntry(bundleRef, frame.Get(), proto, host, true); });
    RequireInvalidArgument([&] { WKBundleRemoveOriginAccessAllowListEntry(bundleRef, origin, frame.Get(), host, true); });
    RequireInvalidArgument([&] { WKBundleRemoveOriginAccessAllowListEntry(bundleRef, origin, proto, frame.Get(), true); });

    WKRelease(origin);
    WKRelease(proto);
    WKRelease(host);
    WKRelease(proto2);
    WKRelease(host2);

    return 0;
}
