#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <string>
#include <string_view>

namespace {

using namespace NKWebKit::WtfAbi;
using Predicate = bool (APS5_VABI *)(char16_t);
std::u16string calls;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF whitespace check failed at line %u\n", location.line());
        std::abort();
    }
}

bool APS5_VABI Whitespace(char16_t c) {
    calls.push_back(c);
    return c == ' ' || c == '\t' || c == '\n';
}

bool APS5_VABI Punctuation(char16_t c) {
    calls.push_back(c);
    return c == ',' || c == '!';
}

String Make(std::u16string_view input, bool is8Bit) {
    if (!is8Bit) return _ZN3WTF10StringImpl6createEPKDsj(input.data(), static_cast<unsigned>(input.size()));
    std::string bytes(input.begin(), input.end());
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(bytes.data()), static_cast<unsigned>(bytes.size()));
}

void Check(std::u16string_view input, std::u16string_view expected, bool is8Bit, Predicate predicate = Whitespace, bool same = false) {
    auto string = Make(input, is8Bit);
    unsigned refs = string.impl->refCount;
    calls.clear();
    {
        auto result = _ZNK3WTF6String18simplifyWhiteSpaceEPFbDsE(&string, predicate);
        Require(result.impl && result.impl->length == expected.size());
        Require(!!(result.impl->hashAndFlags & 4) == (expected.empty() || is8Bit));
        Require((result.impl == string.impl) == same);
        Require(string.impl->refCount == refs + (same ? 2 : 0));
        for (unsigned i = 0; i < expected.size(); ++i) {
            auto value = is8Bit ? static_cast<const unsigned char*>(result.impl->data)[i] : static_cast<const char16_t*>(result.impl->data)[i];
            Require(value == expected[i]);
        }
    }
    Require(string.impl->refCount == refs);
}

}  // namespace

int main() {
    try {
        String null;
        Require(!_ZNK3WTF6String18simplifyWhiteSpaceEPFbDsE(&null, nullptr).impl);
        for (bool is8Bit : {true, false}) {
            Check(u"", u"", is8Bit, nullptr, true);
            Check(u" \t\n ", u"", is8Bit);
            Require(calls == u" \t\n ");
            Check(u"abc", u"abc", is8Bit, Whitespace, true);
            Require(calls == u"aabc");
            Check(u"a b", u"a b", is8Bit, Whitespace, true);
            Require(calls == u"aa  bb");
            Check(u"  a\t \tb  ", u"a b", is8Bit);
            Require(calls == u"  aa\t\t \tbb   ");
            Check(u"a\t b", u"a b", is8Bit);
            Check(u"a\tb", u"a b", is8Bit);
            Check(u"  abc", u"abc", is8Bit);
            Check(u"abc  ", u"abc", is8Bit);
            Check(u",,!a!!b,,", u"a b", is8Bit, Punctuation);
            Require(calls == u",,!aa!!!bb,,,");
            Check(u"a ", u"a", is8Bit, Punctuation);
            auto input = Make(u"a", is8Bit);
            bool threw = false;
            try { auto result = _ZNK3WTF6String18simplifyWhiteSpaceEPFbDsE(&input, nullptr); }
            catch (const std::exception&) { threw = true; }
            Require(threw);
        }
        Check(u" \u03b1\t\t\u03b2 ", u"\u03b1 \u03b2", false);
        Check(u"\u00e9 \u00ff", u"\u00e9 \u00ff", true, Whitespace, true);
        Check(u"\u00e9 \u00ff", u"\u00e9 \u00ff", false, Whitespace, true);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "WTF whitespace exception: %s\n", error.what());
        return 1;
    }
}
