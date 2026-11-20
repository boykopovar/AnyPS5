#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

namespace {

bool EqualLettersIgnoringASCIICase(std::u16string_view a, std::u16string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t index = 0; index < a.size(); ++index) {
        char16_t ca = a[index];
        char16_t cb = b[index];
        if (ca >= u'A' && ca <= u'Z') ca = static_cast<char16_t>(ca - u'A' + u'a');
        if (cb >= u'A' && cb <= u'Z') cb = static_cast<char16_t>(cb - u'A' + u'a');
        if (ca != cb) return false;
    }
    return true;
}

NKWebKit::Retained<NKWebKit::UserContentURLPattern> Parse(std::u16string_view pattern) {
    auto result = NKWebKit::Make<NKWebKit::UserContentURLPattern>();
    const auto schemeEnd = pattern.find(u"://");
    if (schemeEnd == std::u16string_view::npos) return result;
    result->scheme = std::u16string(pattern.substr(0, schemeEnd));
    const bool fileScheme = EqualLettersIgnoringASCIICase(result->scheme, u"file");
    const auto hostStart = schemeEnd + 3;
    if (!fileScheme && hostStart >= pattern.size()) return result;
    const auto pathStart = pattern.find(u'/', hostStart);
    if (pathStart == std::u16string_view::npos) return result;
    result->host = std::u16string(pattern.substr(hostStart, pathStart - hostStart));
    if (result->host == u"*") {
        result->host.clear();
        result->matchSubdomains = true;
    } else if (result->host.starts_with(u"*.")) {
        result->host.erase(0, 2);
        result->matchSubdomains = true;
    } else if (fileScheme && EqualLettersIgnoringASCIICase(result->host, u"localhost")) {
        result->host.clear();
    }
    if (result->host.find(u'*') != std::u16string::npos) return result;
    result->path = std::u16string(pattern.substr(pathStart));
    result->valid = true;
    return result;
}

bool WildcardMatchesPath(std::u16string_view pattern, std::u16string_view path) {
    std::size_t pIndex = 0;
    std::size_t pathIndex = 0;
    std::size_t starP = std::u16string_view::npos;
    std::size_t starPath = 0;
    while (pathIndex < path.size()) {
        if (pIndex < pattern.size() && pattern[pIndex] == u'*') {
            starP = pIndex++;
            starPath = pathIndex;
        } else if (pIndex < pattern.size() && pattern[pIndex] == path[pathIndex]) {
            ++pIndex;
            ++pathIndex;
        } else if (starP != std::u16string_view::npos) {
            pIndex = starP + 1;
            pathIndex = ++starPath;
        } else {
            return false;
        }
    }
    while (pIndex < pattern.size() && pattern[pIndex] == u'*') ++pIndex;
    return pIndex == pattern.size();
}

std::u16string Units(const NKWebKit::WtfAbi::StringView& view) {
    std::u16string text;
    text.reserve(view.length);
    for (unsigned index = 0; index < view.length; ++index) text.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    return text;
}

bool MatchesURL(const NKWebKit::UserContentURLPattern* pattern, const NKWebKit::URL* url) {
    if (!pattern || !pattern->valid || !url) return false;
    const NKWebKit::WtfAbi::String input = _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(url->text.c_str()));
    const NKWebKit::WtfAbi::URL base{};
    NKWebKit::WtfAbi::URL parsed{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&parsed, &base, &input, nullptr);
    const NKWebKit::WtfAbi::String owner(parsed.impl);
    if (!(parsed.bits & 1)) return false;
    const std::u16string urlScheme = Units(_ZNK3WTF3URL8protocolEv(&parsed));
    const std::u16string urlHost = Units(_ZNK3WTF3URL4hostEv(&parsed));
    const std::u16string urlPath = Units(_ZNK3WTF3URL4pathEv(&parsed));
    if (pattern->scheme == u"*") {
        if (urlScheme != u"http" && urlScheme != u"https") return false;
    } else if (!EqualLettersIgnoringASCIICase(urlScheme, pattern->scheme)) {
        return false;
    }
    if (!EqualLettersIgnoringASCIICase(urlHost, pattern->host)) {
        if (!pattern->matchSubdomains) return false;
        if (!pattern->host.empty()) {
            if (urlHost.size() <= pattern->host.size()) return false;
            if (urlHost[urlHost.size() - pattern->host.size() - 1] != u'.') return false;
            if (!EqualLettersIgnoringASCIICase(std::u16string_view(urlHost).substr(urlHost.size() - pattern->host.size()), pattern->host))
                return false;
        }
    }
    return WildcardMatchesPath(pattern->path, urlPath);
}

}

extern "C" {
const void* APS5_VABI WKUserContentURLPatternCreate(const void* patternRef) {
    return NKWebKit::ToRef(Parse(NKWebKit::Cast<NKWebKit::String>(patternRef, __func__)->characters));
}

const void* APS5_VABI WKUserContentURLPatternCopyScheme(const void* urlPatternRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(NKWebKit::Cast<NKWebKit::UserContentURLPattern>(urlPatternRef, __func__)->scheme));
}

const void* APS5_VABI WKUserContentURLPatternCopyHost(const void* urlPatternRef) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(NKWebKit::Cast<NKWebKit::UserContentURLPattern>(urlPatternRef, __func__)->host));
}

bool APS5_VABI WKUserContentURLPatternIsValid(const void* urlPatternRef) {
    return NKWebKit::Cast<NKWebKit::UserContentURLPattern>(urlPatternRef, __func__)->valid;
}

bool APS5_VABI WKUserContentURLPatternMatchesSubdomains(const void* urlPatternRef) {
    return NKWebKit::Cast<NKWebKit::UserContentURLPattern>(urlPatternRef, __func__)->matchSubdomains;
}

bool APS5_VABI WKUserContentURLPatternMatchesURL(const void* urlPatternRef, const void* urlRef) {
    return MatchesURL(NKWebKit::Cast<NKWebKit::UserContentURLPattern>(urlPatternRef, __func__), NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__));
}
}
