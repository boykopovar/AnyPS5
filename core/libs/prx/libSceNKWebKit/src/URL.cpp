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
