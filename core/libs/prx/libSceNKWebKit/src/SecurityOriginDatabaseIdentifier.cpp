#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <optional>
#include <string>
#include <utility>

namespace {

std::u16string EncodeHost(const std::u16string& host) {
    if (host.empty()) return {};
    const auto raw = _ZN3WTF10StringImpl6createEPKDsj(host.data(), static_cast<unsigned>(host.size()));
    const auto encoded = _ZN3WTF14FileSystemImpl17encodeForFileNameERKNS_6StringE(&raw);
    if (!encoded.impl) return {};
    const auto view = NKWebKit::WtfAbi::ViewOf(encoded.impl);
    std::u16string result;
    result.reserve(view.length);
    for (unsigned index = 0; index < view.length; ++index) {
        result.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    }
    return result;
}

std::u16string DecodeHost(const std::u16string& host) {
    if (host.empty()) return {};
    const auto raw = _ZN3WTF10StringImpl6createEPKDsj(host.data(), static_cast<unsigned>(host.size()));
    const auto decoded = _ZN3WTF14FileSystemImpl18decodeFromFilenameERKNS_6StringE(&raw);
    if (!decoded.impl) return {};
    const auto view = NKWebKit::WtfAbi::ViewOf(decoded.impl);
    std::u16string result;
    result.reserve(view.length);
    for (unsigned index = 0; index < view.length; ++index) {
        result.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    }
    return result;
}

std::u16string PortToU16String(std::uint16_t port) {
    const auto digits = std::to_string(port);
    return {digits.begin(), digits.end()};
}

}

extern "C" {

const void* APS5_VABI WKSecurityOriginCopyDatabaseIdentifier(const void* origin) {
    const auto* value = NKWebKit::Cast<NKWebKit::SecurityOrigin>(origin, __func__);
    if (value->protocol == u"file") {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(u"file__0"));
    }
    const auto protocol = value->protocolNull ? std::u16string() : value->protocol;
    const auto host = value->hostNull ? std::u16string() : value->host;
    const auto encodedHost = EncodeHost(host);
    const auto portString = PortToU16String(value->port.value_or(0));
    auto result = protocol + u"_" + encodedHost + u"_" + portString;
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::move(result)));
}

const void* APS5_VABI WKSecurityOriginCreateFromDatabaseIdentifier(const void* databaseIdentifier) {
    const auto* str = NKWebKit::Cast<NKWebKit::String>(databaseIdentifier, __func__);
    if (str->null) return nullptr;
    const auto& text = str->characters;
    if (text == u"file__0") {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SecurityOrigin>(
            NKWebKit::String(u"file"), NKWebKit::String(std::u16string()), std::nullopt));
    }
    const auto first = text.find(u'_');
    if (first == std::u16string::npos) return nullptr;
    const auto last = text.rfind(u'_');
    if (last == std::u16string::npos || first == last) return nullptr;
    const auto protocol = text.substr(0, first);
    const auto encodedHost = text.substr(first + 1, last - first - 1);
    const auto portString = text.substr(last + 1);
    if (portString.empty()) return nullptr;
    for (char16_t unit : portString) {
        if (unit < u'0' || unit > u'9') return nullptr;
    }
    unsigned long long parsedPort = 0;
    try {
        parsedPort = std::stoull(std::string(portString.begin(), portString.end()));
    } catch (...) {
        return nullptr;
    }
    if (parsedPort > 65535) return nullptr;
    std::optional<std::uint16_t> port;
    if (parsedPort != 0) port = static_cast<std::uint16_t>(parsedPort);
    const auto decodedHost = DecodeHost(encodedHost);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SecurityOrigin>(
        NKWebKit::String(protocol), NKWebKit::String(decodedHost), port));
}

}
