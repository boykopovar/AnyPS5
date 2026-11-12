#include "prx/libSceNKWebKit/src/OpenPanelParameters.hpp"
#include "prx/libSceNKWebKit/include/MimeExports.hpp"
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

NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> ToWtfStrings(const std::vector<NKWebKit::Retained<NKWebKit::String>>& strings) {
    NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> result(static_cast<unsigned>(strings.size()));
    for (const auto& string : strings) {
        if (!string || string->null) {
            result.append(NKWebKit::WtfAbi::String{nullptr});
        } else {
            NKWebKit::WtfAbi::String wtfString;
            _ZN3WTF6StringC1EPKDsj(&wtfString, string->characters.data(), static_cast<unsigned>(string->characters.size()));
            result.append(wtfString);
        }
    }
    return result;
}

std::u16string ToUtf16(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl || impl->length == 0) return {};
    if (impl->hashAndFlags & 4) {
        const auto* bytes = static_cast<const unsigned char*>(impl->data);
        std::u16string out;
        out.reserve(impl->length);
        for (unsigned i = 0; i < impl->length; ++i) {
            out.push_back(static_cast<char16_t>(bytes[i]));
        }
        return out;
    }
    return std::u16string(static_cast<const char16_t*>(impl->data), impl->length);
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

const void* APS5_VABI WKOpenPanelParametersCopyAllowedMIMETypes(const void* parametersRef) {
    const auto* parameters = NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__);
    auto mimeTypes = ToWtfStrings(parameters->acceptMIMETypes);
    auto extensions = ToWtfStrings(parameters->acceptFileExtensions);
    auto allowed = _ZN7WebCore16MIMETypeRegistry16allowedMIMETypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(mimeTypes, extensions);
    std::vector<NKWebKit::Retained<NKWebKit::Object>> elements;
    elements.reserve(allowed.size);
    for (unsigned i = 0; i < allowed.size; ++i) {
        elements.push_back(NKWebKit::Make<NKWebKit::String>(ToUtf16(allowed.buffer[i].impl)));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Array>(std::move(elements)));
}

const void* APS5_VABI WKOpenPanelParametersCopyAcceptedFileExtensions(const void* parametersRef) {
    return CopyStrings(NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__)->acceptFileExtensions);
}

const void* APS5_VABI WKOpenPanelParametersCopySelectedFileNames(const void* parametersRef) {
    return CopyStrings(NKWebKit::Cast<NKWebKit::OpenPanelParameters>(parametersRef, __func__)->selectedFiles);
}

}
