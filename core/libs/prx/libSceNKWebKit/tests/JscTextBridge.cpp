#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JscTextBridge check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using Builder = NKWebKit::WtfAbi::StringBuilder;
using Stream = NKWebKit::WtfAbi::TextStream;
using WtfString = NKWebKit::WtfAbi::String;
using WtfUrl = NKWebKit::WtfAbi::URL;
using NKWebKit::WtfAbi::StringImpl;

template<typename Function>
bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

std::u16string Units(const StringImpl* impl, unsigned length) {
    std::u16string result;
    for (unsigned i = 0; i < length; ++i) {
        if (impl->hashAndFlags & 4) result += static_cast<const unsigned char*>(impl->data)[i];
        else result += static_cast<const char16_t*>(impl->data)[i];
    }
    return result;
}

std::u16string Contents(const Builder& builder) {
    const auto length = static_cast<unsigned>(builder.length);
    if (!length) return {};
    if (builder.string.impl) return Units(builder.string.impl, length);
    Require(builder.buffer != nullptr);
    return Units(builder.buffer, length);
}

Stream NewStream() {
    Stream stream{Builder{WtfString()}, 0, 0, true};
    stream.text.shouldCrashOnOverflow = true;
    stream.text.is8Bit = true;
    return stream;
}

void Free(Stream& stream) {
    NKWebKit::WtfAbi::Release(stream.text.buffer);
    stream.text.buffer = nullptr;
}

Jsc::WtfAbi::TextStream* Guest(Stream& stream) {
    return reinterpret_cast<Jsc::WtfAbi::TextStream*>(&stream);
}

void Indent(Stream& stream, int indent) {
    _ZN3WTF11writeIndentERNS_10TextStreamEi(Guest(stream), indent);
}

void Format(Stream& stream, double value) {
    const Jsc::WtfAbi::FormatNumberRespectingIntegers number{value};
    Require(_ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(Guest(stream), &number) == Guest(stream));
}

std::u16string Formatted(double value) {
    Stream stream = NewStream();
    Format(stream, value);
    Require(stream.text.is8Bit && stream.text.buffer && stream.text.buffer->length == 16);
    Require(stream.formattingFlags == 0 && stream.indent == 0 && stream.multiLineMode);
    std::u16string text = Contents(stream.text);
    Free(stream);
    return text;
}

void AppendWide(Stream& stream) {
    const char16_t alpha[] = {0x3b1};
    _ZN3WTF13StringBuilder16appendCharactersEPKDsj(&stream.text, alpha, 1);
    Require(!stream.text.is8Bit);
}

void TestWriteIndent() {
    Stream stream = NewStream();
    Indent(stream, 0);
    Require(stream.text.length == 0 && !stream.text.buffer && !stream.text.string.impl);
    Indent(stream, -4);
    Require(stream.text.length == 0 && !stream.text.buffer && !stream.text.string.impl);
    Indent(stream, 1);
    Require(Contents(stream.text) == u"  " && stream.text.buffer && stream.text.buffer->length == 16);
    Indent(stream, 3);
    Require(Contents(stream.text) == std::u16string(8, u' ') && stream.text.buffer->length == 16);
    Free(stream);

    Stream deep = NewStream();
    Indent(deep, 9);
    Require(Contents(deep.text) == std::u16string(18, u' ') && deep.text.buffer->length == 32);
    Free(deep);

    Stream own = NewStream();
    own.indent = 5;
    own.multiLineMode = false;
    own.formattingFlags = 3;
    Indent(own, 2);
    Require(Contents(own.text) == u"    " && own.indent == 5 && !own.multiLineMode && own.formattingFlags == 3);
    Free(own);

    Stream wide = NewStream();
    AppendWide(wide);
    Indent(wide, 2);
    Require(!wide.text.is8Bit && Contents(wide.text) == u"α    ");
    Free(wide);

    Require(ThrowsInvalidArgument([] { _ZN3WTF11writeIndentERNS_10TextStreamEi(nullptr, 1); }));
}

void TestFormatNumberRespectingIntegers() {
    Require(Formatted(3.0) == u"3");
    Require(Formatted(-7.0) == u"-7");
    Require(Formatted(0.0) == u"0");
    Require(Formatted(-0.0) == u"0");
    Require(Formatted(123456.0) == u"123456");
    Require(Formatted(2147483647.0) == u"2147483647");

    Require(Formatted(1.5) == u"1.50");
    Require(Formatted(-0.5) == u"-0.50");
    Require(Formatted(-2.5) == u"-2.50");
    Require(Formatted(0.125) == u"0.13");
    Require(Formatted(0.375) == u"0.38");
    Require(Formatted(2.005) == u"2.00");
    Require(Formatted(2.9999) == u"3.00");
    Require(Formatted(-1000000.25) == u"-1000000.25");

    Require(Formatted(1.00005) == u"1");
    Require(Formatted(0.0001) == u"0");
    Require(Formatted(1.0002) == u"1.00");
    Require(Formatted(0.00011) == u"0.00");
    Require(Formatted(-0.00011) == u"-0.00");

    Stream stream = NewStream();
    _ZN3WTF10TextStreamlsEPKc(&stream, "ab");
    Format(stream, 2.5);
    Format(stream, 4.0);
    Require(Contents(stream.text) == u"ab2.504");
    Free(stream);

    Stream flagged = NewStream();
    flagged.formattingFlags = 0;
    _ZN3WTF10TextStreamlsEd(&flagged, 4.0);
    Format(flagged, 4.0);
    Require(Contents(flagged.text) == u"4.004" && flagged.formattingFlags == 0);
    Free(flagged);

    Stream wide = NewStream();
    AppendWide(wide);
    Format(wide, 1.5);
    Format(wide, 3.0);
    Require(!wide.text.is8Bit && Contents(wide.text) == u"α1.503");
    Free(wide);

    Stream target = NewStream();
    const Jsc::WtfAbi::FormatNumberRespectingIntegers number{1.0};
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(nullptr, &number); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(Guest(target), nullptr); }));
    Require(target.text.length == 0 && !target.text.buffer);
}

WtfUrl UrlOf(const WtfString& string, bool valid, unsigned queryEnd) {
    WtfUrl url{};
    url.impl = string.impl;
    if (url.impl) url.impl->refCount += 2;
    url.bits = (valid ? 1u : 0u) | (2u << 3) | (4u << 6);
    url.userStart = 7;
    url.userEnd = 8;
    url.passwordEnd = 9;
    url.hostEnd = 10;
    url.pathAfterLastSlash = 11;
    url.pathEnd = 12;
    url.queryEnd = queryEnd;
    return url;
}

bool SameOffsets(const WtfUrl& url, const WtfUrl& reference) {
    return url.bits == reference.bits && url.userStart == reference.userStart && url.userEnd == reference.userEnd
        && url.passwordEnd == reference.passwordEnd && url.hostEnd == reference.hostEnd
        && url.pathAfterLastSlash == reference.pathAfterLastSlash && url.pathEnd == reference.pathEnd
        && url.queryEnd == reference.queryEnd;
}

void RemoveFragment(WtfUrl& url) {
    _ZN3WTF3URL24removeFragmentIdentifierEv(reinterpret_cast<Jsc::WtfAbi::URL*>(&url));
}

WtfString Narrow(const char* text, unsigned length) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), length);
}

void TestRemoveFragmentIdentifier() {
    WtfString original = Narrow("http://a/b?q#frag", 17);
    Require(original.impl->refCount == 2);

    WtfUrl url = UrlOf(original, true, 12);
    const WtfUrl before = url;
    Require(original.impl->refCount == 4);
    RemoveFragment(url);
    Require(url.impl && url.impl != original.impl && url.impl->length == 12 && url.impl->refCount == 2);
    Require((url.impl->hashAndFlags & 4) && !(url.impl->hashAndFlags & 3));
    Require(url.impl->data == reinterpret_cast<const unsigned char*>(url.impl) + 20);
    Require(Units(url.impl, 12) == u"http://a/b?q");
    Require(original.impl->refCount == 2 && Units(original.impl, 17) == u"http://a/b?q#frag");
    Require(SameOffsets(url, before));

    StringImpl* truncated = url.impl;
    RemoveFragment(url);
    Require(url.impl == truncated && truncated->refCount == 2 && SameOffsets(url, before));
    NKWebKit::WtfAbi::Release(url.impl);

    WtfUrl whole = UrlOf(original, true, 17);
    RemoveFragment(whole);
    Require(whole.impl == original.impl && original.impl->refCount == 4);
    whole.queryEnd = 99;
    RemoveFragment(whole);
    Require(whole.impl == original.impl && original.impl->refCount == 4 && whole.queryEnd == 99);
    NKWebKit::WtfAbi::Release(whole.impl);

    WtfUrl invalid = UrlOf(original, false, 12);
    const WtfUrl invalidBefore = invalid;
    RemoveFragment(invalid);
    Require(invalid.impl == original.impl && original.impl->refCount == 4 && SameOffsets(invalid, invalidBefore));
    NKWebKit::WtfAbi::Release(invalid.impl);

    WtfUrl emptied = UrlOf(original, true, 0);
    RemoveFragment(emptied);
    Require(emptied.impl && emptied.impl != original.impl && emptied.impl->length == 0 && original.impl->refCount == 2);
    NKWebKit::WtfAbi::Release(emptied.impl);

    const char16_t wideText[] = {u's', u':', 0x3b1, u'#', u'f'};
    WtfString wide = _ZN3WTF10StringImpl6createEPKDsj(wideText, 5);
    WtfUrl wideUrl = UrlOf(wide, true, 3);
    RemoveFragment(wideUrl);
    Require(wideUrl.impl != wide.impl && wideUrl.impl->length == 3 && !(wideUrl.impl->hashAndFlags & 4));
    Require(Units(wideUrl.impl, 3) == std::u16string(wideText, 3) && wide.impl->refCount == 2);
    NKWebKit::WtfAbi::Release(wideUrl.impl);

    WtfString owned = Narrow("a:b#c", 5);
    WtfUrl sole = UrlOf(owned, true, 3);
    NKWebKit::WtfAbi::Release(owned.impl);
    owned.impl = nullptr;
    RemoveFragment(sole);
    Require(sole.impl && sole.impl->length == 3 && sole.impl->refCount == 2 && Units(sole.impl, 3) == u"a:b");
    NKWebKit::WtfAbi::Release(sole.impl);

    WtfString null;
    WtfUrl nullUrl = UrlOf(null, true, 0);
    RemoveFragment(nullUrl);
    Require(!nullUrl.impl);

    Require(original.impl->refCount == 2);
    Require(ThrowsInvalidArgument([] { _ZN3WTF3URL24removeFragmentIdentifierEv(nullptr); }));
}

}

int main() {
    TestWriteIndent();
    TestFormatNumberRespectingIntegers();
    TestRemoveFragmentIdentifier();
    return 0;
}
