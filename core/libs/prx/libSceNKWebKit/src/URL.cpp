#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace {

const void* MakeWKString(std::string_view text) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(NKWebKit::DecodeUtf8(text.data(), text.size()).value_or(std::u16string())));
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

std::string ResolveURL(std::string_view base, std::string_view relative) {
    if (relative.empty()) return std::string(base);
    const auto relScheme = ExtractScheme(relative);
    if (!relScheme.empty()) return std::string(relative);
    if (base.empty()) return std::string(relative);
    if (relative.starts_with("//")) {
        const auto baseScheme = ExtractScheme(base);
        return baseScheme.empty() ? std::string(relative) : std::string(baseScheme) + ":" + std::string(relative);
    }
    const auto schemeEnd = base.find("://");
    if (schemeEnd == std::string_view::npos) {
        const auto colon = base.find(':');
        if (colon == std::string_view::npos) return std::string(relative);
        return std::string(base.substr(0, colon + 1)) + std::string(relative);
    }
    const auto authorityEnd = base.find_first_of("/?#", schemeEnd + 3);
    const std::string origin(base.substr(0, authorityEnd));
    if (relative.starts_with('/')) return origin + std::string(relative);
    if (relative.starts_with('?')) {
        const auto queryStart = base.find_first_of("?#", schemeEnd + 3);
        const std::string basePath = (queryStart == std::string_view::npos) ? std::string(base) : std::string(base.substr(0, queryStart));
        return basePath + std::string(relative);
    }
    if (relative.starts_with('#')) {
        const auto hashStart = base.find('#');
        const std::string baseNoHash = (hashStart == std::string_view::npos) ? std::string(base) : std::string(base.substr(0, hashStart));
        return baseNoHash + std::string(relative);
    }
    std::string basePath;
    if (authorityEnd == std::string_view::npos) {
        basePath = "/";
    } else {
        const auto queryStart = base.find_first_of("?#", authorityEnd);
        basePath = std::string(base.substr(authorityEnd, queryStart == std::string_view::npos ? std::string_view::npos : queryStart - authorityEnd));
        const auto lastSlash = basePath.rfind('/');
        if (lastSlash != std::string::npos) {
            basePath = basePath.substr(0, lastSlash + 1);
        } else {
            basePath = "/";
        }
    }
    std::string combined = basePath + std::string(relative);
    std::vector<std::string_view> segments;
    std::size_t pos = 0;
    while (pos < combined.size()) {
        const auto nextSlash = combined.find('/', pos);
        const auto segment = combined.substr(pos, nextSlash == std::string::npos ? std::string::npos : nextSlash - pos);
        if (segment == ".") {
        } else if (segment == "..") {
            if (!segments.empty() && segments.back() != "..") segments.pop_back();
        } else if (!segment.empty() || segments.empty()) {
            segments.push_back(segment);
        }
        if (nextSlash == std::string::npos) break;
        pos = nextSlash + 1;
    }
    std::string normalized;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i > 0 || (i == 0 && segments[i].empty())) normalized += "/";
        normalized += segments[i];
    }
    if (combined.ends_with('/') && !normalized.ends_with('/')) normalized += "/";
    return origin + (normalized.starts_with('/') ? normalized : "/" + normalized);
}

}

extern "C" {
const void* APS5_VABI WKURLCreateWithUTF8CString(const char* string) {
    if (!string) return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(""));
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(std::string(string)));
}

const void* APS5_VABI WKURLCopyString(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(NKWebKit::DecodeUtf8(url->text.data(), url->text.size()).value_or(std::u16string())));
}

const void* APS5_VABI WKURLCopyScheme(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return MakeWKString(ExtractScheme(url->text));
}

const void* APS5_VABI WKURLCopyHostName(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return MakeWKString(ExtractHost(url->text));
}

const void* APS5_VABI WKURLCopyPath(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return MakeWKString(ExtractPath(url->text));
}

const void* APS5_VABI WKURLCopyLastPathComponent(const void* urlRef) {
    const auto* url = NKWebKit::Cast<NKWebKit::URL>(urlRef, __func__);
    return MakeWKString(ExtractLastPathComponent(ExtractPath(url->text)));
}

bool APS5_VABI WKURLIsEqual(const void* aRef, const void* bRef) {
    if (aRef == bRef) return true;
    if (!aRef || !bRef) return false;
    const auto* a = NKWebKit::Cast<NKWebKit::URL>(aRef, __func__);
    const auto* b = NKWebKit::Cast<NKWebKit::URL>(bRef, __func__);
    return a->text == b->text;
}

const void* APS5_VABI WKURLCreateWithBaseURL(const void* baseURLRef, const char* relativeURL) {
    if (!baseURLRef) return WKURLCreateWithUTF8CString(relativeURL);
    const auto* base = NKWebKit::Cast<NKWebKit::URL>(baseURLRef, __func__);
    const std::string rel = relativeURL ? relativeURL : "";
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::URL>(ResolveURL(base->text, rel)));
}

}
