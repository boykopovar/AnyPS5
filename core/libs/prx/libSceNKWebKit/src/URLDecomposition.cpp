#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/URLDecompositionExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::URLDecomposition;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

const URLDecomposition& Checked(const URLDecomposition* self) {
    if (!self) throw std::invalid_argument("WebCore::URLDecomposition: null this");
    return *self;
}

class FullURL {
public:
    explicit FullURL(const URLDecomposition* self) : url(Checked(self).vtable->fullURL(self)) {}
    FullURL(const FullURL&) = delete;
    FullURL& operator=(const FullURL&) = delete;
    ~FullURL() { NKWebKit::WtfAbi::Release(url.impl); }
    const URL* get() const { return &url; }

private:
    URL url;
};

StringView WholeString(const URL& url) {
    if (!url.impl) return {nullptr, 0, true};
    return {url.impl->data, url.impl->length, (url.impl->hashAndFlags & 4) != 0};
}

String ToString(StringView view) {
    if (!view.data) return String();
    if (view.is8Bit) return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(view.data), view.length);
    return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(view.data), view.length);
}

String EmptyString() {
    return String(NKWebKit::WtfAbi::EmptyStringImpl());
}

bool QueryIsEmpty(const URL& url) {
    if (url.queryEnd == url.pathEnd) return true;
    unsigned length = url.impl ? url.impl->length : 0;
    unsigned start = url.pathEnd + 1;
    if (start >= length) return true;
    return std::min(url.queryEnd - start, length - start) == 0;
}

String WithTrailingColon(StringView scheme) {
    if (scheme.is8Bit) {
        unsigned char* characters;
        auto result = _ZN3WTF10StringImpl19createUninitializedEjRPh(scheme.length + 1, characters);
        if (scheme.length) std::memcpy(characters, scheme.data, scheme.length);
        characters[scheme.length] = ':';
        return result;
    }
    char16_t* characters;
    auto result = _ZN3WTF10StringImpl19createUninitializedEjRPDs(scheme.length + 1, characters);
    if (scheme.length) std::memcpy(characters, scheme.data, std::size_t(scheme.length) * sizeof(char16_t));
    characters[scheme.length] = u':';
    return result;
}

}  // namespace

extern "C" {

String APS5_VABI _ZNK7WebCore16URLDecomposition8protocolEv(const URLDecomposition* self) {
    FullURL url(self);
    if (_ZN3WTF20protocolIsJavaScriptENS_10StringViewE(WholeString(*url.get()))) {
        return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("javascript:"), 11);
    }
    return WithTrailingColon(_ZNK3WTF3URL8protocolEv(url.get()));
}

String APS5_VABI _ZNK7WebCore16URLDecomposition4hostEv(const URLDecomposition* self) {
    FullURL url(self);
    return _ZNK3WTF3URL11hostAndPortEv(url.get());
}

String APS5_VABI _ZNK7WebCore16URLDecomposition8hostnameEv(const URLDecomposition* self) {
    FullURL url(self);
    return ToString(_ZNK3WTF3URL4hostEv(url.get()));
}

String APS5_VABI _ZNK7WebCore16URLDecomposition4portEv(const URLDecomposition* self) {
    FullURL url(self);
    auto port = NKWebKit::WtfAbi::URLPort(*url.get());
    if (!port) return EmptyString();
    return _ZN3WTF6String6numberEi(*port);
}

String APS5_VABI _ZNK7WebCore16URLDecomposition8pathnameEv(const URLDecomposition* self) {
    FullURL url(self);
    return ToString(_ZNK3WTF3URL4pathEv(url.get()));
}

String APS5_VABI _ZNK7WebCore16URLDecomposition6searchEv(const URLDecomposition* self) {
    FullURL url(self);
    if (QueryIsEmpty(*url.get())) return EmptyString();
    return ToString(_ZNK3WTF3URL28queryWithLeadingQuestionMarkEv(url.get()));
}

String APS5_VABI _ZNK7WebCore16URLDecomposition4hashEv(const URLDecomposition* self) {
    FullURL url(self);
    if (!_ZNK3WTF3URL18fragmentIdentifierEv(url.get()).length) return EmptyString();
    return ToString(_ZNK3WTF3URL39fragmentIdentifierWithLeadingNumberSignEv(url.get()));
}

}
