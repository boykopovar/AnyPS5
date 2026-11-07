#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKNavigationDataCopyNavigationDestinationURL(const void* navigationDataRef) {
    const auto* data = NKWebKit::Cast<NKWebKit::NavigationData>(navigationDataRef, __func__);
    if (!data->navigationDestinationURL) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::URL>::Share(data->navigationDestinationURL.Get()));
}

const void* APS5_VABI WKNavigationDataCopyOriginalRequest(const void* navigationDataRef) {
    const auto* data = NKWebKit::Cast<NKWebKit::NavigationData>(navigationDataRef, __func__);
    if (!data->originalRequest) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::Object>::Share(data->originalRequest.Get()));
}

const void* APS5_VABI WKNavigationDataCopyTitle(const void* navigationDataRef) {
    const auto* data = NKWebKit::Cast<NKWebKit::NavigationData>(navigationDataRef, __func__);
    if (!data->title) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(data->title.Get()));
}

const void* APS5_VABI WKNavigationDataCopyURL(const void* navigationDataRef) {
    const auto* data = NKWebKit::Cast<NKWebKit::NavigationData>(navigationDataRef, __func__);
    if (!data->url) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::URL>::Share(data->url.Get()));
}

}
