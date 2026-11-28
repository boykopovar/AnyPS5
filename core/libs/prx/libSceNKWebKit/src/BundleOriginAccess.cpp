#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <algorithm>

namespace {

std::string StringValue(const void* stringRef, const char* function) {
    if (!stringRef) return {};
    const auto* string = NKWebKit::Cast<NKWebKit::String>(stringRef, function);
    return string->null ? std::string{} : string->Utf8();
}

}  // namespace

extern "C" {

void APS5_VABI WKBundleAddOriginAccessAllowListEntry(const void* bundleRef, const void* sourceOriginRef, const void* destinationProtocolRef, const void* destinationHostRef, bool allowDestinationSubdomains) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    NKWebKit::Bundle::OriginAccessEntry entry{
        .sourceOrigin = StringValue(sourceOriginRef, __func__),
        .destinationProtocol = StringValue(destinationProtocolRef, __func__),
        .destinationHost = StringValue(destinationHostRef, __func__),
        .allowDestinationSubdomains = allowDestinationSubdomains,
    };
    if (std::find(bundle->originAccessAllowList.begin(), bundle->originAccessAllowList.end(), entry) == bundle->originAccessAllowList.end()) {
        bundle->originAccessAllowList.push_back(std::move(entry));
    }
}

void APS5_VABI WKBundleRemoveOriginAccessAllowListEntry(const void* bundleRef, const void* sourceOriginRef, const void* destinationProtocolRef, const void* destinationHostRef, bool allowDestinationSubdomains) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    NKWebKit::Bundle::OriginAccessEntry entry{
        .sourceOrigin = StringValue(sourceOriginRef, __func__),
        .destinationProtocol = StringValue(destinationProtocolRef, __func__),
        .destinationHost = StringValue(destinationHostRef, __func__),
        .allowDestinationSubdomains = allowDestinationSubdomains,
    };
    auto it = std::find(bundle->originAccessAllowList.begin(), bundle->originAccessAllowList.end(), entry);
    if (it != bundle->originAccessAllowList.end()) {
        bundle->originAccessAllowList.erase(it);
    }
}

void APS5_VABI WKBundleResetOriginAccessAllowLists(const void* bundleRef) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->originAccessAllowList.clear();
}

}
