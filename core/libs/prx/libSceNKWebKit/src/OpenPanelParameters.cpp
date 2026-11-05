#include "prx/libSceNKWebKit/src/OpenPanelParameters.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

namespace {

const void* CopyStrings(const std::vector<NKWebKit::Retained<NKWebKit::String>>& strings) {
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(strings.size());
    for (const auto& string : strings) {
        if (string) elements.push_back(NKWebKit::Retained<NKWebKit::Object>::Share(string.Get()));
        else elements.push_back(NKWebKit::Make<NKWebKit::String>(std::u16string()));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

}  // namespace

extern "C" {

bool APS5_VABI WKOpenPanelParametersGetAllowsDirectories(const void* parametersRef) {
    return NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__)->allowsDirectories;
}

bool APS5_VABI WKOpenPanelParametersGetAllowsMultipleFiles(const void* parametersRef) {
    return NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__)->allowsMultipleFiles;
}

const void* APS5_VABI WKOpenPanelParametersCopyAcceptedMIMETypes(const void* parametersRef) {
    return CopyStrings(NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__)->acceptMIMETypes);
}

const void* APS5_VABI WKOpenPanelParametersCopyAcceptedFileExtensions(const void* parametersRef) {
    return CopyStrings(NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__)->acceptFileExtensions);
}

const void* APS5_VABI WKOpenPanelParametersCopySelectedFileNames(const void* parametersRef) {
    return CopyStrings(NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__)->selectedFiles);
}

}
