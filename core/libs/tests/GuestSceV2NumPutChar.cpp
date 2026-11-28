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
extern std::uint64_t _ZNSt8__sce_v27num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28numpunctIcE2idE_nid_postfix;
}

void* APS5_VABI TestHeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
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
constexpr std::int32_t Right = 0x80;
constexpr std::int32_t Internal = 0x100;
constexpr std::int32_t Oct = 0x400;
constexpr std::int32_t Hex = 0x800;
constexpr std::int32_t Scientific = 0x1000;
constexpr std::int32_t Fixed = 0x2000;
constexpr std::int32_t HexFloat = 0x3000;
constexpr std::int32_t BoolAlpha = 0x4000;

static unsigned failures = 0;

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

struct Sink {
    GuestLocale::Streambuf buffer{};
    GuestLocale::StreambufVtable vtable{};
    std::array<char, 8192> storage{};
    char* next = nullptr;
    char* end = nullptr;
    std::int32_t count = 0;
    std::string overflowed;
    bool rejectOverflow = false;
};

static Sink* g_sink = nullptr;

static int APS5_VABI SinkOverflow(GuestLocale::Streambuf*, int character) {
    if (g_sink->rejectOverflow) return -1;
    g_sink->overflowed.push_back(static_cast<char>(character));
    return character;
}

static void ResetSink(Sink& sink, std::int32_t room) {
    sink.vtable.overflow = SinkOverflow;
    sink.buffer.vtable = &sink.vtable;
    sink.next = sink.storage.data();
    sink.end = sink.storage.data() + sink.storage.size();
    sink.count = room;
    sink.buffer.putNext = &sink.next;
    sink.buffer.putEnd = &sink.end;
    sink.buffer.reservedCounts[2] = &sink.count;
    sink.overflowed.clear();
    sink.rejectOverflow = false;
    g_sink = &sink;
}

static std::string Written(const Sink& sink) {
    return std::string(sink.storage.data(), static_cast<const char*>(sink.next)) + sink.overflowed;
}

using PutBool = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char, unsigned char);
using PutSigned = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char, std::int64_t);
using PutUnsigned = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char, std::uint64_t);
using PutDouble = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char, double);
using PutLongDouble = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char, long double);
using PutPointer = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, GuestLocale::SceV2IosBase*, char, const void*);

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

static void Finish(Bench& bench, const std::string& name, const std::string& expected, GuestLocale::OutputIterator out) {
    Require(Written(bench.sink) == expected, name + ": wrote [" + Written(bench.sink) + "], expected [" + expected + "]");
    Require(!out.failed, name + ": iterator stays good");
    Require(out.buffer == &bench.sink.buffer, name + ": iterator keeps the buffer");
    Require(bench.stream.width == 0, name + ": width is consumed");
}

static void CheckLong(Bench& bench, const std::string& name, std::int32_t flags, std::int64_t width, char fill, std::int64_t value, const std::string& expected) {
    Arm(bench, flags, width);
    Finish(bench, name, expected, Slot<PutSigned>(bench.facet, 5)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, fill, value));
}

static void CheckDouble(Bench& bench, const std::string& name, std::int32_t flags, std::int64_t width, std::int64_t precision, double value, const std::string& expected) {
    Arm(bench, flags, width, precision);
    Finish(bench, name, expected, Slot<PutDouble>(bench.facet, 9)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', value));
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
    _ZNSt8__sce_v26locale7_Locimp8_MakelocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(&info, 0x3f, locale, nullptr);
    auto* numpunct = reinterpret_cast<GuestLocale::NumpunctFacet*>(locale->facets[_ZNSt8__sce_v28numpunctIcE2idE_nid_postfix]);

    static Bench bench{};
    bench.facet = locale->facets[_ZNSt8__sce_v27num_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix];
    bench.stream.locale = &locale;
    Require(bench.facet != nullptr && numpunct != nullptr, "_Makeloc installs num_put and numpunct");

    CheckLong(bench, "decimal", 0, 0, '*', -42, "-42");
    CheckLong(bench, "showpos", ShowPos, 0, '*', 42, "+42");
    CheckLong(bench, "octal", Oct | ShowBase, 0, '*', 8, "010");
    CheckLong(bench, "hex", Hex | ShowBase, 0, '*', 255, "0xff");
    CheckLong(bench, "hex upper", Hex | ShowBase | Uppercase, 0, '*', 255, "0XFF");
    CheckLong(bench, "right", Right, 6, '_', 42, "____42");
    CheckLong(bench, "no adjust", 0, 6, '_', 42, "____42");
    CheckLong(bench, "left", Left, 6, '_', 42, "42____");
    CheckLong(bench, "internal sign", Internal, 6, '_', -42, "-___42");
    CheckLong(bench, "internal base", Internal | Hex | ShowBase, 8, '_', 255, "0x____ff");
    CheckLong(bench, "narrow width", 0, 2, '_', 12345, "12345");
    CheckLong(bench, "negative width", 0, -9, '_', 42, "42");

    Arm(bench, 0);
    Finish(bench, "unsigned long", "18446744073709551615", Slot<PutUnsigned>(bench.facet, 6)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', std::numeric_limits<std::uint64_t>::max()));
    Arm(bench, 0);
    Finish(bench, "long long", "-9223372036854775808", Slot<PutSigned>(bench.facet, 7)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', std::numeric_limits<std::int64_t>::min()));
    Arm(bench, Hex);
    Finish(bench, "unsigned long long", "ffffffffffffffff", Slot<PutUnsigned>(bench.facet, 8)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', std::numeric_limits<std::uint64_t>::max()));

    Arm(bench, 0);
    Finish(bench, "bool number", "1", Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', 1));
    Arm(bench, 0);
    Finish(bench, "bool number keeps the byte", "2", Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', 2));
    Arm(bench, BoolAlpha, 7);
    Finish(bench, "bool true", "***true", Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', 1));
    Arm(bench, BoolAlpha | Left, 7);
    Finish(bench, "bool false", "false**", Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', 0));
    Arm(bench, BoolAlpha | Internal, 7);
    Finish(bench, "bool internal", "***true", Slot<PutBool>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', 1));

    CheckDouble(bench, "general", 0, 0, 6, 1.5, "1.5");
    CheckDouble(bench, "general zero precision", 0, 0, 0, 1234.5, "1e+03");
    CheckDouble(bench, "negative precision", Fixed, 0, -3, 1.5, "1.500000");
    CheckDouble(bench, "fixed", Fixed, 0, 2, 1.005, "1.00");
    CheckDouble(bench, "scientific", Scientific, 0, 2, 1500.0, "1.50e+03");
    CheckDouble(bench, "scientific upper", Scientific | Uppercase, 0, 2, 1500.0, "1.50E+03");
    CheckDouble(bench, "fixed ignores uppercase", Fixed | Uppercase, 0, 1, std::numeric_limits<double>::infinity(), "inf");
    CheckDouble(bench, "hex float", HexFloat, 0, 2, 1.5, "0x1.8p+0");
    CheckDouble(bench, "hex float upper", HexFloat | Uppercase, 0, 2, 1.5, "0X1.8P+0");
    CheckDouble(bench, "hex float internal", HexFloat | Internal, 10, 2, -1.5, "-0x*1.8p+0");
    CheckDouble(bench, "showpoint", ShowPoint, 0, 3, 2.0, "2.00");
    CheckDouble(bench, "showpoint skips infinity", ShowPoint, 0, 3, std::numeric_limits<double>::infinity(), "inf");
    CheckDouble(bench, "showpos", ShowPos, 0, 3, 2.0, "+2");
    CheckDouble(bench, "internal", Internal, 7, 3, -2.5, "-***2.5");

    Arm(bench, Fixed, 0, 1);
    Finish(bench, "long double", "1.5", Slot<PutLongDouble>(bench.facet, 10)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', 1.5L));

    Arm(bench, 0);
    auto pointerOut = Slot<PutPointer>(bench.facet, 11)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', &bench);
    Require(pointerOut.buffer == &bench.sink.buffer && !pointerOut.failed, "pointer iterator");
    char pointerText[64];
    std::snprintf(pointerText, sizeof(pointerText), "%llx", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(&bench)));
    Require(Written(bench.sink) == pointerText, "a pointer prints as bare hex digits: " + Written(bench.sink));

    const char* savedGrouping = numpunct->grouping;
    const char savedSeparator = numpunct->thousandsSep;
    const char savedPoint = numpunct->decimalPoint;
    numpunct->thousandsSep = '\'';
    numpunct->decimalPoint = ',';
    numpunct->grouping = "\3\2";
    CheckLong(bench, "grouping cycles the last size", 0, 0, '*', 123456789, "12'34'56'789");
    CheckLong(bench, "grouping keeps the sign", 0, 0, '*', -1234, "-1'234");
    CheckLong(bench, "grouping keeps the base", Hex | ShowBase, 0, '*', 0x12345, "0x12'345");
    CheckLong(bench, "grouping and width", Internal, 8, '_', -1234, "-__1'234");
    CheckDouble(bench, "floating grouping and point", Fixed, 0, 1, 1234567.75, "12'34'567,8");
    CheckDouble(bench, "scientific point", Scientific, 0, 1, 1500.0, "1,5e+03");
    CheckDouble(bench, "no point stops at the exponent", Scientific, 0, 0, 1500.0, "2e+03");
    numpunct->grouping = "\1";
    CheckDouble(bench, "infinity has no groups", Fixed, 0, 1, std::numeric_limits<double>::infinity(), "inf");
    CheckDouble(bench, "nan has no groups", Fixed, 0, 1, std::numeric_limits<double>::quiet_NaN(), "nan");
    CheckDouble(bench, "a finite value has groups", Fixed, 0, 0, 123.0, "1'2'3");
    numpunct->grouping = "\177";
    CheckLong(bench, "grouping of 127 stops", 0, 0, '*', 123456, "123456");
    numpunct->grouping = "\1\0";
    CheckLong(bench, "zero repeats the previous size", 0, 0, '*', 1234, "1'2'3'4");
    numpunct->grouping = savedGrouping;
    numpunct->thousandsSep = savedSeparator;
    numpunct->decimalPoint = savedPoint;

    Arm(bench, 0);
    bench.sink.count = 2;
    Slot<PutSigned>(bench.facet, 5)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', 12345);
    Require(std::string(bench.sink.storage.data(), bench.sink.next) == "12" && bench.sink.overflowed == "345" && bench.sink.count == 0, "the count stops direct writes at the limit");

    Arm(bench, 0);
    bench.sink.next = nullptr;
    bench.sink.count = 100;
    Slot<PutSigned>(bench.facet, 5)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', 7);
    Require(bench.sink.overflowed == "7" && bench.sink.count == 100, "no put area goes through overflow");

    Arm(bench, 0);
    bench.sink.count = 0;
    bench.sink.rejectOverflow = true;
    auto rejected = Slot<PutSigned>(bench.facet, 5)(bench.facet, {false, &bench.sink.buffer}, &bench.stream, '*', 7);
    Require(rejected.failed && rejected.buffer == &bench.sink.buffer, "a rejected overflow fails but keeps the buffer");

    Arm(bench, 0, 3);
    auto missing = Slot<PutSigned>(bench.facet, 5)(bench.facet, {false, nullptr}, &bench.stream, '*', 7);
    Require(missing.failed && missing.buffer == nullptr && bench.stream.width == 0, "a null buffer fails and still consumes the width");

    Arm(bench, 0);
    auto sticky = Slot<PutSigned>(bench.facet, 5)(bench.facet, {true, &bench.sink.buffer}, &bench.stream, '*', 7);
    Require(sticky.failed && Written(bench.sink) == "7", "a failed iterator stays failed but keeps writing");

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
