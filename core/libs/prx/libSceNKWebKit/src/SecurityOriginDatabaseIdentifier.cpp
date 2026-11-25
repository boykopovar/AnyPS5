#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

namespace {

bool EqualLettersIgnoringAsciiCase(const std::u16string& str, const char* expected) {
    if (str.size() != std::strlen(expected)) return false;
    for (std::size_t i = 0; i < str.size(); ++i) {
        char16_t c = str[i];
        unsigned char e = static_cast<unsigned char>(expected[i]);
        char16_t lowerC = (c >= u'A' && c <= u'Z') ? static_cast<char16_t>(c + 32) : c;
        char16_t lowerE = (e >= 'A' && e <= 'Z') ? static_cast<char16_t>(e + 32) : static_cast<char16_t>(e);
        if (lowerC != lowerE) return false;
    }
    return true;
}

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

std::u16string PortToU16String(std::uint16_t port) {
    const auto digits = std::to_string(port);
    return {digits.begin(), digits.end()};
}

}

extern "C" {

const void* APS5_VABI WKSecurityOriginCopyDatabaseIdentifier(const void* origin) {
    const auto* value = NKWebKit::Cast<NKWebKit::SecurityOrigin>(origin, __func__);
    if (!value->protocolNull && EqualLettersIgnoringAsciiCase(value->protocol, "file")) {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"file__0")));
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
            NKWebKit::String(std::u16string(u"file")), NKWebKit::String(std::u16string()), std::nullopt));
    }
    const auto first = text.find(u'_');
    if (first == std::u16string::npos) return nullptr;
    const auto last = text.rfind(u'_');
    if (last == std::u16string::npos || first == last) return nullptr;
    const auto protocol = text.substr(0, first);
    const auto host = text.substr(first + 1, last - first - 1);
    const auto portString = text.substr(last + 1);

    std::optional<std::uint16_t> port;
    if (!portString.empty()) {
        std::size_t idx = 0;
        while (idx < portString.size() && (portString[idx] == u' ' || portString[idx] == u'\t' || portString[idx] == u'\r' || portString[idx] == u'\n' || portString[idx] == u'\f')) {
            ++idx;
        }
        if (idx < portString.size() && portString[idx] == u'+') ++idx;
        if (idx == portString.size() || portString[idx] < u'0' || portString[idx] > u'9') return nullptr;
        std::uint32_t val = 0;
        while (idx < portString.size() && portString[idx] >= u'0' && portString[idx] <= u'9') {
            val = val * 10 + (portString[idx] - u'0');
            if (val > 65535) return nullptr;
            ++idx;
        }
        if (val != 0) port = static_cast<std::uint16_t>(val);
    }

    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SecurityOrigin>(
        NKWebKit::String(protocol), NKWebKit::String(host), port));
}

}
