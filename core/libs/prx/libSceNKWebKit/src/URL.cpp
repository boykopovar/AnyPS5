#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace Wtf = NKWebKit::WtfAbi;

struct ParsedURL {
    Wtf::URL fields{};
    Wtf::String text;
};

ParsedURL Parse(const Wtf::URL& base, const char* text) {
    const Wtf::String input = _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(text));
    ParsedURL parsed;
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&parsed.fields, &base, &input, nullptr);
    parsed.text = Wtf::String(parsed.fields.impl);
    return parsed;
}

ParsedURL ParsedOf(const void* urlRef, const char* caller) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, caller);
    return Parse(Wtf::URL{}, url->text.c_str());
}

std::u16string Units(const Wtf::StringView& view) {
    std::u16string text;
    text.reserve(view.length);
    for (unsigned index = 0; index < view.length; ++index) text.push_back(Wtf::CodeUnitAt(view, index));
    return text;
}

const void* MakeWKString(std::u16string text) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::move(text)));
}

}

extern "C" {
const void* APS5_VABI WKURLCreateWithUTF8CString(const char* string) {
    if (!string) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(""));
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(std::string(string)));
}

const void* APS5_VABI WKURLCopyString(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return MakeWKString(NKWebKit::DecodeUtf8(url->text.data(), url->text.size()).value_or(std::u16string()));
}

const void* APS5_VABI WKURLCopyScheme(const void* urlRef) {
    const ParsedURL url = ParsedOf(urlRef, __func__);
    return MakeWKString(Units(_ZNK3WTF3URL8protocolEv(&url.fields)));
}

const void* APS5_VABI WKURLCopyHostName(const void* urlRef) {
    const ParsedURL url = ParsedOf(urlRef, __func__);
    return MakeWKString(Units(_ZNK3WTF3URL4hostEv(&url.fields)));
}

const void* APS5_VABI WKURLCopyPath(const void* urlRef) {
    const ParsedURL url = ParsedOf(urlRef, __func__);
    return MakeWKString(Units(_ZNK3WTF3URL4pathEv(&url.fields)));
}

const void* APS5_VABI WKURLCopyLastPathComponent(const void* urlRef) {
    const ParsedURL url = ParsedOf(urlRef, __func__);
    return MakeWKString(Units(_ZNK3WTF3URL17lastPathComponentEv(&url.fields)));
}

bool APS5_VABI WKURLIsEqual(const void* aRef, const void* bRef) {
    if (aRef == bRef) return true;
    if (!aRef || !bRef) return false;
    const ParsedURL a = ParsedOf(aRef, __func__);
    const ParsedURL b = ParsedOf(bRef, __func__);
    return Units(Wtf::ViewOf(a.fields.impl)) == Units(Wtf::ViewOf(b.fields.impl));
}

const void* APS5_VABI WKURLCreateWithBaseURL(const void* baseURLRef, const char* relativeURL) {
    if (!baseURLRef) return WKURLCreateWithUTF8CString(relativeURL);
    const ParsedURL base = ParsedOf(baseURLRef, __func__);
    const ParsedURL absolute = Parse(base.fields, relativeURL);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(NKWebKit::EncodeUtf8(Units(Wtf::ViewOf(absolute.fields.impl)))));
}

}

namespace {

using NKWebKit::SecurityOrigin;

const void* CopyOriginString(std::u16string text) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::move(text)));
}

std::u16string PortString(std::uint16_t port) {
    const auto text = std::to_string(port);
    return {text.begin(), text.end()};
}

}  // namespace

extern "C" {

const void* APS5_VABI WKSecurityOriginCreate(const void* protocol, const void* host, int port) {
    std::optional<std::uint16_t> validPort;
    if (port && port <= 65535) validPort = static_cast<std::uint16_t>(port);
    return NKWebKit::ToRef(NKWebKit::Make<SecurityOrigin>(
        *NKWebKit::Cast<NKWebKit::String>(protocol, __func__), *NKWebKit::Cast<NKWebKit::String>(host, __func__), validPort));
}

const void* APS5_VABI WKSecurityOriginCopyProtocol(const void* origin) {
    const auto* value = NKWebKit::Cast<SecurityOrigin>(origin, __func__);
    return CopyOriginString(value->protocolNull ? std::u16string() : value->protocol);
}

const void* APS5_VABI WKSecurityOriginCopyHost(const void* origin) {
    const auto* value = NKWebKit::Cast<SecurityOrigin>(origin, __func__);
    return CopyOriginString(value->hostNull ? std::u16string() : value->host);
}

std::uint16_t APS5_VABI WKSecurityOriginGetPort(const void* origin) {
    return NKWebKit::Cast<SecurityOrigin>(origin, __func__)->port.value_or(0);
}

const void* APS5_VABI WKSecurityOriginCopyToString(const void* origin) {
    const auto* value = NKWebKit::Cast<SecurityOrigin>(origin, __func__);
    if (value->protocol == u"file") return CopyOriginString(u"file://");
    if (value->protocol.empty() && value->host.empty()) return CopyOriginString(std::u16string());
    auto text = value->protocol + u"://" + value->host;
    if (value->port) text += u":" + PortString(*value->port);
    return CopyOriginString(std::move(text));
}

}
