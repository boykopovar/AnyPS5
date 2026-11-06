#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <string_view>

extern "C" {

const void* APS5_VABI WKURLCreateWithUTF8CString(const char* string) {
    if (!string) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(""));
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(std::string(string)));
}

const void* APS5_VABI WKURLCopyString(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(NKWebKit::DecodeUtf8(url->text.data(), url->text.size()).value_or(std::u16string())));
}

bool APS5_VABI WKURLIsEqual(const void* aRef, const void* bRef) {
    const auto* a = NKWebKit::Cast<NKWebKit::URL>(aRef, __func__);
    const auto* b = NKWebKit::Cast<NKWebKit::URL>(bRef, __func__);
    return a->text == b->text;
}

const void* APS5_VABI WKURLCopyHostName(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    std::string_view text = url->text;
    auto schemeEnd = text.find("://");
    if (schemeEnd == std::string_view::npos) {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    }
    std::string_view rest = text.substr(schemeEnd + 3);
    auto hostEnd = rest.find_first_of("/?#:");
    std::string host(rest.substr(0, hostEnd));
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(host.c_str()));
}

const void* APS5_VABI WKURLCopyScheme(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    std::string_view text = url->text;
    auto schemeEnd = text.find("://");
    if (schemeEnd == std::string_view::npos) {
        auto colon = text.find(':');
        if (colon != std::string_view::npos) {
            std::string scheme(text.substr(0, colon));
            return NKWebKit::ToRef(NKWebKit::String::FromUtf8(scheme.c_str()));
        }
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    }
    std::string scheme(text.substr(0, schemeEnd));
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(scheme.c_str()));
}

const void* APS5_VABI WKURLCopyPath(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    std::string_view text = url->text;
    auto schemeEnd = text.find("://");
    std::string_view afterScheme = (schemeEnd != std::string_view::npos) ? text.substr(schemeEnd + 3) : text;
    auto pathStart = afterScheme.find('/');
    if (pathStart == std::string_view::npos) {
        return NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
    }
    std::string_view pathAndRest = afterScheme.substr(pathStart);
    auto pathEnd = pathAndRest.find_first_of("?#");
    std::string path(pathAndRest.substr(0, pathEnd));
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(path.c_str()));
}

const void* APS5_VABI WKURLCopyLastPathComponent(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    std::string_view text = url->text;
    auto schemeEnd = text.find("://");
    std::string_view afterScheme = (schemeEnd != std::string_view::npos) ? text.substr(schemeEnd + 3) : text;
    auto pathStart = afterScheme.find('/');
    if (pathStart == std::string_view::npos) {
        return NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
    }
    std::string_view pathAndRest = afterScheme.substr(pathStart);
    auto pathEnd = pathAndRest.find_first_of("?#");
    std::string_view path = pathAndRest.substr(0, pathEnd);
    while (path.size() > 1 && path.back() == '/') {
        path.remove_suffix(1);
    }
    auto lastSlash = path.rfind('/');
    std::string lastComp((lastSlash != std::string_view::npos) ? path.substr(lastSlash + 1) : path);
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(lastComp.c_str()));
}

const void* APS5_VABI WKURLCreateWithBaseURL(const void* baseURLRef, const char* relativeURL) {
    if (!relativeURL) {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(""));
    }
    std::string rel(relativeURL);
    if (rel.find("://") != std::string::npos || !baseURLRef) {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(std::move(rel)));
    }
    const auto* base = NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__);
    std::string baseStr = base->text;
    if (rel.empty()) {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(std::move(baseStr)));
    }
    if (rel.front() == '/') {
        auto schemeEnd = baseStr.find("://");
        if (schemeEnd != std::string::npos) {
            auto hostSlash = baseStr.find('/', schemeEnd + 3);
            std::string origin = (hostSlash != std::string::npos) ? baseStr.substr(0, hostSlash) : baseStr;
            return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(origin + rel));
        }
    }
    if (baseStr.back() != '/') {
        auto lastSlash = baseStr.rfind('/');
        auto schemeEnd = baseStr.find("://");
        if (schemeEnd != std::string::npos && lastSlash > schemeEnd + 2) {
            baseStr = baseStr.substr(0, lastSlash + 1);
        } else {
            baseStr += '/';
        }
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(baseStr + rel));
}

}

namespace {

class SecurityOrigin final : public NKWebKit::ObjectOf<NKWebKit::ObjectType::SecurityOrigin> {
public:
    SecurityOrigin(const NKWebKit::String& protocol, const NKWebKit::String& host, std::optional<std::uint16_t> port)
        : protocol(protocol.characters), host(host.characters), port(port), protocolNull(protocol.null), hostNull(host.null) {}

    std::u16string protocol;
    std::u16string host;
    std::optional<std::uint16_t> port;
    bool protocolNull;
    bool hostNull;
};

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
    return value->protocolNull ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>()) : CopyOriginString(value->protocol);
}

const void* APS5_VABI WKSecurityOriginCopyHost(const void* origin) {
    const auto* value = NKWebKit::Cast<SecurityOrigin>(origin, __func__);
    return value->hostNull ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>()) : CopyOriginString(value->host);
}

std::uint16_t APS5_VABI WKSecurityOriginGetPort(const void* origin) {
    return NKWebKit::Cast<SecurityOrigin>(origin, __func__)->port.value_or(0);
}

const void* APS5_VABI WKSecurityOriginCopyToString(const void* origin) {
    const auto* value = NKWebKit::Cast<SecurityOrigin>(origin, __func__);
    if (value->protocol == u"file") return CopyOriginString(u"file://");
    if (value->protocol.empty() && value->host.empty()) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    auto text = value->protocol + u"://" + value->host;
    if (value->port) text += u":" + PortString(*value->port);
    return CopyOriginString(std::move(text));
}

}
