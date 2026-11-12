#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<decltype(&ucptrie_get_nid_postfix),
                             std::uint32_t (APS5_VABI*)(const Icu::CodePointTrie*, std::int32_t)>);
static_assert(std::is_same_v<decltype(&umutablecptrie_setRange_nid_postfix),
                             void (APS5_VABI*)(Icu::MutableCodePointTrie*, std::int32_t, std::int32_t, std::uint32_t,
                                               std::int32_t*)>);

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidFormat = 3;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t TypeAny = -1;
constexpr std::int32_t TypeFast = 0;
constexpr std::int32_t TypeSmall = 1;
constexpr std::int32_t WidthAny = -1;
constexpr std::int32_t Width16 = 0;
constexpr std::int32_t Width32 = 1;
constexpr std::int32_t Width8 = 2;
constexpr std::int32_t RangeNormal = 0;
constexpr std::int32_t RangeFixedLeadSurrogates = 1;
constexpr std::int32_t RangeFixedAllSurrogates = 2;
constexpr std::uint32_t Initial = 6;
constexpr std::uint32_t Error = 0xad;

struct TrieLayout {
    const std::uint16_t* index;
    const void* data;
    std::int32_t indexLength;
    std::int32_t dataLength;
    std::int32_t highStart;
    std::uint16_t shifted12HighStart;
    std::int8_t type;
    std::int8_t valueWidth;
    std::uint32_t reserved32;
    std::uint16_t reserved16;
    std::uint16_t index3NullOffset;
    std::int32_t dataNullOffset;
    std::uint32_t nullValue;
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

int filterCalls = 0;

std::uint32_t APS5_VABI LowBit(const void* context, std::uint32_t value) {
    ++filterCalls;
    return (value & 1) + *static_cast<const std::uint32_t*>(context);
}

Icu::MutableCodePointTrie* Build() {
    std::int32_t error = Success;
    Icu::MutableCodePointTrie* trie = umutablecptrie_open_nid_postfix(Initial, Error, &error);
    ExpectInt(error, Success, "open status");
    Expect(trie != nullptr, "open returns a trie");
    umutablecptrie_set_nid_postfix(trie, 0x41, 1, &error);
    umutablecptrie_setRange_nid_postfix(trie, 0x100, 0x1ff, 3, &error);
    umutablecptrie_setRange_nid_postfix(trie, 0x200, 0x2ff, 5, &error);
    umutablecptrie_setRange_nid_postfix(trie, 0xd000, 0xe000, 9, &error);
    umutablecptrie_setRange_nid_postfix(trie, 0x20000, 0x2ffff, 11, &error);
    umutablecptrie_set_nid_postfix(trie, 0x10ffff, 13, &error);
    ExpectInt(error, Success, "set status");
    return trie;
}

void TestMutable() {
    Icu::MutableCodePointTrie* trie = Build();
    ExpectInt(umutablecptrie_get_nid_postfix(trie, 0x40), Initial, "initial value");
    ExpectInt(umutablecptrie_get_nid_postfix(trie, 0x41), 1, "single value");
    ExpectInt(umutablecptrie_get_nid_postfix(trie, 0x1ff), 3, "range end value");
    ExpectInt(umutablecptrie_get_nid_postfix(trie, 0x2abcd), 11, "supplementary range value");
    ExpectInt(umutablecptrie_get_nid_postfix(trie, 0x10ffff), 13, "last code point value");
    ExpectInt(umutablecptrie_get_nid_postfix(trie, 0x110000), Error, "value above the code point range");
    ExpectInt(umutablecptrie_get_nid_postfix(trie, -1), Error, "value of a negative code point");

    std::uint32_t value = 0;
    ExpectInt(umutablecptrie_getRange_nid_postfix(trie, 0, RangeNormal, 0, nullptr, nullptr, &value), 0x40, "first range end");
    ExpectInt(value, Initial, "first range value");
    ExpectInt(umutablecptrie_getRange_nid_postfix(trie, 0x100, RangeNormal, 0, nullptr, nullptr, &value), 0x1ff,
              "second range end");
    ExpectInt(value, 3, "second range value");
    ExpectInt(umutablecptrie_getRange_nid_postfix(trie, 0x100, RangeNormal, 0, nullptr, nullptr, nullptr), 0x1ff,
              "range end without a value pointer");
    ExpectInt(umutablecptrie_getRange_nid_postfix(trie, 0x110000, RangeNormal, 0, nullptr, nullptr, &value), -1,
              "range past the end");

    const std::uint32_t offset = 100;
    filterCalls = 0;
    ExpectInt(umutablecptrie_getRange_nid_postfix(trie, 0x100, RangeNormal, 0, LowBit, &offset, &value), 0x2ff,
              "filtered range joins values with the same low bit");
    ExpectInt(value, 101, "filtered range value");
    Expect(filterCalls > 0, "the filter is called");

    ExpectInt(umutablecptrie_getRange_nid_postfix(trie, 0xd000, RangeNormal, 0, nullptr, nullptr, &value), 0xe000,
              "range across the surrogates");
    ExpectInt(umutablecptrie_getRange_nid_postfix(trie, 0xd000, RangeFixedAllSurrogates, 77, nullptr, nullptr, &value), 0xd7ff,
              "range ends before fixed surrogates");
    ExpectInt(value, 9, "value before fixed surrogates");
    ExpectInt(umutablecptrie_getRange_nid_postfix(trie, 0xd800, RangeFixedAllSurrogates, 77, nullptr, nullptr, &value), 0xdfff,
              "fixed range of all surrogates");
    ExpectInt(value, 77, "fixed value of all surrogates");
    ExpectInt(umutablecptrie_getRange_nid_postfix(trie, 0xd800, RangeFixedLeadSurrogates, 77, nullptr, nullptr, &value), 0xdbff,
              "fixed range of lead surrogates");

    std::int32_t error = Success;
    Icu::MutableCodePointTrie* clone = umutablecptrie_clone_nid_postfix(trie, &error);
    ExpectInt(error, Success, "clone status");
    Expect(clone != nullptr && clone != trie, "clone returns another trie");
    umutablecptrie_set_nid_postfix(clone, 0x41, 2, &error);
    ExpectInt(umutablecptrie_get_nid_postfix(trie, 0x41), 1, "the original keeps its value");
    ExpectInt(umutablecptrie_get_nid_postfix(clone, 0x41), 2, "the clone takes the new value");
    Expect(umutablecptrie_clone_nid_postfix(nullptr, &error) == nullptr && error == Success, "clone of null is null");
    umutablecptrie_close_nid_postfix(clone);

    umutablecptrie_set_nid_postfix(trie, 0x110000, 1, &error);
    ExpectInt(error, IllegalArgument, "set above the code point range");
    error = Success;
    umutablecptrie_setRange_nid_postfix(trie, 0x20, 0x10, 1, &error);
    ExpectInt(error, IllegalArgument, "setRange with a reversed range");
    umutablecptrie_set_nid_postfix(trie, 0x41, 99, &error);
    ExpectInt(umutablecptrie_get_nid_postfix(trie, 0x41), 1, "set does nothing after a failure");
    Expect(umutablecptrie_open_nid_postfix(0, 0, &error) == nullptr, "open returns null after a failure");
    Expect(umutablecptrie_clone_nid_postfix(trie, &error) == nullptr, "clone returns null after a failure");

    ExpectInvalid([&] { umutablecptrie_open_nid_postfix(0, 0, nullptr); }, "open with a null status");
    ExpectInvalid([&] { umutablecptrie_get_nid_postfix(nullptr, 0); }, "get with a null trie");
    ExpectInvalid([&] { umutablecptrie_getRange_nid_postfix(nullptr, 0, RangeNormal, 0, nullptr, nullptr, nullptr); },
                  "getRange with a null trie");
    error = Success;
    ExpectInvalid([&] { umutablecptrie_set_nid_postfix(nullptr, 0, 0, &error); }, "set with a null trie");
    ExpectInvalid([&] { umutablecptrie_setRange_nid_postfix(nullptr, 0, 1, 0, &error); }, "setRange with a null trie");
    ExpectInvalid([&] { umutablecptrie_buildImmutable_nid_postfix(nullptr, TypeFast, Width16, &error); },
                  "buildImmutable with a null trie");
    umutablecptrie_close_nid_postfix(nullptr);
    umutablecptrie_close_nid_postfix(trie);
}

std::uint32_t Data(const Icu::CodePointTrie* trie, std::int32_t index) {
    const auto& layout = *reinterpret_cast<const TrieLayout*>(trie);
    switch (layout.valueWidth) {
    case Width16: return static_cast<const std::uint16_t*>(layout.data)[index];
    case Width32: return static_cast<const std::uint32_t*>(layout.data)[index];
    default: return static_cast<const std::uint8_t*>(layout.data)[index];
    }
}

void CheckImmutable(std::int32_t type, std::int32_t width) {
    Icu::MutableCodePointTrie* builder = Build();
    std::int32_t error = Success;
    Icu::CodePointTrie* trie = umutablecptrie_buildImmutable_nid_postfix(builder, type, width, &error);
    ExpectInt(error, Success, "buildImmutable status");
    Expect(trie != nullptr, "buildImmutable returns a trie");
    umutablecptrie_close_nid_postfix(builder);

    const auto& layout = *reinterpret_cast<const TrieLayout*>(trie);
    ExpectInt(ucptrie_getType_nid_postfix(trie), type, "type");
    ExpectInt(ucptrie_getValueWidth_nid_postfix(trie), width, "value width");
    ExpectInt(layout.type, type, "type field");
    ExpectInt(layout.valueWidth, width, "value width field");
    ExpectInt(layout.highStart, 0x110000, "high start field");
    Expect(layout.index != nullptr && layout.data != nullptr, "index and data fields");

    ExpectInt(ucptrie_get_nid_postfix(trie, 0x40), Initial, "initial value");
    ExpectInt(ucptrie_get_nid_postfix(trie, 0x41), 1, "single value");
    ExpectInt(ucptrie_get_nid_postfix(trie, 0x250), 5, "range value");
    ExpectInt(ucptrie_get_nid_postfix(trie, 0x2abcd), 11, "supplementary range value");
    ExpectInt(ucptrie_get_nid_postfix(trie, 0x10ffff), 13, "last code point value");
    ExpectInt(ucptrie_get_nid_postfix(trie, 0x110000), Error, "value above the code point range");
    ExpectInt(ucptrie_get_nid_postfix(trie, -1), Error, "value of a negative code point");
    const auto* map = reinterpret_cast<const Icu::CodePointMap*>(trie);
    ExpectInt(ucpmap_get_nid_postfix(map, 0x2abcd), 11, "map value");

    ExpectInt(Data(trie, ucptrie_internalSmallIndex_nid_postfix(trie, 0x2abcd)), 11, "small index of a supplementary code point");
    ExpectInt(Data(trie, ucptrie_internalSmallU8Index_nid_postfix(trie, 0x2a, 0x2f, 0x0d)), 11,
              "small index of UTF-8 trail bytes");
    ExpectInt(Data(trie, ucptrie_internalSmallU8Index_nid_postfix(trie, 0x10f, 0x3f, 0x3f)), 13,
              "small index of the last code point");
    const std::uint8_t text[] = {0x41, 0xc8, 0x90, 0xf0, 0xaa, 0xaf, 0x8d, 0x80};
    std::int32_t packed = ucptrie_internalU8PrevIndex_nid_postfix(trie, 0x8d, text, text + 6);
    ExpectInt(packed & 7, 3, "bytes before the last trail byte of a four-byte sequence");
    ExpectInt(Data(trie, packed >> 3), 11, "previous index of a four-byte sequence");
    packed = ucptrie_internalU8PrevIndex_nid_postfix(trie, 0x90, text, text + 2);
    ExpectInt(packed & 7, 1, "bytes before the trail byte of a two-byte sequence");
    ExpectInt(Data(trie, packed >> 3), 5, "previous index of a two-byte sequence");
    packed = ucptrie_internalU8PrevIndex_nid_postfix(trie, 0x80, text, text + 7);
    ExpectInt(packed & 7, 0, "bytes before a lone trail byte");
    ExpectInt(Data(trie, packed >> 3), Error, "previous index of a lone trail byte");

    std::uint32_t value = 0;
    ExpectInt(ucptrie_getRange_nid_postfix(trie, 0x100, RangeNormal, 0, nullptr, nullptr, &value), 0x1ff, "range end");
    ExpectInt(value, 3, "range value");
    const std::uint32_t offset = 100;
    filterCalls = 0;
    ExpectInt(ucptrie_getRange_nid_postfix(trie, 0x100, RangeNormal, 0, LowBit, &offset, &value), 0x2ff, "filtered range end");
    ExpectInt(value, 101, "filtered range value");
    Expect(filterCalls > 0, "the filter is called");
    ExpectInt(ucpmap_getRange_nid_postfix(map, 0xd800, RangeFixedLeadSurrogates, 77, LowBit, &offset, &value), 0xdbff,
              "map range of fixed lead surrogates");
    ExpectInt(value, 77, "map value of fixed lead surrogates");
    ExpectInt(ucptrie_getRange_nid_postfix(trie, 0x110000, RangeNormal, 0, nullptr, nullptr, nullptr), -1, "range past the end");

    const std::int32_t size = ucptrie_toBinary_nid_postfix(trie, nullptr, 0, &error);
    ExpectInt(error, BufferOverflow, "toBinary preflight status");
    Expect(size > 16 && size % 4 == 0, "toBinary preflight size");
    error = Success;
    std::vector<std::uint32_t> binary(static_cast<std::size_t>(size) / 4 + 1, 0xa5a5a5a5u);
    ExpectInt(ucptrie_toBinary_nid_postfix(trie, binary.data(), size, &error), size, "toBinary size");
    ExpectInt(error, Success, "toBinary status");
    ExpectInt(binary.back(), 0xa5a5a5a5u, "toBinary stays inside its capacity");
    ExpectInt(ucptrie_toBinary_nid_postfix(trie, binary.data(), -1, &error), 0, "toBinary with a negative capacity");
    ExpectInt(error, IllegalArgument, "toBinary negative capacity status");
    error = Success;

    std::int32_t actual = 0;
    Icu::CodePointTrie* loaded = ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, binary.data(), size + 4, &actual, &error);
    ExpectInt(error, Success, "openFromBinary status");
    Expect(loaded != nullptr, "openFromBinary returns a trie");
    ExpectInt(actual, size, "openFromBinary actual length");
    ExpectInt(ucptrie_getType_nid_postfix(loaded), type, "loaded type");
    ExpectInt(ucptrie_getValueWidth_nid_postfix(loaded), width, "loaded value width");
    for (const std::int32_t codePoint : {0, 0x41, 0x1ff, 0x200, 0xd7ff, 0xe001, 0x2abcd, 0x10ffff, 0x110000}) {
        ExpectInt(ucptrie_get_nid_postfix(loaded, codePoint), ucptrie_get_nid_postfix(trie, codePoint), "loaded value");
    }
    ucptrie_close_nid_postfix(loaded);

    Expect(ucptrie_openFromBinary_nid_postfix(type == TypeFast ? TypeSmall : TypeFast, WidthAny, binary.data(), size, nullptr,
                                              &error) == nullptr,
           "openFromBinary rejects another type");
    ExpectInt(error, InvalidFormat, "another type status");
    error = Success;
    Expect(ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, binary.data(), 8, nullptr, &error) == nullptr,
           "openFromBinary rejects a short header");
    ExpectInt(error, InvalidFormat, "short header status");
    error = Success;
    Expect(ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, binary.data(), size - 4, nullptr, &error) == nullptr,
           "openFromBinary rejects truncated data");
    ExpectInt(error, InvalidFormat, "truncated data status");
    error = Success;
    Expect(ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, binary.data(), 0, nullptr, &error) == nullptr,
           "openFromBinary rejects an empty length");
    ExpectInt(error, IllegalArgument, "empty length status");
    Expect(ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, nullptr, size, nullptr, &error) == nullptr,
           "openFromBinary returns null after a failure");
    ExpectInt(ucptrie_toBinary_nid_postfix(nullptr, nullptr, 0, &error), 0, "toBinary returns 0 after a failure");
    error = Success;

    Icu::MutableCodePointTrie* copy = umutablecptrie_fromUCPTrie_nid_postfix(trie, &error);
    ExpectInt(error, Success, "fromUCPTrie status");
    Expect(copy != nullptr, "fromUCPTrie returns a trie");
    ExpectInt(umutablecptrie_get_nid_postfix(copy, 0x2abcd), 11, "fromUCPTrie value");
    ExpectInt(umutablecptrie_get_nid_postfix(copy, 0x40), Initial, "fromUCPTrie initial value");
    umutablecptrie_close_nid_postfix(copy);
    copy = umutablecptrie_fromUCPMap_nid_postfix(map, &error);
    ExpectInt(error, Success, "fromUCPMap status");
    Expect(copy != nullptr, "fromUCPMap returns a trie");
    ExpectInt(umutablecptrie_get_nid_postfix(copy, 0x10ffff), 13, "fromUCPMap value");
    umutablecptrie_close_nid_postfix(copy);
    ucptrie_close_nid_postfix(trie);
}

void TestArguments() {
    std::int32_t error = Success;
    Expect(umutablecptrie_fromUCPTrie_nid_postfix(nullptr, &error) == nullptr, "fromUCPTrie of null is null");
    ExpectInt(error, IllegalArgument, "fromUCPTrie of null status");
    error = Success;
    Expect(umutablecptrie_fromUCPMap_nid_postfix(nullptr, &error) == nullptr, "fromUCPMap of null is null");
    ExpectInt(error, IllegalArgument, "fromUCPMap of null status");
    error = Success;

    Icu::MutableCodePointTrie* builder = Build();
    Expect(umutablecptrie_buildImmutable_nid_postfix(builder, 2, Width16, &error) == nullptr, "buildImmutable rejects a type");
    ExpectInt(error, IllegalArgument, "buildImmutable type status");
    error = Success;
    umutablecptrie_close_nid_postfix(builder);

    const std::uint8_t byte = 0;
    ExpectInvalid([&] { ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, &byte, 16, nullptr, nullptr); },
                  "openFromBinary with a null status");
    ExpectInvalid([&] { ucptrie_openFromBinary_nid_postfix(TypeAny, WidthAny, nullptr, 16, nullptr, &error); },
                  "openFromBinary with null data");
    ExpectInvalid([&] { ucptrie_getType_nid_postfix(nullptr); }, "getType with a null trie");
    ExpectInvalid([&] { ucptrie_getValueWidth_nid_postfix(nullptr); }, "getValueWidth with a null trie");
    ExpectInvalid([&] { ucptrie_get_nid_postfix(nullptr, 0); }, "get with a null trie");
    ExpectInvalid([&] { ucptrie_getRange_nid_postfix(nullptr, 0, RangeNormal, 0, nullptr, nullptr, nullptr); },
                  "getRange with a null trie");
    ExpectInvalid([&] { ucptrie_toBinary_nid_postfix(nullptr, nullptr, 0, &error); }, "toBinary with a null trie");
    ExpectInvalid([&] { ucptrie_internalSmallIndex_nid_postfix(nullptr, 0x10000); }, "internalSmallIndex with a null trie");
    ExpectInvalid([&] { ucptrie_internalSmallU8Index_nid_postfix(nullptr, 0, 0, 0); }, "internalSmallU8Index with a null trie");
    ExpectInvalid([&] { ucptrie_internalU8PrevIndex_nid_postfix(nullptr, 0, &byte, &byte); },
                  "internalU8PrevIndex with a null trie");
    ExpectInvalid([&] { ucpmap_get_nid_postfix(nullptr, 0); }, "map get with a null map");
    ExpectInvalid([&] { ucpmap_getRange_nid_postfix(nullptr, 0, RangeNormal, 0, nullptr, nullptr, nullptr); },
                  "map getRange with a null map");
    ucptrie_close_nid_postfix(nullptr);
}

}

int main() {
    TestMutable();
    for (const std::int32_t type : {TypeFast, TypeSmall}) {
        for (const std::int32_t width : {Width16, Width32, Width8}) CheckImmutable(type, width);
    }
    TestArguments();
    return 0;
}
