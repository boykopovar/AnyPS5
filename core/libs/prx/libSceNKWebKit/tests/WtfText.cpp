#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

#include "../src/WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::TextStream;
using NKWebKit::WtfAbi::URL;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF text test failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename T> void Throws(T action, std::source_location location = std::source_location::current()) {
    bool threw = false;
    try { action(); } catch (const std::exception&) { threw = true; }
    Check(threw, location);
}

const unsigned char* U8(const char* data) {
    return reinterpret_cast<const unsigned char*>(data);
}

String Make8(const char* data) {
    return _ZN3WTF10StringImpl6createEPKhj(U8(data), static_cast<unsigned>(std::strlen(data)));
}

String Make16(const std::u16string& data) {
    return _ZN3WTF10StringImpl6createEPKDsj(data.data(), static_cast<unsigned>(data.size()));
}

std::u16string Units(const StringImpl* impl, unsigned length) {
    std::u16string result;
    for (unsigned i = 0; i < length; ++i) {
        if (impl->hashAndFlags & 4) result += static_cast<const unsigned char*>(impl->data)[i];
        else result += static_cast<const char16_t*>(impl->data)[i];
    }
    return result;
}

std::u16string Units(const String& string) {
    Check(string.impl != nullptr);
    return Units(string.impl, string.impl->length);
}

std::u16string Contents(const StringBuilder& builder) {
    if (!builder.length) return {};
    if (builder.string.impl) return Units(builder.string.impl, static_cast<unsigned>(builder.length));
    Check(builder.buffer != nullptr);
    return Units(builder.buffer, static_cast<unsigned>(builder.length));
}

std::u16string Wide(const char* data) {
    std::u16string result;
    for (const char* cursor = data; *cursor; ++cursor) result += static_cast<unsigned char>(*cursor);
    return result;
}

StringBuilder NewBuilder(bool crashOnOverflow = true) {
    StringBuilder builder{String()};
    builder.shouldCrashOnOverflow = crashOnOverflow;
    builder.is8Bit = true;
    return builder;
}

void Free(StringBuilder& builder) {
    NKWebKit::WtfAbi::Release(builder.buffer);
    builder.buffer = nullptr;
}

TextStream NewStream(bool multiLine = true, unsigned flags = 0) {
    TextStream stream{NewBuilder()};
    stream.text = NewBuilder();
    stream.formattingFlags = flags;
    stream.multiLineMode = multiLine;
    return stream;
}

std::u16string Release(TextStream& stream) {
    auto result = _ZN3WTF10TextStream7releaseEv(&stream);
    Check(!stream.text.buffer && !stream.text.string.impl && !stream.text.length && stream.text.is8Bit && !stream.text.characters8);
    return Units(result);
}

void Append8(StringBuilder& builder, const char* data) {
    _ZN3WTF13StringBuilder16appendCharactersEPKhj(&builder, U8(data), static_cast<unsigned>(std::strlen(data)));
}

void TestAppendCharacters() {
    auto builder = NewBuilder();
    _ZN3WTF13StringBuilder16appendCharactersEPKhj(&builder, nullptr, 0);
    Check(!builder.buffer && !builder.length);
    Throws([&] { _ZN3WTF13StringBuilder16appendCharactersEPKhj(&builder, nullptr, 1); });
    Append8(builder, "abc");
    Check(builder.buffer && builder.buffer->length == 16 && builder.length == 3 && builder.is8Bit);
    Check(builder.characters8 == builder.buffer->data && Contents(builder) == u"abc");
    Append8(builder, "defghijklmnop");
    Check(builder.buffer->length == 16 && builder.length == 16);
    Append8(builder, "q");
    Check(builder.buffer->length == 32 && Contents(builder) == u"abcdefghijklmnopq");
    const char16_t latin = u'\xe9';
    _ZN3WTF13StringBuilder16appendCharactersEPKDsj(&builder, &latin, 1);
    Check(builder.is8Bit && builder.length == 18);
    const char16_t wide[] = u"\x263a!";
    _ZN3WTF13StringBuilder16appendCharactersEPKDsj(&builder, wide, 2);
    Check(!builder.is8Bit && !(builder.buffer->hashAndFlags & 4) && builder.buffer->length == 64 && builder.length == 20);
    Check(builder.characters16 == builder.buffer->data && Contents(builder) == u"abcdefghijklmnopq\xe9\x263a!");
    Append8(builder, "\xff");
    Check(Contents(builder) == u"abcdefghijklmnopq\xe9\x263a!\xff");
    Free(builder);

    auto widened = NewBuilder();
    _ZN3WTF13StringBuilder16appendCharactersEPKDsj(&widened, wide, 1);
    Check(!widened.is8Bit && widened.buffer->length == 16 && Contents(widened) == u"\x263a");
    Free(widened);
}

void TestOverflow() {
    auto recording = NewBuilder(false);
    recording.length = 0x7FFFFFF0;
    Append8(recording, "0123456789abcdefg");
    Check(recording.overflowed && !recording.buffer && recording.length == 0x7FFFFFF0);
    Append8(recording, "a");
    _ZN3WTF13StringBuilder15reserveCapacityEj(&recording, 4);
    Check(recording.overflowed && !recording.buffer);
    TextStream stream = NewStream();
    stream.text.shouldCrashOnOverflow = false;
    stream.text.overflowed = true;
    Throws([&] { _ZN3WTF10TextStream7releaseEv(&stream); });

    auto crashing = NewBuilder();
    crashing.length = 0x7FFFFFF0;
    Throws([&] { Append8(crashing, "0123456789abcdefg"); });
    Check(crashing.overflowed);

    auto wrapping = NewBuilder(false);
    wrapping.length = 0x7FFFFFFF;
    const char16_t unit = u'x';
    Throws([&] { _ZN3WTF13StringBuilder16appendCharactersEPKDsj(&wrapping, &unit, 0xFFFFFFFF); });
    Check(!wrapping.overflowed);
}

void TestReserveCapacity() {
    auto builder = NewBuilder();
    _ZN3WTF13StringBuilder15reserveCapacityEj(&builder, 0);
    Check(!builder.buffer);
    _ZN3WTF13StringBuilder15reserveCapacityEj(&builder, 5);
    Check(builder.buffer && builder.buffer->length == 5 && builder.length == 0 && builder.characters8 == builder.buffer->data);
    Append8(builder, "hello");
    auto* first = builder.buffer;
    _ZN3WTF13StringBuilder15reserveCapacityEj(&builder, 3);
    Check(builder.buffer == first);
    _ZN3WTF13StringBuilder15reserveCapacityEj(&builder, 40);
    Check(builder.buffer->length == 40 && builder.buffer->refCount == 2 && Contents(builder) == u"hello");
    Free(builder);

    auto text = Make16(u"\x3b1\x3b2");
    TextStream stream = NewStream();
    _ZN3WTF10TextStreamlsERKNS_6StringE(&stream, text);
    Check(stream.text.string.impl == text.impl && text.impl->refCount == 4 && !stream.text.is8Bit && stream.text.length == 2);
    _ZN3WTF13StringBuilder15reserveCapacityEj(&stream.text, 10);
    Check(!stream.text.string.impl && stream.text.buffer->length == 10 && !(stream.text.buffer->hashAndFlags & 4));
    Check(Contents(stream.text) == u"\x3b1\x3b2" && text.impl->refCount == 2);
    Check(Release(stream) == u"\x3b1\x3b2");

    auto shared = NewBuilder();
    Append8(shared, "abc");
    _ZN3WTF13StringBuilder11shrinkToFitEv(&shared);
    _ZN3WTF13StringBuilder15reserveCapacityEj(&shared, 8);
    Check(shared.buffer->length == 8 && !shared.string.impl && Contents(shared) == u"abc");
    Free(shared);
}

void TestReifyAndShrink() {
    auto builder = NewBuilder();
    _ZNK3WTF13StringBuilder11reifyStringEv(&builder);
    Check(builder.string.impl && builder.string.impl->length == 0 && !builder.buffer);

    auto shortText = NewBuilder();
    Append8(shortText, "abc");
    _ZNK3WTF13StringBuilder11reifyStringEv(&shortText);
    Check(shortText.string.impl != shortText.buffer && (shortText.string.impl->hashAndFlags & 3) == 0);
    Check(Units(shortText.string) == u"abc" && shortText.buffer->refCount == 2);
    auto* reified = shortText.string.impl;
    _ZNK3WTF13StringBuilder11reifyStringEv(&shortText);
    Check(shortText.string.impl == reified);
    Append8(shortText, "d");
    Check(!shortText.string.impl && Contents(shortText) == u"abcd");
    Free(shortText);

    auto longText = NewBuilder();
    Append8(longText, "0123456789abcd");
    _ZNK3WTF13StringBuilder11reifyStringEv(&longText);
    Check((longText.string.impl->hashAndFlags & 3) == 2 && longText.string.impl->data == longText.buffer->data);
    Check(longText.buffer->refCount == 4 && Units(longText.string) == u"0123456789abcd");
    Free(longText);

    auto full = NewBuilder();
    _ZN3WTF13StringBuilder15reserveCapacityEj(&full, 4);
    Append8(full, "wxyz");
    _ZNK3WTF13StringBuilder11reifyStringEv(&full);
    Check(full.string.impl == full.buffer && full.buffer->refCount == 4);
    Free(full);

    auto shrink = NewBuilder();
    Append8(shrink, "abc");
    _ZN3WTF13StringBuilder11shrinkToFitEv(&shrink);
    Check(!shrink.buffer && shrink.string.impl && shrink.string.impl->length == 3 && shrink.string.impl->refCount == 2);
    Check(shrink.characters8 == shrink.string.impl->data && Units(shrink.string) == u"abc");
    _ZN3WTF13StringBuilder11shrinkToFitEv(&shrink);
    Check(shrink.string.impl->length == 3);

    auto nearlyFull = NewBuilder();
    Append8(nearlyFull, "0123456789abc");
    _ZN3WTF13StringBuilder11shrinkToFitEv(&nearlyFull);
    Check(nearlyFull.buffer && nearlyFull.buffer->length == 16 && !nearlyFull.string.impl);
    Free(nearlyFull);

    auto wide = NewBuilder();
    const char16_t text[] = u"\x263a\x263b";
    _ZN3WTF13StringBuilder16appendCharactersEPKDsj(&wide, text, 2);
    _ZN3WTF13StringBuilder11shrinkToFitEv(&wide);
    Check(!wide.buffer && wide.string.impl->length == 2 && !(wide.string.impl->hashAndFlags & 4) && Units(wide.string) == u"\x263a\x263b");
}

void TestQuotedJSON() {
    auto builder = NewBuilder();
    auto text = Make8("a\"b\\\n\x01\x7f\xe9/");
    _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&builder, text);
    Check(builder.is8Bit && builder.buffer->length == 64);
    Check(Contents(builder) == Wide("\"a\\\"b\\\\\\n\\u0001\x7f\xe9/\""));
    Free(builder);

    auto controls = NewBuilder();
    auto escapes = Make8("\b\t\f\r\x1f");
    _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&controls, escapes);
    Check(Contents(controls) == Wide("\"\\b\\t\\f\\r\\u001f\""));
    Free(controls);

    auto wide = NewBuilder();
    Append8(wide, "x");
    auto surrogates = Make16(u"\x263a\xd83d\xde00\xd800-\xdc00\xdbff");
    _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&wide, surrogates);
    Check(!wide.is8Bit && wide.buffer->length == 64);
    Check(Contents(wide) == u"x\"\x263a\xd83d\xde00\\ud800-\\udc00\\udbff\"");
    Free(wide);

    auto empty = NewBuilder();
    String null;
    _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&empty, null);
    Check(Contents(empty) == u"\"\"" && empty.buffer->length == 2);
    Free(empty);

    TextStream stream = NewStream();
    auto ascii = Make8("abc");
    _ZN3WTF10TextStreamlsERKNS_6StringE(&stream, ascii);
    auto quoted = Make8("q");
    _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&stream.text, quoted);
    Check(Contents(stream.text) == u"abc\"q\"" && stream.text.buffer->length == 16);
    Check(Release(stream) == u"abc\"q\"");
    _ZN3WTF10TextStreamlsERKNS_6StringE(&stream, ascii);
    Throws([&] { _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&stream.text, surrogates); });

    auto recording = NewBuilder(false);
    recording.length = 0x7FFFFFF0;
    _ZN3WTF13StringBuilder22appendQuotedJSONStringERKNS_6StringE(&recording, quoted);
    Check(recording.overflowed && !recording.buffer);
}

std::u16string Float(float value) {
    auto builder = NewBuilder();
    _ZN3WTF13StringBuilder12appendNumberEf(&builder, value);
    auto result = Contents(builder);
    Free(builder);
    return result;
}

void TestAppendNumber() {
    Check(Float(0.1f) == u"0.1");
    Check(Float(100.0f) == u"100");
    Check(Float(-2.5f) == u"-2.5");
    Check(Float(123456789.0f) == u"123456790");
    Check(Float(1e20f) == u"100000000000000000000");
    Check(Float(1e21f) == u"1e+21");
    Check(Float(0.000001f) == u"0.000001");
    Check(Float(1.5e-7f) == u"1.5e-7");
    Check(Float(3.4028235e38f) == u"3.4028235e+38");
    Check(Float(1.17549435e-38f) == u"1.1754944e-38");
    Check(Float(1.4e-45f) == u"1e-45");
    Check(Float(0.0f) == u"0" && Float(-0.0f) == u"0");
    Check(Float(std::numeric_limits<float>::quiet_NaN()) == u"NaN");
    Check(Float(std::numeric_limits<float>::infinity()) == u"Infinity");
    Check(Float(-std::numeric_limits<float>::infinity()) == u"-Infinity");
}

void TestTextStreamValues() {
    auto stream = NewStream();
    auto* self = &stream;
    Check(&_ZN3WTF10TextStreamlsEb(self, true) == self);
    _ZN3WTF10TextStreamlsEb(self, false);
    _ZN3WTF10TextStreamlsEc(self, ',');
    _ZN3WTF10TextStreamlsEi(self, std::numeric_limits<int>::min());
    _ZN3WTF10TextStreamlsEc(self, ',');
    _ZN3WTF10TextStreamlsEj(self, std::numeric_limits<unsigned>::max());
    _ZN3WTF10TextStreamlsEc(self, ',');
    _ZN3WTF10TextStreamlsEl(self, std::numeric_limits<std::int64_t>::min());
    _ZN3WTF10TextStreamlsEc(self, ',');
    _ZN3WTF10TextStreamlsEm(self, std::numeric_limits<std::uint64_t>::max());
    _ZN3WTF10TextStreamlsEc(self, ',');
    _ZN3WTF10TextStreamlsEx(self, -42);
    _ZN3WTF10TextStreamlsEc(self, ',');
    _ZN3WTF10TextStreamlsEy(self, 42);
    Check(Release(stream) == u"10,-2147483648,4294967295,-9223372036854775808,18446744073709551615,-42,42");

    _ZN3WTF10TextStreamlsEf(self, 1.5f);
    _ZN3WTF10TextStreamlsEPKc(self, " ");
    _ZN3WTF10TextStreamlsEd(self, 3.14159);
    _ZN3WTF10TextStreamlsEPKc(self, " ");
    _ZN3WTF10TextStreamlsEd(self, 0.125);
    _ZN3WTF10TextStreamlsEPKc(self, " ");
    _ZN3WTF10TextStreamlsEd(self, 0.996);
    _ZN3WTF10TextStreamlsEPKc(self, " ");
    _ZN3WTF10TextStreamlsEd(self, 9.999);
    _ZN3WTF10TextStreamlsEPKc(self, " ");
    _ZN3WTF10TextStreamlsEd(self, -0.001);
    _ZN3WTF10TextStreamlsEPKc(self, " ");
    _ZN3WTF10TextStreamlsEd(self, -0.0);
    _ZN3WTF10TextStreamlsEPKc(self, " ");
    _ZN3WTF10TextStreamlsEd(self, 1e20);
    _ZN3WTF10TextStreamlsEPKc(self, " ");
    _ZN3WTF10TextStreamlsEd(self, std::numeric_limits<double>::quiet_NaN());
    _ZN3WTF10TextStreamlsEPKc(self, " ");
    _ZN3WTF10TextStreamlsEd(self, -std::numeric_limits<double>::infinity());
    _ZN3WTF10TextStreamlsEPKc(self, "|");
    _ZN3WTF10TextStreamlsEd(self, 1e21);
    _ZN3WTF10TextStreamlsEPKc(self, "|");
    Check(Release(stream) == u"1.50 3.14 0.13 1.00 10.00 -0.00 0.00 100000000000000000000.00 NaN -Infinity||");

    auto empty = NewStream();
    _ZN3WTF10TextStreamlsEd(&empty, 1e21);
    Check(empty.text.buffer && empty.text.buffer->length == 16 && !empty.text.length);
    Check(Release(empty).empty());

    auto integers = NewStream(true, 2);
    _ZN3WTF10TextStreamlsEd(&integers, 3.0);
    _ZN3WTF10TextStreamlsEc(&integers, ' ');
    _ZN3WTF10TextStreamlsEd(&integers, 3.5);
    _ZN3WTF10TextStreamlsEc(&integers, ' ');
    _ZN3WTF10TextStreamlsEf(&integers, -7.00001f);
    _ZN3WTF10TextStreamlsEc(&integers, ' ');
    _ZN3WTF10TextStreamlsEd(&integers, 1e10);
    _ZN3WTF10TextStreamlsEc(&integers, ' ');
    _ZN3WTF10TextStreamlsEd(&integers, std::numeric_limits<double>::quiet_NaN());
    Check(Release(integers) == u"3 3.50 -7 10000000000.00 -2147483648");

    _ZN3WTF10TextStreamlsEPKv(self, reinterpret_cast<const void*>(std::uintptr_t{0x1234abcd}));
    _ZN3WTF10TextStreamlsEc(self, ' ');
    _ZN3WTF10TextStreamlsEPKv(self, nullptr);
    _ZN3WTF10TextStreamlsEPKc(self, nullptr);
    Check(Release(stream) == u"1234abcd 0");

    Check(Release(stream).empty());
    Throws([] { _ZN3WTF10TextStreamlsEi(nullptr, 1); });
}

void TestTextStreamStrings() {
    auto stream = NewStream();
    String null;
    _ZN3WTF10TextStreamlsERKNS_6StringE(&stream, null);
    Check(!stream.text.string.impl && !stream.text.length);
    auto first = Make8("first");
    _ZN3WTF10TextStreamlsERKNS_6StringE(&stream, first);
    Check(stream.text.string.impl == first.impl && !stream.text.buffer);
    auto result = _ZN3WTF10TextStream7releaseEv(&stream);
    Check(result.impl == first.impl && first.impl->refCount == 4);

    auto wide = Make16(u"\x3a9");
    _ZN3WTF10TextStreamlsEc(&stream, '<');
    _ZN3WTF10TextStreamlsERKNS_6StringE(&stream, wide);
    _ZN3WTF10TextStreamlsEc(&stream, '>');
    Check(!stream.text.is8Bit && Release(stream) == u"<\x3a9>");

    URL url{};
    auto address = Make8("https://example.com/");
    url.impl = address.impl;
    _ZN3WTFlsERNS_10TextStreamERKNS_3URLE(stream, url);
    _ZN3WTFlsERNS_10TextStreamENS_7SecondsE(stream, WTF::Seconds{2.5});
    _ZN3WTFlsERNS_10TextStreamENS_7SecondsE(stream, WTF::Seconds{-0.25});
    Check(Release(stream) == u"https://example.com/2.50s-0.25s");
}

void TestTextStreamGroups() {
    auto stream = NewStream();
    _ZN3WTF10TextStreamlsEPKc(&stream, "root");
    _ZN3WTF10TextStream10startGroupEv(&stream);
    Check(stream.indent == 1);
    _ZN3WTF10TextStreamlsEPKc(&stream, "a");
    _ZN3WTF10TextStream10startGroupEv(&stream);
    _ZN3WTF10TextStreamlsEPKc(&stream, "b");
    _ZN3WTF10TextStream8nextLineEv(&stream);
    _ZN3WTF10TextStreamlsEPKc(&stream, "c");
    _ZN3WTF10TextStream8endGroupEv(&stream);
    _ZN3WTF10TextStream8endGroupEv(&stream);
    Check(stream.indent == 0);
    _ZN3WTF10TextStream11writeIndentEv(&stream);
    stream.indent = 2;
    _ZN3WTF10TextStream11writeIndentEv(&stream);
    Check(Release(stream) == u"root\n(a\n  (b\n    c))    ");

    auto single = NewStream(false);
    _ZN3WTF10TextStreamlsEPKc(&single, "root");
    _ZN3WTF10TextStream10startGroupEv(&single);
    _ZN3WTF10TextStreamlsEPKc(&single, "a");
    _ZN3WTF10TextStream8nextLineEv(&single);
    _ZN3WTF10TextStreamlsEPKc(&single, "b");
    _ZN3WTF10TextStream8endGroupEv(&single);
    Check(single.indent == 0);
    single.indent = 3;
    _ZN3WTF10TextStream11writeIndentEv(&single);
    Check(single.indent == 3 && Release(single) == u"root (a b)");
}

}  // namespace

int main() {
    TestAppendCharacters();
    TestOverflow();
    TestReserveCapacity();
    TestReifyAndShrink();
    TestQuotedJSON();
    TestAppendNumber();
    TestTextStreamValues();
    TestTextStreamStrings();
    TestTextStreamGroups();
    return 0;
}
