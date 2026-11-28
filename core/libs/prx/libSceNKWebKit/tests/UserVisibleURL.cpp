// Derived from neonmodder123/PS5-WebKit-11.x/Tools/TestWebKitAPI/Tests/WTF/cocoa/URLExtras.mm@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace {

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;
using UserVisibleURL = String (APS5_VABI*)(const CString&);
static_assert(std::is_same_v<decltype(&_ZN3WTF10URLHelpers14userVisibleURLERKNS_7CStringE), UserVisibleURL>);

unsigned checks = 0;
unsigned failures = 0;

void Require(bool condition, int line) {
    ++checks;
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "user visible URL check failed at line %d\n", line);
    }
}

CString Make(std::string_view bytes) {
    CString value;
    _ZN3WTF7CStringC1EPKcm(&value, bytes.data(), bytes.size());
    return value;
}

String Visible(std::string_view bytes) {
    const CString url = Make(bytes);
    return _ZN3WTF10URLHelpers14userVisibleURLERKNS_7CStringE(url);
}

std::u16string Units(const String& string) {
    std::u16string units;
    const auto view = NKWebKit::WtfAbi::ViewOf(string.impl);
    for (unsigned i = 0; i < view.length; ++i) units.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, i));
    return units;
}

bool Is8Bit(const String& string) {
    return string.impl->hashAndFlags & 4;
}

void Expect(std::string_view input, std::u16string_view expected, int line) {
    try {
        const String result = Visible(input);
        const bool matches = result.impl && Units(result) == expected && result.impl->refCount == 2;
        if (!matches) std::fprintf(stderr, "input '%.*s' gave an unexpected string\n", static_cast<int>(input.size()), input.data());
        Require(matches, line);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "input '%.*s' threw: %s\n", static_cast<int>(input.size()), input.data(), error.what());
        Require(false, line);
    }
}

void ExpectWidth(std::string_view input, bool is8Bit, int line) {
    const String result = Visible(input);
    Require(result.impl && Is8Bit(result) == is8Bit, line);
}

void ExpectNotImplemented(std::string_view input, int line) {
    const auto before = NotImplementedCalls_nid_no_patch();
    try {
        Visible(input);
        Require(false, line);
    } catch (const std::runtime_error&) {
        Require(NotImplementedCalls_nid_no_patch() == before + 1, line);
    }
}

}  // namespace

#define EXPECT(input, expected) Expect(input, expected, __LINE__)

int main() {
    using namespace std::string_view_literals;

    Require(!_ZN3WTF10URLHelpers14userVisibleURLERKNS_7CStringE(CString()).impl, __LINE__);
    Require(!Visible("").impl, __LINE__);
    const String nul = Visible("\0"sv);
    Require(nul.impl && !nul.impl->length, __LINE__);

    EXPECT("http://site.com", u"http://site.com");
    EXPECT("http://%77ebsite.com", u"http://%77ebsite.com");
    ExpectWidth("http://site.com", true, __LINE__);
    ExpectWidth("http://site.com%C3%B7othersite.org", false, __LINE__);
    ExpectWidth("a%20b", true, __LINE__);
    ExpectWidth("a b", false, __LINE__);

    EXPECT("http://site.com\xC3\xB7othersite.org", u"http://site.com÷othersite.org");
    EXPECT("http://site.com%C3%B7othersite.org", u"http://site.com÷othersite.org");
    EXPECT("http://site.com%c3%b7othersite.org", u"http://site.com÷othersite.org");
    EXPECT("http://site.com\xEF\xBC\x8Fothersite.org", u"http://site.com%EF%BC%8Fothersite.org");
    EXPECT("http://site.com%EF%BC%8Fothersite.org", u"http://site.com%EF%BC%8Fothersite.org");
    EXPECT("http://site.com\xE3\x80\x80othersite.org", u"http://site.com%E3%80%80othersite.org");
    EXPECT("http://site.com%E3%80%80othersite.org", u"http://site.com%E3%80%80othersite.org");

    EXPECT("%E2%82%AC", u"€");
    EXPECT("%41%7f%7F", u"%41%7f%7F");
    EXPECT("%G1", u"%G1");
    EXPECT("%1G", u"%1G");
    EXPECT("abc%", u"abc%");
    EXPECT("abc%4", u"abc%4");
    EXPECT("%E2%82%A", u"%E2%82%A");
    EXPECT("%%E2%82%AC", u"%€");

    EXPECT("%FF", u"%FF");
    EXPECT("%ff", u"%FF");
    EXPECT("\xFF", u"%FF");
    EXPECT("\xC3\xA9\xFF", u"%C3%A9%FF");
    EXPECT("%C3%A9%C3", u"%C3%A9%C3");
    EXPECT("%C0%AF", u"%C0%AF");
    EXPECT("%ED%A0%80", u"%ED%A0%80");
    EXPECT("%F4%90%80%80", u"%F4%90%80%80");
    EXPECT("a\0b"sv, u"a");
    EXPECT("\xC3\xA9\0\xFF"sv, u"é");

    EXPECT("e%CC%81", u"é");
    EXPECT("e\xCC\x81", u"é");

    EXPECT("a b", u"a%20b");
    EXPECT("a\tb", u"a%09b");
    EXPECT("a\x7F", u"a%7F");
    EXPECT("a\xC2\xA0" "b", u"a%C2%A0b");
    EXPECT("a\xE2\x80\xA8" "b", u"a%E2%80%A8b");
    EXPECT("a\xE2\x80\x8B" "b", u"a%E2%80%8Bb");
    EXPECT("a\xC9\x90" "b", u"a%C9%90b");
    EXPECT("a\xF0\x90\x90\x80" "b", u"a%F0%90%90%80b");
    EXPECT("a\xE2\x88\x95" "b", u"a%E2%88%95b");
    EXPECT("a\xE2\x81\x84" "b", u"a%E2%81%84b");
    EXPECT("a\xF0\x9F\x94\x92" "b", u"a%F0%9F%94%92b");
    EXPECT("a\xC2\xBD", u"a%C2%BD");
    EXPECT("\xEF\xBF\xBD", u"%EF%BF%BD");
    EXPECT("\xF0\x9F\x98\x80", u"\U0001F600");

    EXPECT("\xC4\xB1\xCC\x87", u"ı%CC%87");
    EXPECT("\xC8\xB7\xCC\x87", u"%C8%B7%CC%87");
    EXPECT("\xD7\x95\xCC\x87", u"ו%CC%87");
    EXPECT("i\xCC\x87", u"i̇");

    EXPECT("https://en.wikipedia.org/wiki/.հայ", u"https://en.wikipedia.org/wiki/.հայ");
    EXPECT("https://ճմո.հայ", u"https://ճմո.հայ");
    EXPECT("https://ճ-1-մո.հայ", u"https://ճ-1-մո.հայ");
    EXPECT("https://2ճ_մո.հայ", u"https://2ճ_մո.հայ");
    EXPECT("https://ճ_մո3.հայ", u"https://ճ_մո3.հայ");
    EXPECT("https://gotՑԵՃ.com", u"https://got%D5%91ԵՃ.com");
    EXPECT("https://ՑԵՃfans.net", u"https://ՑԵՃfans.net");
    EXPECT("https://ՑԵorյՃ.biz", u"https://ՑԵorյՃ.biz");
    EXPECT("https://ՑԵand!$^&*()-~+={}or<>,.?յՃ.biz", u"https://ՑԵand!$^&*()-~+={}or<>,.?յՃ.biz");
    EXPECT("https://Ցg/", u"https://Ց%67/");
    EXPECT("https://ցg/", u"https://ց%67/");
    EXPECT("https://oՕ/", u"https://o%D5%95%2F");
    EXPECT("https://oօ/", u"https://o%D6%85%2F");
    EXPECT("/Օ", u"/Օ");
    EXPECT("Օ.", u"Օ.");

    EXPECT("https://௦௧௨௩count/", u"https://௦௧௨௩count/");
    EXPECT("o௦", u"o%E0%AF%A6");
    EXPECT("https://ᑭᐁabc/", u"https://ᑭᐁabc/");
    EXPECT("https://ᖯᐁabc/", u"https://ᖯᐁabc/");
    EXPECT("https://᙮ᐁabc/", u"https://᙮ᐁabc/");
    EXPECT("aᖯ", u"a%E1%96%AF");
    EXPECT("ᖯa", u"ᖯ%61");
    EXPECT("https://กขabc/", u"https://กขabc/");
    EXPECT("nก", u"n%E0%B8%81");
    EXPECT("1ก", u"1ก");
    EXPECT("https://ؠٛabc/", u"https://ؠٛabc/");
    EXPECT("aٛ", u"a%D9%9B");
    EXPECT("ٛ", u"%D9%9B");
    EXPECT("https://íÍabc/", u"https://íÍabc/");
    EXPECT("\U00010548a", u"\U00010548%61");

    ExpectNotImplemented("http://xn--abc.com/", __LINE__);
    ExpectNotImplemented("http://XN--abc.com/", __LINE__);
    ExpectNotImplemented("xn--", __LINE__);
    ExpectNotImplemented("%E2xn--", __LINE__);
    EXPECT("%78n--", u"%78n--");
    EXPECT("%E2-", u"%E2-");
    EXPECT("%E2%E2xn-", u"%E2%E2xn-");
    EXPECT("x-n--", u"x-n--");
    EXPECT("xn-", u"xn-");

    std::printf("%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
