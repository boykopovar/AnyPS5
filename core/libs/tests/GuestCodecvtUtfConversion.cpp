#include "prx/libc/include/CodecvtUtf.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

template<typename TElement>
using InFunction = int (APS5_VABI*)(const void*, void*, const char*, const char*, const char**, TElement*, TElement*, TElement**);
template<typename TElement>
using OutFunction = int (APS5_VABI*)(const void*, void*, const TElement*, const TElement*, const TElement**, char*, char*, char**);
using LengthFunction = int (APS5_VABI*)(const void*, void*, const char*, const char*, std::size_t);

constexpr InFunction<char16_t> In16 = &_ZNKSt7codecvtIDsc9_MbstatetE5do_inERS0_PKcS4_RS4_PDsS6_RS6__nid_postfix;
constexpr OutFunction<char16_t> Out16 = &_ZNKSt7codecvtIDsc9_MbstatetE6do_outERS0_PKDsS4_RS4_PcS6_RS6__nid_postfix;
constexpr LengthFunction Length16 = &CodecvtUtf16Length_nid_no_patch;
constexpr InFunction<char32_t> In32 = &_ZNKSt7codecvtIDic9_MbstatetE5do_inERS0_PKcS4_RS4_PDiS6_RS6__nid_postfix;
constexpr OutFunction<char32_t> Out32 = &_ZNKSt7codecvtIDic9_MbstatetE6do_outERS0_PKDiS4_RS4_PcS6_RS6__nid_postfix;
constexpr LengthFunction Length32 = &_ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix;

static_assert(std::is_same_v<decltype(&_ZNKSt7codecvtIDsc9_MbstatetE5do_inERS0_PKcS4_RS4_PDsS6_RS6__nid_postfix), InFunction<char16_t>>);
static_assert(std::is_same_v<decltype(&_ZNKSt7codecvtIDsc9_MbstatetE6do_outERS0_PKDsS4_RS4_PcS6_RS6__nid_postfix), OutFunction<char16_t>>);
static_assert(std::is_same_v<decltype(&CodecvtUtf16Length_nid_no_patch), LengthFunction>);
static_assert(std::is_same_v<decltype(&_ZNKSt7codecvtIDic9_MbstatetE5do_inERS0_PKcS4_RS4_PDiS6_RS6__nid_postfix), InFunction<char32_t>>);
static_assert(std::is_same_v<decltype(&_ZNKSt7codecvtIDic9_MbstatetE6do_outERS0_PKDiS4_RS4_PcS6_RS6__nid_postfix), OutFunction<char32_t>>);
static_assert(std::is_same_v<decltype(&_ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix), LengthFunction>);

constexpr int Ok = 0;
constexpr int Partial = 1;
constexpr int Error = 2;
constexpr std::uint32_t GenerateHeader = 2;
constexpr std::uint32_t ConsumeHeader = 4;
constexpr std::uint64_t Utf16Maxcode = 0x10ffff;
constexpr std::uint64_t Utf32Maxcode = 0xffffffff;
constexpr std::size_t InSlot = 0x38 / sizeof(std::uintptr_t);

struct Facet {
    const std::uintptr_t* vtable;
    std::uint32_t references;
    std::uint32_t reserved;
    std::uint64_t maxcode;
    std::uint32_t mode;
    std::uint32_t padding;
};

static_assert(offsetof(Facet, maxcode) == 0x10);
static_assert(offsetof(Facet, mode) == 0x18);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "UTF codecvt conversion: %s\n", message);
    std::abort();
}

class TestFacet {
public:
    template<typename TElement>
    TestFacet(InFunction<TElement> in, std::uint64_t maxcode, std::uint32_t mode) {
        slots[InSlot] = reinterpret_cast<std::uintptr_t>(in);
        facet = {slots.data(), 1, 0, maxcode, mode, 0};
    }

    TestFacet(const TestFacet&) = delete;
    TestFacet& operator=(const TestFacet&) = delete;

    const Facet* Get() const { return &facet; }

private:
    std::array<std::uintptr_t, 11> slots{};
    Facet facet{};
};

std::string Bytes(std::initializer_list<unsigned> values) {
    std::string text;
    for (const unsigned value : values) text.push_back(static_cast<char>(value));
    return text;
}

template<typename TElement>
struct InResult {
    int result;
    std::ptrdiff_t consumed;
    std::basic_string<TElement> produced;
};

struct OutResult {
    int result;
    std::ptrdiff_t consumed;
    std::string produced;
};

template<typename TElement>
InResult<TElement> RunIn(InFunction<TElement> in, const TestFacet& facet, std::uint64_t* state, const std::string& bytes, std::size_t room) {
    const auto sentinel = static_cast<TElement>(0x7777);
    std::vector<TElement> output(room + 1, sentinel);
    const char* mid1 = nullptr;
    TElement* mid2 = nullptr;
    const int result = in(facet.Get(), state, bytes.data(), bytes.data() + bytes.size(), &mid1, output.data(), output.data() + room, &mid2);
    Require(output[room] == sentinel, "do_in writes nothing past the output end");
    Require(mid1 >= bytes.data() && mid1 <= bytes.data() + bytes.size(), "do_in leaves its input position inside the input");
    Require(mid2 >= output.data() && mid2 <= output.data() + room, "do_in leaves its output position inside the output");
    return {result, mid1 - bytes.data(), std::basic_string<TElement>(output.data(), mid2)};
}

template<typename TElement>
OutResult RunOut(OutFunction<TElement> out, const TestFacet& facet, std::uint64_t* state, const std::basic_string<TElement>& text, std::size_t room) {
    std::string output(room + 1, '\x77');
    const TElement* mid1 = nullptr;
    char* mid2 = nullptr;
    const int result = out(facet.Get(), state, text.data(), text.data() + text.size(), &mid1, output.data(), output.data() + room, &mid2);
    Require(output[room] == '\x77', "do_out writes nothing past the output end");
    Require(mid1 >= text.data() && mid1 <= text.data() + text.size(), "do_out leaves its input position inside the input");
    Require(mid2 >= output.data() && mid2 <= output.data() + room, "do_out leaves its output position inside the output");
    return {result, mid1 - text.data(), std::string(output.data(), mid2)};
}

std::uint16_t Pending(const std::uint64_t* state) {
    return static_cast<std::uint16_t>(state[0] & 0xffff);
}

unsigned Started(const std::uint64_t* state) {
    return static_cast<unsigned>(state[0] & 0xff);
}

int g_retries = 0;
const char* g_retryFirst = nullptr;

int APS5_VABI RecordIn16(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char16_t* first2, char16_t* last2, char16_t** mid2) {
    ++g_retries;
    g_retryFirst = first1;
    return In16(self, state, first1, last1, mid1, first2, last2, mid2);
}

int APS5_VABI RecordIn32(const void* self, void* state, const char* first1, const char* last1, const char** mid1, char32_t* first2, char32_t* last2, char32_t** mid2) {
    ++g_retries;
    g_retryFirst = first1;
    return In32(self, state, first1, last1, mid1, first2, last2, mid2);
}

void CheckUtf16InSequences() {
    const TestFacet facet(In16, Utf16Maxcode, 0);
    std::uint64_t state[2] = {0x1122334455660000, 0x8899aabbccddeeff};
    const auto run = RunIn(In16, facet, state, Bytes({0x41, 0xc3, 0xa9, 0xe2, 0x82, 0xac}), 8);
    Require(run.result == Ok && run.consumed == 6 && run.produced == std::u16string{0x41, 0xe9, 0x20ac}, "char16 do_in decodes one, two and three byte sequences");
    Require(state[0] == 0x1122334455660001 && state[1] == 0x8899aabbccddeeff, "char16 do_in keeps its state in the first two bytes");
}

void CheckUtf16InSurrogates() {
    const TestFacet facet(In16, Utf16Maxcode, 0);
    std::uint64_t state[2] = {};
    const auto whole = RunIn(In16, facet, state, Bytes({0xf0, 0x9f, 0x98, 0x80}), 2);
    Require(whole.result == Ok && whole.consumed == 4 && whole.produced == std::u16string{0xd83d, 0xde00} && Pending(state) == 1, "char16 do_in splits a four byte sequence into a surrogate pair");
    std::uint64_t split[2] = {};
    const auto first = RunIn(In16, facet, split, Bytes({0xf0, 0x9f, 0x98, 0x80}), 1);
    Require(first.result == Ok && first.consumed == 3 && first.produced == std::u16string{0xd83d} && Pending(split) == 0xde00, "char16 do_in leaves the last byte and the low surrogate in the state when one unit fits");
    const auto second = RunIn(In16, facet, split, Bytes({0x80}), 1);
    Require(second.result == Ok && second.consumed == 1 && second.produced == std::u16string{0xde00} && Pending(split) == 1, "char16 do_in completes the low surrogate from the state");
    const auto maximum = RunIn(In16, facet, state, Bytes({0xf4, 0x8f, 0xbf, 0xbf}), 2);
    Require(maximum.result == Ok && maximum.produced == std::u16string{0xdbff, 0xdfff}, "char16 do_in decodes U+10FFFF");
}

void CheckUtf16InErrors() {
    const TestFacet facet(In16, Utf16Maxcode, 0);
    std::uint64_t state[2] = {};
    const auto lead = RunIn(In16, facet, state, Bytes({0x80, 0x41}), 4);
    Require(lead.result == Error && lead.consumed == 1 && lead.produced.empty(), "char16 do_in steps over a continuation byte in lead position and fails");
    const auto trail = RunIn(In16, facet, state, Bytes({0xc3, 0x41}), 4);
    Require(trail.result == Error && trail.consumed == 1 && trail.produced.empty(), "char16 do_in stops at a byte that does not continue the sequence");
    std::uint64_t pending[2] = {0xde00, 0};
    const auto low = RunIn(In16, facet, pending, Bytes({0x41}), 4);
    Require(low.result == Error && low.consumed == 0 && low.produced.empty() && Pending(pending) == 0xde00, "char16 do_in needs a continuation byte while a low surrogate is pending");
}

void CheckUtf16InPartial() {
    const TestFacet facet(In16, Utf16Maxcode, 0);
    std::uint64_t state[2] = {};
    const auto truncated = RunIn(In16, facet, state, Bytes({0xe2, 0x82}), 4);
    Require(truncated.result == Partial && truncated.consumed == 0 && truncated.produced.empty(), "char16 do_in reports a truncated sequence as partial");
    const auto tail = RunIn(In16, facet, state, Bytes({0x41, 0xe2, 0x82}), 4);
    Require(tail.result == Partial && tail.consumed == 1 && tail.produced == std::u16string{0x41}, "char16 do_in reports partial when its last sequence is truncated, after converting the ones before");
    const auto empty = RunIn(In16, facet, state, std::string(), 4);
    Require(empty.result == Partial && empty.consumed == 0, "char16 do_in reports empty input as partial");
    const auto full = RunIn(In16, facet, state, Bytes({0x41}), 0);
    Require(full.result == Partial && full.consumed == 0, "char16 do_in reports an empty output as partial");
}

void CheckUtf16InMaxcode() {
    std::uint64_t state[2] = {};
    const TestFacet ascii(In16, 0x7f, 0);
    const auto small = RunIn(In16, ascii, state, Bytes({0xc3, 0xa9}), 4);
    Require(small.result == Error && small.consumed == 2 && small.produced.empty(), "char16 do_in rejects a value above the facet's maximum code");
    const TestFacet wide(In16, Utf32Maxcode, 0);
    const auto beyond = RunIn(In16, wide, state, Bytes({0xf4, 0x90, 0x80, 0x80}), 4);
    Require(beyond.result == Error && beyond.consumed == 3 && beyond.produced.empty(), "char16 do_in limits a larger maximum code to U+10FFFF");
}

void CheckUtf16InNoncanonical() {
    const TestFacet facet(In16, Utf16Maxcode, 0);
    std::uint64_t state[2] = {};
    const auto overlong = RunIn(In16, facet, state, Bytes({0xf8, 0x80, 0x80, 0x81, 0x81}), 4);
    Require(overlong.result == Ok && overlong.consumed == 5 && overlong.produced == std::u16string{0x41}, "char16 do_in accepts a five byte encoding of a small value");
    const auto truncated = RunIn(In16, facet, state, Bytes({0xf8, 0x80, 0x80, 0x81}), 4);
    Require(truncated.result == Partial && truncated.consumed == 0 && truncated.produced.empty(), "char16 do_in rolls back a noncanonical sequence that lacks its last byte");
}

void CheckUtf16InHeader() {
    const TestFacet consume(In16, Utf16Maxcode, ConsumeHeader);
    std::uint64_t state[2] = {};
    const auto dropped = RunIn(In16, consume, state, Bytes({0xef, 0xbb, 0xbf, 0x41}), 4);
    Require(dropped.result == Ok && dropped.consumed == 4 && dropped.produced == std::u16string{0x41} && Pending(state) == 1, "char16 do_in drops a leading byte order mark when it consumes headers");
    std::uint64_t alone[2] = {};
    const auto rolled = RunIn(In16, consume, alone, Bytes({0xef, 0xbb, 0xbf}), 4);
    Require(rolled.result == Partial && rolled.consumed == 0 && rolled.produced.empty() && Pending(alone) == 0, "char16 do_in rolls back a byte order mark with nothing after it");
    std::uint64_t later[2] = {1, 0};
    const auto kept = RunIn(In16, consume, later, Bytes({0xef, 0xbb, 0xbf}), 4);
    Require(kept.result == Ok && kept.produced == std::u16string{0xfeff}, "char16 do_in keeps a byte order mark after the first character");
    const TestFacet plain(In16, Utf16Maxcode, 0);
    std::uint64_t first[2] = {};
    const auto delivered = RunIn(In16, plain, first, Bytes({0xef, 0xbb, 0xbf}), 4);
    Require(delivered.result == Ok && delivered.produced == std::u16string{0xfeff}, "char16 do_in keeps a byte order mark when it does not consume headers");
}

void CheckUtf16InRetryIsVirtual() {
    const TestFacet facet(RecordIn16, Utf16Maxcode, ConsumeHeader);
    std::uint64_t state[2] = {};
    const std::string bytes = Bytes({0xef, 0xbb, 0xbf, 0x41});
    g_retries = 0;
    const auto run = RunIn(In16, facet, state, bytes, 4);
    Require(g_retries == 1 && g_retryFirst == bytes.data() + 3 && run.result == Ok && run.produced == std::u16string{0x41}, "char16 do_in retries after the byte order mark through the vtable's do_in slot");
}

void CheckUtf16OutSequences() {
    const TestFacet facet(In16, Utf16Maxcode, 0);
    std::uint64_t state[2] = {0x1122334455660000, 0};
    const auto run = RunOut(Out16, facet, state, std::u16string{0x41, 0xe9, 0x20ac, 0xdc00}, 16);
    Require(run.result == Ok && run.consumed == 4 && run.produced == Bytes({0x41, 0xc3, 0xa9, 0xe2, 0x82, 0xac, 0xed, 0xb0, 0x80}), "char16 do_out encodes one, two and three byte sequences, a lone low surrogate as itself");
    Require(state[0] == 0x1122334455660001, "char16 do_out keeps its state in the first two bytes");
}

void CheckUtf16OutSurrogates() {
    const TestFacet facet(In16, Utf16Maxcode, 0);
    std::uint64_t state[2] = {};
    const auto whole = RunOut(Out16, facet, state, std::u16string{0xd83d, 0xde00}, 8);
    Require(whole.result == Ok && whole.consumed == 2 && whole.produced == Bytes({0xf0, 0x9f, 0x98, 0x80}) && Pending(state) == 1, "char16 do_out encodes a surrogate pair as four bytes");
    std::uint64_t split[2] = {};
    const auto high = RunOut(Out16, facet, split, std::u16string{0xd83d}, 8);
    Require(high.result == Ok && high.consumed == 1 && high.produced == Bytes({0xf0}) && Pending(split) == 0x7d, "char16 do_out writes the lead byte of a high surrogate and keeps its bits in the state");
    const auto low = RunOut(Out16, facet, split, std::u16string{0xde00}, 8);
    Require(low.result == Ok && low.consumed == 1 && low.produced == Bytes({0x9f, 0x98, 0x80}) && Pending(split) == 1, "char16 do_out finishes the sequence from the low surrogate");
    std::uint64_t pending[2] = {0x7d, 0};
    const auto bad = RunOut(Out16, facet, pending, std::u16string{0x41}, 8);
    Require(bad.result == Error && bad.consumed == 0 && bad.produced.empty() && Pending(pending) == 0x7d, "char16 do_out needs a low surrogate after a high one");
}

void CheckUtf16OutRoom() {
    const TestFacet facet(In16, Utf16Maxcode, 0);
    std::uint64_t state[2] = {};
    const auto none = RunOut(Out16, facet, state, std::u16string{0xe9}, 1);
    Require(none.result == Partial && none.consumed == 0 && none.produced.empty(), "char16 do_out reports partial when the first character does not fit");
    const auto some = RunOut(Out16, facet, state, std::u16string{0x41, 0xe9}, 2);
    Require(some.result == Ok && some.consumed == 1 && some.produced == Bytes({0x41}), "char16 do_out reports ok after converting what fits");
}

void CheckUtf16OutHeader() {
    const TestFacet facet(In16, Utf16Maxcode, GenerateHeader);
    std::uint64_t state[2] = {};
    const auto cramped = RunOut(Out16, facet, state, std::u16string{0x41}, 3);
    Require(cramped.result == Partial && cramped.produced.empty() && Pending(state) == 0, "char16 do_out writes nothing when the header and the first character do not both fit");
    const auto headed = RunOut(Out16, facet, state, std::u16string{0x41}, 4);
    Require(headed.result == Ok && headed.produced == Bytes({0xef, 0xbb, 0xbf, 0x41}) && Pending(state) == 1, "char16 do_out generates a byte order mark before the first character");
    const auto later = RunOut(Out16, facet, state, std::u16string{0x42}, 4);
    Require(later.result == Ok && later.produced == Bytes({0x42}), "char16 do_out generates the header only once");
}

void CheckUtf32InSequences() {
    const TestFacet facet(In32, Utf32Maxcode, 0);
    std::uint64_t state[2] = {0x1122334455667700, 0};
    const auto run = RunIn(In32, facet, state, Bytes({0x41, 0xc3, 0xa9, 0xe2, 0x82, 0xac, 0xf0, 0x9f, 0x98, 0x80, 0xfd, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xff, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf}), 8);
    Require(run.result == Ok && run.consumed == 22 && run.produced == std::u32string{0x41, 0xe9, 0x20ac, 0x1f600, 0x7fffffff, 0xffffffff}, "char32 do_in decodes one to six byte sequences");
    Require(state[0] == 0x1122334455667701, "char32 do_in keeps its state in the first byte");
}

void CheckUtf32InErrors() {
    const TestFacet facet(In32, Utf32Maxcode, 0);
    std::uint64_t state[2] = {};
    const auto lead = RunIn(In32, facet, state, Bytes({0x80, 0x41}), 4);
    Require(lead.result == Error && lead.consumed == 1 && lead.produced.empty(), "char32 do_in steps over a continuation byte in lead position and fails");
    const auto trail = RunIn(In32, facet, state, Bytes({0xc3, 0x41}), 4);
    Require(trail.result == Error && trail.consumed == 1 && trail.produced.empty(), "char32 do_in stops at a byte that does not continue the sequence");
}

void CheckUtf32InPartial() {
    const TestFacet facet(In32, Utf32Maxcode, 0);
    std::uint64_t state[2] = {};
    const auto truncated = RunIn(In32, facet, state, Bytes({0xe2, 0x82}), 4);
    Require(truncated.result == Partial && truncated.consumed == 0 && truncated.produced.empty(), "char32 do_in reports a truncated sequence as partial");
    const auto tail = RunIn(In32, facet, state, Bytes({0x41, 0xe2, 0x82}), 4);
    Require(tail.result == Ok && tail.consumed == 1 && tail.produced == std::u32string{0x41}, "char32 do_in reports ok when it converted something before a truncated sequence");
}

void CheckUtf32InMaxcode() {
    std::uint64_t state[2] = {};
    const TestFacet unicode(In32, Utf16Maxcode, 0);
    const auto beyond = RunIn(In32, unicode, state, Bytes({0xf4, 0x90, 0x80, 0x80}), 4);
    Require(beyond.result == Error && beyond.consumed == 4 && beyond.produced.empty(), "char32 do_in rejects a value above the facet's maximum code after reading it");
    const TestFacet ascii(In32, 0x7f, ConsumeHeader);
    std::uint64_t first[2] = {};
    const auto header = RunIn(In32, ascii, first, Bytes({0xef, 0xbb, 0xbf, 0x41}), 4);
    Require(header.result == Ok && header.produced == std::u32string{0x41}, "char32 do_in drops the byte order mark before checking the maximum code");
    const TestFacet plain(In32, 0x7f, 0);
    std::uint64_t kept[2] = {};
    const auto rejected = RunIn(In32, plain, kept, Bytes({0xef, 0xbb, 0xbf, 0x41}), 4);
    Require(rejected.result == Error && rejected.consumed == 3, "char32 do_in rejects a kept byte order mark above the maximum code");
}

void CheckUtf32InHeader() {
    const TestFacet consume(In32, Utf32Maxcode, ConsumeHeader);
    std::uint64_t state[2] = {};
    const auto dropped = RunIn(In32, consume, state, Bytes({0xef, 0xbb, 0xbf, 0x41}), 4);
    Require(dropped.result == Ok && dropped.consumed == 4 && dropped.produced == std::u32string{0x41} && Started(state) == 1, "char32 do_in drops a leading byte order mark when it consumes headers");
    std::uint64_t alone[2] = {};
    const auto rolled = RunIn(In32, consume, alone, Bytes({0xef, 0xbb, 0xbf}), 4);
    Require(rolled.result == Partial && rolled.consumed == 0 && rolled.produced.empty() && Started(alone) == 0, "char32 do_in rolls back a byte order mark with nothing after it");
    std::uint64_t later[2] = {1, 0};
    const auto kept = RunIn(In32, consume, later, Bytes({0xef, 0xbb, 0xbf}), 4);
    Require(kept.result == Ok && kept.produced == std::u32string{0xfeff}, "char32 do_in keeps a byte order mark after the first character");
}

void CheckUtf32InRetryIsVirtual() {
    const TestFacet facet(RecordIn32, Utf32Maxcode, ConsumeHeader);
    std::uint64_t state[2] = {};
    const std::string bytes = Bytes({0xef, 0xbb, 0xbf, 0x41});
    g_retries = 0;
    const auto run = RunIn(In32, facet, state, bytes, 4);
    Require(g_retries == 1 && g_retryFirst == bytes.data() + 3 && run.result == Ok && run.produced == std::u32string{0x41}, "char32 do_in retries after the byte order mark through the vtable's do_in slot");
}

void CheckUtf32OutSequences() {
    const TestFacet facet(In32, Utf32Maxcode, 0);
    std::uint64_t state[2] = {0x1122334455667700, 0};
    const auto run = RunOut(Out32, facet, state, std::u32string{0x41, 0xe9, 0x20ac, 0x1f600, 0x200000, 0x7fffffff, 0xffffffff}, 40);
    Require(run.result == Ok && run.consumed == 7 && run.produced == Bytes({0x41, 0xc3, 0xa9, 0xe2, 0x82, 0xac, 0xf0, 0x9f, 0x98, 0x80, 0xf8, 0x88, 0x80, 0x80, 0x80, 0xfd, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xff, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf}), "char32 do_out encodes one to six byte sequences");
    Require(state[0] == 0x1122334455667701, "char32 do_out keeps its state in the first byte");
}

void CheckUtf32OutMaxcode() {
    const TestFacet facet(In32, Utf16Maxcode, 0);
    std::uint64_t state[2] = {};
    const auto rejected = RunOut(Out32, facet, state, std::u32string{0x41, 0x110000}, 8);
    Require(rejected.result == Error && rejected.consumed == 1 && rejected.produced == Bytes({0x41}), "char32 do_out rejects a value above the facet's maximum code");
}

void CheckUtf32OutRoom() {
    const TestFacet facet(In32, Utf32Maxcode, 0);
    std::uint64_t state[2] = {};
    const auto none = RunOut(Out32, facet, state, std::u32string{0xe9}, 1);
    Require(none.result == Partial && none.consumed == 0 && none.produced.empty(), "char32 do_out reports partial when the first character does not fit");
    const auto some = RunOut(Out32, facet, state, std::u32string{0x41, 0xe9}, 2);
    Require(some.result == Ok && some.consumed == 1 && some.produced == Bytes({0x41}), "char32 do_out reports ok after converting what fits");
}

void CheckUtf32OutHeader() {
    const TestFacet facet(In32, Utf32Maxcode, GenerateHeader);
    std::uint64_t state[2] = {};
    const auto cramped = RunOut(Out32, facet, state, std::u32string{0x41}, 3);
    Require(cramped.result == Partial && cramped.consumed == 0 && cramped.produced.empty() && Started(state) == 1, "char32 do_out gives up the header when it and the first character do not both fit");
    const auto after = RunOut(Out32, facet, state, std::u32string{0x41}, 3);
    Require(after.result == Ok && after.produced == Bytes({0x41}), "char32 do_out does not generate the header it gave up");
    std::uint64_t first[2] = {};
    const auto headed = RunOut(Out32, facet, first, std::u32string{0x41}, 4);
    Require(headed.result == Ok && headed.produced == Bytes({0xef, 0xbb, 0xbf, 0x41}) && Started(first) == 1, "char32 do_out generates a byte order mark before the first character");
}

int g_noconvCalls = 0;

int APS5_VABI NoConvIn16(const void*, void*, const char* first1, const char*, const char** mid1, char16_t* first2, char16_t*, char16_t** mid2) {
    ++g_noconvCalls;
    *mid1 = first1 + (first1 != nullptr ? 1 : 0);
    *mid2 = first2;
    return 3;
}

template<typename TCall>
void RequireInvalid(TCall call, const char* message) {
    bool threw = false;
    try {
        call();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, message);
}

class GuardedInput {
public:
    GuardedInput() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr, "guarded input allocation succeeds");
        DWORD previous = 0;
        Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "guarded input page protection succeeds");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "host page size is available");
        pageSize = static_cast<std::size_t>(size);
        void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED, "guarded input allocation succeeds");
        memory = static_cast<char*>(allocation);
        Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "guarded input page protection succeeds");
#endif
    }

    ~GuardedInput() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, pageSize * 2);
#endif
    }

    GuardedInput(const GuardedInput&) = delete;
    GuardedInput& operator=(const GuardedInput&) = delete;

    std::pair<const char*, const char*> Place(const std::string& bytes) {
        Require(bytes.size() <= pageSize, "guarded input fits inside one page");
        char* start = memory + pageSize - bytes.size();
        if (!bytes.empty()) std::memcpy(start, bytes.data(), bytes.size());
        return {start, start + bytes.size()};
    }

private:
    char* memory = nullptr;
    std::size_t pageSize = 0;
};

template<typename TElement>
void CheckLengthMatchesIn(LengthFunction length, InFunction<TElement> in, const TestFacet& facet, std::uint64_t initialState0, const std::string& bytes, std::size_t maxUnits, int expectedBytes, std::uint64_t expectedState0, const char* message) {
    std::uint64_t lenState[2] = {initialState0, 0x8899aabbccddeeffULL};
    std::uint64_t inState[2] = {initialState0, 0x8899aabbccddeeffULL};
    const int measured = length(facet.Get(), lenState, bytes.data(), bytes.data() + bytes.size(), maxUnits);
    const auto converted = RunIn(in, facet, inState, bytes, maxUnits);
    Require(measured == expectedBytes, message);
    Require(lenState[0] == expectedState0 && lenState[1] == 0x8899aabbccddeeffULL, message);
    if (converted.result != Error) {
        Require(static_cast<std::ptrdiff_t>(measured) == converted.consumed, message);
        Require(lenState[0] == inState[0], message);
    } else {
        Require(static_cast<std::ptrdiff_t>(measured) <= converted.consumed, message);
        Require(lenState[0] == inState[0], message);
        std::uint64_t prefixState[2] = {initialState0, 0x8899aabbccddeeffULL};
        const auto prefix = RunIn(in, facet, prefixState, bytes.substr(0, static_cast<std::size_t>(measured)), maxUnits);
        Require(prefix.result == (measured == 0 ? Partial : Ok), message);
        Require(prefix.consumed == measured && prefix.produced == converted.produced, message);
    }
}

void CheckUtf16Length() {
    const TestFacet plain(In16, Utf16Maxcode, 0);
    CheckLengthMatchesIn(Length16, In16, plain, 0, "abcde", 0, 0, 0, "char16 do_length with max=0 consumes 0 bytes and preserves state");
    CheckLengthMatchesIn(Length16, In16, plain, 0, "abcde", 1, 1, 1, "char16 do_length counts 1 ASCII unit");
    CheckLengthMatchesIn(Length16, In16, plain, 0, "abcde", 3, 3, 1, "char16 do_length stops at max ASCII units");
    CheckLengthMatchesIn(Length16, In16, plain, 0, "abcde", 100, 5, 1, "char16 do_length stops at input end");

    const std::string mixed = Bytes({0x41, 0xc3, 0xa9, 0xe2, 0x82, 0xac});
    CheckLengthMatchesIn(Length16, In16, plain, 0, mixed, 1, 1, 1, "char16 do_length multibyte max=1");
    CheckLengthMatchesIn(Length16, In16, plain, 0, mixed, 2, 3, 1, "char16 do_length multibyte max=2");
    CheckLengthMatchesIn(Length16, In16, plain, 0, mixed, 3, 6, 1, "char16 do_length multibyte max=3");

    const std::string supplementary = Bytes({0xf0, 0x9f, 0x98, 0x80});
    std::uint64_t splitState[2] = {0, 0};
    Require(Length16(plain.Get(), splitState, supplementary.data(), supplementary.data() + supplementary.size(), 1) == 3, "char16 do_length supplementary max=1 consumes 3 lead bytes");
    Require(Pending(splitState) == 0xde00, "char16 do_length supplementary max=1 saves low surrogate in state");
    Require(Length16(plain.Get(), splitState, supplementary.data() + 3, supplementary.data() + 4, 1) == 1, "char16 do_length completes low surrogate from state");
    Require(Pending(splitState) == 1, "char16 do_length clears pending surrogate to 1");
    CheckLengthMatchesIn(Length16, In16, plain, 0, supplementary, 2, 4, 1, "char16 do_length supplementary max=2 consumes all 4 bytes");

    const std::string withSupp = Bytes({0x41, 0xf0, 0x9f, 0x98, 0x80, 0x5a});
    CheckLengthMatchesIn(Length16, In16, plain, 0, withSupp, 2, 4, 0xde00, "char16 do_length stops after high surrogate when budget is 2");
    CheckLengthMatchesIn(Length16, In16, plain, 0, withSupp, 3, 5, 1, "char16 do_length finishes surrogate pair when budget is 3");
    CheckLengthMatchesIn(Length16, In16, plain, 0, withSupp, 4, 6, 1, "char16 do_length includes trailing ASCII when budget is 4");

    CheckLengthMatchesIn(Length16, In16, plain, 0, "", 5, 0, 0, "char16 do_length empty input returns 0");
    std::uint64_t nullState[2] = {};
    Require(Length16(plain.Get(), nullState, nullptr, nullptr, 5) == 0 && Pending(nullState) == 0, "char16 do_length accepts null empty range");

    CheckLengthMatchesIn(Length16, In16, plain, 0, Bytes({0xe2, 0x82}), 4, 0, 0, "char16 do_length stops at truncated sequence");
    CheckLengthMatchesIn(Length16, In16, plain, 0, Bytes({0x41, 0xe2, 0x82}), 4, 1, 1, "char16 do_length counts prefix before truncated sequence");
    CheckLengthMatchesIn(Length16, In16, plain, 0, Bytes({0x80, 0x41}), 4, 0, 0, "char16 do_length stops on invalid lead byte");
    CheckLengthMatchesIn(Length16, In16, plain, 0, Bytes({0x41, 0x80, 0x42}), 4, 1, 1, "char16 do_length stops on invalid mid-stream byte");
    CheckLengthMatchesIn(Length16, In16, plain, 0, Bytes({0xc3, 0x41}), 4, 0, 0, "char16 do_length stops on invalid continuation byte");
    CheckLengthMatchesIn(Length16, In16, plain, 0xde00, Bytes({0x41}), 4, 0, 0xde00, "char16 do_length requires continuation byte when low surrogate is pending");
    CheckLengthMatchesIn(Length16, In16, plain, 0, Bytes({0xf0, 0x9f, 0x98, 0x41}), 2, 3, 0xde00, "char16 do_length stops before invalid surrogate tail byte");

    const std::string noncanonical = Bytes({0xf8, 0x80, 0x80, 0x81, 0x81});
    CheckLengthMatchesIn(Length16, In16, plain, 0, noncanonical, 1, 5, 1, "char16 do_length accepts legacy 5-byte sequence");
    CheckLengthMatchesIn(Length16, In16, plain, 0, noncanonical.substr(0, 4), 1, 0, 0, "char16 do_length stops on truncated 5-byte sequence");

    const TestFacet asciiMax(In16, 0x7f, 0);
    CheckLengthMatchesIn(Length16, In16, asciiMax, 0, Bytes({0x41, 0xc3, 0xa9}), 4, 1, 1, "char16 do_length stops at maxcode");
    const TestFacet wideMax(In16, Utf32Maxcode, 0);
    CheckLengthMatchesIn(Length16, In16, wideMax, 0, Bytes({0xf4, 0x90, 0x80, 0x80}), 4, 0, 0, "char16 do_length clamps maxcode to U+10FFFF");

    const TestFacet consume(In16, Utf16Maxcode, ConsumeHeader);
    CheckLengthMatchesIn(Length16, In16, consume, 0, Bytes({0xef, 0xbb, 0xbf, 0x41}), 0, 0, 0, "char16 do_length max=0 does not consume BOM");
    CheckLengthMatchesIn(Length16, In16, plain, 0xde00, Bytes({0x80, 0x41}), 0, 0, 0xde00, "char16 do_length max=0 preserves non-zero state");
    CheckLengthMatchesIn(Length16, In16, consume, 0, Bytes({0xef, 0xbb, 0xbf, 0x80}), 1, 0, 1, "char16 do_length stops cleanly on BOM followed by invalid byte");
    CheckLengthMatchesIn(Length16, In16, consume, 0, Bytes({0xef, 0xbb, 0xbf, 0x41}), 1, 4, 1, "char16 do_length consumes BOM and first unit when state=0");
    CheckLengthMatchesIn(Length16, In16, consume, 0, Bytes({0xef, 0xbb, 0xbf}), 1, 0, 0, "char16 do_length rolls back BOM alone");
    CheckLengthMatchesIn(Length16, In16, consume, 0, Bytes({0xef, 0xbb, 0xbf, 0xe2, 0x82}), 1, 0, 0, "char16 do_length rolls back BOM before truncated char");
    CheckLengthMatchesIn(Length16, In16, consume, 1, Bytes({0xef, 0xbb, 0xbf, 0x41}), 1, 3, 1, "char16 do_length keeps BOM as 0xfeff when state is non-initial");
    CheckLengthMatchesIn(Length16, In16, consume, 1, Bytes({0xef, 0xbb, 0xbf, 0x41}), 2, 4, 1, "char16 do_length counts BOM + A as 2 units when state is non-initial");
    CheckLengthMatchesIn(Length16, In16, plain, 0, Bytes({0xef, 0xbb, 0xbf, 0x41}), 1, 3, 1, "char16 do_length keeps BOM without ConsumeHeader");

    const TestFacet virtualRetry(RecordIn16, Utf16Maxcode, ConsumeHeader);
    const std::string bomBytes = Bytes({0xef, 0xbb, 0xbf, 0x41});
    std::uint64_t retryState[2] = {};
    g_retries = 0;
    Require(Length16(virtualRetry.Get(), retryState, bomBytes.data(), bomBytes.data() + bomBytes.size(), 1) == 4, "char16 do_length retries BOM through vtable do_in");
    Require(g_retries == 1 && g_retryFirst == bomBytes.data() + 3, "char16 do_length BOM retry passes post-BOM pointer");

    std::uint64_t largeBudgetState16[2] = {};
    Require(Length16(plain.Get(), largeBudgetState16, mixed.data(), mixed.data() + mixed.size(), std::numeric_limits<std::size_t>::max()) == 6 && Pending(largeBudgetState16) == 1, "char16 do_length handles SIZE_MAX budget without allocation");

    const TestFacet noconvFacet(NoConvIn16, Utf16Maxcode, ConsumeHeader);
    const std::string noconvBytes = Bytes({0xef, 0xbb, 0xbf, 'a', 'b', 'c', 'd', 'e', 'f'});
    std::uint64_t noconvState[2] = {};
    g_noconvCalls = 0;
    Require(Length16(noconvFacet.Get(), noconvState, noconvBytes.data(), noconvBytes.data() + noconvBytes.size(), 4) == 4, "char16 do_length handles noconv up to max from original cursor");
    noconvState[0] = 0;
    Require(Length16(noconvFacet.Get(), noconvState, noconvBytes.data(), noconvBytes.data() + noconvBytes.size(), 10) == 9, "char16 do_length handles noconv up to last1 from original cursor");
    Require(g_noconvCalls == 2, "char16 do_length reaches noconv override through BOM retry");

    GuardedInput guarded;
    const auto [guardStart, guardEnd] = guarded.Place(Bytes({0xf0, 0x9f, 0x98, 0x80, 0x41}));
    std::uint64_t guardState[2] = {};
    Require(Length16(plain.Get(), guardState, guardStart, guardEnd, 10) == 5 && Pending(guardState) == 1, "char16 do_length does not read past last1");

    RequireInvalid([&] { Length16(nullptr, guardState, guardStart, guardEnd, 1); }, "char16 do_length rejects null facet");
    RequireInvalid([&] { Length16(plain.Get(), nullptr, guardStart, guardEnd, 1); }, "char16 do_length rejects null state");
    RequireInvalid([&] { Length16(plain.Get(), guardState, nullptr, guardEnd, 1); }, "char16 do_length rejects null first with non-null last");
    RequireInvalid([&] { Length16(plain.Get(), guardState, guardStart, nullptr, 1); }, "char16 do_length rejects non-null first with null last");
    RequireInvalid([&] { Length16(plain.Get(), guardState, guardEnd, guardStart, 1); }, "char16 do_length rejects reversed range");
}

void CheckUtf32Length() {
    const TestFacet plain(In32, Utf32Maxcode, 0);
    CheckLengthMatchesIn(Length32, In32, plain, 0, "abcde", 0, 0, 0, "char32 do_length with max=0 consumes 0 bytes and preserves state");
    CheckLengthMatchesIn(Length32, In32, plain, 0, "abcde", 1, 1, 1, "char32 do_length counts 1 ASCII unit");
    CheckLengthMatchesIn(Length32, In32, plain, 0, "abcde", 3, 3, 1, "char32 do_length stops at max ASCII units");
    CheckLengthMatchesIn(Length32, In32, plain, 0, "abcde", 100, 5, 1, "char32 do_length stops at input end");

    const std::string multi = Bytes({0x41, 0xc3, 0xa9, 0xe2, 0x82, 0xac, 0xf0, 0x9f, 0x98, 0x80, 0xfd, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xff, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf});
    CheckLengthMatchesIn(Length32, In32, plain, 0, multi, 1, 1, 1, "char32 do_length max=1");
    CheckLengthMatchesIn(Length32, In32, plain, 0, multi, 2, 3, 1, "char32 do_length max=2");
    CheckLengthMatchesIn(Length32, In32, plain, 0, multi, 3, 6, 1, "char32 do_length max=3");
    CheckLengthMatchesIn(Length32, In32, plain, 0, multi, 4, 10, 1, "char32 do_length supplementary max=4 consumes 4-byte sequence as 1 unit");
    CheckLengthMatchesIn(Length32, In32, plain, 0, multi, 6, 22, 1, "char32 do_length decodes 1..6 byte sequences");

    const std::string supplementary = Bytes({0xf0, 0x9f, 0x98, 0x80});
    CheckLengthMatchesIn(Length32, In32, plain, 0, supplementary, 1, 4, 1, "char32 do_length supplementary budget 1 consumes all 4 bytes");

    CheckLengthMatchesIn(Length32, In32, plain, 0, "", 5, 0, 0, "char32 do_length empty input returns 0");
    std::uint64_t nullState[2] = {};
    Require(Length32(plain.Get(), nullState, nullptr, nullptr, 5) == 0 && Started(nullState) == 0, "char32 do_length accepts null empty range");

    CheckLengthMatchesIn(Length32, In32, plain, 0, Bytes({0xe2, 0x82}), 4, 0, 0, "char32 do_length stops at truncated sequence");
    CheckLengthMatchesIn(Length32, In32, plain, 0, Bytes({0x41, 0xe2, 0x82}), 4, 1, 1, "char32 do_length counts prefix before truncated sequence");
    CheckLengthMatchesIn(Length32, In32, plain, 0, Bytes({0x80, 0x41}), 4, 0, 0, "char32 do_length stops on invalid lead byte");
    CheckLengthMatchesIn(Length32, In32, plain, 0, Bytes({0x41, 0xc3, 0x42}), 4, 1, 1, "char32 do_length stops on invalid continuation byte");

    const TestFacet unicodeMax(In32, Utf16Maxcode, 0);
    CheckLengthMatchesIn(Length32, In32, unicodeMax, 0, Bytes({0xf4, 0x90, 0x80, 0x80}), 4, 0, 1, "char32 do_length stops at maxcode=0x10ffff");
    const TestFacet asciiConsume(In32, 0x7f, ConsumeHeader);
    CheckLengthMatchesIn(Length32, In32, asciiConsume, 0, Bytes({0xef, 0xbb, 0xbf, 0x41}), 1, 4, 1, "char32 do_length drops BOM before checking maxcode");
    const TestFacet asciiPlain(In32, 0x7f, 0);
    CheckLengthMatchesIn(Length32, In32, asciiPlain, 0, Bytes({0xef, 0xbb, 0xbf, 0x41}), 1, 0, 1, "char32 do_length rejects kept BOM above maxcode");

    const TestFacet consume(In32, Utf32Maxcode, ConsumeHeader);
    CheckLengthMatchesIn(Length32, In32, consume, 0, Bytes({0xef, 0xbb, 0xbf, 0x41}), 0, 0, 0, "char32 do_length max=0 preserves BOM state");
    CheckLengthMatchesIn(Length32, In32, plain, 1, Bytes({0xef, 0xbb, 0xbf, 0x41}), 0, 0, 1, "char32 do_length max=0 preserves non-zero state");
    CheckLengthMatchesIn(Length32, In32, consume, 0, Bytes({0xef, 0xbb, 0xbf, 0x80}), 1, 0, 1, "char32 do_length stops cleanly on BOM followed by invalid byte");
    CheckLengthMatchesIn(Length32, In32, consume, 0, Bytes({0xef, 0xbb, 0xbf}), 1, 0, 0, "char32 do_length rolls back BOM alone");
    CheckLengthMatchesIn(Length32, In32, consume, 1, Bytes({0xef, 0xbb, 0xbf, 0x41}), 1, 3, 1, "char32 do_length keeps BOM when state is non-initial");
    CheckLengthMatchesIn(Length32, In32, consume, 1, Bytes({0xef, 0xbb, 0xbf, 0x41}), 2, 4, 1, "char32 do_length consumes BOM + A when state is non-initial and max=2");

    const TestFacet virtualRetry(RecordIn32, Utf32Maxcode, ConsumeHeader);
    const std::string bomBytes = Bytes({0xef, 0xbb, 0xbf, 0x41});
    std::uint64_t retryState[2] = {};
    g_retries = 0;
    Require(Length32(virtualRetry.Get(), retryState, bomBytes.data(), bomBytes.data() + bomBytes.size(), 1) == 4, "char32 do_length retries BOM through vtable do_in");
    Require(g_retries == 1 && g_retryFirst == bomBytes.data() + 3, "char32 do_length BOM retry passes post-BOM pointer");

    std::uint64_t largeBudgetState32[2] = {};
    Require(Length32(plain.Get(), largeBudgetState32, multi.data(), multi.data() + multi.size(), std::numeric_limits<std::size_t>::max()) == 22 && Started(largeBudgetState32) == 1, "char32 do_length handles SIZE_MAX budget without allocation");

    GuardedInput guarded;
    const auto [guardStart, guardEnd] = guarded.Place(Bytes({0xf0, 0x9f, 0x98, 0x80, 0x41}));
    std::uint64_t guardState[2] = {};
    Require(Length32(plain.Get(), guardState, guardStart, guardEnd, 10) == 5 && Started(guardState) == 1, "char32 do_length does not read past last1");

    RequireInvalid([&] { Length32(nullptr, guardState, guardStart, guardEnd, 1); }, "char32 do_length rejects null facet");
    RequireInvalid([&] { Length32(plain.Get(), nullptr, guardStart, guardEnd, 1); }, "char32 do_length rejects null state");
    RequireInvalid([&] { Length32(plain.Get(), guardState, nullptr, guardEnd, 1); }, "char32 do_length rejects null first with non-null last");
    RequireInvalid([&] { Length32(plain.Get(), guardState, guardStart, nullptr, 1); }, "char32 do_length rejects non-null first with null last");
    RequireInvalid([&] { Length32(plain.Get(), guardState, guardEnd, guardStart, 1); }, "char32 do_length rejects reversed range");
}

}

int main() {
    CheckUtf16InSequences();
    CheckUtf16InSurrogates();
    CheckUtf16InErrors();
    CheckUtf16InPartial();
    CheckUtf16InMaxcode();
    CheckUtf16InNoncanonical();
    CheckUtf16InHeader();
    CheckUtf16InRetryIsVirtual();
    CheckUtf16OutSequences();
    CheckUtf16OutSurrogates();
    CheckUtf16OutRoom();
    CheckUtf16OutHeader();
    CheckUtf32InSequences();
    CheckUtf32InErrors();
    CheckUtf32InPartial();
    CheckUtf32InMaxcode();
    CheckUtf32InHeader();
    CheckUtf32InRetryIsVirtual();
    CheckUtf32OutSequences();
    CheckUtf32OutMaxcode();
    CheckUtf32OutRoom();
    CheckUtf32OutHeader();
    CheckUtf16Length();
    CheckUtf32Length();
    return 0;
}
