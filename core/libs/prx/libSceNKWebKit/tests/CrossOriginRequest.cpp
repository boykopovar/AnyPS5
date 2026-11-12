#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstdio>
#include <exception>
#include <string>
#include <string_view>

namespace {

using namespace NKWebKit::HttpAbi;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

unsigned checks = 0;
unsigned failures = 0;

struct Text {
    std::u16string wide;
    std::string narrow;
    StringImpl impl;
    String value;

    Text(std::u16string_view text, bool isWide) : wide(text), impl{2, static_cast<unsigned>(text.size()), nullptr, isWide ? 0u : 4u}, value(&impl) {
        for (auto c : text) narrow.push_back(static_cast<char>(c));
        impl.data = isWide ? static_cast<const void*>(wide.data()) : narrow.data();
    }
    ~Text() { value.impl = nullptr; }
};

void Check(std::u16string_view method, const HTTPHeaderMap& map, bool expected, const char* label, bool wide) {
    Text text(method, wide);
    ++checks;
    try {
        auto actual = _ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(text.value, map);
        if (actual == expected) return;
        std::fprintf(stderr, "%s wide=%d: expected %d got %d\n", label, wide, expected, actual);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s wide=%d: %s\n", label, wide, error.what());
    }
    ++failures;
}

void Header(HTTPHeaderName name, std::u16string_view value, bool expected, bool wide) {
    Text text(value, wide);
    CommonHeader entry{name, text.value};
    HTTPHeaderMap map{{&entry, 1, 1}, {nullptr, 0, 0}};
    Check(u"GET", map, expected, "header", wide);
    entry.value.impl = nullptr;
}

}  // namespace

int main() {
    for (bool wide : {false, true}) {
        HTTPHeaderMap empty{};
        for (auto method : {u"GET", u"HEAD", u"POST"}) Check(method, empty, true, "simple method", wide);
        for (auto method : {u"PUT", u"get", u"", u"DELETE", u"OPTIONS", u"Get", u"POST "})
            Check(method, empty, false, "non-simple method", wide);
        struct Case { HTTPHeaderName name; std::u16string_view value; bool expected; };
        const Case cases[] = {
            {HTTPHeaderName::Accept, u"", true},
            {HTTPHeaderName::Accept, u"text/html, application/json;q=0.9", true},
            {HTTPHeaderName::Accept, u"*/*\t", true},
            {HTTPHeaderName::Accept, u"text/plain; x=\"a\"", false},
            {HTTPHeaderName::Accept, u"\u00e9", true},
            {HTTPHeaderName::AcceptLanguage, u"en-US, *;q=0.5", true},
            {HTTPHeaderName::AcceptLanguage, u"", true},
            {HTTPHeaderName::AcceptLanguage, u"en\tUS", false},
            {HTTPHeaderName::AcceptLanguage, u"en_US", false},
            {HTTPHeaderName::AcceptLanguage, u"en/US", false},
            {HTTPHeaderName::ContentLanguage, u"en-US, fr;q=1.0", true},
            {HTTPHeaderName::ContentLanguage, u"", true},
            {HTTPHeaderName::ContentLanguage, u"en:US", false},
            {HTTPHeaderName::ContentLanguage, u"\u00e9", false},
            {HTTPHeaderName::ContentType, u"application/x-www-form-urlencoded", true},
            {HTTPHeaderName::ContentType, u"APPLICATION/X-WWW-FORM-URLENCODED;charset=utf-8", true},
            {HTTPHeaderName::ContentType, u"multipart/form-data;boundary=abc-123", true},
            {HTTPHeaderName::ContentType, u"text/plain; charset=utf-8", true},
            {HTTPHeaderName::ContentType, u" \tTeXt/PlAiN \t", true},
            {HTTPHeaderName::ContentType, u"text/plain;", true},
            {HTTPHeaderName::ContentType, u"text/plain;;x", true},
            {HTTPHeaderName::ContentType, u"text/plain;a=", true},
            {HTTPHeaderName::ContentType, u"text/plain;bad(name)=x", false},
            {HTTPHeaderName::ContentType, u"text/plain;charset=\"utf-8\"", false},
            {HTTPHeaderName::ContentType, u"text/plain;a=\u00e9", true},
            {HTTPHeaderName::ContentType, u"text/plain/extra", false},
            {HTTPHeaderName::ContentType, u"text/ plain", false},
            {HTTPHeaderName::ContentType, u"text /plain", false},
            {HTTPHeaderName::ContentType, u"/plain", false},
            {HTTPHeaderName::ContentType, u"text/", false},
            {HTTPHeaderName::ContentType, u"text", false},
            {HTTPHeaderName::ContentType, u"", false},
            {HTTPHeaderName::ContentType, u"application/json", false},
            {HTTPHeaderName::ContentType, u"text/html", false},
            {HTTPHeaderName::ContentType, u"multipart/mixed", false},
            {HTTPHeaderName::Range, u"bytes=0-499", true},
            {HTTPHeaderName::Range, u"BYTES=500-", true},
            {HTTPHeaderName::Range, u"bytes=-500", false},
            {HTTPHeaderName::Range, u"bytes=0-1,3-4", false},
            {HTTPHeaderName::Range, u"bytes=2-1", false},
            {HTTPHeaderName::Range, u"bytes=0--1", true},
            {HTTPHeaderName::Range, u"bytes=0--2", false},
            {HTTPHeaderName::Range, u"bytes=+0-+1", true},
            {HTTPHeaderName::Range, u"bytes=0-9223372036854775807", true},
            {HTTPHeaderName::Range, u"bytes=0-9223372036854775808", false},
            {HTTPHeaderName::Range, u"bytes=9223372036854775808-", false},
            {HTTPHeaderName::Range, u"bytes=0--9223372036854775808", false},
            {HTTPHeaderName::Range, u"bytes=0--9223372036854775809", false},
            {HTTPHeaderName::Range, u"bytes=0-1 ", false},
            {HTTPHeaderName::Range, u"bytes =0-1", false},
            {HTTPHeaderName::Range, u"bytes=\t0-1", false},
            {HTTPHeaderName::Range, u"bytes\r\n=0-1\r\n", true},
            {HTTPHeaderName::Range, u"bytes=\v0-\f1\v", true},
            {HTTPHeaderName::Range, u"bytes=0-1junk", false},
            {HTTPHeaderName::Range, u"bytes=0", false},
            {HTTPHeaderName::Range, u"bytes=", false},
            {HTTPHeaderName::Range, u"items=0-1", false},
            {HTTPHeaderName::Range, u"", false},
            {HTTPHeaderName::Authorization, u"", false},
            {HTTPHeaderName::ContentLength, u"0", false},
        };
        for (const auto& test : cases) Header(test.name, test.value, test.expected, wide);
        for (auto name : {HTTPHeaderName::Accept, HTTPHeaderName::AcceptLanguage, HTTPHeaderName::ContentLanguage}) {
            Header(name, std::u16string(128, 'a'), true, wide);
            Header(name, std::u16string(129, 'a'), false, wide);
        }
        for (unsigned c = 0; c <= 255; ++c) {
            bool unsafe = (c < 0x20 && c != 9) || c == 127;
            for (auto forbidden : u"\"():<>?@[\\]{}") if (forbidden && c == forbidden) unsafe = true;
            Header(HTTPHeaderName::Accept, std::u16string(1, static_cast<char16_t>(c)), !unsafe, wide);
            auto content = std::u16string(u"text/plain;x=") + static_cast<char16_t>(c);
            Header(HTTPHeaderName::ContentType, content, !unsafe, wide);
        }
        Header(HTTPHeaderName::ContentType, std::u16string(u"text/plain;x=") + std::u16string(115, 'a'), true, wide);
        Header(HTTPHeaderName::ContentType, std::u16string(u"text/plain;x=") + std::u16string(116, 'a'), false, wide);
        Header(HTTPHeaderName::Range, std::u16string(u"bytes=") + std::u16string(121, '0') + u"-", true, wide);
        Header(HTTPHeaderName::Range, std::u16string(u"bytes=") + std::u16string(122, '0') + u"-", false, wide);
        Text key(u"X-Custom", wide);
        Text value(u"ok", wide);
        UncommonHeader uncommon{key.value, value.value};
        HTTPHeaderMap unknown{{nullptr, 0, 0}, {&uncommon, 1, 1}};
        Check(u"GET", unknown, false, "uncommon header", wide);
        Text acceptKey(u"Accept", wide);
        uncommon.key.impl = acceptKey.value.impl;
        Check(u"POST", unknown, false, "uncommon entry has no enum key", wide);
        uncommon.key.impl = nullptr;
        uncommon.value.impl = nullptr;
        for (int index = 0; index < 95; ++index) {
            auto name = static_cast<HTTPHeaderName>(index);
            if (name == HTTPHeaderName::Accept || name == HTTPHeaderName::AcceptLanguage
                || name == HTTPHeaderName::ContentLanguage || name == HTTPHeaderName::ContentType
                || name == HTTPHeaderName::Range) continue;
            Header(name, u"", false, wide);
        }
        Text good(u"text/plain", wide);
        Text bad(u"bad\nvalue", wide);
        std::array<CommonHeader, 3> entries{{{HTTPHeaderName::Accept, good.value},
            {HTTPHeaderName::ContentType, good.value}, {HTTPHeaderName::AcceptLanguage, bad.value}}};
        HTTPHeaderMap multiple{{entries.data(), 3, 2}, {nullptr, 0, 0}};
        Check(u"POST", multiple, true, "all safe", wide);
        multiple.common.size = 3;
        Check(u"HEAD", multiple, false, "last header unsafe", wide);
        for (auto& entry : entries) entry.value.impl = nullptr;
    }
    Header(HTTPHeaderName::Accept, u"\u0100\u1234", true, true);
    Header(HTTPHeaderName::AcceptLanguage, u"\u0100", false, true);
    Header(HTTPHeaderName::ContentLanguage, u"\u0100", false, true);
    Header(HTTPHeaderName::ContentType, u"text/plain;x=\u0100", true, true);
    Header(HTTPHeaderName::ContentType, u"text/pl\u0100in", false, true);
    Header(HTTPHeaderName::Range, u"bytes=\uff10-1", false, true);
    String nullString;
    CommonHeader nullHeader{HTTPHeaderName::Accept, nullString};
    HTTPHeaderMap nullValue{{&nullHeader, 1, 1}, {nullptr, 0, 0}};
    Check(u"GET", nullValue, true, "null Accept value", false);
    nullHeader.key = HTTPHeaderName::ContentType;
    Check(u"POST", nullValue, false, "null Content-Type value", false);
    Text get(u"GET", false);
    for (unsigned kind = 0; kind < 3; ++kind) {
        HTTPHeaderMap broken{};
        StringImpl brokenImpl{2, 1, nullptr, 4};
        CommonHeader header{HTTPHeaderName::Accept, String(&brokenImpl)};
        if (kind == 0) broken.common = {nullptr, 1, 1};
        if (kind == 1) broken.uncommon = {nullptr, 1, 1};
        if (kind == 2) broken.common = {&header, 1, 1};
        ++checks;
        try {
            _ZN7WebCore32isSimpleCrossOriginAccessRequestERKN3WTF6StringERKNS_13HTTPHeaderMapE(get.value, broken);
            ++failures;
            std::fprintf(stderr, "invalid accessed storage did not throw\n");
        } catch (const std::exception&) {
        }
        header.value.impl = nullptr;
    }
    std::printf("%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
