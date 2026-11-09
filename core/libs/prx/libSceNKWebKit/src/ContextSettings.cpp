#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

extern "C" {

std::uint32_t APS5_VABI WKContextGetCacheModel(const void* contextRef) {
    const auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return context->cacheModel;
}

void APS5_VABI WKContextSetCacheModel(const void* contextRef, std::uint32_t cacheModel) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->cacheModel = cacheModel;
}

bool APS5_VABI WKContextGetAccessibilityEnabled(const void* contextRef) {
    const auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return context->accessibilityEnabled;
}

void APS5_VABI WKContextSetAccessibilityEnabled(const void* contextRef, bool enabled) {
    auto* context = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    context->accessibilityEnabled = enabled;
}

}
