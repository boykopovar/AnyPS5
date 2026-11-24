#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKContextCreate() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProcessPool>());
}

const void* APS5_VABI WKContextCreateWithConfiguration(const void* configurationRef) {
    auto config = configurationRef
        ? NKWebKit::Cast<NKWebKit::ProcessPoolConfiguration>(configurationRef, __func__)->Duplicate()
        : NKWebKit::Make<NKWebKit::ProcessPoolConfiguration>();
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProcessPool>(std::move(config)));
}

const void* APS5_VABI WKContextCreateWithInjectedBundlePath(const void* pathRef) {
    auto config = NKWebKit::Make<NKWebKit::ProcessPoolConfiguration>();
    if (pathRef) {
        config->injectedBundlePath = NKWebKit::OptionalString(pathRef, __func__);
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProcessPool>(std::move(config)));
}

bool APS5_VABI WKContextGetUsesSingleWebProcess(const void* contextRef) {
    const auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    return pool->usesSingleWebProcess;
}

void APS5_VABI WKContextSetUsesSingleWebProcess(const void* contextRef, bool usesSingleWebProcess) {
    auto* pool = NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    pool->usesSingleWebProcess = usesSingleWebProcess;
}

const void* APS5_VABI WKContextDownloadURLRequest(const void* contextRef, const void* requestRef) {
    NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    auto* request = const_cast<NKWebKit::URLRequest*>(NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__));
    auto download = NKWebKit::Make<NKWebKit::Download>();
    download->request = NKWebKit::Retained<NKWebKit::Object>::Share(request);
    return NKWebKit::ToRef(download);
}

const void* APS5_VABI WKContextResumeDownload(const void* contextRef, const void* resumeDataRef, const void* bundlePathRef) {
    NKWebKit::Cast<NKWebKit::ProcessPool>(contextRef, __func__);
    auto* data = const_cast<NKWebKit::Data*>(NKWebKit::Cast<NKWebKit::Data>(resumeDataRef, __func__));
    NKWebKit::Retained<NKWebKit::String> bundlePath;
    if (bundlePathRef != nullptr) {
        auto* str = const_cast<NKWebKit::String*>(NKWebKit::Cast<NKWebKit::String>(bundlePathRef, __func__));
        bundlePath = NKWebKit::Retained<NKWebKit::String>::Share(str);
    }
    auto download = NKWebKit::Make<NKWebKit::Download>();
    download->resumeData = NKWebKit::Retained<NKWebKit::Data>::Share(data);
    download->bundlePath = std::move(bundlePath);
    return NKWebKit::ToRef(download);
}

}
