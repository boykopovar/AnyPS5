#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::ResourceResponseBase;

const ResourceResponseBase& Checked(const ResourceResponseBase* self) {
    if (!self) throw std::invalid_argument("null WebCore::ResourceResponseBase");
    return *self;
}

}

extern "C" {

const NKWebKit::WtfAbi::URL& APS5_VABI _ZNK7WebCore20ResourceResponseBase3urlEv(const ResourceResponseBase* self) {
    return Checked(self).url;
}

const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase8mimeTypeEv(const ResourceResponseBase* self) {
    return Checked(self).mimeType;
}

long long APS5_VABI _ZNK7WebCore20ResourceResponseBase21expectedContentLengthEv(const ResourceResponseBase* self) {
    return Checked(self).expectedContentLength;
}

const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase16textEncodingNameEv(const ResourceResponseBase* self) {
    return Checked(self).textEncodingName;
}

const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase14httpStatusTextEv(const ResourceResponseBase* self) {
    return Checked(self).httpStatusText;
}

const NKWebKit::WebCoreAbi::StringSlot& APS5_VABI _ZNK7WebCore20ResourceResponseBase11httpVersionEv(const ResourceResponseBase* self) {
    return Checked(self).httpVersion;
}

const NKWebKit::WebCoreAbi::ResponseHeaderStorage& APS5_VABI _ZNK7WebCore20ResourceResponseBase16httpHeaderFieldsEv(const ResourceResponseBase* self) {
    return Checked(self).httpHeaderFields;
}

bool APS5_VABI _ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(const ResourceResponseBase* self) {
    const auto& url = Checked(self).url;
    if (!url.impl || !(url.bits & 1)) return false;
    NKWebKit::WtfAbi::StringView view{url.impl->data, url.impl->length, !!(url.impl->hashAndFlags & 4)};
    return _ZN3WTF22protocolIsInHTTPFamilyENS_10StringViewE(view);
}

}
