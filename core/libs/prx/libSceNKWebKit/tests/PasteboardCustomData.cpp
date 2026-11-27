#include "prx/libSceNKWebKit/include/PasteboardCustomDataExports.hpp"
#include "prx/libSceNKWebKit/include/SharedBufferExports.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace NKWebKit::WtfAbi;
using namespace NKWebKit::WebCoreAbi;

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "PasteboardCustomData check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

String Text(std::string_view value) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(value.data()), value.size());
}

bool Equal(const String& value, std::string_view text) {
    return value.impl && _ZN3WTF5equalEPKNS_10StringImplEPKhj(value.impl, reinterpret_cast<const unsigned char*>(text.data()), text.size());
}

struct Wire {
    std::vector<std::uint8_t> bytes;

    void Number(std::uint64_t value, unsigned size) {
        for (unsigned i = 0; i < size; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    }

    void String8(std::string_view value) {
        Number(value.size(), 4);
        bytes.push_back(1);
        bytes.insert(bytes.end(), value.begin(), value.end());
    }

    void NullString() { Number(0xFFFFFFFFU, 4); }

    void Header(unsigned version = 1) {
        Number(version, 4);
        String8("a");
    }
};

PasteboardCustomData DecodeBytes(const void* data, std::size_t size) {
    auto buffer = _ZN7WebCore12SharedBuffer6createEPKcm(static_cast<const char*>(data), size);
    return _ZN7WebCore20PasteboardCustomData16fromSharedBufferERKNS_12SharedBufferE(buffer.ptr);
}

void TestWireFormat() {
    PasteboardCustomData empty;
    auto buffer = _ZNK7WebCore20PasteboardCustomData18createSharedBufferEv(&empty);
    const std::array<std::uint8_t, 24> emptyBytes{1, 0, 0, 0, 255, 255, 255, 255};
    Require(buffer.ptr->size == emptyBytes.size());
    Require(std::memcmp(_ZNK7WebCore12SharedBuffer4dataEv(buffer.ptr), emptyBytes.data(), emptyBytes.size()) == 0);
    auto decodedEmpty = _ZN7WebCore20PasteboardCustomData16fromSharedBufferERKNS_12SharedBufferE(buffer.ptr);
    Require(!decodedEmpty.origin.impl && !decodedEmpty.data.size);

    Wire wire;
    wire.Header();
    wire.Number(1, 8);
    wire.String8("t");
    wire.String8("v");
    wire.Number(1, 8);
    wire.String8("t");
    PasteboardCustomData data;
    data.origin = Text("a");
    PasteboardEntry entry;
    entry.type = Text("t");
    entry.customData = Text("v");
    data.data.append(entry);
    auto encoded = _ZNK7WebCore20PasteboardCustomData18createSharedBufferEv(&data);
    Require(encoded.ptr->size == wire.bytes.size());
    Require(std::memcmp(_ZNK7WebCore12SharedBuffer4dataEv(encoded.ptr), wire.bytes.data(), wire.bytes.size()) == 0);
    auto decoded = DecodeBytes(wire.bytes.data(), wire.bytes.size());
    Require(Equal(decoded.origin, "a") && decoded.data.size == 1);
    Require(Equal(decoded.data[0].type, "t") && Equal(decoded.data[0].customData, "v"));
    Require(decoded.data[0].platformData.index == 0 && !decoded.data[0].platformData.string);

    for (std::size_t size = 0; size < wire.bytes.size(); ++size) {
        auto partial = DecodeBytes(wire.bytes.data(), size);
        Require(!partial.origin.impl && !partial.data.size);
    }
    wire.bytes[0] = 0;
    Require(DecodeBytes(wire.bytes.data(), wire.bytes.size()).data.size == 1);
    wire.bytes[0] = 2;
    auto unsupported = DecodeBytes(wire.bytes.data(), wire.bytes.size());
    Require(!unsupported.origin.impl && !unsupported.data.size);
    wire.bytes[0] = 1;
    wire.bytes[4] = 0xFE;
    wire.bytes[5] = wire.bytes[6] = wire.bytes[7] = 0xFF;
    auto tooLong = DecodeBytes(wire.bytes.data(), wire.bytes.size());
    Require(!tooLong.origin.impl && !tooLong.data.size);
}

void TestDictionaryAndOrder() {
    Wire wire;
    wire.Header();
    wire.Number(3, 8);
    wire.String8("t");
    wire.String8("v");
    wire.String8("u");
    wire.NullString();
    wire.String8("unused");
    wire.String8("ignored");
    wire.Number(4, 8);
    wire.String8("t");
    wire.String8("u");
    wire.String8("t");
    wire.String8("missing");
    auto result = DecodeBytes(wire.bytes.data(), wire.bytes.size());
    Require(result.data.size == 3 && Equal(result.origin, "a"));
    Require(Equal(result.data[0].type, "u") && !result.data[0].customData.impl);
    Require(Equal(result.data[1].type, "t") && Equal(result.data[1].customData, "v"));
    Require(Equal(result.data[2].type, "missing") && !result.data[2].customData.impl);

    Wire duplicate;
    duplicate.Header();
    duplicate.Number(2, 8);
    duplicate.String8("t");
    duplicate.String8("first");
    duplicate.String8("t");
    duplicate.String8("second");
    duplicate.Number(0, 8);
    auto invalid = DecodeBytes(duplicate.bytes.data(), duplicate.bytes.size());
    Require(!invalid.origin.impl && !invalid.data.size);

    Wire hugeCount;
    hugeCount.Header();
    hugeCount.Number(std::numeric_limits<std::uint64_t>::max(), 8);
    Require(!DecodeBytes(hugeCount.bytes.data(), hugeCount.bytes.size()).origin.impl);

    PasteboardCustomData repeated;
    repeated.data.append(PasteboardEntry{Text("t"), Text("first"), {}});
    repeated.data.append(PasteboardEntry{Text("u"), String(), {}});
    repeated.data.append(PasteboardEntry{Text("t"), Text("last"), {}});
    auto encoded = _ZNK7WebCore20PasteboardCustomData18createSharedBufferEv(&repeated);
    auto decoded = _ZN7WebCore20PasteboardCustomData16fromSharedBufferERKNS_12SharedBufferE(encoded.ptr);
    Require(decoded.data.size == 2 && Equal(decoded.data[0].type, "u"));
    Require(Equal(decoded.data[1].type, "t") && Equal(decoded.data[1].customData, "last"));
}

void TestUnicodeAndGrowth() {
    PasteboardCustomData data;
    const char16_t unicode[]{0x03BB, 0, 0xD83D, 0xDE00};
    data.origin = _ZN3WTF10StringImpl6createEPKDsj(unicode, std::size(unicode));
    for (unsigned i = 0; i < 40; ++i) {
        PasteboardEntry entry;
        entry.type = Text("type/" + std::to_string(i));
        entry.customData = Text(std::string(300, static_cast<char>('a' + i % 26)));
        data.data.append(entry);
    }
    data.data[0].customData = _ZN3WTF10StringImpl6createEPKDsj(unicode, std::size(unicode));
    data.data[1].customData = Text("");
    data.data[2].customData = String();
    data.data[3].customData = Text(std::string_view("a\0b\xFF", 4));
    auto buffer = _ZNK7WebCore20PasteboardCustomData18createSharedBufferEv(&data);
    Require(buffer.ptr->size > 4096);
    auto decoded = _ZN7WebCore20PasteboardCustomData16fromSharedBufferERKNS_12SharedBufferE(buffer.ptr);
    Require(decoded.data.size == data.data.size);
    Require(_ZN3WTF5equalEPKNS_10StringImplEPKDsj(decoded.origin.impl, unicode, std::size(unicode)));
    for (unsigned i = 0; i < data.data.size; ++i) {
        Require(_ZN3WTF5equalEPKNS_10StringImplES2_(decoded.data[i].type.impl, data.data[i].type.impl));
        Require(_ZN3WTF5equalEPKNS_10StringImplES2_(decoded.data[i].customData.impl, data.data[i].customData.impl));
        Require(!decoded.data[i].platformData.string);
    }
    Require(decoded.data[1].customData.impl && !decoded.data[1].customData.impl->length);
    Require(!decoded.data[2].customData.impl);
}

void TestDecoderContinuation() {
    Encoder encoder;
    _ZN3WTF11Persistence7EncoderC1Ev(&encoder);
    _ZN3WTF11Persistence7EncoderlsEj(&encoder, 1);
    const auto origin = Text("origin");
    _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(encoder, origin);
    _ZN3WTF11Persistence7EncoderlsEm(&encoder, 0);
    _ZN3WTF11Persistence7EncoderlsEm(&encoder, 0);
    const unsigned payloadSize = encoder.size;
    _ZN3WTF11Persistence7Encoder14encodeChecksumEv(&encoder);
    Decoder decoder{encoder.buffer, encoder.size, encoder.buffer, {}};
    _ZN3WTF4SHA1C1Ev(&decoder.sha1);
    auto data = _ZN7WebCore20PasteboardCustomData22fromPersistenceDecoderEON3WTF11Persistence7DecoderE(&decoder);
    Require(Equal(data.origin, "origin") && !data.data.size);
    Require(decoder.bufferPosition == decoder.bufferData + payloadSize);
    Require(_ZN3WTF11Persistence7Decoder14verifyChecksumEv(&decoder));
    _ZN3WTF11Persistence7EncoderD1Ev(&encoder);
}

void TestEntryOwnership() {
    auto text = Text("owned");
    const unsigned base = text.impl->refCount;
    {
        PasteboardEntry source;
        source.type = text;
        source.customData = text;
        source.platformData.string = text.impl;
        text.impl->refCount += 2;
        alignas(PasteboardEntry) std::byte storage[sizeof(PasteboardEntry)];
        auto* copy = reinterpret_cast<PasteboardEntry*>(storage);
        _ZN7WebCore20PasteboardCustomData5EntryC1ERKS1_(copy, &source);
        Require(text.impl->refCount == base + 12);
        Require(copy->platformData.string == text.impl);
        PasteboardEntry moved;
        Require(&_ZN7WebCore20PasteboardCustomData5EntryaSEOS1_(&moved, copy) == &moved);
        Require(!copy->type.impl && !copy->customData.impl && !copy->platformData.string);
        copy->~PasteboardEntry();
        Require(&_ZN7WebCore20PasteboardCustomData5EntryaSERKS1_(&moved, &moved) == &moved);
        Require(text.impl->refCount == base + 12);
    }
    Require(text.impl->refCount == base);

    auto buffer = _ZN7WebCore12SharedBuffer6createEPKcm("payload", 7);
    {
        PasteboardEntry source;
        source.platformData.index = 1;
        source.platformData.buffer = buffer.ptr;
        ++buffer.ptr->refCount;
        PasteboardEntry copy;
        _ZN7WebCore20PasteboardCustomData5EntryaSERKS1_(&copy, &source);
        Require(buffer.ptr->refCount == 3 && copy.platformData.buffer == buffer.ptr);
        PasteboardCustomData data;
        data.data.append(copy);
        data.data[0].type = Text("buffer");
        auto encoded = _ZNK7WebCore20PasteboardCustomData18createSharedBufferEv(&data);
        auto decoded = _ZN7WebCore20PasteboardCustomData16fromSharedBufferERKNS_12SharedBufferE(encoded.ptr);
        Require(decoded.data.size == 1 && !decoded.data[0].platformData.string);
        _ZN7WebCore20PasteboardCustomData5EntryaSEOS1_(&copy, &source);
        Require(buffer.ptr->refCount == 3 && !source.platformData.buffer);
    }
    Require(buffer.ptr->refCount == 1);

    PasteboardEntry valueless;
    valueless.platformData.index = -1;
    PasteboardEntry copy;
    _ZN7WebCore20PasteboardCustomData5EntryaSERKS1_(&copy, &valueless);
    Require(copy.platformData.index == 0 && !copy.platformData.string);
}

void TestDataOwnership() {
    String origin = Text("origin");
    Vector<PasteboardEntry> entries;
    entries.append(PasteboardEntry{Text("type"), Text("value"), {}});
    auto* originalBuffer = entries.buffer;
    alignas(PasteboardCustomData) std::byte storage[sizeof(PasteboardCustomData)];
    auto* data = reinterpret_cast<PasteboardCustomData*>(storage);
    _ZN7WebCore20PasteboardCustomDataC1EON3WTF6StringEONS1_6VectorINS0_5EntryELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(data, &origin, &entries);
    Require(!origin.impl && !entries.buffer && data->data.buffer == originalBuffer);
    alignas(PasteboardCustomData) std::byte copyStorage[sizeof(PasteboardCustomData)];
    auto* copy = reinterpret_cast<PasteboardCustomData*>(copyStorage);
    _ZN7WebCore20PasteboardCustomDataC2ERKS0_(copy, data);
    Require(copy->data.buffer != data->data.buffer && copy->data[0].type.impl == data->data[0].type.impl);
    PasteboardCustomData assigned;
    _ZN7WebCore20PasteboardCustomDataaSERKS0_(&assigned, copy);
    _ZN7WebCore20PasteboardCustomDataaSERKS0_(&assigned, &assigned);
    Require(assigned.data.size == 1 && Equal(assigned.origin, "origin"));
    _ZN7WebCore20PasteboardCustomDataD1Ev(copy);
    alignas(PasteboardCustomData) std::byte moveStorage[sizeof(PasteboardCustomData)];
    auto* moved = reinterpret_cast<PasteboardCustomData*>(moveStorage);
    _ZN7WebCore20PasteboardCustomDataC2EOS0_(moved, data);
    Require(!data->data.buffer && !data->origin.impl && moved->data.buffer == originalBuffer);
    _ZN7WebCore20PasteboardCustomDataD2Ev(data);
    _ZN7WebCore20PasteboardCustomDataD1Ev(moved);
}

}  // namespace

int main() {
    TestWireFormat();
    TestDictionaryAndOrder();
    TestUnicodeAndGrowth();
    TestDecoderContinuation();
    TestEntryOwnership();
    TestDataOwnership();
}
