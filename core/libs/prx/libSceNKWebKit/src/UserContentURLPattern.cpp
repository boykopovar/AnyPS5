#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

namespace {

bool EqualLettersIgnoringASCIICase(std::u16string_view text, std::u16string_view lowercaseLetters) {
    if (text.size() != lowercaseLetters.size()) return false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        char16_t character = text[index];
        if (character >= u'A' && character <= u'Z') character = static_cast<char16_t>(character - u'A' + u'a');
        if (character != lowercaseLetters[index]) return false;
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
        if (pIndex < pattern.size() && pattern[pIndex] == path[pathIndex]) {
            ++pIndex;
            ++pathIndex;
        } else if (pIndex < pattern.size() && pattern[pIndex] == u'*') {
            starP = pIndex++;
            starPath = pathIndex;
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

bool MatchesURL(const NKWebKit::UserContentURLPattern* pattern, const NKWebKit::URL* url) {
    if (!pattern || !pattern->valid || !url) return false;
    const auto decoded = NKWebKit::DecodeUtf8(url->text.data(), url->text.size());
    if (!decoded.has_value()) return false;
    const std::u16string_view urlString = *decoded;
    const auto schemeEnd = urlString.find(u"://");
    if (schemeEnd == std::u16string_view::npos) return false;
    const auto urlScheme = urlString.substr(0, schemeEnd);
    if (pattern->scheme == u"*") {
        if (!EqualLettersIgnoringASCIICase(urlScheme, u"http") && !EqualLettersIgnoringASCIICase(urlScheme, u"https"))
            return false;
    } else if (!EqualLettersIgnoringASCIICase(urlScheme, pattern->scheme)) {
        return false;
    }
    const bool fileScheme = EqualLettersIgnoringASCIICase(pattern->scheme, u"file");
    const auto hostStart = schemeEnd + 3;
    const auto pathStart = urlString.find(u'/', hostStart);
    std::u16string_view urlHost;
    std::u16string_view urlPath;
    if (pathStart == std::u16string_view::npos) {
        urlHost = urlString.substr(hostStart);
        urlPath = u"/";
    } else {
        urlHost = urlString.substr(hostStart, pathStart - hostStart);
        const auto queryOrFragment = urlString.find_first_of(u"?#", pathStart);
        urlPath = (queryOrFragment == std::u16string_view::npos) ? urlString.substr(pathStart) : urlString.substr(pathStart, queryOrFragment - pathStart);
    }
    const auto portStart = urlHost.find(u':');
    if (portStart != std::u16string_view::npos) urlHost = urlHost.substr(0, portStart);
    if (!fileScheme) {
        if (pattern->host.empty()) {
            if (!pattern->matchSubdomains) return false;
        } else if (pattern->matchSubdomains) {
            if (!EqualLettersIgnoringASCIICase(urlHost, pattern->host)) {
                if (urlHost.size() <= pattern->host.size() + 1) return false;
                if (urlHost[urlHost.size() - pattern->host.size() - 1] != u'.') return false;
                if (!EqualLettersIgnoringASCIICase(urlHost.substr(urlHost.size() - pattern->host.size()), pattern->host))
                    return false;
            }
        } else if (!EqualLettersIgnoringASCIICase(urlHost, pattern->host)) {
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
