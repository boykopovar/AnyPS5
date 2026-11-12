#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>
#include <vector>

static_assert(std::is_same_v<decltype(&utrie2_get32_nid_postfix), std::uint32_t (APS5_VABI*)(const Icu::Trie2*, std::int32_t)>);

namespace {

constexpr std::int32_t Success = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidFormat = 3;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t Bits16 = 0;
constexpr std::int32_t Bits32 = 1;
constexpr std::uint32_t Initial = 0x55;
constexpr std::uint32_t Error = 0x66;

struct Trie2Layout {
    const std::uint16_t* index;
    const std::uint16_t* data16;
    const std::uint32_t* data32;
    std::int32_t indexLength;
    std::int32_t dataLength;
    std::uint16_t index2NullOffset;
    std::uint16_t dataNullOffset;
    std::uint32_t initialValue;
    std::uint32_t errorValue;
    std::int32_t highStart;
    std::int32_t highValueIndex;
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

struct Ranges {
    std::int32_t calls = 0;
    std::int32_t stopAfter = 0;
    std::int32_t start[4] = {};
    std::int32_t end[4] = {};
    std::uint32_t value[4] = {};
};

std::int8_t APS5_VABI Record(const void* context, std::int32_t start, std::int32_t end, std::uint32_t value) {
    auto& ranges = *const_cast<Ranges*>(static_cast<const Ranges*>(context));
    if (ranges.calls < 4) {
        ranges.start[ranges.calls] = start;
        ranges.end[ranges.calls] = end;
        ranges.value[ranges.calls] = value;
    }
    ++ranges.calls;
    return ranges.calls < ranges.stopAfter;
}

std::uint32_t APS5_VABI Double(const void*, std::uint32_t value) {
    return value * 2;
}

std::uint32_t Data(const Icu::Trie2* trie, std::int32_t index) {
    const auto& layout = *reinterpret_cast<const Trie2Layout*>(trie);
    return layout.data32 ? layout.data32[index] : layout.index[index];
}

void CheckTrie(const Icu::Trie2* trie, std::int32_t bits) {
    const auto& layout = *reinterpret_cast<const Trie2Layout*>(trie);
    Expect(layout.index != nullptr, "index field");
    Expect(bits == Bits16 ? layout.data16 != nullptr && layout.data32 == nullptr : layout.data32 != nullptr && layout.data16 == nullptr,
           "data fields match the value width");
    ExpectInt(layout.initialValue, Initial, "initial value field");
    ExpectInt(layout.errorValue, Error, "error value field");
    if (bits == Bits16) ExpectInt(layout.data16[0x41], Initial, "ASCII value read through data16");
    else ExpectInt(layout.data32[0x41], Initial, "ASCII value read through data32");

    Expect(utrie2_isFrozen_nid_postfix(trie) != 0, "the trie is frozen");
    for (const std::int32_t codePoint : {0, 0x41, 0x7ff, 0xd800, 0xffff, 0x10000, 0x10ffff}) {
        ExpectInt(utrie2_get32_nid_postfix(trie, codePoint), Initial, "value in range");
    }
    ExpectInt(utrie2_get32_nid_postfix(trie, 0x110000), Error, "value above the code point range");
    ExpectInt(utrie2_get32_nid_postfix(trie, -1), Error, "value of a negative code point");
    ExpectInt(utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(trie, 0xd800), Initial, "lead surrogate code unit value");
    ExpectInt(utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(trie, 0x41), Error, "value of a code unit that is not a lead");

    Ranges ranges;
    ranges.stopAfter = 10;
    utrie2_enum_nid_postfix(trie, nullptr, Record, &ranges);
    ExpectInt(ranges.calls, 1, "one range over all code points");
    ExpectInt(ranges.start[0], 0, "range start");
    ExpectInt(ranges.end[0], 0x10ffff, "range end");
    ExpectInt(ranges.value[0], Initial, "range value");
    ranges = Ranges();
    ranges.stopAfter = 10;
    utrie2_enum_nid_postfix(trie, Double, Record, &ranges);
    ExpectInt(ranges.value[0], Initial * 2, "range value passes through the value function");
    ranges = Ranges();
    ranges.stopAfter = 10;
    utrie2_enumForLeadSurrogate_nid_postfix(trie, 0xd801, nullptr, Record, &ranges);
    ExpectInt(ranges.calls, 1, "one range for a lead surrogate");
    ExpectInt(ranges.start[0], 0x10400, "lead surrogate range start");
    ExpectInt(ranges.end[0], 0x107ff, "lead surrogate range end");
    ranges = Ranges();
    utrie2_enumForLeadSurrogate_nid_postfix(trie, 0x41, nullptr, Record, &ranges);
    ExpectInt(ranges.calls, 0, "no ranges for a code unit that is not a lead");
    utrie2_enum_nid_postfix(trie, Double, nullptr, &ranges);
    ExpectInt(ranges.calls, 0, "no ranges without a range function");

    const std::uint8_t text[] = {0x41, 0xc8, 0x90, 0xf0, 0xaa, 0xaf, 0x8d, 0x80};
    std::int32_t packed = utrie2_internalU8NextIndex_nid_postfix(trie, 0xc8, text + 2, text + 8);
    ExpectInt(packed & 7, 1, "trail bytes of a two-byte sequence");
    ExpectInt(Data(trie, packed >> 3), Initial, "next index of a two-byte sequence");
    packed = utrie2_internalU8NextIndex_nid_postfix(trie, 0xf0, text + 4, text + 8);
    ExpectInt(packed & 7, 3, "trail bytes of a four-byte sequence");
    ExpectInt(Data(trie, packed >> 3), Initial, "next index of a four-byte sequence");
    packed = utrie2_internalU8PrevIndex_nid_postfix(trie, 0x8d, text, text + 6);
    ExpectInt(packed & 7, 3, "bytes before the last trail byte of a four-byte sequence");
    ExpectInt(Data(trie, packed >> 3), Initial, "previous index of a four-byte sequence");
    packed = utrie2_internalU8PrevIndex_nid_postfix(trie, 0x80, text, text + 7);
    ExpectInt(packed & 7, 0, "bytes before a lone trail byte");
    ExpectInt(Data(trie, packed >> 3), Error, "previous index of a lone trail byte");
}

void TestTrie(std::int32_t bits) {
    std::int32_t error = Success;
    Icu::Trie2* trie = utrie2_openDummy_nid_postfix(bits, Initial, Error, &error);
    ExpectInt(error, Success, "openDummy status");
    Expect(trie != nullptr, "openDummy returns a trie");
    CheckTrie(trie, bits);

    const std::int32_t size = utrie2_serialize_nid_postfix(trie, nullptr, 0, &error);
    ExpectInt(error, BufferOverflow, "serialize preflight status");
    Expect(size > 16 && size % 4 == 0, "serialize preflight size");
    error = Success;
    std::vector<std::uint32_t> binary(static_cast<std::size_t>(size) / 4 + 1, 0xa5a5a5a5u);
    ExpectInt(utrie2_serialize_nid_postfix(trie, binary.data(), size, &error), size, "serialize size");
    ExpectInt(error, Success, "serialize status");
    ExpectInt(binary[0], 0x54726932u, "serialized signature");
    ExpectInt(binary.back(), 0xa5a5a5a5u, "serialize stays inside its capacity");
    ExpectInt(utrie2_serialize_nid_postfix(trie, binary.data(), -1, &error), 0, "serialize with a negative capacity");
    ExpectInt(error, IllegalArgument, "serialize negative capacity status");
    error = Success;
    ExpectInt(utrie2_serialize_nid_postfix(nullptr, binary.data(), size, &error), 0, "serialize of a null trie");
    ExpectInt(error, IllegalArgument, "serialize null trie status");
    error = Success;

    std::int32_t actual = 0;
    Icu::Trie2* loaded = utrie2_openFromSerialized_nid_postfix(bits, binary.data(), size + 4, &actual, &error);
    ExpectInt(error, Success, "openFromSerialized status");
    Expect(loaded != nullptr, "openFromSerialized returns a trie");
    ExpectInt(actual, size, "openFromSerialized actual length");
    CheckTrie(loaded, bits);
    utrie2_close_nid_postfix(loaded);

    Expect(utrie2_openFromSerialized_nid_postfix(bits == Bits16 ? Bits32 : Bits16, binary.data(), size, nullptr, &error) == nullptr,
           "openFromSerialized rejects another value width");
    ExpectInt(error, InvalidFormat, "another value width status");
    error = Success;
    Expect(utrie2_openFromSerialized_nid_postfix(bits, binary.data(), 8, nullptr, &error) == nullptr,
           "openFromSerialized rejects a short header");
    ExpectInt(error, InvalidFormat, "short header status");
    error = Success;
    Expect(utrie2_openFromSerialized_nid_postfix(bits, binary.data(), size - 4, nullptr, &error) == nullptr,
           "openFromSerialized rejects truncated data");
    ExpectInt(error, InvalidFormat, "truncated data status");
    error = Success;
    Expect(utrie2_openFromSerialized_nid_postfix(bits, binary.data(), 0, nullptr, &error) == nullptr,
           "openFromSerialized rejects an empty length");
    ExpectInt(error, IllegalArgument, "empty length status");
    Expect(utrie2_openFromSerialized_nid_postfix(bits, nullptr, size, nullptr, &error) == nullptr,
           "openFromSerialized returns null after a failure");
    Expect(utrie2_openDummy_nid_postfix(bits, 0, 0, &error) == nullptr, "openDummy returns null after a failure");
    ExpectInt(utrie2_serialize_nid_postfix(trie, binary.data(), size, &error), 0, "serialize returns 0 after a failure");
    utrie2_close_nid_postfix(trie);
}

void TestArguments() {
    std::int32_t error = Success;
    Expect(utrie2_openDummy_nid_postfix(2, 0, 0, &error) == nullptr, "openDummy rejects a value width");
    ExpectInt(error, IllegalArgument, "openDummy value width status");
    error = Success;
    Expect(utrie2_openDummy_nid_postfix(-1, 0, 0, &error) == nullptr, "openDummy rejects a negative value width");
    ExpectInt(error, IllegalArgument, "openDummy negative value width status");
    error = Success;

    const std::uint8_t byte = 0;
    ExpectInvalid([&] { utrie2_openDummy_nid_postfix(Bits16, 0, 0, nullptr); }, "openDummy with a null status");
    ExpectInvalid([&] { utrie2_openFromSerialized_nid_postfix(Bits16, &byte, 16, nullptr, nullptr); },
                  "openFromSerialized with a null status");
    ExpectInvalid([&] { utrie2_openFromSerialized_nid_postfix(Bits16, nullptr, 16, nullptr, &error); },
                  "openFromSerialized with null data");
    ExpectInvalid([&] { utrie2_serialize_nid_postfix(nullptr, nullptr, 0, nullptr); }, "serialize with a null status");
    ExpectInvalid([&] { utrie2_isFrozen_nid_postfix(nullptr); }, "isFrozen with a null trie");
    ExpectInvalid([&] { utrie2_get32_nid_postfix(nullptr, 0); }, "get32 with a null trie");
    ExpectInvalid([&] { utrie2_get32FromLeadSurrogateCodeUnit_nid_postfix(nullptr, 0xd800); },
                  "get32FromLeadSurrogateCodeUnit with a null trie");
    Ranges ranges;
    ExpectInvalid([&] { utrie2_enum_nid_postfix(nullptr, nullptr, Record, &ranges); }, "enum with a null trie");
    ExpectInvalid([&] { utrie2_enumForLeadSurrogate_nid_postfix(nullptr, 0xd800, nullptr, Record, &ranges); },
                  "enumForLeadSurrogate with a null trie");
    ExpectInvalid([&] { utrie2_internalU8NextIndex_nid_postfix(nullptr, 0xc8, &byte, &byte); }, "internalU8NextIndex with a null trie");
    ExpectInvalid([&] { utrie2_internalU8PrevIndex_nid_postfix(nullptr, 0x80, &byte, &byte); }, "internalU8PrevIndex with a null trie");
    utrie2_close_nid_postfix(nullptr);
}

}

int main() {
    TestTrie(Bits16);
    TestTrie(Bits32);
    TestArguments();
    return 0;
}
