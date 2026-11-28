#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include "prx/libSceNKWebKit/src/Jsc.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

namespace NKWebKit {

Bundle::~Bundle() {
    if (context) {
        JSGlobalContextRelease(context);
        context = nullptr;
    }
}

}  // namespace NKWebKit

extern "C" {

void APS5_VABI WKBundleClearHistoryForTesting(const void* bundleRef) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->historyClearedForTesting = true;
}

void APS5_VABI WKBundleClearResourceLoadStatistics(const void* bundleRef) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    bundle->resourceLoadStatisticsCleared = true;
}

std::size_t APS5_VABI WKBundleGetJavaScriptObjectsCount(const void* bundleRef) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    if (!bundle->context) {
        bundle->context = JSGlobalContextCreate(nullptr);
    }
    std::size_t count = bundle->javaScriptObjectsCount;
    if (bundle->context && bundle->context->group && bundle->context->group->rt) {
        JSMemoryUsage usage{};
        JS_ComputeMemoryUsage(bundle->context->group->rt, &usage);
        count += static_cast<std::size_t>(usage.obj_count);
    }
    return count;
}

void APS5_VABI WKBundleReleaseMemory(const void* bundleRef) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    ++bundle->memoryReleaseCount;
    if (bundle->context) {
        JSGarbageCollect(bundle->context);
    }
}

void APS5_VABI WKBundleResourceLoadStatisticsNotifyObserver(const void* bundleRef) {
    auto* bundle = NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    ++bundle->resourceLoadStatisticsNotifyCount;
}

}
