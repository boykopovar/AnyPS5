#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

unsigned checks = 0;
unsigned failures = 0;

void Check(const NKWebKit::WtfAbi::String& input, WebCore::Mode mode, bool expected, unsigned row, bool wide) {
    ++checks;
    try {
        bool actual = _ZN7WebCore18isValidContentTypeERKN3WTF6StringENS_4ModeE(input, mode);
        if (actual == expected) return;
        std::fprintf(stderr, "row %u, wide %d, mode %d: expected %d, got %d\n", row, wide, static_cast<int>(mode), expected, actual);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "row %u: %s\n", row, error.what());
    }
    ++failures;
}

void CheckNullStorage(WebCore::Mode mode) {
    ++checks;
    NKWebKit::WtfAbi::StringImpl impl{2, 4, nullptr, 4};
    NKWebKit::WtfAbi::String input(&impl);
    try {
        _ZN7WebCore18isValidContentTypeERKN3WTF6StringENS_4ModeE(input, mode);
        std::fprintf(stderr, "null storage, mode %d: no exception\n", static_cast<int>(mode));
        ++failures;
    } catch (const std::invalid_argument&) {
    } catch (const std::exception& error) {
        std::fprintf(stderr, "null storage, mode %d: %s\n", static_cast<int>(mode), error.what());
        ++failures;
    }
    input.impl = nullptr;
}

}  // namespace

int main() {
    struct Case { std::u16string_view text; bool rfc; bool sniff; };
    const Case cases[] = {
        {u"", false, false}, {u" \t", false, false},
        {u"text/plain", true, true}, {u"APPLICATION/X-Test+json", true, true},
        {u"text", false, false}, {u"/plain", false, false}, {u"text/", false, false},
        {u"te xt/plain", false, false}, {u"text /plain", false, false},
        {u"text/ plain", false, false}, {u"text/plain/extra", true, false},
        {u"text/pl(ain", true, false}, {u"te(xt/plain", false, false},
        {u"text/plain;", false, true}, {u"text/plain;;", false, true},
        {u"text/plain; charset=utf-8", true, true},
        {u"text/plain;charset=utf-8;x=y", true, true},
        {u"text/plain; charset", false, true}, {u"text/plain; =x", false, true},
        {u"text/plain;a=", false, true}, {u"text/plain;a=;b=x", false, true},
        {u"text/plain;a =x", false, true}, {u"text/plain;a= x", false, true},
        {u"text/plain;a=x y", false, true},
        {u"text/plain;a=\"\"", true, true}, {u"text/plain;a=\"x\"", true, true},
        {u"text/plain;a=\"x\";", true, true}, {u"text/plain;a=\"x\";b=y", false, true},
        {u"text/plain;a=\"x\";;b=y", true, true},
        {u"text/plain;a=\"x", false, true}, {u"text/plain;a=\"", false, true},
        {u"text/plain;a=\"x\\", false, true},
        {u"text/plain;a=\"x\\\"y\"", true, true},
        {u"text/plain;a=\"x\\\\\"", true, true},
        {u"text/plain;a=\"x\"z", true, true},
        {u"text/plain;a=\"x\"zz", false, true},
        {u" \ttext/plain \t", true, true},
        {u"\r\ntext/plain\r\n", false, true},
        {u"text/pl\nain", false, false}, {u"te\nxt/plain", false, false},
        {u"text/plain;a=\"x\ny\"", false, true},
        {u"text/plain;\nb=x", false, true},
        {u"text/\u007f", true, false}, {u"text/\u00e9", false, false},
        {u"text/plain;a=\"\u00e9\"", true, true},
        {u"text/plain;a=\u00e9", false, true},
        {u"text/plain;bad(name)=x", false, true},
        {u"text/ \t;x=y", false, false},
        {u"text/plain;a=x;", true, true}, {u"text/plain;a=x;;", false, true},
        {u"text/plain;a=x\r", false, true}, {u"text/plain\f", true, false},
        {std::u16string_view(u"text/plain;a=\"\0x\"", 17), true, true},
    };
    for (bool wide : {false, true}) {
        unsigned row = 0;
        for (const auto& test : cases) {
            ++row;
            std::string narrow;
            for (auto c : test.text) narrow.push_back(static_cast<char>(c));
            NKWebKit::WtfAbi::StringImpl impl{2, static_cast<unsigned>(test.text.size()), wide ? static_cast<const void*>(test.text.data()) : narrow.data(), wide ? 0u : 4u};
            NKWebKit::WtfAbi::String input(&impl);
            Check(input, WebCore::Mode::Rfc2045, test.rfc, row, wide);
            Check(input, WebCore::Mode::MimeSniff, test.sniff, row, wide);
            input.impl = nullptr;
        }
    }
    for (const auto& test : {Case{u"text/\u03a9", false, false}, Case{u"text/pl\u03a9", true, false},
            Case{u"text/plain;a=\"\u03a9\"", true, true}, Case{u"text/plain;a=\"\u0122", true, true}}) {
        NKWebKit::WtfAbi::StringImpl impl{2, static_cast<unsigned>(test.text.size()), test.text.data(), 0};
        NKWebKit::WtfAbi::String input(&impl);
        Check(input, WebCore::Mode::Rfc2045, test.rfc, 100, true);
        Check(input, WebCore::Mode::MimeSniff, test.sniff, 100, true);
        input.impl = nullptr;
    }
    NKWebKit::WtfAbi::String null;
    Check(null, WebCore::Mode::Rfc2045, false, 0, false);
    Check(null, WebCore::Mode::MimeSniff, false, 0, false);
    CheckNullStorage(WebCore::Mode::Rfc2045);
    CheckNullStorage(WebCore::Mode::MimeSniff);
    std::printf("%u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
