#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstring>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string_view TrimWhitespace(std::string_view str) {
    while (!str.empty() && (static_cast<unsigned char>(str.front()) <= ' ' || str.front() == '\t' || str.front() == '\r' || str.front() == '\n')) {
        str.remove_prefix(1);
    }
    while (!str.empty() && (static_cast<unsigned char>(str.back()) <= ' ' || str.back() == '\t' || str.back() == '\r' || str.back() == '\n')) {
        str.remove_suffix(1);
    }
    return str;
}

std::string ToLowerAscii(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (char c : text) {
        if (c >= 'A' && c <= 'Z') result.push_back(static_cast<char>(c - 'A' + 'a'));
        else result.push_back(c);
    }
    return result;
}

std::string_view ExtractScheme(std::string_view url) {
    const auto colon = url.find(':');
    if (colon == std::string_view::npos || colon == 0) return {};
    for (std::size_t i = 0; i < colon; ++i) {
        char c = url[i];
        if (i == 0) {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return {};
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')) return {};
        }
    }
    return url.substr(0, colon);
}

bool IsSpecialScheme(std::string_view scheme) {
    return scheme == "http" || scheme == "https" || scheme == "file" || scheme == "ws" || scheme == "wss" || scheme == "ftp";
}

std::string CanonicalizeURL(std::string_view raw) {
    auto str = TrimWhitespace(raw);
    const auto rawScheme = ExtractScheme(str);
    if (rawScheme.empty()) return std::string(str);
    const std::string scheme = ToLowerAscii(rawScheme);
    const bool isSpecial = IsSpecialScheme(scheme);
    std::string rest(str.substr(rawScheme.size() + 1));
    if (isSpecial) {
        for (char& c : rest) {
            if (c == '\\') c = '/';
        }
    }
    if (rest.starts_with("//")) {
        const std::size_t authStart = 2;
        auto authEnd = rest.find_first_of("/?#", authStart);
        if (authEnd == std::string::npos) authEnd = rest.size();
        std::string authority = rest.substr(authStart, authEnd - authStart);
        std::string pathAndRest = rest.substr(authEnd);
        if (pathAndRest.empty() || pathAndRest.front() == '?' || pathAndRest.front() == '#') {
            pathAndRest = "/" + pathAndRest;
        }
        std::string userinfo;
        std::string hostPart = authority;
        const auto at = authority.rfind('@');
        if (at != std::string::npos) {
            userinfo = authority.substr(0, at);
            hostPart = authority.substr(at + 1);
        }
        std::string host = hostPart;
        std::string port;
        if (hostPart.starts_with('[')) {
            const auto bracket = hostPart.find(']');
            if (bracket != std::string::npos) {
                host = hostPart.substr(0, bracket + 1);
                port = hostPart.substr(bracket + 1);
            }
        } else {
            const auto colon = hostPart.find(':');
            if (colon != std::string::npos) {
                host = hostPart.substr(0, colon);
                port = hostPart.substr(colon);
            }
        }
        host = ToLowerAscii(host);
        std::string canonAuth = (userinfo.empty() ? "" : userinfo + "@") + host + port;
        return scheme + "://" + canonAuth + pathAndRest;
    }
    return scheme + ":" + rest;
}

std::string_view ExtractHost(std::string_view url) {
    const auto schemeEnd = url.find("://");
    if (schemeEnd == std::string_view::npos) return {};
    const auto hostStart = schemeEnd + 3;
    if (hostStart >= url.size()) return {};
    auto end = url.find_first_of("/?#", hostStart);
    if (end == std::string_view::npos) end = url.size();
    auto authority = url.substr(hostStart, end - hostStart);
    const auto at = authority.rfind('@');
    if (at != std::string_view::npos) authority = authority.substr(at + 1);
    if (authority.starts_with('[')) {
        const auto closeBracket = authority.find(']');
        if (closeBracket != std::string_view::npos) return authority.substr(0, closeBracket + 1);
    }
    const auto portColon = authority.find(':');
    if (portColon != std::string_view::npos) return authority.substr(0, portColon);
    return authority;
}

std::string_view ExtractPath(std::string_view url) {
    const auto schemeEnd = url.find("://");
    if (schemeEnd != std::string_view::npos) {
        const auto hostStart = schemeEnd + 3;
        const auto pathStart = url.find_first_of("/?#", hostStart);
        if (pathStart == std::string_view::npos) return "/";
        if (url[pathStart] != '/') return "/";
        const auto pathEnd = url.find_first_of("?#", pathStart);
        if (pathEnd == std::string_view::npos) return url.substr(pathStart);
        return url.substr(pathStart, pathEnd - pathStart);
    }
    const auto colon = url.find(':');
    if (colon == std::string_view::npos) return {};
    const auto pathStart = colon + 1;
    const auto pathEnd = url.find_first_of("?#", pathStart);
    if (pathEnd == std::string_view::npos) return url.substr(pathStart);
    return url.substr(pathStart, pathEnd - pathStart);
}

std::string_view ExtractLastPathComponent(std::string_view path) {
    if (path.empty() || path == "/") return {};
    std::size_t end = path.size() - 1;
    if (path[end] == '/') {
        if (end == 0) return {};
        --end;
    }
    const auto slash = path.rfind('/', end);
    const auto start = (slash == std::string_view::npos) ? 0 : slash + 1;
    if (start > end) return {};
    return path.substr(start, end - start + 1);
}

std::string RemoveDotSegments(std::string_view path) {
    std::string ib(path);
    std::string ob;
    while (!ib.empty()) {
        if (ib.starts_with("../")) {
            ib.erase(0, 3);
        } else if (ib.starts_with("./")) {
            ib.erase(0, 2);
        } else if (ib.starts_with("/./")) {
            ib.erase(0, 2);
        } else if (ib == "/.") {
            ib = "/";
        } else if (ib.starts_with("/../")) {
            ib.erase(0, 3);
            const auto lastSlash = ob.rfind('/');
            if (lastSlash != std::string::npos) ob.erase(lastSlash);
            else ob.clear();
        } else if (ib == "/..") {
            ib = "/";
            const auto lastSlash = ob.rfind('/');
            if (lastSlash != std::string::npos) ob.erase(lastSlash);
            else ob.clear();
        } else if (ib == "." || ib == "..") {
            ib.clear();
        } else {
            if (ib.front() == '/') {
                const auto nextSlash = ib.find('/', 1);
                if (nextSlash != std::string::npos) {
                    ob.append(ib, 0, nextSlash);
                    ib.erase(0, nextSlash);
                } else {
                    ob += ib;
                    ib.clear();
                }
            } else {
                const auto nextSlash = ib.find('/');
                if (nextSlash != std::string::npos) {
                    ob.append(ib, 0, nextSlash);
                    ib.erase(0, nextSlash);
                } else {
                    ob += ib;
                    ib.clear();
                }
            }
        }
    }
    return ob;
}

std::string ResolveURL(std::string_view baseRaw, std::string_view relRaw) {
    const std::string base = CanonicalizeURL(baseRaw);
    std::string rel(TrimWhitespace(relRaw));
    if (rel.empty()) {
        const auto hashPos = base.find('#');
        return (hashPos == std::string::npos) ? base : base.substr(0, hashPos);
    }
    const auto relScheme = ExtractScheme(rel);
    if (!relScheme.empty()) return CanonicalizeURL(rel);
    if (base.empty()) return CanonicalizeURL(rel);
    const auto baseScheme = ExtractScheme(base);
    if (IsSpecialScheme(baseScheme)) {
        for (char& c : rel) {
            if (c == '\\') c = '/';
        }
    }
    if (rel.starts_with("//")) {
        return CanonicalizeURL((baseScheme.empty() ? "" : std::string(baseScheme) + ":") + rel);
    }
    const auto schemeEnd = base.find("://");
    if (schemeEnd == std::string::npos) {
        const auto colon = base.find(':');
        if (colon == std::string::npos) return rel;
        return base.substr(0, colon + 1) + rel;
    }
    auto authEnd = base.find_first_of("/?#", schemeEnd + 3);
    if (authEnd == std::string::npos) authEnd = base.size();
    const std::string origin = base.substr(0, authEnd);
    if (rel.starts_with('#')) {
        const auto hashPos = base.find('#');
        const std::string baseNoHash = (hashPos == std::string::npos) ? base : base.substr(0, hashPos);
        return baseNoHash + rel;
    }
    std::string basePath = base.substr(authEnd);
    const auto queryPos = basePath.find('?');
    if (queryPos != std::string::npos) basePath = basePath.substr(0, queryPos);
    const auto hashInPath = basePath.find('#');
    if (hashInPath != std::string::npos) basePath = basePath.substr(0, hashInPath);
    if (basePath.empty()) basePath = "/";
    if (rel.starts_with('?')) {
        return origin + basePath + rel;
    }
    const auto qOrH = rel.find_first_of("?#");
    const std::string relPath = (qOrH == std::string::npos) ? rel : rel.substr(0, qOrH);
    const std::string relQueryAndFrag = (qOrH == std::string::npos) ? "" : rel.substr(qOrH);
    std::string normPath;
    if (relPath.starts_with('/')) {
        normPath = RemoveDotSegments(relPath);
    } else {
        const auto lastSlash = basePath.rfind('/');
        const std::string baseDir = (lastSlash != std::string::npos) ? basePath.substr(0, lastSlash + 1) : "/";
        normPath = RemoveDotSegments(baseDir + relPath);
    }
    if (!normPath.starts_with('/')) normPath = "/" + normPath;
    return origin + normPath + relQueryAndFrag;
}

const void* MakeWKString(std::string_view text) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(NKWebKit::DecodeUtf8(text.data(), text.size()).value_or(std::u16string())));
}

}

extern "C" {
const void* APS5_VABI WKURLCreateWithUTF8CString(const char* string) {
    if (!string) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(""));
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(std::string(string)));
}

const void* APS5_VABI WKURLCopyString(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return MakeWKString(url->text);
}

const void* APS5_VABI WKURLCopyScheme(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return MakeWKString(ExtractScheme(CanonicalizeURL(url->text)));
}

const void* APS5_VABI WKURLCopyHostName(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return MakeWKString(ExtractHost(CanonicalizeURL(url->text)));
}

const void* APS5_VABI WKURLCopyPath(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return MakeWKString(ExtractPath(CanonicalizeURL(url->text)));
}

const void* APS5_VABI WKURLCopyLastPathComponent(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return MakeWKString(ExtractLastPathComponent(ExtractPath(CanonicalizeURL(url->text))));
}

bool APS5_VABI WKURLIsEqual(const void* aRef, const void* bRef) {
    if (aRef == bRef) return true;
    if (!aRef || !bRef) return false;
    const auto* a = NKWebKit::Cast<NKWebKit::URL>(aRef, __func__);
    const auto* b = NKWebKit::Cast<NKWebKit::URL>(bRef, __func__);
    return CanonicalizeURL(a->text) == CanonicalizeURL(b->text);
}

const void* APS5_VABI WKURLCreateWithBaseURL(const void* baseURLRef, const char* relativeURL) {
    if (!baseURLRef) return WKURLCreateWithUTF8CString(relativeURL);
    const auto* base = NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__);
    const std::string rel = relativeURL ? relativeURL : "";
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(ResolveURL(base->text, rel)));
}

}

namespace {

std::optional<std::uint16_t> WithoutDefaultPort(const std::u16string& protocol, std::optional<std::uint16_t> port) {
    if (!port) return port;
    struct DefaultPort {
        std::u16string_view protocol;
        std::uint16_t port;
    };
    static constexpr DefaultPort defaults[] = {
        {u"ftp", 21}, {u"ws", 80}, {u"http", 80}, {u"wss", 443}, {u"https", 443},
    };
    for (const auto& entry : defaults) {
        if (entry.protocol == protocol && entry.port == *port) return std::nullopt;
    }
    return port;
}

class SecurityOrigin final : public NKWebKit::ObjectOf<NKWebKit::ObjectType::SecurityOrigin> {
public:
    SecurityOrigin(const NKWebKit::String& protocol, const NKWebKit::String& host, std::optional<std::uint16_t> port)
        : protocol(protocol.characters), host(host.characters), port(WithoutDefaultPort(protocol.characters, port)),
          protocolNull(protocol.null), hostNull(host.null) {}
    SecurityOrigin(std::u16string protocol, std::u16string host, std::optional<std::uint16_t> port)
        : protocol(std::move(protocol)), host(std::move(host)), port(WithoutDefaultPort(this->protocol, port)) {}

    std::u16string protocol;
    std::u16string host;
    std::optional<std::uint16_t> port;
    bool protocolNull = false;
    bool hostNull = false;
};

const void* CopyOriginString(std::u16string text) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::move(text)));
}

const void* CreateOpaqueOrigin() {
    return NKWebKit::ToRef(NKWebKit::Make<SecurityOrigin>(std::u16string(), std::u16string(), std::nullopt));
}

std::u16string PortString(std::uint16_t port) {
    const auto text = std::to_string(port);
    return {text.begin(), text.end()};
}

void ConvertToASCIILowercase(std::u16string& text) {
    for (auto& c : text) {
        if (c >= u'A' && c <= u'Z') c = static_cast<char16_t>(c - u'A' + u'a');
    }
}

bool EqualsLettersIgnoringASCIICase(const std::u16string& text, std::u16string_view lowercaseLiteral) {
    auto copy = text;
    ConvertToASCIILowercase(copy);
    return copy == lowercaseLiteral;
}

bool IsValidScheme(const std::u16string& scheme) {
    if (scheme.empty()) return false;
    for (std::size_t i = 0; i < scheme.size(); ++i) {
        const char16_t c = scheme[i];
        const bool alpha = (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
        const bool other = (c >= u'0' && c <= u'9') || c == u'+' || c == u'-' || c == u'.';
        if (!(alpha || (i && other))) return false;
    }
    return true;
}

bool ParseAuthorityPort(std::u16string_view text, std::optional<std::uint16_t>& port) {
    port.reset();
    if (text.empty()) return true;
    unsigned value = 0;
    for (const char16_t c : text) {
        if (c < u'0' || c > u'9') return false;
        value = value * 10 + (c - u'0');
        if (value > 65535) return false;
    }
    port = static_cast<std::uint16_t>(value);
    return true;
}

bool IsUnicodeCompatibleASCIIWhitespace(char16_t c) {
    return c == u' ' || c == u'\n' || c == u'\t' || c == u'\r' || c == u'\f';
}

std::optional<std::uint16_t> ParseUInt16AllowingTrailingJunk(std::u16string_view text) {
    std::size_t i = 0;
    while (i < text.size() && IsUnicodeCompatibleASCIIWhitespace(text[i])) ++i;
    if (i < text.size() && text[i] == u'+') ++i;
    if (i == text.size() || text[i] < u'0' || text[i] > u'9') return std::nullopt;
    unsigned value = 0;
    for (; i < text.size() && text[i] >= u'0' && text[i] <= u'9'; ++i) {
        value = value * 10 + (text[i] - u'0');
        if (value > 65535) return std::nullopt;
    }
    return static_cast<std::uint16_t>(value);
}

std::u16string EncodeForFileName(const std::u16string& text) {
    if (text.size() > 0x7FFFFFFFu) throw std::length_error("file name component exceeds maximum length");
    void* characters = nullptr;
    auto* impl = NKWebKit::WtfAbi::TryCreateUninitialized(static_cast<unsigned>(text.size()), false, characters);
    if (!impl) throw std::bad_alloc();
    NKWebKit::WtfAbi::String input(impl);
    if (!text.empty()) std::memcpy(characters, text.data(), text.size() * sizeof(char16_t));
    const auto output = _ZN3WTF14FileSystemImpl17encodeForFileNameERKNS_6StringE(&input);
    if (!output.impl) throw std::bad_alloc();
    std::u16string result;
    result.reserve(output.impl->length);
    const bool is8Bit = output.impl->hashAndFlags & 4;
    for (unsigned i = 0; i < output.impl->length; ++i) {
        result.push_back(is8Bit ? static_cast<const unsigned char*>(output.impl->data)[i]
                                : static_cast<const char16_t*>(output.impl->data)[i]);
    }
    return result;
}

}  // namespace

extern "C" {

const void* APS5_VABI WKSecurityOriginCreate(const void* protocol, const void* host, int port) {
    std::optional<std::uint16_t> validPort;
    if (port && port <= 65535) validPort = static_cast<std::uint16_t>(port);
    return NKWebKit::ToRef(NKWebKit::Make<SecurityOrigin>(
        *NKWebKit::Cast<NKWebKit::String>(protocol, __func__), *NKWebKit::Cast<NKWebKit::String>(host, __func__), validPort));
}

const void* APS5_VABI WKSecurityOriginCreateFromString(const void* string) {
    const auto* str = NKWebKit::Cast<NKWebKit::String>(string, __func__);
    if (str->null || str->characters.empty()) return CreateOpaqueOrigin();
    const auto& text = str->characters;
    const auto schemeEnd = text.find(u"://");
    if (schemeEnd == std::u16string::npos) return CreateOpaqueOrigin();
    auto protocol = text.substr(0, schemeEnd);
    if (!IsValidScheme(protocol)) return CreateOpaqueOrigin();
    ConvertToASCIILowercase(protocol);
    if (protocol == u"file") {
        return NKWebKit::ToRef(NKWebKit::Make<SecurityOrigin>(std::move(protocol), std::u16string(), std::nullopt));
    }
    const auto hostStart = schemeEnd + 3;
    const auto pathStart = text.find_first_of(u"/?#", hostStart);
    auto authority = (pathStart != std::u16string::npos) ? text.substr(hostStart, pathStart - hostStart) : text.substr(hostStart);
    const auto atPos = authority.rfind(u'@');
    if (atPos != std::u16string::npos) authority = authority.substr(atPos + 1);

    std::u16string host;
    std::u16string portText;
    if (authority.starts_with(u"[")) {
        const auto bracketEnd = authority.find(u']');
        if (bracketEnd == std::u16string::npos) return CreateOpaqueOrigin();
        host = authority.substr(0, bracketEnd + 1);
        const auto rest = authority.substr(bracketEnd + 1);
        if (!rest.empty()) {
            if (rest.front() != u':') return CreateOpaqueOrigin();
            portText = rest.substr(1);
        }
    } else {
        const auto colon = authority.rfind(u':');
        host = authority.substr(0, colon);
        if (colon != std::u16string::npos) portText = authority.substr(colon + 1);
    }
    std::optional<std::uint16_t> port;
    if (!ParseAuthorityPort(portText, port)) return CreateOpaqueOrigin();
    const bool requiresHost = protocol == u"http" || protocol == u"https" || protocol == u"ftp";
    if (requiresHost && host.empty()) return CreateOpaqueOrigin();
    ConvertToASCIILowercase(host);
    return NKWebKit::ToRef(NKWebKit::Make<SecurityOrigin>(std::move(protocol), std::move(host), port));
}

const void* APS5_VABI WKSecurityOriginCreateFromDatabaseIdentifier(const void* identifier) {
    const auto* string = NKWebKit::Cast<NKWebKit::String>(identifier, __func__);
    if (string->null) return nullptr;
    const auto& text = string->characters;
    const auto separator1 = text.find(u'_');
    if (separator1 == std::u16string::npos) return nullptr;
    const auto separator2 = text.rfind(u'_');
    if (separator1 == separator2) return nullptr;

    const auto portText = std::u16string_view(text).substr(separator2 + 1);
    auto port = ParseUInt16AllowingTrailingJunk(portText);
    if (!port && !portText.empty()) return nullptr;
    if (port && !*port) port = std::nullopt;

    auto protocol = text.substr(0, separator1);
    auto host = text.substr(separator1 + 1, separator2 - separator1 - 1);
    return NKWebKit::ToRef(NKWebKit::Make<SecurityOrigin>(std::move(protocol), std::move(host), port));
}

const void* APS5_VABI WKSecurityOriginCopyDatabaseIdentifier(const void* origin) {
    const auto* value = NKWebKit::Cast<SecurityOrigin>(origin, __func__);
    if (EqualsLettersIgnoringASCIICase(value->protocol, u"file")) return CopyOriginString(u"file__0");
    auto text = value->protocol + u"_" + EncodeForFileName(value->host) + u"_" + PortString(value->port.value_or(0));
    return CopyOriginString(std::move(text));
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
