#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

extern "C" {
extern std::uint64_t _ZNSt8__sce_v27num_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28numpunctIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v25ctypeIwE2idE_nid_postfix;
}

static std::size_t g_lastAllocation = 0;

void* APS5_VABI TestHeapAllocate(std::size_t bytes) {
    g_lastAllocation = bytes;
    return std::malloc(bytes);
}
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer != nullptr ? 0 : -1;
}

constexpr std::int32_t Uppercase = 0x4;
constexpr std::int32_t ShowBase = 0x8;
constexpr std::int32_t ShowPoint = 0x10;
constexpr std::int32_t ShowPos = 0x20;
constexpr std::int32_t Left = 0x40;
constexpr std::int32_t Internal = 0x100;
constexpr std::int32_t Hex = 0x800;
constexpr std::int32_t Scientific = 0x1000;
constexpr std::int32_t Fixed = 0x2000;
constexpr std::int32_t HexFloat = 0x3000;
constexpr std::int32_t BoolAlpha = 0x4000;

static unsigned failures = 0;

static std::string Printable(const std::u16string& text) {
    std::string result;
    char item[8];
    for (const char16_t character : text) {
        if (character >= 0x20 && character < 0x7f) {
            result.push_back(static_cast<char>(character));
        } else {
            std::snprintf(item, sizeof(item), "\\u%04x", static_cast<unsigned>(character));
            result += item;
        }
    }
    return result;
}

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

struct Sink {
    GuestLocale::Streambuf buffer{};
    GuestLocale::StreambufVtable vtable{};
    std::array<char16_t, 4096> storage{};
    char16_t* next = nullptr;
    char16_t* end = nullptr;
    std::int32_t count = 0;
    std::u16string overflowed;
    bool rejectOverflow = false;
};

static Sink* g_sink = nullptr;

static int APS5_VABI SinkOverflow(GuestLocale::Streambuf*, int character) {
    if (g_sink->rejectOverflow) return -1;
    g_sink->overflowed.push_back(static_cast<char16_t>(character));
    return character;
}

static void ResetSink(Sink& sink, std::int32_t room) {
    sink.vtable.overflow = SinkOverflow;
    sink.buffer.vtable = &sink.vtable;
    sink.next = sink.storage.data();
    sink.end = sink.storage.data() + sink.storage.size();
    sink.count = room;
    sink.buffer.putNext = reinterpret_cast<char**>(&sink.next);
    sink.buffer.putEnd = reinterpret_cast<char**>(&sink.end);
    sink.buffer.reservedCounts[2] = &sink.count;
    sink.overflowed.clear();
    sink.rejectOverflow = false;
    g_sink = &sink;
}

static std::u16string Written(const Sink& sink) {
    return std::u16string(sink.storage.data(), static_cast<const char16_t*>(sink.next)) + sink.overflowed;
}

using PutBool = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char16_t, unsigned char);
using PutSigned = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char16_t, std::int64_t);
using PutUnsigned = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char16_t, std::uint64_t);
using PutDouble = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char16_t, double);
using PutLongDouble = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char16_t, long double);
using PutPointer = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char16_t, const void*);

using CtypeIs = bool (APS5_VABI *)(const void*, short, char16_t);
using CtypeIsRange = const char16_t* (APS5_VABI *)(const void*, const char16_t*, const char16_t*, short*);
using CtypeScan = const char16_t* (APS5_VABI *)(const void*, short, const char16_t*, const char16_t*);
using CtypeCase = char16_t (APS5_VABI *)(const void*, char16_t);
using CtypeCaseRange = const char16_t* (APS5_VABI *)(const void*, char16_t*, const char16_t*);
using CtypeWiden = char16_t (APS5_VABI *)(const void*, char);
using CtypeWidenRange = const char* (APS5_VABI *)(const void*, const char*, const char*, char16_t*);
using CtypeNarrow = char (APS5_VABI *)(const void*, char16_t, char);
using CtypeNarrowRange = const char16_t* (APS5_VABI *)(const void*, const char16_t*, const char16_t*, char, char*);

struct WideString {
    union {
        char16_t buffer[8];
        char16_t* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

struct NarrowString {
    union {
        char buffer[16];
        char* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

using PunctChar = char16_t (APS5_VABI *)(const void*);
using PunctGrouping = NarrowString* (APS5_VABI *)(NarrowString*, const void*);
using PunctName = WideString* (APS5_VABI *)(WideString*, const void*);

template<typename T> static T Slot(const void* facet, int index) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(facet))[index]);
}

struct Bench {
    const void* facet;
    GuestLocale::SceV2IosBase stream;
    Sink sink;
};

static void Arm(Bench& bench, std::int32_t flags, std::int64_t width = 0, std::int64_t precision = 6) {
    bench.stream.flags = flags;
    bench.stream.width = width;
    bench.stream.precision = precision;
    ResetSink(bench.sink, static_cast<std::int32_t>(bench.sink.storage.size()));
}

static void Finish(Bench& bench, const std::string& name, const std::u16string& expected, GuestLocale::OutputIterator out) {
    Require(Written(bench.sink) == expected, name + ": wrote [" + Printable(Written(bench.sink)) + "], expected [" + Printable(expected) + "]");
    Require(!out.failed, name + ": iterator stays good");
    Require(out.buffer == &bench.sink.buffer, name + ": iterator keeps the buffer");
    Require(bench.stream.width == 0, name + ": width is consumed");
}

static void CheckLong(Bench& bench, const std::string& name, std::int32_t flags, std::int64_t width, char16_t fill, std::int64_t value, const std::u16string& expected) {
    Arm(bench, flags, width);
    Finish(bench, name, expected, Slot<PutSigned>(bench.facet, 5)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, fill, value));
}

static void CheckDouble(Bench& bench, const std::string& name, std::int32_t flags, std::int64_t width, std::int64_t precision, double value, const std::u16string& expected) {
    Arm(bench, flags, width, precision);
    Finish(bench, name, expected, Slot<PutDouble>(bench.facet, 9)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', value));
}

static void TestNumPut(Bench& bench, GuestLocale::WideNumpunctFacet* numpunct) {
    CheckLong(bench, "decimal", 0, 0, u'*', -42, u"-42");
    CheckLong(bench, "hex", Hex | ShowBase, 0, u'*', 255, u"0xff");
    CheckLong(bench, "hex upper", Hex | ShowBase | Uppercase, 0, u'*', 255, u"0XFF");
    CheckLong(bench, "showpos", ShowPos, 0, u'*', 42, u"+42");
    CheckLong(bench, "a wide fill", 0, 6, u'€', 42, u"€€€€42");
    CheckLong(bench, "left", Left, 6, u'_', 42, u"42____");
    CheckLong(bench, "internal sign", Internal, 6, u'_', -42, u"-___42");
    CheckLong(bench, "internal base", Internal | Hex | ShowBase, 8, u'_', 255, u"0x____ff");
    CheckLong(bench, "negative width", 0, -9, u'_', 42, u"42");

    Arm(bench, 0);
    Finish(bench, "unsigned long", u"18446744073709551615", Slot<PutUnsigned>(bench.facet, 6)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', std::numeric_limits<std::uint64_t>::max()));
    Arm(bench, 0);
    Finish(bench, "long long", u"-9223372036854775808", Slot<PutSigned>(bench.facet, 7)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', std::numeric_limits<std::int64_t>::min()));
    Arm(bench, Hex);
    Finish(bench, "unsigned long long", u"ffffffffffffffff", Slot<PutUnsigned>(bench.facet, 8)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', std::numeric_limits<std::uint64_t>::max()));

    Arm(bench, 0);
    Finish(bench, "bool number keeps the byte", u"2", Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', 2));
    Arm(bench, BoolAlpha, 7);
    Finish(bench, "bool true", u"***true", Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', 1));
    Arm(bench, BoolAlpha | Left, 7);
    Finish(bench, "bool false", u"false**", Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', 0));

    const std::uint16_t* savedFalse = numpunct->falsename;
    const std::uint16_t* savedTrue = numpunct->truename;
    static const char16_t longName[] = u"a name longer than the inline storage €";
    numpunct->truename = reinterpret_cast<const std::uint16_t*>(longName);
    numpunct->falsename = reinterpret_cast<const std::uint16_t*>(u"é");
    Arm(bench, BoolAlpha);
    Finish(bench, "a name on the heap", longName, Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', 1));
    Arm(bench, BoolAlpha);
    Finish(bench, "a wide name", u"é", Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', 0));
    static const std::u16string largeName(3000, u'\u20ac');
    numpunct->truename = reinterpret_cast<const std::uint16_t*>(largeName.c_str());
    Arm(bench, BoolAlpha);
    Finish(bench, "a name in a large allocation", largeName, Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', 1));
    numpunct->falsename = savedFalse;
    numpunct->truename = savedTrue;

    CheckDouble(bench, "general", 0, 0, 6, 1.5, u"1.5");
    CheckDouble(bench, "fixed", Fixed, 0, 2, 1.25, u"1.25");
    CheckDouble(bench, "scientific upper", Scientific | Uppercase, 0, 2, 1500.0, u"1.50E+03");
    CheckDouble(bench, "hex float", HexFloat, 0, 2, 1.5, u"0x1.8p+0");
    CheckDouble(bench, "hex float internal", HexFloat | Internal, 10, 2, -1.5, u"-0x*1.8p+0");
    CheckDouble(bench, "showpoint", ShowPoint, 0, 3, 2.0, u"2.00");
    CheckDouble(bench, "showpoint skips infinity", ShowPoint, 0, 3, std::numeric_limits<double>::infinity(), u"inf");
    CheckDouble(bench, "fixed ignores uppercase", Fixed | Uppercase, 0, 1, std::numeric_limits<double>::infinity(), u"inf");

    Arm(bench, Fixed, 0, 1);
    Finish(bench, "long double", u"1.5", Slot<PutLongDouble>(bench.facet, 10)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', 1.5L));

    Arm(bench, 0);
    auto pointerOut = Slot<PutPointer>(bench.facet, 11)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', &bench);
    Require(pointerOut.buffer == &bench.sink.buffer && !pointerOut.failed, "pointer iterator");
    char pointerText[64];
    const int pointerLength = std::snprintf(pointerText, sizeof(pointerText), "%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&bench)));
    Require(Written(bench.sink) == std::u16string(pointerText, pointerText + pointerLength), "a pointer prints as bare hex digits: " + Printable(Written(bench.sink)));

    const char* savedGrouping = numpunct->grouping;
    const std::uint16_t savedSeparator = numpunct->thousandsSep;
    const std::uint16_t savedPoint = numpunct->decimalPoint;
    numpunct->thousandsSep = 0x00b7;
    numpunct->decimalPoint = 0x066b;
    numpunct->grouping = "\3\2";
    CheckLong(bench, "grouping cycles the last size", 0, 0, u'*', 123456789, u"12·34·56·789");
    CheckLong(bench, "grouping keeps the sign", 0, 0, u'*', -1234, u"-1·234");
    CheckLong(bench, "grouping keeps the base", Hex | ShowBase, 0, u'*', 0x12345, u"0x12·345");
    CheckDouble(bench, "floating grouping and point", Fixed, 0, 1, 1234567.75, u"12·34·567٫8");
    CheckDouble(bench, "scientific point", Scientific, 0, 1, 1500.0, u"1٫5e+03");
    numpunct->grouping = "\1";
    CheckDouble(bench, "infinity has no groups", Fixed, 0, 1, std::numeric_limits<double>::infinity(), u"inf");
    CheckDouble(bench, "a finite value has groups", Fixed, 0, 0, 123.0, u"1·2·3");
    numpunct->grouping = savedGrouping;
    numpunct->thousandsSep = savedSeparator;
    numpunct->decimalPoint = savedPoint;

    Arm(bench, 0);
    bench.sink.count = 2;
    Slot<PutSigned>(bench.facet, 5)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', 12345);
    Require(std::u16string(bench.sink.storage.data(), bench.sink.next) == u"12" && bench.sink.overflowed == u"345" && bench.sink.count == 0, "the count stops direct writes at the limit");

    Arm(bench, 0, 3);
    bench.sink.next = nullptr;
    bench.sink.count = 100;
    Slot<PutSigned>(bench.facet, 5)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'€', 7);
    Require(bench.sink.overflowed == u"€€7" && bench.sink.count == 100, "overflow receives all sixteen bits of a character");

    Arm(bench, 0);
    bench.sink.count = 0;
    bench.sink.rejectOverflow = true;
    auto rejected = Slot<PutSigned>(bench.facet, 5)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, u'*', 7);
    Require(rejected.failed && rejected.buffer == &bench.sink.buffer, "a rejected overflow fails but keeps the buffer");

    Arm(bench, 0, 3);
    auto missing = Slot<PutSigned>(bench.facet, 5)(bench.facet, {false, nullptr}, &bench.stream, u'*', 7);
    Require(missing.failed && missing.buffer == nullptr && bench.stream.width == 0, "a null buffer fails and still consumes the width");

    Arm(bench, 0);
    auto sticky = Slot<PutSigned>(bench.facet, 5)(bench.facet, {true, &bench.sink.buffer}, &bench.stream, u'*', 7);
    Require(sticky.failed && Written(bench.sink) == u"7", "a failed iterator stays failed but keeps writing");
}

static char APS5_VABI NarrowToA(const void*, char16_t, char) {
    return 'A';
}

static bool APS5_VABI AlwaysIs(const void*, short, char16_t) {
    return true;
}

static void TestCtype(GuestLocale::Facet* facet) {
    const short* table = nullptr;
    std::memcpy(&table, reinterpret_cast<const char*>(facet) + 0x10, sizeof(table));
    Require(table != nullptr, "ctype<wchar_t> has a table");
    const short lowerMask = table['a'] & ~table['A'];
    Require(lowerMask != 0, "the table tells the cases apart");

    Require(Slot<CtypeIs>(facet, 4)(facet, lowerMask, u'a'), "is finds a lower-case letter");
    Require(!Slot<CtypeIs>(facet, 4)(facet, lowerMask, u'A'), "is rejects an upper-case letter");
    Require(Slot<CtypeIs>(facet, 4)(facet, static_cast<short>(table['a'] | table['1']), u'a'), "is needs one bit of the mask");

    static const char16_t text[] = u"Ab1";
    std::array<short, 3> masks{};
    Require(Slot<CtypeIsRange>(facet, 5)(facet, text, text + 3, masks.data()) == text + 3, "is over a range returns the end");
    Require(masks[0] == table['A'] && masks[1] == table['b'] && masks[2] == table['1'], "is over a range reads the table");

    Require(Slot<CtypeScan>(facet, 6)(facet, lowerMask, text, text + 3) == text + 1, "scan_is stops at the first match");
    Require(Slot<CtypeScan>(facet, 7)(facet, lowerMask, text + 1, text + 3) == text + 2, "scan_not stops at the first mismatch");
    Require(Slot<CtypeScan>(facet, 6)(facet, lowerMask, text + 2, text + 3) == text + 3, "scan_is without a match returns the end");
    Require(Slot<CtypeScan>(facet, 7)(facet, lowerMask, text + 1, text + 2) == text + 2, "scan_not without a mismatch returns the end");

    Require(Slot<CtypeCase>(facet, 8)(facet, u'Q') == u'q', "tolower");
    Require(Slot<CtypeCase>(facet, 10)(facet, u'q') == u'Q', "toupper");
    std::array<char16_t, 3> cases{u'A', u'b', u'1'};
    Require(Slot<CtypeCaseRange>(facet, 9)(facet, cases.data(), cases.data() + 3) == cases.data() + 3, "tolower over a range returns the end");
    Require(cases[0] == u'a' && cases[1] == u'b' && cases[2] == u'1', "tolower over a range");
    cases = {u'A', u'b', u'1'};
    Require(Slot<CtypeCaseRange>(facet, 11)(facet, cases.data(), cases.data() + 3) == cases.data() + 3, "toupper over a range returns the end");
    Require(cases[0] == u'A' && cases[1] == u'B' && cases[2] == u'1', "toupper over a range");

    Require(Slot<CtypeWiden>(facet, 12)(facet, 'x') == u'x', "widen");
    Require(Slot<CtypeWiden>(facet, 12)(facet, '\0') == 0, "widen of a null");
    Require(Slot<CtypeWiden>(facet, 12)(facet, '\xaf') == 0xffff, "widen of an invalid byte");
    static const char bytes[] = "h\xafi";
    std::array<char16_t, 3> widened{};
    Require(Slot<CtypeWidenRange>(facet, 13)(facet, bytes, bytes + 3, widened.data()) == bytes + 3, "widen over a range returns the end");
    Require(widened[0] == u'h' && widened[1] == 0xffff && widened[2] == u'i', "widen over a range");

    Require(Slot<CtypeNarrow>(facet, 14)(facet, u'x', '?') == 'x', "narrow");
    Require(Slot<CtypeNarrow>(facet, 14)(facet, 0xea77, '#') == '#', "narrow falls back");
    static const char16_t wide[] = {u'o', 0xea77, u'k'};
    std::array<char, 3> narrowed{};
    Require(Slot<CtypeNarrowRange>(facet, 15)(facet, wide, wide + 3, '?', narrowed.data()) == wide + 3, "narrow over a range returns the end");
    Require(narrowed[0] == 'o' && narrowed[1] == '?' && narrowed[2] == 'k', "narrow over a range");

    std::array<void*, 16> replaced{};
    std::memcpy(replaced.data(), *reinterpret_cast<void***>(facet), sizeof(replaced));
    replaced[14] = reinterpret_cast<void*>(&NarrowToA);
    void** saved = *reinterpret_cast<void***>(facet);
    *reinterpret_cast<void***>(facet) = replaced.data();
    Require(!Slot<CtypeIs>(facet, 4)(facet, lowerMask, u'b'), "is goes through the narrow virtual");
    replaced[14] = saved[14];
    replaced[4] = reinterpret_cast<void*>(&AlwaysIs);
    Require(Slot<CtypeScan>(facet, 6)(facet, lowerMask, text + 2, text + 3) == text + 2, "scan_is goes through the is virtual");
    Require(Slot<CtypeScan>(facet, 7)(facet, lowerMask, text, text + 3) == text + 3, "scan_not goes through the is virtual");
    *reinterpret_cast<void***>(facet) = saved;
}

static void TestNumpunct(GuestLocale::Facet* facet, GuestLocale::WideNumpunctFacet* numpunct) {
    numpunct->decimalPoint = 0x066b;
    numpunct->thousandsSep = 0x00b7;
    Require(Slot<PunctChar>(facet, 4)(facet) == 0x066b, "decimal_point");
    Require(Slot<PunctChar>(facet, 5)(facet) == 0x00b7, "thousands_sep");

    const char* savedGrouping = numpunct->grouping;
    numpunct->grouping = "\3\2";
    NarrowString grouping{};
    Require(Slot<PunctGrouping>(facet, 6)(&grouping, facet) == &grouping, "grouping returns its result");
    Require(grouping.size == 2 && grouping.capacity == 15 && std::memcmp(grouping.buffer, "\3\2", 3) == 0, "grouping is a narrow string");
    numpunct->grouping = savedGrouping;

    const std::uint16_t* savedFalse = numpunct->falsename;
    numpunct->falsename = reinterpret_cast<const std::uint16_t*>(u"sevenCh");
    WideString name{};
    Require(Slot<PunctName>(facet, 7)(&name, facet) == &name, "falsename returns its result");
    Require(name.size == 7 && name.capacity == 7 && std::u16string(name.buffer) == u"sevenCh", "a name of seven characters stays inline");

    const std::uint16_t* savedTrue = numpunct->truename;
    numpunct->truename = reinterpret_cast<const std::uint16_t*>(u"eightChr");
    WideString heap{};
    Require(Slot<PunctName>(facet, 8)(&heap, facet) == &heap, "truename returns its result");
    Require(heap.size == 8 && heap.capacity == 15 && std::u16string(heap.pointer) == u"eightChr", "a name of eight characters moves to the heap");
    Require(g_lastAllocation == 32, "a name allocates its capacity and the terminator in sixteen-bit units");
    _ZdlPv_nid_postfix(heap.pointer);
    numpunct->falsename = savedFalse;
    numpunct->truename = savedTrue;
}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&TestHeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&TestHeapFree);
    heapApi[2] = reinterpret_cast<void*>(&TestHeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&TestHeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&TestHeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&TestHeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&TestHeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    static const GuestLocale::SceV2Locinfo info{0, "C", "C"};
    auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    _ZNSt8__sce_v26locale7_Locimp9_MakewlocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(&info, 0x3f, locale, nullptr);
    auto* punctFacet = locale->facets[_ZNSt8__sce_v28numpunctIwE2idE_nid_postfix];
    auto* ctypeFacet = locale->facets[_ZNSt8__sce_v25ctypeIwE2idE_nid_postfix];
    auto* numpunct = reinterpret_cast<GuestLocale::WideNumpunctFacet*>(punctFacet);

    static Bench bench{};
    bench.facet = locale->facets[_ZNSt8__sce_v27num_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix];
    bench.stream.locale = &locale;
    Require(bench.facet != nullptr && numpunct != nullptr && ctypeFacet != nullptr, "_Makewloc installs num_put, numpunct and ctype");

    TestNumPut(bench, numpunct);
    TestCtype(ctypeFacet);
    TestNumpunct(punctFacet, numpunct);

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
