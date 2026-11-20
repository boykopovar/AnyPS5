#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <string>

namespace {

NKWebKit::Retained<NKWebKit::URL> ParsedCopy(const NKWebKit::URL* url) {
    const NKWebKit::WtfAbi::String input = _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(url->text.c_str()));
    const NKWebKit::WtfAbi::URL base{};
    NKWebKit::WtfAbi::URL parsed{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&parsed, &base, &input, nullptr);
    const NKWebKit::WtfAbi::String owner(parsed.impl);
    if (!parsed.impl) return nullptr;
    const NKWebKit::WtfAbi::StringView view = NKWebKit::WtfAbi::ViewOf(parsed.impl);
    std::u16string text;
    text.reserve(view.length);
    for (unsigned index = 0; index < view.length; ++index) text.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    return NKWebKit::Make<NKWebKit::URL>(NKWebKit::EncodeUtf8(text));
}

}

extern "C" {

const void* APS5_VABI WKURLRequestCreateWithWKURL(const void* urlRef) {
    if (!urlRef) return nullptr;
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URLRequest>(ParsedCopy(url)));
}

const void* APS5_VABI WKURLRequestCopyURL(const void* requestRef) {
    const auto* request = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    if (!request->url) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(request->url->text));
}

const void* APS5_VABI WKURLRequestCopyFirstPartyForCookies(const void* requestRef) {
    const auto* request = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    if (!request->firstPartyForCookies) return nullptr;
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(request->firstPartyForCookies->text));
}

const void* APS5_VABI WKURLRequestCopyHTTPMethod(const void* requestRef) {
    const auto* request = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(request->httpMethod.c_str()));
}

const void* APS5_VABI WKURLRequestCopySettingHTTPBody(const void* requestRef, const void* bodyRef) {
    const auto* request = NKWebKit::Cast<NKWebKit::URLRequest>(requestRef, __func__);
    const auto* body = NKWebKit::CastOrNull<NKWebKit::Data>(bodyRef, __func__);
    auto copy = NKWebKit::Make<NKWebKit::URLRequest>(request->url ? NKWebKit::Make<NKWebKit::URL>(request->url->text) : nullptr);
    if (request->firstPartyForCookies) {
        copy->firstPartyForCookies = NKWebKit::Make<NKWebKit::URL>(request->firstPartyForCookies->text);
    }
    copy->httpMethod = request->httpMethod;
    if (body) {
        copy->httpBody = NKWebKit::Make<NKWebKit::Data>(body->bytes);
    }
    return NKWebKit::ToRef(std::move(copy));
}

}
