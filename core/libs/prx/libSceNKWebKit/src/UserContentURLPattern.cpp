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

}  // namespace

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
}
