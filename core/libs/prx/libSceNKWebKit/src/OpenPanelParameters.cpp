#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

namespace {

const void* ToStringArrayRef(const std::vector<std::u16string>& strings) {
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(strings.size());
    for (const auto& str : strings) {
        elements.push_back(NKWebKit::Make<NKWebKit::String>(str));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

}

extern "C" {

const void* APS5_VABI WKOpenPanelParametersCopyAcceptedFileExtensions(const void* parametersRef) {
    const auto* params = NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__);
    return ToStringArrayRef(params->acceptedFileExtensions);
}

const void* APS5_VABI WKOpenPanelParametersCopyAcceptedMIMETypes(const void* parametersRef) {
    const auto* params = NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__);
    return ToStringArrayRef(params->acceptedMIMETypes);
}

const void* APS5_VABI WKOpenPanelParametersCopyAllowedMIMETypes(const void* parametersRef) {
    const auto* params = NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__);
    return ToStringArrayRef(params->allowedMIMETypes);
}

const void* APS5_VABI WKOpenPanelParametersCopySelectedFileNames(const void* parametersRef) {
    const auto* params = NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__);
    return ToStringArrayRef(params->selectedFileNames);
}

bool APS5_VABI WKOpenPanelParametersGetAllowsDirectories(const void* parametersRef) {
    const auto* params = NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__);
    return params->allowsDirectories;
}

bool APS5_VABI WKOpenPanelParametersGetAllowsMultipleFiles(const void* parametersRef) {
    const auto* params = NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__);
    return params->allowsMultipleFiles;
}

}
