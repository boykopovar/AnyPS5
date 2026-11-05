// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/URL.cpp
/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

#include "WtfURLSetters.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

using namespace NKWebKit::WtfAbi;
using namespace NKWebKit::URLSetters;

namespace {

template<typename T> T& Checked(T* value) {
    if (!value) throw std::invalid_argument("null WTF object");
    return *value;
}

}  // namespace

namespace {

bool SpecialURL(const URL& url) {
    const auto scheme = URLBytes_nid_no_patch(URLUnits_nid_no_patch(Slice_nid_no_patch(View_nid_no_patch(url.impl), 0, url.bits >> 6)), 2);
    return URLDefaultPort_nid_no_patch(scheme) || scheme == "file";
}

void ReparseURL(URL& url, const std::u16string& text, bool keepTrailing = false) {
    String input(CopyImpl_nid_no_patch({text.data(), static_cast<unsigned>(text.size()), false}));
    URL empty{};
    URL result{};
    const void* encoding = keepTrailing ? reinterpret_cast<const void*>(std::numeric_limits<std::uintptr_t>::max()) : nullptr;
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&result, &empty, &input, encoding);
    Release(url.impl);
    url = result;
}

void RemoveURLPart(URL& url, unsigned start, unsigned length) {
    if (!length) return;
    auto text = URLUnits_nid_no_patch(View_nid_no_patch(url.impl));
    text.erase(start, length);
    ReparseURL(url, text);
}

unsigned URLCredentialsEnd(const URL& url) {
    auto end = url.passwordEnd;
    if (end != url.hostEnd && Unit_nid_no_patch(View_nid_no_patch(url.impl), end) == '@') ++end;
    return end;
}

void CheckSetterHost(const URL& url, std::u16string_view host) {
    if (SpecialURL(url) && host.size() <= 2048 && std::any_of(host.begin(), host.end(), [](char16_t unit) { return unit > 127; }))
        throw std::runtime_error("WTF::URL host setter requires IDNA");
}

bool SetterPort(std::u16string_view text) {
    auto space = [](char16_t unit) { return unit == ' ' || (unit >= 9 && unit <= 13); };
    while (!text.empty() && space(text.front())) text.remove_prefix(1);
    while (!text.empty() && space(text.back())) text.remove_suffix(1);
    if (text.starts_with(u"+")) text.remove_prefix(1);
    if (text.empty()) return false;
    unsigned value = 0;
    for (char16_t unit : text) {
        if (unit < '0' || unit > '9') return false;
        value = value * 10 + unit - '0';
        if (value > 65535) return false;
    }
    return true;
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN3WTF3URL11setProtocolENS_10StringViewE(URL* self, StringView protocol) {
    auto& url = Checked(self);
    auto scheme = URLUnits_nid_no_patch(protocol);
    scheme.resize(std::min(scheme.size(), scheme.find(':')));
    auto ignored = [](char16_t unit) { return unit == '\t' || unit == '\n' || unit == '\r'; };
    auto alpha = [](char16_t unit) { return (unit >= 'a' && unit <= 'z') || (unit >= 'A' && unit <= 'Z'); };
    std::size_t i = 0;
    while (i < scheme.size() && ignored(scheme[i])) ++i;
    if (i == scheme.size() || !alpha(scheme[i++])) return false;
    for (; i < scheme.size(); ++i)
        if (!alpha(scheme[i]) && !(scheme[i] >= '0' && scheme[i] <= '9') && scheme[i] != '+' && scheme[i] != '-' && scheme[i] != '.' && !ignored(scheme[i])) return false;
    for (auto& unit : scheme) unit = AsciiCase_nid_no_patch(unit, true);
    auto text = URLUnits_nid_no_patch(View_nid_no_patch(url.impl));
    if (!(url.bits & 1)) { ReparseURL(url, scheme + u":" + text); return true; }
    if ((url.passwordEnd != url.userStart || ((url.bits >> 3) & 7)) && scheme == u"file") return true;
    if (text.substr(0, url.bits >> 6) == u"file" && HostStart_nid_no_patch(url) == url.hostEnd) return true;
    ReparseURL(url, scheme + text.substr(url.bits >> 6));
    return true;
}

void APS5_VABI _ZN3WTF3URL7setHostENS_10StringViewE(URL* self, StringView host) {
    auto& url = Checked(self);
    if (!(url.bits & 1)) return;
    auto value = URLUnits_nid_no_patch(host);
    if (value.find(':') != std::u16string::npos && !value.starts_with(u"[")) return;
    value.resize(std::min(value.size(), value.find_first_of(SpecialURL(url) ? u"/\\?#" : u"/?#")));
    CheckSetterHost(url, value);
    auto text = URLUnits_nid_no_patch(View_nid_no_patch(url.impl));
    ReparseURL(url, text.substr(0, HostStart_nid_no_patch(url)) + (url.userStart == (url.bits >> 6) + 1 ? u"//" : u"") + value + text.substr(url.hostEnd));
}

void APS5_VABI _ZN3WTF3URL14setHostAndPortENS_10StringViewE(URL* self, StringView hostAndPort) {
    auto& url = Checked(self);
    if (!(url.bits & 1)) return;
    auto host = URLUnits_nid_no_patch(hostAndPort);
    std::u16string port;
    const auto colon = host.rfind(':');
    if (colon != std::u16string::npos) {
        port = host.substr(colon + 1);
        host.resize(colon);
        if (host.find(':') != std::u16string::npos && !host.starts_with(u"[")) return;
        if (!SetterPort(port)) port.clear();
    }
    if (host.empty()) { RemoveURLPart(url, HostStart_nid_no_patch(url), PathStart_nid_no_patch(url) - HostStart_nid_no_patch(url)); return; }
    CheckSetterHost(url, host);
    auto text = URLUnits_nid_no_patch(View_nid_no_patch(url.impl));
    ReparseURL(url, text.substr(0, HostStart_nid_no_patch(url)) + (url.userStart == (url.bits >> 6) + 1 ? u"//" : u"") + host + (port.empty() ? u"" : u":") + port + text.substr(PathStart_nid_no_patch(url)));
}

void APS5_VABI _ZN3WTF3URL7setUserENS_10StringViewE(URL* self, StringView user) {
    auto& url = Checked(self);
    if (!(url.bits & 1)) return;
    auto text = URLUnits_nid_no_patch(View_nid_no_patch(url.impl));
    auto end = url.userEnd;
    if (user.length) {
        const bool separator = end == url.hostEnd || (end == url.passwordEnd && text[end] != '@');
        const auto encoded = EncodeURL_nid_no_patch(URLUnits_nid_no_patch(user), 2, 0);
        ReparseURL(url, text.substr(0, url.userStart) + (url.userStart == (url.bits >> 6) + 1 ? u"//" : u"")
            + std::u16string(encoded.begin(), encoded.end()) + (separator ? u"@" : u"") + text.substr(end));
    } else {
        if (url.userEnd == url.passwordEnd && end != url.hostEnd && text[end] == '@') ++end;
        RemoveURLPart(url, url.userStart, end - url.userStart);
    }
}

void APS5_VABI _ZN3WTF3URL17removeCredentialsEv(URL* self) {
    auto& url = Checked(self);
    if (url.bits & 1) RemoveURLPart(url, url.userStart, URLCredentialsEnd(url) - url.userStart);
}

void APS5_VABI _ZN3WTF3URL21setFragmentIdentifierENS_10StringViewE(URL* self, StringView fragment) {
    auto& url = Checked(self);
    if (url.bits & 1) ReparseURL(url, URLUnits_nid_no_patch(View_nid_no_patch(url.impl)).substr(0, url.queryEnd) + u"#" + URLUnits_nid_no_patch(fragment), true);
}

void APS5_VABI _ZN3WTF3URL8setQueryENS_10StringViewE(URL* self, StringView query) {
    auto& url = Checked(self);
    if (!(url.bits & 1)) return;
    auto text = URLUnits_nid_no_patch(View_nid_no_patch(url.impl));
    auto value = URLUnits_nid_no_patch(query);
    ReparseURL(url, text.substr(0, url.pathEnd) + (!value.starts_with(u"?") && query.data ? u"?" : u"") + value + text.substr(url.queryEnd));
}

void APS5_VABI _ZN3WTF3URL7setPathENS_10StringViewE(URL* self, StringView path) {
    auto& url = Checked(self);
    if (!(url.bits & 1)) return;
    const auto value = URLUnits_nid_no_patch(path);
    const bool slash = value.starts_with(u"/") || (value.starts_with(u"\\") && SpecialURL(url)) || (!SpecialURL(url) && value.empty() && (url.bits >> 6) + 1 < PathStart_nid_no_patch(url));
    const bool dot = !SpecialURL(url) && HostStart_nid_no_patch(url) == url.hostEnd && value.starts_with(u"//") && value.size() > 2;
    std::u16string encoded;
    constexpr char16_t hex[] = u"0123456789ABCDEF";
    for (unsigned char byte : URLBytes_nid_no_patch(value, 0)) {
        if (byte == '?' || byte == '#' || byte > 127) {
            encoded.push_back('%'); encoded.push_back(hex[byte >> 4]); encoded.push_back(hex[byte & 15]);
        } else encoded.push_back(byte);
    }
    auto text = URLUnits_nid_no_patch(View_nid_no_patch(url.impl));
    ReparseURL(url, text.substr(0, PathStart_nid_no_patch(url)) + (slash ? u"" : u"/") + (dot ? u"/." : u"") + encoded + text.substr(url.pathEnd));
}

}
