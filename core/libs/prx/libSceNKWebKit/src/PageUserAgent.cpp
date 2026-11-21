#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/UserAgentExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"

#include <string>

namespace {

using NKWebKit::WtfAbi::String;

String ToWtfString(const std::u16string& text) {
    void* characters = nullptr;
    String result(NKWebKit::WtfAbi::CreateUninitialized(static_cast<unsigned>(text.size()), false, characters));
    if (!text.empty()) std::char_traits<char16_t>::copy(static_cast<char16_t*>(characters), text.data(), text.size());
    return result;
}

std::u16string StandardUserAgent(const std::u16string& applicationName) {
    const String name = ToWtfString(applicationName);
    const String version;
    const String userAgent = _ZN7WebCore17standardUserAgentERKN3WTF6StringES3_(&name, &version);
    const auto view = NKWebKit::WtfAbi::ViewOf(userAgent.impl);
    std::u16string result(view.length, u'\0');
    for (unsigned i = 0; i < view.length; ++i) result[i] = NKWebKit::WtfAbi::CodeUnitAt(view, i);
    return result;
}

}

extern "C" {

const void* APS5_VABI WKPageCopyUserAgent(const void* pageRef) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const std::u16string custom = page->customUserAgent.value_or(std::u16string());
    return NKWebKit::ToCopiedStringRef(custom.empty() ? StandardUserAgent(page->applicationNameForUserAgent.value_or(std::u16string())) : custom);
}

const void* APS5_VABI WKPageCopyStandardUserAgentWithApplicationName(const void* applicationNameRef) {
    return NKWebKit::ToCopiedStringRef(StandardUserAgent(NKWebKit::Cast<NKWebKit::String>(applicationNameRef, __func__)->characters));
}

}
