#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKFramePolicyListener check failed at line %u\n", location.line());
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
    Require(WKFramePolicyListenerGetTypeID() == 57);

    auto listener = NKWebKit::Make<NKWebKit::FramePolicyListener>();
    const void* listenerRef = NKWebKit::ToRef(listener);
    Require(WKGetTypeID(listenerRef) == WKFramePolicyListenerGetTypeID());
    Require(listener->decision == NKWebKit::FramePolicyListener::Decision::None);
    Require(listener->policies.Get() == nullptr);

    WKFramePolicyListenerUse(listenerRef);
    Require(listener->decision == NKWebKit::FramePolicyListener::Decision::Use);

    WKFramePolicyListenerDownload(listenerRef);
    Require(listener->decision == NKWebKit::FramePolicyListener::Decision::Download);

    WKFramePolicyListenerIgnore(listenerRef);
    Require(listener->decision == NKWebKit::FramePolicyListener::Decision::Ignore);

    WKFramePolicyListenerUseInNewProcess(listenerRef);
    Require(listener->decision == NKWebKit::FramePolicyListener::Decision::UseInNewProcess);

    const void* policiesRef = WKWebsitePoliciesCreate();
    Require(policiesRef != nullptr);
    Require(WKGetTypeID(policiesRef) == WKWebsitePoliciesGetTypeID());

    WKFramePolicyListenerUseWithPolicies(listenerRef, policiesRef);
    Require(listener->decision == NKWebKit::FramePolicyListener::Decision::Use);
    Require(listener->policies.Get() == policiesRef);

    WKFramePolicyListenerUseWithPolicies(listenerRef, nullptr);
    Require(listener->decision == NKWebKit::FramePolicyListener::Decision::Use);
    Require(listener->policies.Get() == nullptr);

    WKFramePolicyListenerUseInNewProcessWithPolicies(listenerRef, policiesRef);
    Require(listener->decision == NKWebKit::FramePolicyListener::Decision::UseInNewProcess);
    Require(listener->policies.Get() == policiesRef);

    WKFramePolicyListenerUseInNewProcessWithPolicies(listenerRef, nullptr);
    Require(listener->decision == NKWebKit::FramePolicyListener::Decision::UseInNewProcess);
    Require(listener->policies.Get() == nullptr);

    auto invalidObj = NKWebKit::Make<NKWebKit::URLRequest>();
    const void* invalidRef = NKWebKit::ToRef(invalidObj);

    RequireInvalidArgument([&] { WKFramePolicyListenerUse(nullptr); });
    RequireInvalidArgument([&] { WKFramePolicyListenerUse(invalidRef); });
    RequireInvalidArgument([&] { WKFramePolicyListenerDownload(nullptr); });
    RequireInvalidArgument([&] { WKFramePolicyListenerDownload(invalidRef); });
    RequireInvalidArgument([&] { WKFramePolicyListenerIgnore(nullptr); });
    RequireInvalidArgument([&] { WKFramePolicyListenerIgnore(invalidRef); });
    RequireInvalidArgument([&] { WKFramePolicyListenerUseInNewProcess(nullptr); });
    RequireInvalidArgument([&] { WKFramePolicyListenerUseInNewProcess(invalidRef); });
    RequireInvalidArgument([&] { WKFramePolicyListenerUseWithPolicies(nullptr, policiesRef); });
    RequireInvalidArgument([&] { WKFramePolicyListenerUseWithPolicies(invalidRef, policiesRef); });
    RequireInvalidArgument([&] { WKFramePolicyListenerUseWithPolicies(listenerRef, invalidRef); });
    RequireInvalidArgument([&] { WKFramePolicyListenerUseInNewProcessWithPolicies(nullptr, policiesRef); });
    RequireInvalidArgument([&] { WKFramePolicyListenerUseInNewProcessWithPolicies(invalidRef, policiesRef); });
    RequireInvalidArgument([&] { WKFramePolicyListenerUseInNewProcessWithPolicies(listenerRef, invalidRef); });

    WKRelease(listenerRef);
    WKRelease(policiesRef);
    WKRelease(invalidRef);
    return 0;
}
