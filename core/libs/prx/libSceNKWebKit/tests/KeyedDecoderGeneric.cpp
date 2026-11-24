#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using NKWebKit::WebCoreAbi::GenericKeyedDecoder;
using NKWebKit::WebCoreAbi::KeyedDecoderPtr;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "keyed decoder check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename TException, typename TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

static String Text(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static String Wide(std::u16string_view units) {
    return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
}

static std::u16string Contents(const StringImpl* impl) {
    std::u16string text;
    if (!impl) return u"<null>";
    const auto view = NKWebKit::WtfAbi::ViewOf(impl);
    for (unsigned index = 0; index < view.length; ++index) text.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    return text;
}

enum Tag : std::uint64_t { kBytes, kBool, kUInt32, kUInt64, kInt32, kInt64, kFloat, kDouble, kString, kBeginObject, kEndObject, kBeginArray, kBeginArrayElement, kEndArrayElement, kEndArray };

struct Stream {
    std::vector<std::uint8_t> bytes;

    template<typename T>
    Stream& Raw(T value) {
        const auto* data = reinterpret_cast<const std::uint8_t*>(&value);
        bytes.insert(bytes.end(), data, data + sizeof(T));
        return *this;
    }
    Stream& Type(std::uint64_t tag) {
        return Raw<std::uint64_t>(tag);
    }
    Stream& Str(std::string_view text) {
        Raw<std::uint64_t>(text.size());
        bytes.insert(bytes.end(), text.begin(), text.end());
        return *this;
    }
    template<typename T>
    Stream& Value(std::uint64_t tag, std::string_view key, T value) {
        return Type(tag).Str(key).Raw<T>(value);
    }
    Stream& Bytes(std::string_view key, std::string_view data) {
        return Type(kBytes).Str(key).Str(data);
    }
    Stream& String(std::string_view key, std::string_view value) {
        return Type(kString).Str(key).Str(value);
    }
    Stream& Begin(std::uint64_t tag, std::string_view key) {
        return Type(tag).Str(key);
    }
    Stream& Cut(std::size_t count) {
        bytes.resize(bytes.size() - count);
        return *this;
    }
    KeyedDecoderPtr Decoder() const {
        return _ZN7WebCore12KeyedDecoder7decoderEPKhm(bytes.data(), bytes.size());
    }
};

struct Reader {
    KeyedDecoderPtr owner;
    GenericKeyedDecoder* decoder;

    explicit Reader(const Stream& stream) : owner(stream.Decoder()), decoder(owner.ptr) {
        Require(decoder != nullptr && decoder->vptr != nullptr);
    }

    template<typename T, typename TFunction>
    bool Get(TFunction function, std::string_view key, T& result) {
        const ::String name = Text(key);
        return function(decoder, &name, &result);
    }
    bool UInt32(std::string_view key, std::uint32_t& result) { return Get(decoder->vptr->decodeUInt32, key, result); }
    bool UInt64(std::string_view key, std::uint64_t& result) { return Get(decoder->vptr->decodeUInt64, key, result); }
    bool Int32(std::string_view key, std::int32_t& result) { return Get(decoder->vptr->decodeInt32, key, result); }
    bool Int64(std::string_view key, std::int64_t& result) { return Get(decoder->vptr->decodeInt64, key, result); }
    bool Float(std::string_view key, float& result) { return Get(decoder->vptr->decodeFloat, key, result); }
    bool Double(std::string_view key, double& result) { return Get(decoder->vptr->decodeDouble, key, result); }
    bool Bool(std::string_view key, bool& result) { return Get(decoder->vptr->decodeBool, key, result); }
    bool Str(std::string_view key, ::String& result) { return Get(decoder->vptr->decodeString, key, result); }
    bool Bytes(std::string_view key, const std::uint8_t*& data, std::size_t& size) {
        const ::String name = Text(key);
        return decoder->vptr->decodeBytes(decoder, &name, &data, &size);
    }
    bool BeginObject(std::string_view key) {
        const ::String name = Text(key);
        return decoder->vptr->beginObject(decoder, &name);
    }
    void EndObject() { decoder->vptr->endObject(decoder); }
    bool BeginArray(std::string_view key) {
        const ::String name = Text(key);
        return decoder->vptr->beginArray(decoder, &name);
    }
    bool BeginArrayElement() { return decoder->vptr->beginArrayElement(decoder); }
    void EndArrayElement() { decoder->vptr->endArrayElement(decoder); }
    void EndArray() { decoder->vptr->endArray(decoder); }
    bool Has(std::string_view key) {
        std::uint32_t value = 0;
        return UInt32(key, value);
    }
};

static void TestSimpleValues() {
    Stream stream;
    stream.Value<std::uint8_t>(kBool, "yes", 1).Value<std::uint8_t>(kBool, "no", 0).Value<std::uint8_t>(kBool, "odd", 2);
    stream.Value<std::uint32_t>(kUInt32, "u32", 0xFFFFFFFFu).Value<std::uint64_t>(kUInt64, "u64", 0xFEDCBA9876543210ull);
    stream.Value<std::int32_t>(kInt32, "i32", std::numeric_limits<std::int32_t>::min()).Value<std::int64_t>(kInt64, "i64", std::numeric_limits<std::int64_t>::min());
    stream.Value<float>(kFloat, "float", 1.5f).Value<std::uint32_t>(kFloat, "nanf", 0x7FA12345u);
    stream.Value<double>(kDouble, "double", -2.25).Value<std::uint64_t>(kDouble, "nan", 0x7FF4000000ABCDEFull);
    stream.Value<std::uint32_t>(kUInt32, "", 7);
    Reader reader(stream);

    bool flag = false;
    Require(reader.Bool("yes", flag) && flag);
    Require(reader.Bool("no", flag) && !flag);
    unsigned char raw = 0;
    {
        const String name = Text("odd");
        Require(reader.decoder->vptr->decodeBool(reader.decoder, &name, reinterpret_cast<bool*>(&raw)) && raw == 2);
    }
    std::uint32_t u32 = 0;
    std::uint64_t u64 = 0;
    std::int32_t i32 = 0;
    std::int64_t i64 = 0;
    float f = 0;
    double d = 0;
    Require(reader.UInt32("u32", u32) && u32 == 0xFFFFFFFFu);
    Require(reader.UInt64("u64", u64) && u64 == 0xFEDCBA9876543210ull);
    Require(reader.Int32("i32", i32) && i32 == std::numeric_limits<std::int32_t>::min());
    Require(reader.Int64("i64", i64) && i64 == std::numeric_limits<std::int64_t>::min());
    Require(reader.Float("float", f) && f == 1.5f);
    Require(reader.Double("double", d) && d == -2.25);
    std::uint32_t floatBits = 0;
    std::uint64_t doubleBits = 0;
    Require(reader.Float("nanf", f));
    std::memcpy(&floatBits, &f, sizeof(f));
    Require(floatBits == 0x7FA12345u);
    Require(reader.Double("nan", d));
    std::memcpy(&doubleBits, &d, sizeof(d));
    Require(doubleBits == 0x7FF4000000ABCDEFull);
    Require(reader.UInt32("", u32) && u32 == 7);

    u32 = 42;
    i32 = 42;
    u64 = 42;
    Require(!reader.UInt32("absent", u32) && u32 == 42);
    Require(!reader.UInt32("i32", u32) && !reader.Int32("u32", i32) && !reader.UInt64("u32", u64) && !reader.UInt32("u64", u32));
    Require(!reader.Int64("u64", i64) && !reader.Float("double", f) && !reader.Double("float", d) && !reader.Bool("u32", flag));
    Require(!reader.UInt32("yes", u32) && !reader.UInt32("U32", u32) && !reader.UInt32("u32 ", u32));
    Require(u32 == 42 && i32 == 42 && u64 == 42);
    {
        const String wideKey = Wide(u"u32");
        Require(reader.decoder->vptr->decodeUInt32(reader.decoder, &wideKey, &u32) && u32 == 0xFFFFFFFFu);
    }
}

static void TestBytesAndStrings() {
    Stream stream;
    stream.Bytes("data", std::string_view("\x00\x01\xFF\x7F", 4)).Bytes("empty", "");
    stream.String("ascii", "hello").String("none", "").String("utf8", "h\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80").String("caf\xC3\xA9", "key");
    Reader reader(stream);

    const std::uint8_t* data = nullptr;
    std::size_t size = 99;
    Require(reader.Bytes("data", data, size) && size == 4 && data != nullptr && std::memcmp(data, "\x00\x01\xFF\x7F", 4) == 0);
    const std::uint8_t* again = nullptr;
    Require(reader.Bytes("data", again, size) && again == data);
    data = reinterpret_cast<const std::uint8_t*>(&size);
    Require(reader.Bytes("empty", data, size) && size == 0 && data == nullptr);
    size = 99;
    Require(!reader.Bytes("absent", data, size) && !reader.Bytes("ascii", data, size) && size == 99);

    String value = Text("previous");
    const StringImpl* previous = value.impl;
    String keepPrevious(value);
    Require(reader.Str("ascii", value) && Contents(value.impl) == u"hello" && (value.impl->hashAndFlags & 4));
    Require(previous->refCount == 2 && value.impl->refCount == 4);
    String second;
    Require(reader.Str("ascii", second) && second.impl == value.impl && value.impl->refCount == 6);
    Require(reader.Str("none", second) && second.impl == NKWebKit::WtfAbi::EmptyStringImpl());
    Require(reader.Str("utf8", second) && Contents(second.impl) == u"hé€\U0001F600" && !(second.impl->hashAndFlags & 4));
    Require(!reader.Str("absent", second) && !reader.Str("data", second) && Contents(second.impl) == u"hé€\U0001F600");
    {
        const String wideKey = Wide(u"café");
        String keyed;
        Require(reader.decoder->vptr->decodeString(reader.decoder, &wideKey, &keyed) && Contents(keyed.impl) == u"key");
        const String narrowKey = Text("caf\xE9");
        Require(reader.decoder->vptr->decodeString(reader.decoder, &narrowKey, &keyed));
    }
    const StringImpl* held = value.impl;
    String keepHeld(value);
    const unsigned beforeDestruction = held->refCount;
    reader.owner.ptr->vptr->deletingDestructor(reader.owner.ptr);
    reader.owner.ptr = nullptr;
    Require(held->refCount == beforeDestruction - 2);
}

static void TestFirstKeyWins() {
    Stream stream;
    stream.Value<std::uint32_t>(kUInt32, "k", 1).Value<std::uint32_t>(kUInt32, "k", 2).Value<std::int32_t>(kInt32, "k", 3).String("k", "s");
    stream.String("s", "first").Value<std::uint32_t>(kUInt32, "s", 9).Value<std::uint32_t>(kUInt32, "after", 5);
    stream.Bytes("b", "one").Bytes("b", "second").String("b", "text");
    Reader reader(stream);
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    Require(reader.Bytes("b", data, size) && size == 3 && std::memcmp(data, "one", 3) == 0);
    std::uint32_t u32 = 0;
    std::int32_t i32 = 0;
    String text;
    Require(reader.UInt32("k", u32) && u32 == 1 && !reader.Int32("k", i32) && !reader.Str("k", text));
    Require(reader.Str("s", text) && Contents(text.impl) == u"first" && !reader.UInt32("s", u32));
    Require(reader.UInt32("after", u32) && u32 == 5);
}

static void TestObjects() {
    Stream stream;
    stream.Value<std::uint32_t>(kUInt32, "top", 1);
    stream.Begin(kBeginObject, "outer").Value<std::uint32_t>(kUInt32, "a", 2).Value<std::uint32_t>(kUInt32, "top", 20);
    stream.Begin(kBeginObject, "inner").Value<std::uint32_t>(kUInt32, "b", 3).Type(kEndObject);
    stream.Value<std::uint32_t>(kUInt32, "c", 4).Type(kEndObject);
    stream.Value<std::uint32_t>(kUInt32, "tail", 5).Begin(kBeginObject, "empty").Type(kEndObject);
    Reader reader(stream);
    std::uint32_t value = 0;
    Require(reader.UInt32("top", value) && value == 1 && reader.UInt32("tail", value) && value == 5);
    Require(!reader.Has("a") && !reader.Has("b") && !reader.Has("outer"));
    Require(!reader.BeginObject("absent") && !reader.BeginObject("top") && !reader.BeginObject("inner"));
    Require(reader.BeginObject("outer"));
    Require(reader.UInt32("a", value) && value == 2 && reader.UInt32("top", value) && value == 20 && reader.UInt32("c", value) && value == 4);
    Require(!reader.Has("tail") && !reader.Has("b") && !reader.BeginObject("outer"));
    Require(reader.BeginObject("inner"));
    Require(reader.UInt32("b", value) && value == 3 && !reader.Has("a"));
    reader.EndObject();
    Require(reader.UInt32("a", value) && value == 2);
    Require(reader.BeginObject("inner"));
    reader.EndObject();
    reader.EndObject();
    Require(reader.UInt32("top", value) && value == 1);
    Require(reader.BeginObject("empty") && !reader.Has("top"));
    reader.EndObject();

    Stream unterminated;
    unterminated.Begin(kBeginObject, "open").Value<std::uint32_t>(kUInt32, "in", 1).Begin(kBeginObject, "deeper").Value<std::uint32_t>(kUInt32, "deep", 2);
    Reader open(unterminated);
    Require(!open.Has("in") && open.BeginObject("open") && open.Has("in") && open.BeginObject("deeper") && open.Has("deep"));

    Stream duplicate;
    duplicate.Begin(kBeginObject, "o").Value<std::uint32_t>(kUInt32, "first", 1).Type(kEndObject);
    duplicate.Begin(kBeginObject, "o").Value<std::uint32_t>(kUInt32, "second", 2).Type(kEndObject);
    duplicate.Value<std::uint32_t>(kUInt32, "o", 3).Value<std::uint32_t>(kUInt32, "after", 4);
    Reader twice(duplicate);
    Require(twice.Has("after") && !twice.Has("o") && !twice.Has("second"));
    Require(twice.BeginObject("o") && twice.Has("first") && !twice.Has("second") && !twice.Has("after"));
}

static void TestArrays() {
    Stream stream;
    stream.Begin(kBeginArray, "list");
    for (std::uint32_t index = 0; index < 3; ++index)
        stream.Type(kBeginArrayElement).Value<std::uint32_t>(kUInt32, "i", index).Type(kEndArrayElement);
    stream.Type(kBeginArrayElement).Begin(kBeginArray, "nested").Type(kBeginArrayElement).Value<std::uint32_t>(kUInt32, "n", 9).Type(kEndArrayElement);
    stream.Type(kEndArray).Type(kEndArrayElement).Type(kEndArray);
    stream.Begin(kBeginArray, "none").Type(kEndArray).Value<std::uint32_t>(kUInt32, "tail", 5);
    Reader reader(stream);
    std::uint32_t value = 0;
    Require(reader.Has("tail") && !reader.Has("i"));
    Require(!reader.BeginArray("absent") && !reader.BeginArray("tail") && !reader.BeginObject("list"));
    Require(reader.BeginArray("list"));
    for (std::uint32_t index = 0; index < 3; ++index) {
        Require(reader.BeginArrayElement() && reader.UInt32("i", value) && value == index && !reader.Has("tail"));
        reader.EndArrayElement();
    }
    Require(reader.BeginArrayElement() && !reader.Has("i") && reader.BeginArray("nested"));
    Require(reader.BeginArrayElement() && reader.UInt32("n", value) && value == 9);
    reader.EndArrayElement();
    Require(!reader.BeginArrayElement() && !reader.BeginArrayElement());
    reader.EndArray();
    reader.EndArrayElement();
    Require(!reader.BeginArrayElement());
    reader.EndArray();
    Require(reader.Has("tail"));
    Require(reader.BeginArray("list") && reader.BeginArrayElement() && reader.UInt32("i", value) && value == 0);
    reader.EndArrayElement();
    reader.EndArray();
    Require(reader.BeginArray("none") && !reader.BeginArrayElement());
    reader.EndArray();
    Require(Throws<std::out_of_range>([&] { reader.BeginArrayElement(); }));
    Require(Throws<std::out_of_range>([&] { reader.EndArray(); }));

    Stream duplicate;
    duplicate.Value<std::uint32_t>(kUInt32, "a", 1).Begin(kBeginArray, "a").Type(kBeginArrayElement).Value<std::uint32_t>(kUInt32, "x", 2).Type(kEndArrayElement).Type(kEndArray);
    duplicate.Value<std::uint32_t>(kUInt32, "after", 3);
    Reader twice(duplicate);
    Require(twice.UInt32("a", value) && value == 1 && !twice.BeginArray("a") && !twice.Has("x") && twice.Has("after"));

    Stream closed;
    closed.Begin(kBeginArray, "a").Type(kBeginArrayElement).Value<std::uint32_t>(kUInt32, "x", 1).Type(kEndArrayElement).Type(kEndArray);
    closed.Value<std::uint32_t>(kUInt32, "before", 2).Type(kBeginArrayElement).Value<std::uint32_t>(kUInt32, "after", 3);
    Reader afterEnd(closed);
    Require(afterEnd.Has("before") && !afterEnd.Has("after"));
    Require(afterEnd.BeginArray("a") && afterEnd.BeginArrayElement() && afterEnd.Has("x"));
    afterEnd.EndArrayElement();
    Require(!afterEnd.BeginArrayElement());

    Stream nesting;
    nesting.Begin(kBeginArray, "outer").Type(kBeginArrayElement).Begin(kBeginArray, "inner").Type(kBeginArrayElement).Type(kEndArrayElement).Type(kEndArray);
    nesting.Type(kEndArrayElement).Type(kBeginArrayElement).Value<std::uint32_t>(kUInt32, "second", 2).Type(kEndArrayElement).Type(kEndArray);
    Reader nested(nesting);
    Require(nested.BeginArray("outer") && nested.BeginArrayElement() && nested.BeginArray("inner"));
    Require(nested.BeginArrayElement() && !nested.Has("second"));
    nested.EndArrayElement();
    Require(!nested.BeginArrayElement());
    nested.EndArray();
    nested.EndArrayElement();
    Require(nested.BeginArrayElement() && nested.Has("second"));
    nested.EndArrayElement();
    Require(!nested.BeginArrayElement());

    Stream unterminated;
    unterminated.Value<std::uint32_t>(kUInt32, "top", 1).Begin(kBeginArray, "open").Type(kBeginArrayElement).Value<std::uint32_t>(kUInt32, "in", 2);
    Reader open(unterminated);
    Require(open.Has("top") && !open.Has("in"));
    Require(Throws<std::out_of_range>([&] { open.BeginArrayElement(); }));
    Require(open.BeginArray("open") && open.BeginArrayElement() && open.Has("in") && !open.Has("top"));
}

static void TestBrokenStreams() {
    std::uint32_t value = 0;
    {
        Reader empty{Stream()};
        Require(!empty.Has("k"));
        const KeyedDecoderPtr fromNull = _ZN7WebCore12KeyedDecoder7decoderEPKhm(nullptr, 0);
        Require(fromNull.ptr != nullptr);
        Require(Throws<std::invalid_argument>([] { _ZN7WebCore12KeyedDecoder7decoderEPKhm(nullptr, 1); }));
    }
    for (std::uint64_t badType : {std::uint64_t{15}, std::uint64_t{16}, std::uint64_t{255}, std::uint64_t{1} << 32, ~std::uint64_t{0}}) {
        Stream stream;
        stream.Value<std::uint32_t>(kUInt32, "before", 1).Type(badType).Value<std::uint32_t>(kUInt32, "after", 2);
        Reader reader(stream);
        Require(reader.Has("before") && !reader.Has("after"));
    }
    for (std::size_t cut = 1; cut <= 4 + 8 + 5 + 8; ++cut) {
        Stream stream;
        stream.Value<std::uint32_t>(kUInt32, "before", 1).Value<std::uint32_t>(kUInt32, "after", 2).Cut(cut);
        Reader reader(stream);
        Require(reader.Has("before") && !reader.Has("after"));
    }
    {
        Stream stream;
        stream.Value<std::uint32_t>(kUInt32, "before", 1).Type(kBytes).Str("data").Raw<std::uint64_t>(5).Raw<std::uint32_t>(0x01020304).Value<std::uint32_t>(kUInt32, "after", 2);
        stream.bytes.resize(stream.bytes.size() - (8 + 8 + 5 + 4) - 1);
        Reader reader(stream);
        const std::uint8_t* data = nullptr;
        std::size_t size = 0;
        Require(reader.Has("before") && !reader.Bytes("data", data, size));
    }
    {
        Stream stream;
        stream.Value<std::uint32_t>(kUInt32, "before", 1).Type(kBytes).Str("huge").Raw<std::uint64_t>(~std::uint64_t{0}).Value<std::uint32_t>(kUInt32, "after", 2);
        Reader reader(stream);
        Require(reader.Has("before") && !reader.Has("after"));
    }
    for (std::string_view invalid : {std::string_view("\xFF"), std::string_view("\xC3"), std::string_view("a\xED\xA0\x80"), std::string_view("\xC0\x80")}) {
        Stream badValue;
        badValue.Value<std::uint32_t>(kUInt32, "before", 1).String("text", invalid).Value<std::uint32_t>(kUInt32, "after", 2);
        Reader value1(badValue);
        String text;
        Require(value1.Has("before") && !value1.Str("text", text) && !value1.Has("after"));
        Stream badKey;
        badKey.Value<std::uint32_t>(kUInt32, "before", 1).Value<std::uint32_t>(kUInt32, invalid, 2).Value<std::uint32_t>(kUInt32, "after", 3);
        Reader key1(badKey);
        Require(key1.Has("before") && !key1.Has("after"));
    }
    {
        Stream stream;
        stream.Type(kUInt32).Raw<std::uint64_t>(std::uint64_t{1} << 32);
        Require(Throws<std::length_error>([&] { stream.Decoder(); }));
        Stream longest;
        longest.Value<std::uint32_t>(kUInt32, "before", 1).Type(kUInt32).Raw<std::uint64_t>(0xFFFFFFFFull);
        Reader reader(longest);
        Require(reader.Has("before"));
    }
    {
        Stream stream;
        stream.Value<std::uint32_t>(kUInt32, "before", 1).Type(kBeginArrayElement).Value<std::uint32_t>(kUInt32, "after", 2);
        Reader noArray(stream);
        Require(noArray.Has("before") && !noArray.Has("after"));
        Stream endArray;
        endArray.Value<std::uint32_t>(kUInt32, "before", 1).Type(kEndArray).Value<std::uint32_t>(kUInt32, "after", 2);
        Reader stray(endArray);
        Require(stray.Has("before") && !stray.Has("after"));
    }
    for (std::uint64_t closing : {std::uint64_t{kEndObject}, std::uint64_t{kEndArrayElement}}) {
        Stream stream;
        stream.Value<std::uint32_t>(kUInt32, "before", 1).Type(closing).Value<std::uint32_t>(kUInt32, "after", 2);
        Reader reader(stream);
        Require(Throws<std::out_of_range>([&] { reader.UInt32("before", value); }));
        Require(Throws<std::out_of_range>([&] { reader.BeginObject("before"); }));
        Require(Throws<std::out_of_range>([&] { reader.EndObject(); }));
        Require(Throws<std::out_of_range>([&] { reader.EndArrayElement(); }));
    }
    {
        Stream stream;
        stream.Begin(kBeginObject, "o").Type(kEndObject).Value<std::uint32_t>(kUInt32, "after", 2);
        Reader reader(stream);
        Require(reader.Has("after"));
        reader.EndObject();
        Require(Throws<std::out_of_range>([&] { reader.Has("after"); }));
    }
}

static void TestArguments() {
    Stream filled;
    filled.Value<std::uint32_t>(kUInt32, "k", 1);
    Reader reader(filled);
    Reader empty{Stream()};
    const String none;
    const String key = Text("k");
    std::uint32_t value = 0;
    Require(!empty.decoder->vptr->decodeUInt32(empty.decoder, &none, &value));
    Require(!empty.decoder->vptr->beginObject(empty.decoder, &none) && !empty.decoder->vptr->beginArray(empty.decoder, &none));
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->decodeUInt32(reader.decoder, &none, &value); }));
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->beginObject(reader.decoder, &none); }));
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->decodeUInt32(reader.decoder, nullptr, &value); }));
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->decodeUInt32(reader.decoder, &key, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->decodeUInt32(nullptr, &key, &value); }));
    const std::uint8_t* data = nullptr;
    std::size_t size = 0;
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->decodeBytes(reader.decoder, &key, nullptr, &size); }));
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->decodeBytes(reader.decoder, &key, &data, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->decodeString(reader.decoder, &key, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->completeDestructor(nullptr); }));

    reader.decoder->vptr->completeDestructor(reader.decoder);
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->decodeUInt32(reader.decoder, &key, &value); }));
    Require(Throws<std::invalid_argument>([&] { reader.decoder->vptr->endObject(reader.decoder); }));
    reader.decoder->vptr->completeDestructor(reader.decoder);
}

int main() {
    TestSimpleValues();
    TestBytesAndStrings();
    TestFirstKeyWins();
    TestObjects();
    TestArrays();
    TestBrokenStreams();
    TestArguments();
    std::puts("libSceNKWebKit keyed decoder tests passed");
    return 0;
}
