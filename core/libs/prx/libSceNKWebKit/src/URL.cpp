#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
const void* APS5_VABI WKURLCreateWithUTF8CString(const char* string) {
    if (!string) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(""));
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(std::string(string)));
}

const void* APS5_VABI WKURLCopyString(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(NKWebKit::DecodeUtf8(url->text.data(), url->text.size()).value_or(std::u16string())));
}

}
