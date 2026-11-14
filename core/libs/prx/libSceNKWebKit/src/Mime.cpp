// Derived from Source/WebCore/platform/MIMETypeRegistry.cpp@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WebCore/platform/playstation/MIMETypeRegistryPlayStation.cpp@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/MimeExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

char16_t GetChar(const NKWebKit::WtfAbi::StringImpl* impl, unsigned index) {
    if (!impl || index >= impl->length) return 0;
    if (impl->hashAndFlags & 4) {
        return static_cast<const unsigned char*>(impl->data)[index];
    }
    return static_cast<const char16_t*>(impl->data)[index];
}

char16_t GetChar(const NKWebKit::WtfAbi::StringView& view, unsigned index) {
    if (!view.data || index >= view.length) return 0;
    if (view.is8Bit) {
        return static_cast<const unsigned char*>(view.data)[index];
    }
    return static_cast<const char16_t*>(view.data)[index];
}

bool EqualIgnoringASCIICase(const NKWebKit::WtfAbi::StringImpl* impl, std::string_view s) {
    if (!impl) return s.empty();
    if (impl->length != s.length()) return false;
    for (unsigned i = 0; i < impl->length; ++i) {
        char16_t c1 = GetChar(impl, i);
        char c2 = s[i];
        if (std::tolower(static_cast<unsigned char>(c1)) != std::tolower(static_cast<unsigned char>(c2))) {
            return false;
        }
    }
    return true;
}

bool EqualIgnoringASCIICase(const NKWebKit::WtfAbi::StringView& view, std::string_view s) {
    if (view.length != s.length()) return false;
    for (unsigned i = 0; i < view.length; ++i) {
        char16_t c1 = GetChar(view, i);
        char c2 = s[i];
        if (std::tolower(static_cast<unsigned char>(c1)) != std::tolower(static_cast<unsigned char>(c2))) {
            return false;
        }
    }
    return true;
}

bool StartsWithIgnoringASCIICase(const NKWebKit::WtfAbi::StringImpl* impl, std::string_view prefix) {
    if (!impl) return prefix.empty();
    if (impl->length < prefix.length()) return false;
    for (unsigned i = 0; i < prefix.length(); ++i) {
        char16_t c1 = GetChar(impl, i);
        char c2 = prefix[i];
        if (std::tolower(static_cast<unsigned char>(c1)) != std::tolower(static_cast<unsigned char>(c2))) {
            return false;
        }
    }
    return true;
}

bool StartsWithLettersIgnoringASCIICase(const NKWebKit::WtfAbi::StringImpl* impl, std::string_view lowercaseLetters) {
    unsigned length = impl ? impl->length : 0;
    if (length < lowercaseLetters.length()) return false;
    for (unsigned i = 0; i < lowercaseLetters.length(); ++i) {
        if ((GetChar(impl, i) | 0x20) != static_cast<char16_t>(lowercaseLetters[i])) {
            return false;
        }
    }
    return true;
}

bool EqualLettersIgnoringASCIICase(const NKWebKit::WtfAbi::StringImpl* impl, std::string_view lowercaseLetters) {
    unsigned length = impl ? impl->length : 0;
    return length == lowercaseLetters.length() && StartsWithLettersIgnoringASCIICase(impl, lowercaseLetters);
}

char16_t ToASCIILower(char16_t c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char16_t>(c | 0x20) : c;
}

bool LiteralLessThanASCIICaseFolding(std::string_view lowercaseLiteral, const NKWebKit::WtfAbi::StringImpl* impl) {
    unsigned length = impl ? impl->length : 0;
    for (unsigned i = 0; i < length; ++i) {
        if (i >= lowercaseLiteral.length()) return true;
        char16_t c = ToASCIILower(GetChar(impl, i));
        char16_t literal = static_cast<char16_t>(lowercaseLiteral[i]);
        if (c != literal) return literal < c;
    }
    return false;
}

bool EqualIgnoringASCIICase(std::string_view a, std::string_view b) {
    if (a.length() != b.length()) return false;
    for (std::size_t i = 0; i < a.length(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool EndsWithIgnoringASCIICase(const NKWebKit::WtfAbi::StringImpl* impl, std::string_view suffix) {
    if (!impl) return suffix.empty();
    if (impl->length < suffix.length()) return false;
    unsigned offset = impl->length - static_cast<unsigned>(suffix.length());
    for (unsigned i = 0; i < suffix.length(); ++i) {
        char16_t c1 = GetChar(impl, offset + i);
        char c2 = suffix[i];
        if (std::tolower(static_cast<unsigned char>(c1)) != std::tolower(static_cast<unsigned char>(c2))) {
            return false;
        }
    }
    return true;
}

std::string ToString(const NKWebKit::WtfAbi::StringImpl* impl, unsigned start = 0) {
    if (!impl || start >= impl->length) return {};
    std::string res;
    res.reserve(impl->length - start);
    for (unsigned i = start; i < impl->length; ++i) {
        char16_t c = GetChar(impl, i);
        res.push_back(c < 128 ? static_cast<char>(c) : '?');
    }
    return res;
}

std::string ToStdStringLower(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl) return {};
    std::string res;
    res.reserve(impl->length);
    for (unsigned i = 0; i < impl->length; ++i) {
        char16_t c = GetChar(impl, i);
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c < 128 ? c : '?'))));
    }
    return res;
}

NKWebKit::WtfAbi::String MakeString(const char* data, unsigned length) {
    NKWebKit::WtfAbi::String res;
    _ZN3WTF6StringC1EPKcj(&res, data, length);
    return res;
}

NKWebKit::WtfAbi::String MakeString(std::string_view s) {
    return MakeString(s.data(), static_cast<unsigned>(s.length()));
}

NKWebKit::WtfAbi::String MakeStringNull() {
    return NKWebKit::WtfAbi::String(nullptr);
}

NKWebKit::WtfAbi::FixedVector<NKWebKit::WtfAbi::ASCIILiteral> MakeFixedVector(std::initializer_list<const char*> items) {
    NKWebKit::WtfAbi::FixedVector<NKWebKit::WtfAbi::ASCIILiteral> res(static_cast<unsigned>(items.size()));
    unsigned idx = 0;
    for (const char* it : items) {
        res[idx++] = NKWebKit::WtfAbi::ASCIILiteral(it);
    }
    return res;
}

std::mutex g_schemeMutex;
std::vector<std::string> g_localSchemes = {"file"};
std::vector<std::string> g_secureSchemes = {"https", "about", "data", "wss"};
std::vector<std::string> g_noAccessSchemes = {"about", "javascript", "data"};
std::vector<std::string> g_corsSchemes = {"http", "https"};
std::vector<std::string> g_emptyDocSchemes = {"about"};
std::vector<std::string> g_displayIsolatedSchemes;
std::vector<std::string> g_cachePartitionedSchemes;
std::vector<std::string> g_canDisplayOnlySchemes = {"blob"};
std::vector<std::string> g_alwaysRevalidatedSchemes;
std::vector<std::string> g_domainRelaxationForbiddenSchemes;
std::vector<std::string> g_cspBypassingSchemes;
std::vector<std::string> g_databaseAccessSchemes;

void AddScheme(std::vector<std::string>& vec, const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl) return;
    std::string s = ToStdStringLower(impl);
    if (s.empty()) return;
    if (std::find(vec.begin(), vec.end(), s) == vec.end()) {
        vec.push_back(std::move(s));
    }
}

void RemoveScheme(std::vector<std::string>& vec, const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl) return;
    std::string s = ToStdStringLower(impl);
    auto it = std::find(vec.begin(), vec.end(), s);
    if (it != vec.end()) {
        vec.erase(it);
    }
}

bool HasScheme(const std::vector<std::string>& vec, const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl) return false;
    std::string s = ToStdStringLower(impl);
    return std::find(vec.begin(), vec.end(), s) != vec.end();
}

bool IsBuiltinScheme(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl) return false;
    static const char* const builtins[] = {
        "ftp", "file", "http", "https", "ws", "wss",
        "about", "data", "javascript", "blob", "webkit-fake-url"
    };
    for (const char* b : builtins) {
        if (EqualIgnoringASCIICase(impl, b)) {
            return true;
        }
    }
    return false;
}

static const char* const kJavaScriptTypes[] = {
    "application/ecmascript",
    "application/javascript",
    "application/x-ecmascript",
    "application/x-javascript",
    "text/ecmascript",
    "text/javascript",
    "text/javascript1.0",
    "text/javascript1.1",
    "text/javascript1.2",
    "text/javascript1.3",
    "text/javascript1.4",
    "text/javascript1.5",
    "text/jscript",
    "text/livescript",
    "text/x-ecmascript",
    "text/x-javascript"
};

bool IsSupportedJavaScriptMIMEType(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl) return false;
    for (const char* t : kJavaScriptTypes) {
        if (EqualIgnoringASCIICase(impl, t)) return true;
    }
    return false;
}

static const char* const kNonImageTypes[] = {
    "text/html",
    "text/xml",
    "text/xsl",
    "text/plain",
    "text/",
    "application/xml",
    "application/xhtml+xml",
    "application/vnd.wap.xhtml+xml",
    "application/rss+xml",
    "application/atom+xml",
    "application/json",
    "image/svg+xml",
    "multipart/x-mixed-replace"
};

bool IsSupportedNonImageMIMEType(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl || impl->length == 0) return false;
    for (const char* t : kNonImageTypes) {
        if (EqualIgnoringASCIICase(impl, t)) return true;
    }
    return IsSupportedJavaScriptMIMEType(impl);
}

static const char* const kImageTypes[] = {
    "image/apng",
    "image/bmp",
    "image/gif",
    "image/jpeg",
    "image/jpg",
    "image/png",
    "image/vnd.microsoft.icon",
    "image/webp",
    "image/x-icon",
    "image/x-xbitmap"
};

const char* NormalizeImageMIMEType(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl) return "";
    if (EqualIgnoringASCIICase(impl, "application/ico")) return "image/vnd.microsoft.icon";
    if (EqualIgnoringASCIICase(impl, "image/ico")) return "image/vnd.microsoft.icon";
    if (EqualIgnoringASCIICase(impl, "image/icon")) return "image/vnd.microsoft.icon";
    if (EqualIgnoringASCIICase(impl, "image/jpg")) return "image/jpeg";
    if (EqualIgnoringASCIICase(impl, "image/pjpeg")) return "image/jpeg";
    if (EqualIgnoringASCIICase(impl, "image/vnd.rim.png")) return "image/png";
    if (EqualIgnoringASCIICase(impl, "image/x-bitmap")) return "image/bmp";
    if (EqualIgnoringASCIICase(impl, "image/x-bmp")) return "image/bmp";
    if (EqualIgnoringASCIICase(impl, "image/x-icon")) return "image/vnd.microsoft.icon";
    if (EqualIgnoringASCIICase(impl, "image/x-ms-bitmap")) return "image/bmp";
    if (EqualIgnoringASCIICase(impl, "image/x-ms-bmp")) return "image/bmp";
    if (EqualIgnoringASCIICase(impl, "image/x-png")) return "image/png";
    if (EqualIgnoringASCIICase(impl, "image/x-windows-bmp")) return "image/bmp";
    if (EqualIgnoringASCIICase(impl, "text/ico")) return "image/vnd.microsoft.icon";
    return "";
}

bool IsSupportedImageMIMEType(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl || impl->length == 0) return false;
    const char* norm = NormalizeImageMIMEType(impl);
    if (norm[0] != '\0') {
        for (const char* t : kImageTypes) {
            if (std::strcmp(norm, t) == 0) return true;
        }
    }
    for (const char* t : kImageTypes) {
        if (EqualIgnoringASCIICase(impl, t)) return true;
    }
    return false;
}

static const char* const kUnsupportedTextTypes[] = {
    "text/calendar",
    "text/directory",
    "text/ldif",
    "text/qif",
    "text/rtf",
    "text/vcalendar",
    "text/vcard",
    "text/x-calendar",
    "text/x-csv",
    "text/x-qif",
    "text/x-vcalendar",
    "text/x-vcard",
    "text/x-vcf"
};

bool IsUnsupportedTextMIMEType(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl) return false;
    for (const char* t : kUnsupportedTextTypes) {
        if (EqualIgnoringASCIICase(impl, t)) return true;
    }
    return false;
}

bool IsSupportedJSONMIMEType(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl || impl->length == 0) return false;
    if (EqualIgnoringASCIICase(impl, "application/json") || EqualIgnoringASCIICase(impl, "text/json")) {
        return true;
    }
    if (EndsWithIgnoringASCIICase(impl, "+json") && impl->length >= 8) {
        int slashPos = -1;
        for (unsigned i = 0; i < impl->length; ++i) {
            if (GetChar(impl, i) == '/') {
                slashPos = static_cast<int>(i);
                break;
            }
        }
        if (slashPos > 0 && static_cast<unsigned>(slashPos) <= impl->length - 6) {
            return true;
        }
    }
    return false;
}

bool IsXMLMIMEType(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl) return false;
    if (EqualIgnoringASCIICase(impl, "text/xml") || EqualIgnoringASCIICase(impl, "application/xml")) {
        return true;
    }
    if (!EndsWithIgnoringASCIICase(impl, "+xml")) {
        return false;
    }
    int slashPos = -1;
    for (unsigned i = 0; i < impl->length; ++i) {
        if (GetChar(impl, i) == '/') {
            slashPos = static_cast<int>(i);
            break;
        }
    }
    if (slashPos <= 0 || static_cast<unsigned>(slashPos) == impl->length - 5) {
        return false;
    }
    auto isValidXMLChar = [](char16_t c) {
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return true;
        switch (c) {
        case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+':
        case '-': case '.': case '^': case '_': case '`': case '{': case '|': case '}': case '~':
            return true;
        default:
            return false;
        }
    };
    for (unsigned i = 0; i < impl->length - 4; ++i) {
        char16_t c = GetChar(impl, i);
        if (!isValidXMLChar(c) && static_cast<int>(i) != slashPos) {
            return false;
        }
    }
    return true;
}

struct TypeExtensionPair {
    const char* type;
    const char* extension;
};

static const TypeExtensionPair kPlatformMediaTypes[] = {
    { "image/bmp", "bmp" },
    { "text/css", "css" },
    { "image/gif", "gif" },
    { "text/html", "htm" },
    { "text/html", "html" },
    { "image/x-icon", "ico" },
    { "image/jpeg", "jpeg" },
    { "image/jpeg", "jpg" },
    { "application/x-javascript", "js" },
    { "application/x-javascript", "mjs" },
    { "video/mp4", "mp4" },
    { "application/pdf", "pdf" },
    { "image/png", "png" },
    { "image/webp", "webp" },
    { "application/rss+xml", "rss" },
    { "image/svg+xml", "svg" },
    { "application/x-shockwave-flash", "swf" },
    { "text/plain", "text" },
    { "text/plain", "txt" },
    { "text/vnd.wap.wml", "wml" },
    { "application/vnd.wap.wmlc", "wmlc" },
    { "image/x-xbitmap", "xbm" },
    { "application/xhtml+xml", "xhtml" },
    { "text/xml", "xml" },
    { "text/xsl", "xsl" }
};

const char* PreferredExtensionForMIMEType(const NKWebKit::WtfAbi::StringImpl* impl) {
    if (!impl) return "";
    for (const auto& entry : kPlatformMediaTypes) {
        if (EqualIgnoringASCIICase(impl, entry.type)) {
            return entry.extension;
        }
    }
    return "";
}

const char* MimeTypeForExtension(std::string_view ext) {
    for (const auto& entry : kPlatformMediaTypes) {
        if (EqualIgnoringASCIICase(ext, entry.extension)) return entry.type;
    }
    return "";
}

static const TypeExtensionPair kCommonMediaTypes[] = {
    { "application/ogg", "ogx" },
    { "audio/ogg", "ogg" },
    { "audio/ogg", "oga" },
    { "video/ogg", "ogv" },
    { "application/annodex", "anx" },
    { "audio/annodex", "axa" },
    { "video/annodex", "axv" },
    { "audio/speex", "spx" },
    { "video/webm", "webm" },
    { "audio/webm", "webm" },
    { "audio/mpeg", "m1a" },
    { "audio/mpeg", "m2a" },
    { "audio/mpeg", "m1s" },
    { "audio/mpeg", "mpa" },
    { "video/mpeg", "mpg" },
    { "video/mpeg", "m15" },
    { "video/mpeg", "m1s" },
    { "video/mpeg", "m1v" },
    { "video/mpeg", "m75" },
    { "video/mpeg", "mpa" },
    { "video/mpeg", "mpeg" },
    { "video/mpeg", "mpm" },
    { "video/mpeg", "mpv" },
    { "application/vnd.apple.mpegurl", "m3u8" },
    { "application/mpegurl", "m3u8" },
    { "application/x-mpegurl", "m3u8" },
    { "audio/mpegurl", "m3url" },
    { "audio/x-mpegurl", "m3url" },
    { "audio/mpegurl", "m3u" },
    { "audio/x-mpegurl", "m3u" },
    { "video/x-m4v", "m4v" },
    { "audio/x-m4a", "m4a" },
    { "audio/x-m4b", "m4b" },
    { "audio/x-m4p", "m4p" },
    { "audio/mp4", "m4a" },
    { "audio/mp3", "mp3" },
    { "audio/x-mp3", "mp3" },
    { "audio/x-mpeg", "mp3" },
    { "video/x-mpeg2", "mp2" },
    { "video/mpeg2", "vob" },
    { "video/mpeg2", "mod" },
    { "video/m2ts", "m2ts" },
    { "video/x-m2ts", "m2t" },
    { "video/x-m2ts", "ts" },
    { "audio/3gpp", "3gpp" },
    { "audio/3gpp2", "3g2" },
    { "application/x-mpeg", "amc" },
    { "audio/aac", "aac" },
    { "audio/aac", "adts" },
    { "audio/x-aac", "m4r" },
    { "audio/x-caf", "caf" },
    { "audio/x-gsm", "gsm" },
    { "audio/x-wav", "wav" },
    { "audio/vnd.wave", "wav" }
};

const char* MediaMIMETypeForExtension(std::string_view ext) {
    const char* systemType = MimeTypeForExtension(ext);
    if (systemType[0] != '\0') return systemType;
    for (const auto& entry : kCommonMediaTypes) {
        if (EqualIgnoringASCIICase(ext, entry.extension)) return entry.type;
    }
    return "";
}

constexpr std::string_view kSystemPreviewMIMETypes[] = {
    "model/usd",
    "model/vnd.pixar.usd",
    "model/vnd.reality",
    "model/vnd.usdz+zip"
};

bool IsSystemPreviewMIMEType(const NKWebKit::WtfAbi::StringImpl* impl) {
    auto it = std::lower_bound(std::begin(kSystemPreviewMIMETypes), std::end(kSystemPreviewMIMETypes), impl,
        [](std::string_view literal, const NKWebKit::WtfAbi::StringImpl* key) { return LiteralLessThanASCIICaseFolding(literal, key); });
    return it != std::end(kSystemPreviewMIMETypes) && EqualLettersIgnoringASCIICase(impl, *it);
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore20LegacySchemeRegistry15isBuiltinSchemeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    return IsBuiltinScheme(scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry24registerURLSchemeAsLocalERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_localSchemes, scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry25registerURLSchemeAsSecureERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_secureSchemes, scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry27registerURLSchemeAsNoAccessERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_noAccessSchemes, scheme.impl);
}

bool APS5_VABI _ZN7WebCore20LegacySchemeRegistry27shouldTreatURLSchemeAsLocalERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    return HasScheme(g_localSchemes, scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry30registerURLSchemeAsCORSEnabledERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_corsSchemes, scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry32registerURLSchemeAsEmptyDocumentERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_emptyDocSchemes, scheme.impl);
}

bool APS5_VABI _ZN7WebCore20LegacySchemeRegistry33shouldTreatURLSchemeAsCORSEnabledERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    return HasScheme(g_corsSchemes, scheme.impl);
}

bool APS5_VABI _ZN7WebCore20LegacySchemeRegistry34shouldLoadURLSchemeAsEmptyDocumentERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    return HasScheme(g_emptyDocSchemes, scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry34registerURLSchemeAsDisplayIsolatedERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_displayIsolatedSchemes, scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry35registerURLSchemeAsCachePartitionedERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_cachePartitionedSchemes, scheme.impl);
}

NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> APS5_VABI _ZN7WebCore20LegacySchemeRegistry36allURLSchemesRegisteredAsCORSEnabledEv() {
    std::lock_guard lock(g_schemeMutex);
    NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> result(static_cast<unsigned>(g_corsSchemes.size()));
    for (const auto& s : g_corsSchemes) {
        result.append(MakeString(s));
    }
    return result;
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry36registerAsCanDisplayOnlyIfCanRequestERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_canDisplayOnlySchemes, scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry36registerURLSchemeAsAlwaysRevalidatedERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_alwaysRevalidatedSchemes, scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry40setDomainRelaxationForbiddenForURLSchemeEbRKN3WTF6StringE(bool forbidden, const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    if (forbidden) {
        AddScheme(g_domainRelaxationForbiddenSchemes, scheme.impl);
    } else {
        RemoveScheme(g_domainRelaxationForbiddenSchemes, scheme.impl);
    }
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry49registerURLSchemeAsBypassingContentSecurityPolicyERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_cspBypassingSchemes, scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry57removeURLSchemeRegisteredAsBypassingContentSecurityPolicyERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    RemoveScheme(g_cspBypassingSchemes, scheme.impl);
}

void APS5_VABI _ZN7WebCore20LegacySchemeRegistry58registerURLSchemeAsAllowingDatabaseAccessInPrivateBrowsingERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scheme) {
    std::lock_guard lock(g_schemeMutex);
    AddScheme(g_databaseAccessSchemes, scheme.impl);
}

NKWebKit::WtfAbi::FixedVector<NKWebKit::WtfAbi::ASCIILiteral> APS5_VABI _ZN7WebCore16MIMETypeRegistry12pdfMIMETypesEv() {
    return MakeFixedVector({"application/pdf", "text/pdf"});
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry13isXMLMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    return IsXMLMIMEType(mimeType.impl);
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry14isTextMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    if (!mimeType.impl) return false;
    if (IsSupportedJavaScriptMIMEType(mimeType.impl)) return true;
    if (IsSupportedJSONMIMEType(mimeType.impl)) return true;
    if (StartsWithIgnoringASCIICase(mimeType.impl, "text/")) {
        if (!EqualIgnoringASCIICase(mimeType.impl, "text/html")
            && !EqualIgnoringASCIICase(mimeType.impl, "text/xml")
            && !EqualIgnoringASCIICase(mimeType.impl, "text/xsl")) {
            return true;
        }
    }
    return false;
}

NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> APS5_VABI _ZN7WebCore16MIMETypeRegistry16allowedMIMETypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(
    const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& mimeTypes,
    const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& extensions) {
    std::vector<std::optional<std::string>> collected;
    for (unsigned i = 0; i < mimeTypes.size; ++i) {
        if (!mimeTypes.buffer[i].impl) {
            if (std::find(collected.begin(), collected.end(), std::nullopt) == collected.end()) {
                collected.push_back(std::nullopt);
            }
        } else {
            std::string s = ToStdStringLower(mimeTypes.buffer[i].impl);
            if (std::find_if(collected.begin(), collected.end(), [&](const auto& opt) { return opt && *opt == s; }) == collected.end()) {
                collected.push_back(std::move(s));
            }
        }
    }
    for (unsigned i = 0; i < extensions.size; ++i) {
        if (extensions.buffer[i].impl) {
            std::string ext = ToString(extensions.buffer[i].impl);
            std::string_view view = ext;
            if (!view.empty() && view.front() == '.') {
                view.remove_prefix(1);
            }
            const char* mime = MimeTypeForExtension(view);
            if (mime[0] != '\0') {
                std::string s;
                for (std::size_t j = 0; mime[j] != '\0'; ++j) {
                    s.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(mime[j]))));
                }
                if (std::find_if(collected.begin(), collected.end(), [&](const auto& opt) { return opt && *opt == s; }) == collected.end()) {
                    collected.push_back(std::move(s));
                }
            }
        }
    }
    NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String> result(static_cast<unsigned>(collected.size()));
    for (const auto& item : collected) {
        if (!item) {
            result.append(MakeStringNull());
        } else {
            result.append(MakeString(*item));
        }
    }
    return result;
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry19isXMLEntityMIMETypeEN3WTF10StringViewE(NKWebKit::WtfAbi::StringView mimeType) {
    return EqualIgnoringASCIICase(mimeType, "text/xml-external-parsed-entity")
        || EqualIgnoringASCIICase(mimeType, "application/xml-external-parsed-entity");
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry23isSupportedJSONMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    return IsSupportedJSONMIMEType(mimeType.impl);
}

NKWebKit::WtfAbi::FixedVector<NKWebKit::WtfAbi::ASCIILiteral> APS5_VABI _ZN7WebCore16MIMETypeRegistry23supportedImageMIMETypesEv() {
    return MakeFixedVector({
        "image/apng",
        "image/bmp",
        "image/gif",
        "image/jpeg",
        "image/jpg",
        "image/png",
        "image/vnd.microsoft.icon",
        "image/webp",
        "image/x-icon",
        "image/x-xbitmap"
    });
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry24isSupportedImageMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    return IsSupportedImageMIMEType(mimeType.impl);
}

NKWebKit::WtfAbi::FixedVector<NKWebKit::WtfAbi::ASCIILiteral> APS5_VABI _ZN7WebCore16MIMETypeRegistry24unsupportedTextMIMETypesEv() {
    return MakeFixedVector({
        "text/calendar",
        "text/directory",
        "text/ldif",
        "text/qif",
        "text/rtf",
        "text/vcalendar",
        "text/vcard",
        "text/x-calendar",
        "text/x-csv",
        "text/x-qif",
        "text/x-vcalendar",
        "text/x-vcard",
        "text/x-vcf"
    });
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry25isPDFOrPostScriptMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    if (!mimeType.impl) return false;
    return EqualIgnoringASCIICase(mimeType.impl, "application/pdf")
        || EqualIgnoringASCIICase(mimeType.impl, "text/pdf")
        || EqualIgnoringASCIICase(mimeType.impl, "application/postscript");
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry27isSupportedNonImageMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    return IsSupportedNonImageMIMEType(mimeType.impl);
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry28isSupportedTextTrackMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    if (!mimeType.impl) return false;
    return EqualIgnoringASCIICase(mimeType.impl, "text/vtt");
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry29isSupportedJavaScriptMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    return IsSupportedJavaScriptMIMEType(mimeType.impl);
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16MIMETypeRegistry29preferredExtensionForMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& type) {
    const char* ext = PreferredExtensionForMIMEType(type.impl);
    if (ext[0] == '\0') {
        return MakeString("", 0);
    }
    return MakeString(ext);
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16MIMETypeRegistry30appendFileExtensionIfNecessaryERKN3WTF6StringES4_(
    const NKWebKit::WtfAbi::String& filename,
    const NKWebKit::WtfAbi::String& mimeType) {
    if (!filename.impl || filename.impl->length == 0) {
        return filename;
    }
    for (unsigned i = 0; i < filename.impl->length; ++i) {
        if (GetChar(filename.impl, i) == '.') {
            return filename;
        }
    }
    if (EqualIgnoringASCIICase(mimeType.impl, "application/octet-stream")) {
        return filename;
    }
    const char* ext = PreferredExtensionForMIMEType(mimeType.impl);
    if (ext[0] == '\0') {
        return filename;
    }
    std::string base = ToString(filename.impl);
    base.push_back('.');
    base.append(ext);
    return MakeString(base);
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry32containsImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(
    const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& mimeTypes,
    const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& extensions) {
    auto preferred = _ZN7WebCore16MIMETypeRegistry33preferredImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(mimeTypes, extensions);
    return preferred.impl != nullptr;
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16MIMETypeRegistry33preferredImageMIMETypeForEncodingERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(
    const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& mimeTypes,
    const NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::String>& extensions) {
    auto allowed = _ZN7WebCore16MIMETypeRegistry16allowedMIMETypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEES8_(mimeTypes, extensions);
    for (unsigned i = 0; i < allowed.size; ++i) {
        if (allowed.buffer[i].impl && EqualIgnoringASCIICase(allowed.buffer[i].impl, "image/png")) {
            return NKWebKit::WtfAbi::String(allowed.buffer[i]);
        }
    }
    return MakeStringNull();
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry34isSupportedImageVideoOrSVGMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    if (!mimeType.impl) return false;
    return IsSupportedImageMIMEType(mimeType.impl) || EqualIgnoringASCIICase(mimeType.impl, "image/svg+xml");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16MIMETypeRegistry15mimeTypeForPathERKN3WTF6StringE(const NKWebKit::WtfAbi::String& path) {
    const NKWebKit::WtfAbi::StringImpl* impl = path.impl;
    for (unsigned i = impl ? impl->length : 0; i > 0; --i) {
        if (GetChar(impl, i - 1) == '.') {
            const char* type = MimeTypeForExtension(ToString(impl, i));
            if (type[0] != '\0') return MakeString(type);
            break;
        }
    }
    return _ZN7WebCore15defaultMIMETypeEv();
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry20isJavaAppletMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    return StartsWithLettersIgnoringASCIICase(mimeType.impl, "application/x-java-applet")
        || StartsWithLettersIgnoringASCIICase(mimeType.impl, "application/x-java-bean")
        || StartsWithLettersIgnoringASCIICase(mimeType.impl, "application/x-java-vm");
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16MIMETypeRegistry20mimeTypeForExtensionERKN3WTF6StringE(const NKWebKit::WtfAbi::String& extension) {
    return MakeString(MimeTypeForExtension(ToString(extension.impl)));
}

bool APS5_VABI _ZN7WebCore16MIMETypeRegistry23isSystemPreviewMIMETypeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& mimeType) {
    return IsSystemPreviewMIMEType(mimeType.impl);
}

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16MIMETypeRegistry25mediaMIMETypeForExtensionERKN3WTF6StringE(const NKWebKit::WtfAbi::String& extension) {
    return MakeString(MediaMIMETypeForExtension(ToString(extension.impl)));
}

}