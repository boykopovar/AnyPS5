#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceJsc/include/ApiBridgeExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "Jsc URL bridge check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

std::u16string Units(const std::string& text) {
    std::u16string units;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 5 < text.size() && text[i + 1] == 'u') {
            units.push_back(static_cast<char16_t>(std::stoul(text.substr(i + 2, 4), nullptr, 16)));
            i += 5;
        } else
            units.push_back(static_cast<unsigned char>(text[i]));
    }
    return units;
}

String Decode(std::string text) {
    if (text == "\\N") return String();
    bool wide = text.rfind("\\W", 0) == 0;
    if (wide) text = text.substr(2);
    std::u16string units = Units(text);
    bool latin1 = !wide;
    for (char16_t unit : units) latin1 &= unit <= 0xFF;
    if (latin1) {
        std::string bytes(units.begin(), units.end());
        return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<unsigned>(bytes.size()));
    }
    return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
}

std::string Encode(const String& value) {
    if (!value.impl) return "\\N";
    std::string out;
    char buffer[8];
    const bool is8Bit = value.impl->hashAndFlags & 4;
    for (unsigned i = 0; i < value.impl->length; ++i) {
        const char16_t unit = is8Bit ? static_cast<const unsigned char*>(value.impl->data)[i] : static_cast<const char16_t*>(value.impl->data)[i];
        if (unit >= 0x20 && unit < 0x7F && unit != '\\') out.push_back(static_cast<char>(unit));
        else {
            std::snprintf(buffer, sizeof buffer, "\\u%04X", unit);
            out += buffer;
        }
    }
    return out;
}

std::string Describe(const URL& url) {
    const String string(url.impl ? (url.impl->refCount += 2, url.impl) : nullptr);
    char fields[160];
    const unsigned bits = url.bits;
    std::snprintf(fields, sizeof fields, "%u %u %u %u %u %u %u %u %u %u %u %u %d ", bits & 1, (bits >> 1) & 1, (bits >> 2) & 1, (bits >> 3) & 7, bits >> 6,
        url.userStart, url.userEnd, url.passwordEnd, url.hostEnd, url.pathAfterLastSlash, url.pathEnd, url.queryEnd,
        string.impl ? static_cast<int>(!!(string.impl->hashAndFlags & 4)) : -1);
    return fields + Encode(string);
}

struct EncodedBytes {
    unsigned char* buffer;
    unsigned capacity;
    unsigned size;
};

void APS5_VABI Latin1OrQuestionMark(EncodedBytes* result, const void*, NKWebKit::WtfAbi::StringView view) {
    result->buffer = static_cast<unsigned char*>(_ZN3WTF10fastMallocEm(view.length ? view.length : 1));
    result->capacity = view.length;
    result->size = view.length;
    for (unsigned i = 0; i < view.length; ++i) {
        const char16_t unit = view.is8Bit ? static_cast<const unsigned char*>(view.data)[i] : static_cast<const char16_t*>(view.data)[i];
        result->buffer[i] = unit <= 0xFF ? static_cast<unsigned char>(unit) : '?';
    }
}

const void* const EncodingVtable[] = {reinterpret_cast<const void*>(&Latin1OrQuestionMark), nullptr, nullptr};
const struct {
    const void* const* vtable;
} Encoding {EncodingVtable};

URL Parse(const URL& base, const String& input, const void* encoding = nullptr) {
    URL url;
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &input, encoding);
    return url;
}

Jsc::WtfAbi::URL* Guest(URL* url) {
    return reinterpret_cast<Jsc::WtfAbi::URL*>(url);
}

const Jsc::WtfAbi::URL* Guest(const URL* url) {
    return reinterpret_cast<const Jsc::WtfAbi::URL*>(url);
}

const Jsc::WtfAbi::String* Guest(const String* string) {
    return reinterpret_cast<const Jsc::WtfAbi::String*>(string);
}

URL Construct(const URL& base, const String& input, const void* encoding = nullptr) {
    URL url;
    _ZN3WTF3URLC1ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(Guest(&url), Guest(&base), Guest(&input), encoding);
    return url;
}

Jsc::WtfAbi::StringView ViewOf(const String& string) {
    if (!string.impl) return {nullptr, 0, true};
    return {string.impl->data, string.impl->length, !!(string.impl->hashAndFlags & 4)};
}

void SetPassword(URL& url, const String& password) {
    _ZN3WTF3URL11setPasswordENS_10StringViewE(Guest(&url), ViewOf(password));
}

struct ConstructorCase {
    const char* base;
    const char* input;
    const char* expected;
};

const ConstructorCase ConstructorCases[] = {
    {R"(http://example.org/foo/bar)", R"(http://example\u0009.\u000Aorg)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.org/)"},
    {R"(http://example.org/foo/bar)", R"(http://user:pass@foo:21/bar;par?b#c)", R"(1 1 0 3 4 7 11 16 20 24 31 33 1 http://user:pass@foo:21/bar;par?b#c)"},
    {R"(http://example.org/foo/bar)", R"(http:foo.com)", R"(1 1 0 0 4 7 7 7 18 23 30 30 1 http://example.org/foo/foo.com)"},
    {R"(http://example.org/foo/bar)", R"(a:\u0009 foo.com)", R"(1 0 1 0 1 2 2 2 2 2 10 10 1 a: foo.com)"},
    {R"(http://example.org/foo/bar)", R"(http://f:b/c)", R"(0 0 0 0 0 0 0 0 0 0 0 0 1 http://f:b/c)"},
    {R"(file:///tmp/mock/path)", R"(C|/foo/bar)", R"(1 0 0 0 4 7 7 7 7 15 18 18 1 file:///C:/foo/bar)"},
    {R"(mailto:x@x.com)", R"(#x)", R"(1 0 0 0 6 7 7 7 7 7 14 14 1 mailto:x@x.com#x)"},
    {R"(\N)", R"(\Ehttp://h/p?)", R"(1 1 0 0 4 7 7 7 8 9 10 11 1 http://h/p?)"},
    {R"(\N)", R"(\E\Whttp://h/p?)", R"(1 1 0 0 4 7 7 7 8 9 10 11 0 http://h/p?)"},
    {R"(\N)", R"(\Ehttp://h/p?\u20AC=x)", R"(1 1 0 0 4 7 7 7 8 9 10 14 1 http://h/p??=x)"},
};

void TestConstructorCases() {
    for (const ConstructorCase& test : ConstructorCases) {
        const String baseText = Decode(test.base);
        const URL empty {};
        URL base {};
        if (baseText.impl) base = Parse(empty, baseText);
        std::string input = test.input;
        const bool encoded = input.rfind("\\E", 0) == 0;
        if (encoded) input = input.substr(2);
        const String relative = Decode(input);
        const void* const encoding = encoded ? &Encoding : nullptr;
        URL url = Construct(base, relative, encoding);
        URL reference = Parse(base, relative, encoding);
        const std::string actual = Describe(url);
        if (actual != test.expected || actual != Describe(reference)) {
            std::fprintf(stderr, "base <%s> input <%s>\n  expected %s\n  actual   %s\n  C2       %s\n", test.base, test.input, test.expected,
                actual.c_str(), Describe(reference).c_str());
            std::abort();
        }
        String owner(url.impl);
        String referenceOwner(reference.impl);
        String baseOwner(base.impl);
    }
}

void TestConstructorAgainstBaseObjectConstructor() {
    const URL empty {};
    const struct {
        const char* base;
        const char* input;
    } pairs[] = {
        {R"(\N)", R"(foo)"},
        {R"(\N)", R"(\N)"},
        {R"(\N)", R"()"},
        {R"(http://f:b/c)", R"(d)"},
        {R"(http://h/a/b?q#f)", R"(../c d)"},
        {R"(http://h/a/b?q#f)", R"(\W?\u03B1)"},
        {R"(sc://host/path)", R"(//other/x)"},
    };
    for (const auto& pair : pairs) {
        const String baseText = Decode(pair.base);
        URL base {};
        if (baseText.impl) base = Parse(empty, baseText);
        const String relative = Decode(pair.input);
        URL url = Construct(base, relative);
        URL reference = Parse(base, relative);
        Require(Describe(url) == Describe(reference));
        String owner(url.impl);
        String referenceOwner(reference.impl);
        String baseOwner(base.impl);
    }

    const String query = Decode(R"(http://h/p?\u20AC=x)");
    URL plain = Construct(empty, query);
    URL encoded = Construct(empty, query, &Encoding);
    Require(Describe(plain) == Describe(Parse(empty, query)));
    Require(Describe(encoded) == "1 1 0 0 4 7 7 7 8 9 10 14 1 http://h/p??=x");
    Require(Describe(plain) != Describe(encoded));
    String plainOwner(plain.impl);
    String encodedOwner(encoded.impl);
}

void TestConstructorReferences() {
    const URL empty {};
    const String canonical = Decode("https://example.com/a?b#c");
    const unsigned before = canonical.impl->refCount;
    {
        URL url = Construct(empty, canonical);
        Require(url.impl == canonical.impl);
        Require(canonical.impl->refCount == before + 2);
        String owner(url.impl);
    }
    Require(canonical.impl->refCount == before);

    const String changed = Decode("HTTPS://Example.com:443/./a");
    URL url = Construct(empty, changed);
    Require(url.impl != changed.impl);
    Require(Describe(url) == "1 1 0 0 5 8 8 8 19 20 21 21 1 https://example.com/a");
    String owner(url.impl);
    Require(changed.impl->refCount == 2);

    URL base = Parse(empty, Decode("http://h/p?q#f"));
    String baseOwner(base.impl);
    const unsigned baseReferences = base.impl->refCount;
    URL fromNull = Construct(base, String());
    Require(Describe(fromNull) == "1 1 0 0 4 7 7 7 8 9 10 12 1 http://h/p?q");
    String fromNullOwner(fromNull.impl);
    Require(base.impl->refCount == baseReferences);
}

void TestConstructorArguments() {
    const URL base {};
    const String input = Decode("http://h/");
    URL url {};
    RequireInvalidArgument([&] { _ZN3WTF3URLC1ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(nullptr, Guest(&base), Guest(&input), nullptr); });
    RequireInvalidArgument([&] { _ZN3WTF3URLC1ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(Guest(&url), nullptr, Guest(&input), nullptr); });
    RequireInvalidArgument([&] { _ZN3WTF3URLC1ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(Guest(&url), Guest(&base), nullptr, nullptr); });
    const struct {
        const void* const* vtable;
    } noVtable {nullptr};
    RequireInvalidArgument([&] { _ZN3WTF3URLC1ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(Guest(&url), Guest(&base), Guest(&input), &noVtable); });
    Require(!url.impl && !url.bits && !url.queryEnd);
    Require(input.impl->refCount == 2);
}

struct PasswordCase {
    const char* input;
    const char* password;
    const char* assembled;
    const char* expected;
};

const PasswordCase PasswordCases[] = {
    {R"(http://example.net)", R"(secret)", R"(http://:secret@example.net/)", R"(1 1 0 0 4 7 7 14 26 27 27 27 1 http://:secret@example.net/)"},
    {R"(http://me@example.net)", R"(secret)", R"(http://me:secret@example.net/)", R"(1 1 0 0 4 7 9 16 28 29 29 29 1 http://me:secret@example.net/)"},
    {R"(http://me:old@example.net)", R"(secret)", R"(http://me:secret@example.net/)", R"(1 1 0 0 4 7 9 16 28 29 29 29 1 http://me:secret@example.net/)"},
    {R"(http://:old@example.net)", R"(secret)", R"(http://:secret@example.net/)", R"(1 1 0 0 4 7 7 14 26 27 27 27 1 http://:secret@example.net/)"},
    {R"(http://:secret@example.net)", R"()", R"(http://example.net/)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.net/)"},
    {R"(http://me:secret@example.net)", R"()", R"(http://me@example.net/)", R"(1 1 0 0 4 7 9 9 21 22 22 22 1 http://me@example.net/)"},
    {R"(http://me:secret@example.net)", R"(\N)", R"(http://me@example.net/)", R"(1 1 0 0 4 7 9 9 21 22 22 22 1 http://me@example.net/)"},
    {R"(http://me:secret@example.net)", R"(\W)", R"(http://me@example.net/)", R"(1 1 0 0 4 7 9 9 21 22 22 22 1 http://me@example.net/)"},
    {R"(http://me@example.net)", R"()", R"(http://me@example.net/)", R"(1 1 0 0 4 7 9 9 21 22 22 22 1 http://me@example.net/)"},
    {R"(http://example.net)", R"()", R"(http://example.net/)", R"(1 1 0 0 4 7 7 7 18 19 19 19 1 http://example.net/)"},
    {R"(http://example.net)", R"(p@s:s/w d\u00E9)", R"(http://:p%40s%3As%2Fw%20d%C3%A9@example.net/)", R"(1 1 0 0 4 7 7 31 43 44 44 44 1 http://:p%40s%3As%2Fw%20d%C3%A9@example.net/)"},
    {R"(http://example.net)", R"(%c3%89t\u00E9)", R"(http://:%c3%89t%C3%A9@example.net/)", R"(1 1 0 0 4 7 7 21 33 34 34 34 1 http://:%c3%89t%C3%A9@example.net/)"},
    {R"(http://example.net)", R"(\Wp\u03B1)", R"(http://:p%CE%B1@example.net/)", R"(1 1 0 0 4 7 7 15 27 28 28 28 1 http://:p%CE%B1@example.net/)"},
    {R"(http://example.net)", R"(\Wsecret)", R"(\Whttp://:secret@example.net/)", R"(1 1 0 0 4 7 7 14 26 27 27 27 0 http://:secret@example.net/)"},
    {R"(http://me@host:8080/p?q#f)", R"(x)", R"(http://me:x@host:8080/p?q#f)", R"(1 1 0 5 4 7 9 11 16 22 23 25 1 http://me:x@host:8080/p?q#f)"},
    {R"(mailto:me@example.net)", R"(secret)", R"(mailto://:secret@me@example.net)", nullptr},
    {R"(javascript:alert(1))", R"(secret)", R"(javascript://:secret@alert(1))", nullptr},
    {R"(file:///home/x)", R"(secret)", R"(file://:secret@/home/x)", nullptr},
    {R"(sc://x/)", R"(pw)", R"(sc://:pw@x/)", nullptr},
    {R"(sc://u@x/)", R"(pw)", R"(sc://u:pw@x/)", nullptr},
};

void TestPasswordCases() {
    const URL empty {};
    for (const PasswordCase& test : PasswordCases) {
        URL url = Parse(empty, Decode(test.input));
        const String password = Decode(test.password);
        SetPassword(url, password);
        URL reference = Parse(empty, Decode(test.assembled));
        const std::string actual = Describe(url);
        const std::string parsed = Describe(reference);
        if (actual != parsed || (test.expected && actual != test.expected)) {
            std::fprintf(stderr, "setPassword <%s> on <%s>\n  expected %s\n  parsed   %s\n  actual   %s\n", test.password, test.input,
                test.expected ? test.expected : "(the parsed assembly)", parsed.c_str(), actual.c_str());
            std::abort();
        }
        String owner(url.impl);
        String referenceOwner(reference.impl);
    }
}

void TestPasswordLeavesInvalidUrls() {
    const URL empty {};
    const String secret = Decode("secret");

    URL invalid = Parse(empty, Decode("http://f:b/c"));
    String invalidOwner(invalid.impl);
    Require(Describe(invalid) == "0 0 0 0 0 0 0 0 0 0 0 0 1 http://f:b/c");
    StringImpl* const invalidImpl = invalid.impl;
    const unsigned invalidReferences = invalidImpl->refCount;
    SetPassword(invalid, secret);
    Require(invalid.impl == invalidImpl && invalidImpl->refCount == invalidReferences);
    Require(Describe(invalid) == "0 0 0 0 0 0 0 0 0 0 0 0 1 http://f:b/c");
    SetPassword(invalid, String());
    Require(invalid.impl == invalidImpl && invalidImpl->refCount == invalidReferences);

    URL null {};
    SetPassword(null, secret);
    Require(!null.impl && !null.bits && !null.userStart && !null.userEnd && !null.passwordEnd && !null.hostEnd);
    Require(!null.pathAfterLastSlash && !null.pathEnd && !null.queryEnd);
    Require(secret.impl->refCount == 2);
}

void TestPasswordReferences() {
    const URL empty {};
    const String text = Decode("http://me:old@example.net/");
    URL url = Parse(empty, text);
    Require(url.impl == text.impl);
    const unsigned before = text.impl->refCount;
    const String replacement = Decode("new");
    SetPassword(url, replacement);
    Require(url.impl && url.impl != text.impl && url.impl->refCount == 2);
    Require(text.impl->refCount == before - 2);
    Require(replacement.impl->refCount == 2);
    Require(Describe(url) == "1 1 0 0 4 7 9 13 25 26 26 26 1 http://me:new@example.net/");

    StringImpl* const withPassword = url.impl;
    withPassword->refCount += 2;
    SetPassword(url, String());
    Require(url.impl && url.impl != withPassword && url.impl->refCount == 2);
    Require(withPassword->refCount == 2);
    Require(Describe(url) == "1 1 0 0 4 7 9 9 21 22 22 22 1 http://me@example.net/");
    NKWebKit::WtfAbi::Release(withPassword);

    StringImpl* const withoutPassword = url.impl;
    SetPassword(url, String());
    Require(url.impl == withoutPassword && withoutPassword->refCount == 2);
    SetPassword(url, Decode(""));
    Require(url.impl == withoutPassword && withoutPassword->refCount == 2);
    Require(Describe(url) == "1 1 0 0 4 7 9 9 21 22 22 22 1 http://me@example.net/");
    String owner(url.impl);
}

void TestPasswordArguments() {
    const URL empty {};
    URL url = Parse(empty, Decode("http://h/"));
    String owner(url.impl);
    const Jsc::WtfAbi::StringView missing {nullptr, 3, true};
    const Jsc::WtfAbi::StringView none {nullptr, 0, true};
    RequireInvalidArgument([&] { _ZN3WTF3URL11setPasswordENS_10StringViewE(nullptr, none); });
    RequireInvalidArgument([&] { _ZN3WTF3URL11setPasswordENS_10StringViewE(Guest(&url), missing); });
    Require(Describe(url) == "1 1 0 0 4 7 7 7 8 9 9 9 1 http://h/");
    Require(url.impl->refCount == 2);
}

}

int main() {
    TestConstructorCases();
    TestConstructorAgainstBaseObjectConstructor();
    TestConstructorReferences();
    TestConstructorArguments();
    TestPasswordCases();
    TestPasswordLeavesInvalidUrls();
    TestPasswordReferences();
    TestPasswordArguments();
    std::puts("Jsc URL bridge tests passed");
    return 0;
}
