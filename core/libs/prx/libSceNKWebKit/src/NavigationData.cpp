#include "prx/libSceNKWebKit/src/NavigationData.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

namespace {

const void* CopyURL(const std::optional<std::string>& url) {
    if (!url) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(*url));
}

}  // namespace

extern "C" {

const void* APS5_VABI WKNavigationDataCopyTitle(const void* navigationDataRef) {
    const auto* data = NKWebKit::Cast<NKWebKit::NavigationData>(navigationDataRef, __func__);
    if (!data->title) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string()));
    return NKWebKit::ToRef(NKWebKit::Retained<NKWebKit::String>::Share(data->title.Get()));
}

const void* APS5_VABI WKNavigationDataCopyURL(const void* navigationDataRef) {
    return CopyURL(NKWebKit::Cast<NKWebKit::NavigationData>(navigationDataRef, __func__)->originalRequestURL);
}

const void* APS5_VABI WKNavigationDataCopyNavigationDestinationURL(const void* navigationDataRef) {
    return CopyURL(NKWebKit::Cast<NKWebKit::NavigationData>(navigationDataRef, __func__)->url);
}

const void* APS5_VABI WKNavigationDataCopyOriginalRequest(const void* navigationDataRef) {
    const auto* data = NKWebKit::Cast<NKWebKit::NavigationData>(navigationDataRef, __func__);
    if (!data->originalRequestURL || data->originalRequestURL->empty()) return nullptr;
    auto url = NKWebKit::Make<NKWebKit::URL>(*data->originalRequestURL);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URLRequest>(std::move(url)));
}

}
