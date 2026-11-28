#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
extern std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix;
}

namespace {

constexpr std::int32_t Left = 0x2;
constexpr std::int32_t Internal = 0x8;
constexpr std::int32_t Hex = 0x40;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "num_put field: %s\n", message);
    std::abort();
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

char* g_noBuffer = nullptr;

struct NarrowStream {
    GuestLocale::Streambuf buffer;
    std::string accepted;
    int offered;
    int capacity;
};

int APS5_VABI NarrowOverflow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<NarrowStream*>(self);
    ++stream->offered;
    if (static_cast<int>(stream->accepted.size()) >= stream->capacity) return -1;
    stream->accepted += static_cast<char>(character);
    return character;
}

const GuestLocale::StreambufVtable g_narrowVtable{{}, NarrowOverflow};

struct WideStream {
    GuestLocale::Streambuf buffer;
    std::u16string accepted;
    int offered;
    int capacity;
};

int APS5_VABI WideOverflow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<WideStream*>(self);
    ++stream->offered;
    if (static_cast<int>(stream->accepted.size()) >= stream->capacity) return -1;
    stream->accepted += static_cast<char16_t>(character);
    return character;
}

const GuestLocale::StreambufVtable g_wideVtable{{}, WideOverflow};

NarrowStream MakeNarrow(int capacity) {
    return NarrowStream{{&g_narrowVtable, {}, &g_noBuffer, {}, &g_noBuffer}, {}, 0, capacity};
}

WideStream MakeWide(int capacity) {
    return WideStream{{&g_wideVtable, {}, &g_noBuffer, {}, &g_noBuffer}, {}, 0, capacity};
}

void APS5_VABI FacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI FacetRelease(GuestLocale::Facet*) { return nullptr; }

const char* g_grouping = "";

char APS5_VABI GroupDecimalPoint(const GuestLocale::NumpunctFacet*) { return '.'; }
char APS5_VABI GroupThousandsSep(const GuestLocale::NumpunctFacet*) { return '\''; }
GuestLocale::String* APS5_VABI GroupGrouping(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, g_grouping); }
GuestLocale::String* APS5_VABI GroupFalse(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, "false"); }
GuestLocale::String* APS5_VABI GroupTrue(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, "true"); }

const GuestLocale::NumpunctVtable g_groupVtable{{FacetNop, FacetNop, FacetNop, FacetRelease}, GroupDecimalPoint, GroupThousandsSep, GroupGrouping, GroupFalse, GroupTrue};

using Iput = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, char*, std::size_t);
const Iput IntegerPut = _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_IputES3_RSt8ios_basecPcm_nid_postfix;

std::string PutInteger(GuestLocale::Implementation** locale, std::int32_t flags, std::int32_t width, const char* digits) {
    GuestLocale::IosBase ios{};
    ios.flags = flags;
    ios.width = width;
    ios.locale = locale;
    auto stream = MakeNarrow(64);
    std::string buffer(digits);
    const GuestLocale::OutputIterator out = IntegerPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '*', buffer.data(), buffer.size());
    Require(!out.failed && out.buffer == &stream.buffer, "_Iput returns the iterator it wrote through");
    Require(ios.width == 0, "_Iput resets the stream width");
    return stream.accepted;
}

void CheckIputAdjustment() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(PutInteger(classic, 0, 6, "-42") == "***-42", "_Iput pads on the left by default");
    Require(PutInteger(classic, Internal, 6, "-42") == "-***42", "_Iput pads after the sign for internal");
    Require(PutInteger(classic, Left, 6, "-42") == "-42***", "_Iput pads on the right for left");
    Require(PutInteger(classic, Left | Internal, 6, "+7") == "****+7", "_Iput treats a mixed adjustfield as right");
    Require(PutInteger(classic, Hex | Internal, 8, "0x1f") == "0x****1f", "_Iput keeps the hex prefix before internal fill");
    Require(PutInteger(classic, Hex | Internal, 8, "-0X1F") == "-0X***1F", "_Iput keeps the sign and hex prefix before internal fill");
    Require(PutInteger(classic, Internal, 6, "0x1f") == "**0x1f", "_Iput sees no hex prefix without the hex base");
    Require(PutInteger(classic, 0, 2, "12345") == "12345", "_Iput does not truncate to the width");
    Require(PutInteger(classic, 0, -3, "9") == "9", "_Iput ignores a negative width");
    Require(PutInteger(classic, 0, 3, "") == "***", "_Iput fills an empty field");
}

void CheckIputGrouping() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    PutInteger(classic, 0, 0, "1");
    const std::uint64_t id = _ZNSt8numpunctIcE2idE_nid_postfix;
    Require(id != 0, "_Iput looks up numpunct<char>");
    GuestLocale::NumpunctFacet punct{{&g_groupVtable.facet, 1, 0}, nullptr, '.', '\'', nullptr, nullptr};
    std::vector<GuestLocale::Facet*> facets(id + 1, nullptr);
    facets[id] = &punct.base;
    GuestLocale::Implementation implementation{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
    GuestLocale::Implementation* locale = &implementation;
    g_grouping = "\3";
    Require(PutInteger(&locale, 0, 0, "1234567") == "1'234'567", "_Iput repeats the last group");
    Require(PutInteger(&locale, 0, 0, "-1234") == "-1'234", "_Iput does not group the sign");
    Require(PutInteger(&locale, 0, 0, "123") == "123", "_Iput inserts no separator in a single group");
    Require(PutInteger(&locale, Hex, 0, "0x12345") == "0x12'345", "_Iput does not group the hex prefix");
    Require(PutInteger(&locale, Internal, 10, "-12345") == "-***12'345", "_Iput pads the grouped field");
    g_grouping = "\1\2";
    Require(PutInteger(&locale, 0, 0, "12345") == "12'34'5", "_Iput follows each group in turn");
    g_grouping = "\2\x7f";
    Require(PutInteger(&locale, 0, 0, "12345") == "123'45", "_Iput stops at CHAR_MAX");
    g_grouping = "\x7f";
    Require(PutInteger(&locale, 0, 0, "12345") == "12345", "_Iput does not group when the first group is CHAR_MAX");
    g_grouping = "";
}

void CheckIputFailure() {
    GuestLocale::IosBase ios{};
    ios.width = 5;
    ios.locale = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    auto stream = MakeNarrow(2);
    char digits[] = "42";
    GuestLocale::OutputIterator out = IntegerPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '.', digits, 2);
    Require(out.failed && stream.accepted == ".." && stream.offered == 5, "_Iput keeps offering characters after a failed write");
    ios.width = 0;
    out = IntegerPut(nullptr, GuestLocale::OutputIterator{false, nullptr}, &ios, '.', digits, 2);
    Require(out.failed && out.buffer == nullptr, "_Iput through a null streambuf fails");
    Require(Throws([&] { IntegerPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, nullptr, '.', digits, 2); }), "_Iput rejects a null stream");
}

using Rep = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, std::uint16_t, std::size_t);
using Put = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, const std::uint16_t*, std::size_t);
const Rep WideRep = _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_RepES3_wm_nid_postfix;
const Put WidePut = _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE4_PutES3_PKwm_nid_postfix;

void CheckWideRep() {
    auto stream = MakeWide(16);
    GuestLocale::OutputIterator out = WideRep(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, 0x263a, 4);
    Require(!out.failed && out.buffer == &stream.buffer && stream.accepted == u"☺☺☺☺", "_Rep writes the element count times");
    out = WideRep(nullptr, out, u'x', 0);
    Require(!out.failed && stream.offered == 4, "_Rep with a zero count writes nothing");
    auto full = MakeWide(2);
    out = WideRep(nullptr, GuestLocale::OutputIterator{false, &full.buffer}, u'y', 5);
    Require(out.failed && full.accepted == u"yy" && full.offered == 5, "_Rep marks the iterator failed and keeps offering");
    std::array<std::uint16_t, 4> direct{};
    char* next = reinterpret_cast<char*>(direct.data());
    char* end = reinterpret_cast<char*>(direct.data() + 3);
    auto buffered = MakeWide(8);
    buffered.buffer.putNext = &next;
    buffered.buffer.putEnd = &end;
    out = WideRep(nullptr, GuestLocale::OutputIterator{false, &buffered.buffer}, 0xffff, 5);
    Require(!out.failed && direct[0] == 0xffff && direct[2] == 0xffff && direct[3] == 0, "_Rep fills the put area first");
    Require(next == end && buffered.accepted == u"￿￿", "_Rep overflows past the put area");
    out = WideRep(nullptr, GuestLocale::OutputIterator{true, nullptr}, u'z', 2);
    Require(out.failed && out.buffer == nullptr, "_Rep through a null streambuf stays failed");
}

void CheckWidePut() {
    const std::u16string hello = u"héllo";
    auto stream = MakeWide(64);
    GuestLocale::OutputIterator out = WidePut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, reinterpret_cast<const std::uint16_t*>(hello.data()), hello.size());
    Require(!out.failed && out.buffer == &stream.buffer && stream.accepted == hello, "_Put writes the elements in order");
    out = WidePut(nullptr, out, nullptr, 0);
    Require(!out.failed && stream.offered == static_cast<int>(hello.size()), "_Put with a zero count reads nothing");
    std::vector<std::uint16_t> many(700);
    for (std::size_t index = 0; index < many.size(); ++index) many[index] = static_cast<std::uint16_t>(0x100 + index);
    auto large = MakeWide(1000);
    out = WidePut(nullptr, GuestLocale::OutputIterator{false, &large.buffer}, many.data(), many.size());
    Require(!out.failed && large.accepted.size() == many.size(), "_Put writes a long run");
    Require(std::memcmp(large.accepted.data(), many.data(), many.size() * sizeof(std::uint16_t)) == 0, "_Put keeps a long run in order");
    auto full = MakeWide(3);
    out = WidePut(nullptr, GuestLocale::OutputIterator{false, &full.buffer}, reinterpret_cast<const std::uint16_t*>(hello.data()), hello.size());
    Require(out.failed && full.accepted == u"hél" && full.offered == static_cast<int>(hello.size()), "_Put marks the iterator failed and keeps offering");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckIputAdjustment();
    CheckIputGrouping();
    CheckIputFailure();
    CheckWideRep();
    CheckWidePut();
    std::puts("num_put field checks passed");
    return 0;
}
