#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t NoMatch = 0;
constexpr std::int32_t NoValue = 1;
constexpr std::int32_t FinalValue = 2;
constexpr std::int32_t IntermediateValue = 3;

constexpr std::uint8_t Bytes[] = {
    0x0a, 0x72, 0x1d, 0x75, 0x15, 0x75, 0x31, 0x76, 0x31, 0x7a, 0x01, 0x65, 0x2c, 0x7a, 0xff, 0xff, 0xff, 0xff, 0xfb, 0x12,
    0x62, 0x72, 0x61, 0xdb, 0x86, 0xa0, 0x72, 0x31, 0x73, 0x31, 0x74, 0x31, 0x61, 0x30, 0x62, 0x29, 0x63, 0x36, 0x64, 0x3a,
    0x71, 0x31, 0x22, 0x10, 0x62, 0x24, 0x10, 0x63, 0x27, 0x11, 0x61, 0x74, 0x2b, 0x10, 0x6f, 0x2e, 0x10, 0x67, 0x2d,
};

constexpr char16_t Units[] = {
    0x000c, 0x0073, 0x001f, 0x0076, 0x0017, 0x0076, 0x8008, 0x007a, 0x0006, 0x00e9, 0x800a, 0xd83d, 0x0030, 0xde00, 0x8009, 0x0001,
    0x0065, 0x0004, 0x007a, 0xffff, 0xffff, 0xfffb, 0x0032, 0x0062, 0x0072, 0x0061, 0xc001, 0x86a0, 0x0073, 0x8008, 0x0074, 0x8008,
    0x0075, 0x8008, 0x0064, 0x000b, 0x0064, 0x0004, 0x0071, 0x8008, 0x0072, 0x8008, 0x0030, 0x006f, 0x0230, 0x0067, 0x8006, 0x0061,
    0x0007, 0x0062, 0x8004, 0x0063, 0x0031, 0x0061, 0x0074, 0x8005, 0x00b0, 0x0062, 0x00f0, 0x0063, 0x8003,
};

struct BytesTrieLayout {
    std::uint8_t* ownedArray;
    const std::uint8_t* bytes;
    const std::uint8_t* pos;
    std::int32_t remainingMatchLength;
};

struct UCharsTrieLayout {
    char16_t* ownedArray;
    const char16_t* uchars;
    const char16_t* pos;
    std::int32_t remainingMatchLength;
};

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

void ExpectInt(std::int64_t actual, std::int64_t expected, const char* what) {
    if (actual == expected) return;
    std::fprintf(stderr, "failed: %s: got %lld, expected %lld\n", what, static_cast<long long>(actual),
                 static_cast<long long>(expected));
    std::abort();
}

template<typename TCall>
void ExpectInvalid(const TCall& call, const char* what) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    std::fprintf(stderr, "failed: %s did not throw\n", what);
    std::abort();
}

BytesTrieLayout NewBytes() {
    return {nullptr, Bytes, Bytes, -1};
}

UCharsTrieLayout NewUChars() {
    return {nullptr, Units, Units, -1};
}

Icu::BytesTrie* AsTrie(BytesTrieLayout& layout) {
    return reinterpret_cast<Icu::BytesTrie*>(&layout);
}

Icu::UCharsTrie* AsTrie(UCharsTrieLayout& layout) {
    return reinterpret_cast<Icu::UCharsTrie*>(&layout);
}

std::int32_t BytesValue(const BytesTrieLayout& layout) {
    const std::uint8_t* pos = layout.pos;
    const std::int32_t lead = *pos++;
    return _ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(pos, lead >> 1);
}

std::int32_t UCharsValue(const UCharsTrieLayout& layout) {
    const char16_t* pos = layout.pos;
    std::int32_t lead = *pos++;
    if ((lead & 0x8000) != 0) {
        lead &= 0x7fff;
        if (lead < 0x4000) return lead;
        if (lead < 0x7fff) return ((lead - 0x4000) << 16) | pos[0];
        return (pos[0] << 16) | pos[1];
    }
    lead >>= 6;
    if (lead < 0x100) return lead - 1;
    if (lead < 0x1ff) return ((lead - 0x101) << 16) | pos[0];
    return (pos[0] << 16) | pos[1];
}

struct GuestByteSink {
    const void* const* table;
    std::string bytes;
    std::int32_t calls;
};

void APS5_VABI SinkDestroy(GuestByteSink*) {}

void APS5_VABI SinkAppend(GuestByteSink* sink, const char* bytes, std::int32_t length) {
    sink->bytes.append(bytes, static_cast<std::size_t>(length));
    ++sink->calls;
}

char* APS5_VABI SinkBuffer(GuestByteSink*, std::int32_t, std::int32_t, char* scratch, std::int32_t, std::int32_t* capacity) {
    *capacity = 0;
    return scratch;
}

void APS5_VABI SinkFlush(GuestByteSink*) {}

const void* const SinkTable[] = {reinterpret_cast<const void*>(&SinkDestroy), reinterpret_cast<const void*>(&SinkDestroy),
                                 reinterpret_cast<const void*>(&SinkAppend), reinterpret_cast<const void*>(&SinkBuffer),
                                 reinterpret_cast<const void*>(&SinkFlush)};

struct GuestAppendable {
    const void* const* table;
    std::u16string units;
    std::int32_t reserved;
};

void APS5_VABI AppendableDestroy(GuestAppendable*) {}

void* APS5_VABI AppendableClass(const GuestAppendable*) {
    return nullptr;
}

std::int8_t APS5_VABI AppendUnit(GuestAppendable* appendable, char16_t unit) {
    appendable->units.push_back(unit);
    return 1;
}

std::int8_t APS5_VABI AppendCodePoint(GuestAppendable* appendable, std::int32_t codePoint) {
    appendable->units.push_back(static_cast<char16_t>(codePoint));
    return 1;
}

std::int8_t APS5_VABI AppendString(GuestAppendable* appendable, const char16_t* text, std::int32_t length) {
    appendable->units.append(text, static_cast<std::size_t>(length));
    return 1;
}

std::int8_t APS5_VABI AppendReserve(GuestAppendable* appendable, std::int32_t capacity) {
    appendable->reserved += capacity;
    return 1;
}

const void* const AppendableTable[] = {
    reinterpret_cast<const void*>(&AppendableDestroy), reinterpret_cast<const void*>(&AppendableDestroy),
    reinterpret_cast<const void*>(&AppendableClass),   reinterpret_cast<const void*>(&AppendUnit),
    reinterpret_cast<const void*>(&AppendCodePoint),   reinterpret_cast<const void*>(&AppendString),
    reinterpret_cast<const void*>(&AppendReserve),     nullptr,
};

Icu::ByteSink* AsSink(GuestByteSink& sink) {
    return reinterpret_cast<Icu::ByteSink*>(&sink);
}

Icu::Appendable* AsAppendable(GuestAppendable& appendable) {
    return reinterpret_cast<Icu::Appendable*>(&appendable);
}

std::int32_t BytesNext(BytesTrieLayout& layout, const char* text) {
    return _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(AsTrie(layout), text, static_cast<std::int32_t>(std::char_traits<char>::length(text)));
}

void TestBytesMatching() {
    BytesTrieLayout layout = NewBytes();
    Icu::BytesTrie* trie = AsTrie(layout);
    ExpectInt(_ZNK6icu_689BytesTrie7currentEv_nid_postfix(trie), NoValue, "current at the root");
    ExpectInt(_ZN6icu_689BytesTrie4nextEi_nid_postfix(trie, 'a'), IntermediateValue, "a has a value and children");
    ExpectInt(BytesValue(layout), 1, "value of a");
    ExpectInt(_ZN6icu_689BytesTrie4nextEi_nid_postfix(trie, 'b'), IntermediateValue, "ab has a value and children");
    ExpectInt(BytesValue(layout), 2, "value of ab");
    ExpectInt(_ZN6icu_689BytesTrie4nextEi_nid_postfix(trie, 'c'), FinalValue, "abc is final");
    ExpectInt(BytesValue(layout), 3, "value of abc");
    ExpectInt(_ZNK6icu_689BytesTrie7currentEv_nid_postfix(trie), FinalValue, "current after abc");
    ExpectInt(_ZN6icu_689BytesTrie4nextEi_nid_postfix(trie, 'd'), NoMatch, "nothing follows abc");
    Expect(layout.pos == nullptr, "a failed match clears the position");
    ExpectInt(_ZNK6icu_689BytesTrie7currentEv_nid_postfix(trie), NoMatch, "current after a failed match");

    layout = NewBytes();
    ExpectInt(BytesNext(layout, "zebra"), FinalValue, "zebra is final");
    ExpectInt(BytesValue(layout), 100000, "value of zebra");
    layout = NewBytes();
    ExpectInt(BytesNext(layout, "zz"), FinalValue, "zz is final");
    ExpectInt(BytesValue(layout), -5, "negative value of zz");
    layout = NewBytes();
    ExpectInt(BytesNext(layout, "do"), IntermediateValue, "do has a value and children");
    ExpectInt(BytesValue(layout), 7, "value of do");
    ExpectInt(_ZN6icu_689BytesTrie4nextEi_nid_postfix(trie, 'g'), FinalValue, "dog after do");
    ExpectInt(BytesValue(layout), 6, "value of dog");
    layout = NewBytes();
    ExpectInt(BytesNext(layout, "ca"), NoValue, "ca is a prefix without a value");
    ExpectInt(layout.remainingMatchLength, 0, "one byte of the linear match is left");
    layout = NewBytes();
    ExpectInt(BytesNext(layout, "cab"), NoMatch, "cab does not match");
    layout = NewBytes();
    ExpectInt(BytesNext(layout, ""), NoValue, "an empty string leaves the root");
    ExpectInt(_ZN6icu_689BytesTrie4nextEPKci_nid_postfix(trie, nullptr, 0), NoValue, "a null empty string leaves the root");
    ExpectInt(_ZN6icu_689BytesTrie4nextEi_nid_postfix(trie, 0x1a2), NoMatch, "only the low byte of the input counts");

    layout = NewBytes();
    ExpectInt(_ZN6icu_689BytesTrie8nextImplEPKhi_nid_postfix(trie, Bytes, 'c'), NoValue, "nextImpl from the root");
    BytesTrieLayout expected = NewBytes();
    _ZN6icu_689BytesTrie4nextEi_nid_postfix(AsTrie(expected), 'c');
    Expect(layout.pos == expected.pos && layout.remainingMatchLength == expected.remainingMatchLength, "nextImpl moves like next");

    layout = NewBytes();
    ExpectInt(_ZN6icu_689BytesTrie10branchNextEPKhii_nid_postfix(trie, Bytes + 1, Bytes[0], 'a'), IntermediateValue,
              "branchNext at the root branch");
    ExpectInt(BytesValue(layout), 1, "branchNext reaches the value of a");
    layout = NewBytes();
    ExpectInt(_ZN6icu_689BytesTrie10branchNextEPKhii_nid_postfix(trie, Bytes + 1, Bytes[0], 'x'), NoMatch,
              "branchNext with a byte that is not in the branch");
    Expect(layout.pos == nullptr, "branchNext clears the position on a failed match");
}

void TestBytesHelpers() {
    std::uint8_t deltas[0x110] = {0x05};
    Expect(_ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(deltas) == deltas + 6, "one-byte delta");
    deltas[0] = 0xc1;
    deltas[1] = 0x02;
    Expect(_ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(deltas) == deltas + 2 + 0x102, "two-byte delta");
    const std::uint8_t value[] = {0x05, 0x06, 0x07, 0x08};
    ExpectInt(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(value, 0x13), 3, "one-byte value");
    ExpectInt(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(value, 0x52), 0x105, "two-byte value");
    ExpectInt(_ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(value, 0x7f), 0x05060708, "four-byte value");

    BytesTrieLayout layout = NewBytes();
    std::int32_t unique = 0;
    ExpectInt(_ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(Bytes, 0, &unique), 0, "the root has many values");
    Expect(_ZN6icu_689BytesTrie25findUniqueValueFromBranchEPKhiaRi_nid_postfix(Bytes + 1, Bytes[0] + 1, 0, &unique) == nullptr,
           "the root branch has many values");
    BytesNext(layout, "zeb");
    ExpectInt(_ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(layout.pos + layout.remainingMatchLength + 1, 0, &unique), 1,
              "zeb leads to one value");
    ExpectInt(unique, 100000, "the unique value after zeb");
    unique = 4;
    ExpectInt(_ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(layout.pos + layout.remainingMatchLength + 1, 1, &unique), 0,
              "a different known value is not unique");

    layout = NewBytes();
    GuestByteSink sink{SinkTable, {}, 0};
    ExpectInt(_ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(AsTrie(layout), AsSink(sink)), 11, "next bytes at the root");
    Expect(sink.bytes == "abcdqrstuvz", "the next bytes at the root");
    sink = {SinkTable, {}, 0};
    _ZN6icu_689BytesTrie18getNextBranchBytesEPKhiRNS_8ByteSinkE_nid_postfix(Bytes + 1, Bytes[0] + 1, AsSink(sink));
    Expect(sink.bytes == "abcdqrstuvz", "the bytes of the root branch");
    sink = {SinkTable, {}, 0};
    BytesNext(layout, "d");
    ExpectInt(_ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(AsTrie(layout), AsSink(sink)), 1, "next bytes after d");
    Expect(sink.bytes == "o", "the next byte after d");
    BytesNext(layout, "og");
    sink = {SinkTable, {}, 0};
    ExpectInt(_ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(AsTrie(layout), AsSink(sink)), 0, "no bytes after dog");
    ExpectInt(sink.calls, 0, "nothing is appended after dog");
    _ZN6icu_689BytesTrie6appendERNS_8ByteSinkEi_nid_postfix(AsSink(sink), 'x');
    Expect(sink.bytes == "x" && sink.calls == 1, "append writes one byte");

    _ZN6icu_689BytesTrieD1Ev_nid_postfix(AsTrie(layout));
    layout = NewBytes();
    _ZN6icu_689BytesTrieD2Ev_nid_postfix(AsTrie(layout));
}

std::int32_t UCharsNext(UCharsTrieLayout& layout, const char16_t* text, std::int32_t length) {
    return _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(AsTrie(layout), &text, length);
}

void TestUChars() {
    UCharsTrieLayout layout = NewUChars();
    Icu::UCharsTrie* trie = AsTrie(layout);
    ExpectInt(_ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(trie), NoValue, "current at the root");
    ExpectInt(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(trie, u'a'), IntermediateValue, "a has a value and children");
    ExpectInt(UCharsValue(layout), 1, "value of a");
    ExpectInt(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(trie, u'b'), IntermediateValue, "ab has a value and children");
    ExpectInt(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(trie, u'c'), FinalValue, "abc is final");
    ExpectInt(UCharsValue(layout), 3, "value of abc");
    ExpectInt(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(trie, u'd'), NoMatch, "nothing follows abc");
    ExpectInt(_ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(trie), NoMatch, "current after a failed match");

    layout = NewUChars();
    ExpectInt(UCharsNext(layout, u"zebra", 5), FinalValue, "zebra is final");
    ExpectInt(UCharsValue(layout), 100000, "value of zebra");
    layout = NewUChars();
    ExpectInt(UCharsNext(layout, u"zz", -1), FinalValue, "a NUL-terminated zz is final");
    ExpectInt(UCharsValue(layout), -5, "negative value of zz");
    layout = NewUChars();
    ExpectInt(UCharsNext(layout, u"ca", 2), NoValue, "ca is a prefix without a value");
    layout = NewUChars();
    const char16_t* none = nullptr;
    ExpectInt(_ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(trie, &none, 0), NoValue, "a null empty string leaves the root");

    layout = NewUChars();
    ExpectInt(_ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(trie, 0x1f600), FinalValue, "a supplementary code point");
    ExpectInt(UCharsValue(layout), 9, "value of the supplementary code point");
    ExpectInt(_ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(trie, 0xe9), FinalValue, "firstForCodePoint starts over");
    ExpectInt(UCharsValue(layout), 10, "value of a BMP code point");
    ExpectInt(_ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(trie, u'd'), NoValue, "firstForCodePoint of d");
    ExpectInt(_ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(trie, u'o'), IntermediateValue, "nextForCodePoint of o");
    ExpectInt(UCharsValue(layout), 7, "value of do");
    ExpectInt(_ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(trie, 0x1f600), NoMatch, "nextForCodePoint without a match");

    layout = NewUChars();
    ExpectInt(_ZN6icu_6810UCharsTrie8nextImplEPKDsi_nid_postfix(trie, Units, u'c'), NoValue, "nextImpl from the root");
    UCharsTrieLayout expected = NewUChars();
    _ZN6icu_6810UCharsTrie4nextEi_nid_postfix(AsTrie(expected), u'c');
    Expect(layout.pos == expected.pos && layout.remainingMatchLength == expected.remainingMatchLength, "nextImpl moves like next");
    layout = NewUChars();
    ExpectInt(_ZN6icu_6810UCharsTrie10branchNextEPKDsii_nid_postfix(trie, Units + 1, Units[0], u'z'), NoValue,
              "branchNext at the root branch");
    ExpectInt(_ZN6icu_6810UCharsTrie4nextEi_nid_postfix(trie, u'z'), FinalValue, "zz after branchNext");

    std::int32_t unique = 0;
    ExpectInt(_ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(Units, 0, &unique), 0, "the root has many values");
    Expect(_ZN6icu_6810UCharsTrie25findUniqueValueFromBranchEPKDsiaRi_nid_postfix(Units + 1, Units[0] + 1, 0, &unique) == nullptr,
           "the root branch has many values");
    layout = NewUChars();
    UCharsNext(layout, u"zeb", 3);
    ExpectInt(_ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(layout.pos + layout.remainingMatchLength + 1, 0, &unique), 1,
              "zeb leads to one value");
    ExpectInt(unique, 100000, "the unique value after zeb");

    layout = NewUChars();
    GuestAppendable appendable{AppendableTable, {}, 0};
    ExpectInt(_ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(trie, AsAppendable(appendable)), 13,
              "next units at the root");
    Expect(appendable.units == u"abcdqrstuvzé\xd83d", "the next units at the root");
    ExpectInt(appendable.reserved, 13, "getNextUChars reserves capacity");
    appendable = {AppendableTable, {}, 0};
    _ZN6icu_6810UCharsTrie19getNextBranchUCharsEPKDsiRNS_10AppendableE_nid_postfix(Units + 1, Units[0] + 1, AsAppendable(appendable));
    Expect(appendable.units == u"abcdqrstuvzé\xd83d", "the units of the root branch");
    appendable = {AppendableTable, {}, 0};
    UCharsNext(layout, u"do", 2);
    ExpectInt(_ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(trie, AsAppendable(appendable)), 1,
              "next units after do");
    Expect(appendable.units == u"g", "the next unit after do");

    _ZN6icu_6810UCharsTrieD1Ev_nid_postfix(trie);
    layout = NewUChars();
    _ZN6icu_6810UCharsTrieD2Ev_nid_postfix(trie);
}

void TestArguments() {
    BytesTrieLayout bytes = NewBytes();
    UCharsTrieLayout units = NewUChars();
    std::int32_t value = 0;
    GuestByteSink sink{SinkTable, {}, 0};
    GuestAppendable appendable{AppendableTable, {}, 0};
    ExpectInvalid([&] { _ZNK6icu_689BytesTrie7currentEv_nid_postfix(nullptr); }, "BytesTrie current with a null trie");
    ExpectInvalid([&] { _ZN6icu_689BytesTrie4nextEi_nid_postfix(nullptr, 'a'); }, "BytesTrie next with a null trie");
    ExpectInvalid([&] { _ZN6icu_689BytesTrie4nextEPKci_nid_postfix(AsTrie(bytes), nullptr, 2); }, "BytesTrie next with null text");
    ExpectInvalid([&] { _ZN6icu_689BytesTrie8nextImplEPKhi_nid_postfix(AsTrie(bytes), nullptr, 'a'); }, "nextImpl with a null position");
    ExpectInvalid([&] { _ZN6icu_689BytesTrie10branchNextEPKhii_nid_postfix(AsTrie(bytes), nullptr, 1, 'a'); },
                  "branchNext with a null position");
    ExpectInvalid([&] { _ZN6icu_689BytesTrie9readValueEPKhi_nid_postfix(nullptr, 0x13); }, "readValue with a null position");
    ExpectInvalid([&] { _ZN6icu_689BytesTrie11jumpByDeltaEPKh_nid_postfix(nullptr); }, "jumpByDelta with a null position");
    ExpectInvalid([&] { _ZN6icu_689BytesTrie15findUniqueValueEPKhaRi_nid_postfix(Bytes, 0, nullptr); }, "findUniqueValue with a null value");
    ExpectInvalid([&] { _ZN6icu_689BytesTrie25findUniqueValueFromBranchEPKhiaRi_nid_postfix(nullptr, 1, 0, &value); },
                  "findUniqueValueFromBranch with a null position");
    ExpectInvalid([&] { _ZNK6icu_689BytesTrie12getNextBytesERNS_8ByteSinkE_nid_postfix(AsTrie(bytes), nullptr); },
                  "getNextBytes with a null sink");
    ExpectInvalid([&] { _ZN6icu_689BytesTrie18getNextBranchBytesEPKhiRNS_8ByteSinkE_nid_postfix(nullptr, 1, AsSink(sink)); },
                  "getNextBranchBytes with a null position");
    ExpectInvalid([&] { _ZN6icu_689BytesTrie6appendERNS_8ByteSinkEi_nid_postfix(nullptr, 'a'); }, "append with a null sink");
    ExpectInvalid([&] { _ZN6icu_689BytesTrieD1Ev_nid_postfix(nullptr); }, "BytesTrie destructor with a null trie");
    ExpectInvalid([&] { _ZNK6icu_6810UCharsTrie7currentEv_nid_postfix(nullptr); }, "UCharsTrie current with a null trie");
    ExpectInvalid([&] { _ZN6icu_6810UCharsTrie17firstForCodePointEi_nid_postfix(nullptr, 'a'); }, "firstForCodePoint with a null trie");
    ExpectInvalid([&] { _ZN6icu_6810UCharsTrie16nextForCodePointEi_nid_postfix(nullptr, 'a'); }, "nextForCodePoint with a null trie");
    ExpectInvalid([&] { _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(AsTrie(units), nullptr, 1); },
                  "UCharsTrie next with a null pointer object");
    const char16_t* none = nullptr;
    ExpectInvalid([&] { _ZN6icu_6810UCharsTrie4nextENS_14ConstChar16PtrEi_nid_postfix(AsTrie(units), &none, 1); },
                  "UCharsTrie next with null text");
    ExpectInvalid([&] { _ZN6icu_6810UCharsTrie8nextImplEPKDsi_nid_postfix(AsTrie(units), nullptr, 'a'); },
                  "UCharsTrie nextImpl with a null position");
    ExpectInvalid([&] { _ZN6icu_6810UCharsTrie10branchNextEPKDsii_nid_postfix(nullptr, Units, 1, 'a'); },
                  "UCharsTrie branchNext with a null trie");
    ExpectInvalid([&] { _ZN6icu_6810UCharsTrie15findUniqueValueEPKDsaRi_nid_postfix(nullptr, 0, &value); },
                  "UCharsTrie findUniqueValue with a null position");
    ExpectInvalid([&] { _ZNK6icu_6810UCharsTrie13getNextUCharsERNS_10AppendableE_nid_postfix(AsTrie(units), nullptr); },
                  "getNextUChars with a null appendable");
    ExpectInvalid([&] { _ZN6icu_6810UCharsTrie19getNextBranchUCharsEPKDsiRNS_10AppendableE_nid_postfix(nullptr, 1, AsAppendable(appendable)); },
                  "getNextBranchUChars with a null position");
    ExpectInvalid([&] { _ZN6icu_6810UCharsTrieD2Ev_nid_postfix(nullptr); }, "UCharsTrie destructor with a null trie");
}

}

int main() {
    TestBytesMatching();
    TestBytesHelpers();
    TestUChars();
    TestArguments();
    return 0;
}
