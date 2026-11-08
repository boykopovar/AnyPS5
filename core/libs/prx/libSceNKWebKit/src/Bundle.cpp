#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

extern "C" {

void APS5_VABI WKBundleClearHistoryForTesting(const void* bundleRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__)->historyCleared = true;
}

void APS5_VABI WKBundleClearResourceLoadStatistics(const void* bundleRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__)->resourceLoadStatisticsCleared = true;
}

void APS5_VABI WKBundleResetOriginAccessAllowLists(const void* bundleRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__)->originAccessAllowListsReset = true;
}

}
