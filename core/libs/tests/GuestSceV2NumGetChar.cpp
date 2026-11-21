#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern std::uint64_t _ZNSt8__sce_v27num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28numpunctIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v25ctypeIcE2idE_nid_postfix;
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

constexpr std::int32_t Dec = 0x200;
constexpr std::int32_t Oct = 0x400;
constexpr std::int32_t Hex = 0x800;
constexpr std::int32_t BoolAlpha = 0x4000;
constexpr std::int32_t EofBit = 0x1;
constexpr std::int32_t FailBit = 0x2;
constexpr std::int32_t BadBit = 0x4;

static unsigned failures = 0;

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

struct InputIterator {
    GuestLocale::Streambuf* buffer;
    bool got;
    char value;
};

struct InputVtable {
    const void* reserved[7];
    int (APS5_VABI *underflow)(GuestLocale::Streambuf* self);
    int (APS5_VABI *uflow)(GuestLocale::Streambuf* self);
};

struct Source {
    GuestLocale::Streambuf buffer{};
    InputVtable vtable{};
    std::array<char, 256> storage{};
    std::string text;
    char* next = nullptr;
    std::int32_t count = 0;
    std::size_t direct = 0;
    std::size_t slow = 0;
    int underflows = 0;
    int uflows = 0;
};

static Source* g_source = nullptr;

static int APS5_VABI SourceUnderflow(GuestLocale::Streambuf*) {
    ++g_source->underflows;
    return g_source->slow < g_source->text.size() ? static_cast<unsigned char>(g_source->text[g_source->slow]) : -1;
}

static int APS5_VABI SourceUflow(GuestLocale::Streambuf*) {
    ++g_source->uflows;
    return g_source->slow < g_source->text.size() ? static_cast<unsigned char>(g_source->text[g_source->slow++]) : -1;
}

static void Load(Source& source, const std::string& text, std::size_t direct) {
    source.vtable.underflow = SourceUnderflow;
    source.vtable.uflow = SourceUflow;
    source.buffer.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(&source.vtable);
    source.text = text;
    std::memcpy(source.storage.data(), text.data(), text.size());
    source.direct = direct < text.size() ? direct : text.size();
    source.next = source.storage.data();
    source.count = static_cast<std::int32_t>(source.direct);
    source.slow = source.direct;
    source.underflows = 0;
    source.uflows = 0;
    source.buffer.reserved[6] = &source.next;
    source.buffer.reservedCounts[1] = &source.count;
    g_source = &source;
}

static std::size_t Consumed(const Source& source) {
    return static_cast<std::size_t>(source.next - source.storage.data()) + source.slow - source.direct;
}

template<typename Value> using Get = InputIterator (APS5_VABI *)(const void*, InputIterator, InputIterator, GuestLocale::SceV2IosBase*, std::int32_t*, Value*);

template<typename T> static T Slot(const void* facet, int index) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(facet))[index]);
}

struct Bench {
    const void* facet;
    GuestLocale::SceV2IosBase stream;
    Source source;
};

template<typename Value> struct Outcome {
    Value value;
    std::int32_t state;
    std::size_t consumed;
    InputIterator out;
};

template<typename Value> static Outcome<Value> Run(Bench& bench, int slot, const std::string& text, std::int32_t flags, std::size_t direct = 256, std::int32_t state = 0) {
    Load(bench.source, text, direct);
    bench.stream.flags = flags;
    Outcome<Value> outcome{};
    outcome.state = state;
    outcome.out = Slot<Get<Value>>(bench.facet, slot)(bench.facet, {&bench.source.buffer, false, 0}, {nullptr, false, 0}, &bench.stream, &outcome.state, &outcome.value);
    outcome.consumed = Consumed(bench.source);
    return outcome;
}

static void CheckLongLong(Bench& bench, const std::string& name, const std::string& text, std::int32_t flags, std::int64_t value, std::int32_t state, std::size_t consumed) {
    const auto outcome = Run<std::int64_t>(bench, 9, text, flags);
    Require(outcome.value == value, name + ": value " + std::to_string(outcome.value) + ", expected " + std::to_string(value));
    Require(outcome.state == state, name + ": state " + std::to_string(outcome.state) + ", expected " + std::to_string(state));
    Require(outcome.consumed == consumed, name + ": consumed " + std::to_string(outcome.consumed) + ", expected " + std::to_string(consumed));
    const bool ended = consumed == text.size();
    Require(outcome.out.got && (outcome.out.buffer == nullptr) == ended, name + ": returned iterator");
    if (!ended) Require(outcome.out.value == text[consumed], name + ": the returned iterator holds the next character");
}

static void CheckState(Bench& bench, const std::string& name, int slot, const std::string& text, std::int32_t flags, std::uint64_t value, std::int32_t state) {
    const auto outcome = Run<std::uint64_t>(bench, slot, text, flags);
    Require(outcome.value == value, name + ": value " + std::to_string(outcome.value) + ", expected " + std::to_string(value));
    Require(outcome.state == state, name + ": state " + std::to_string(outcome.state) + ", expected " + std::to_string(state));
}

static void CheckShort(Bench& bench, const std::string& name, const std::string& text, std::uint16_t value, std::int32_t state) {
    const auto outcome = Run<std::uint16_t>(bench, 5, text, Dec);
    Require(outcome.value == value && outcome.state == state, name + ": value " + std::to_string(outcome.value) + " state " + std::to_string(outcome.state));
}

static void CheckInt(Bench& bench, const std::string& name, const std::string& text, std::uint32_t value, std::int32_t state) {
    const auto outcome = Run<std::uint32_t>(bench, 6, text, Dec);
    Require(outcome.value == value && outcome.state == state, name + ": value " + std::to_string(outcome.value) + " state " + std::to_string(outcome.state));
}

static void CheckBool(Bench& bench, const std::string& name, const std::string& text, std::int32_t flags, bool value, std::int32_t state, std::size_t consumed) {
    const auto outcome = Run<bool>(bench, 4, text, flags);
    Require(outcome.value == value && outcome.state == state && outcome.consumed == consumed,
        name + ": value " + std::to_string(outcome.value) + " state " + std::to_string(outcome.state) + " consumed " + std::to_string(outcome.consumed));
}

static InputIterator APS5_VABI RejectLongLong(const void*, InputIterator first, InputIterator, GuestLocale::SceV2IosBase*, std::int32_t* state, std::int64_t* value) {
    *state = BadBit;
    *value = -1;
    return first;
}

static const char* APS5_VABI WidenOneToI(const void*, const char* first, const char* last, char* destination) {
    for (; first != last; ++first, ++destination) *destination = *first == '1' ? 'I' : *first;
    return last;
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
    void* ctype = locale->facets[_ZNSt8__sce_v25ctypeIcE2idE_nid_postfix];

    static Bench bench{};
    bench.facet = locale->facets[_ZNSt8__sce_v27num_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix];
    bench.stream.locale = &locale;
    Require(bench.facet != nullptr && numpunct != nullptr && ctype != nullptr, "_Makeloc installs num_get, numpunct and ctype");

    CheckLongLong(bench, "decimal to the end", "123", Dec, 123, EofBit, 3);
    CheckLongLong(bench, "negative stops at a letter", "-42x", Dec, -42, 0, 3);
    CheckLongLong(bench, "plus sign", "+7", Dec, 7, EofBit, 2);
    CheckLongLong(bench, "empty input", "", Dec, 0, FailBit | EofBit, 0);
    CheckLongLong(bench, "no digits", "x", Dec, 0, FailBit, 0);
    CheckLongLong(bench, "a lone sign", "-", Dec, 0, FailBit | EofBit, 1);
    CheckLongLong(bench, "automatic hex", "0x1f", 0, 31, EofBit, 4);
    CheckLongLong(bench, "automatic octal", "017", 0, 15, EofBit, 3);
    CheckLongLong(bench, "automatic octal stops at eight", "08", 0, 0, 0, 1);
    CheckLongLong(bench, "automatic decimal", "19", 0, 19, EofBit, 2);
    CheckLongLong(bench, "hex digits", "fF", Hex, 255, EofBit, 2);
    CheckLongLong(bench, "hex prefix", "-0XfF", Hex, -255, EofBit, 5);
    CheckLongLong(bench, "a prefix alone", "0x", Hex, 0, FailBit | EofBit, 2);
    CheckLongLong(bench, "octal takes no prefix", "0x10", Oct, 0, 0, 1);
    CheckLongLong(bench, "decimal takes no prefix", "0x10", Dec, 0, 0, 1);
    CheckLongLong(bench, "octal digits", "777", Oct, 511, EofBit, 3);
    CheckLongLong(bench, "leading zeros", "000123", Dec, 123, EofBit, 6);
    CheckLongLong(bench, "zero", "0", Dec, 0, EofBit, 1);
    CheckLongLong(bench, "many zeros", "-0000", Dec, 0, EofBit, 5);
    CheckLongLong(bench, "zeros are not counted", std::string(50, '0') + "5", Dec, 5, EofBit, 51);
    CheckLongLong(bench, "both fields set", "12", Oct | Hex, 12, EofBit, 2);
    CheckLongLong(bench, "the largest value", "9223372036854775807", Dec, INT64_MAX, EofBit, 19);
    const auto overflow = Run<std::int64_t>(bench, 9, std::string(40, '9'), Dec);
    Require(overflow.state == (FailBit | EofBit) && overflow.consumed == 40, "an overflow fails after reading every digit");

    const auto overflowState = Run<std::int64_t>(bench, 9, std::string(40, '9'), Dec, 256, BadBit);
    Require(overflowState.state == (FailBit | EofBit), "a failed conversion replaces the state");
    const auto pointerOverflow = Run<std::uint64_t>(bench, 14, "1" + std::string(20, '0'), Dec);
    Require(pointerOverflow.value == 0 && pointerOverflow.state == (FailBit | EofBit), "a pointer that does not convert is null");
    const auto keep = Run<std::int64_t>(bench, 9, "12 ", Dec, 256, BadBit);
    Require(keep.state == BadBit && keep.value == 12, "success leaves the state alone");
    const auto replace = Run<std::int64_t>(bench, 9, "q", Dec, 256, BadBit | EofBit);
    Require(replace.state == FailBit, "a failure replaces the state");
    const auto addEof = Run<std::int64_t>(bench, 9, "12", Dec, 256, BadBit);
    Require(addEof.state == (BadBit | EofBit), "the end adds eofbit");

    CheckState(bench, "long", 7, "-5", Dec, static_cast<std::uint64_t>(-5), EofBit);
    CheckState(bench, "unsigned long", 8, "18446744073709551615", Dec, UINT64_MAX, EofBit);
    CheckState(bench, "unsigned long long", 10, "ff", Hex, 255, EofBit);
    CheckState(bench, "pointer is hex", 14, "1f", Dec, 31, EofBit);
    CheckState(bench, "pointer prefix", 14, "0x1F ", 0, 31, 0);
    CheckState(bench, "pointer failure", 14, "zz", Dec, 0, FailBit);

    CheckShort(bench, "unsigned short", "65535", 65535, EofBit);
    CheckShort(bench, "unsigned short overflow", "65536", 0xffff, FailBit | EofBit);
    CheckShort(bench, "unsigned short negates", "-1", 0xffff, EofBit);
    CheckShort(bench, "unsigned short negative zero", "-0", 0, EofBit);
    CheckShort(bench, "unsigned short no digits", "-", 0, FailBit | EofBit);
    CheckInt(bench, "unsigned int", "4294967295", 4294967295u, EofBit);
    CheckInt(bench, "unsigned int overflow", "4294967296", 4294967295u, FailBit | EofBit);
    CheckInt(bench, "unsigned int no digits", "+", 0, FailBit | EofBit);

    CheckBool(bench, "bool one", "1", Dec, true, EofBit, 1);
    CheckBool(bench, "bool zero", "0", Dec, false, EofBit, 1);
    CheckBool(bench, "bool two", "2", Dec, true, FailBit | EofBit, 1);
    CheckBool(bench, "bool minus one", "-1", Dec, true, FailBit | EofBit, 2);
    CheckBool(bench, "bool no digits", "t", Dec, false, FailBit, 0);
    CheckBool(bench, "bool true", "true", BoolAlpha, true, EofBit, 4);
    CheckBool(bench, "bool false", "false", BoolAlpha, false, EofBit, 5);
    CheckBool(bench, "bool true then more", "truex", BoolAlpha, true, 0, 4);
    CheckBool(bench, "bool short", "fals", BoolAlpha, false, FailBit | EofBit, 4);
    CheckBool(bench, "bool mismatch", "trap", BoolAlpha, false, FailBit, 2);
    CheckBool(bench, "bool empty", "", BoolAlpha, false, FailBit | EofBit, 0);
    CheckBool(bench, "bool is case sensitive", "True", BoolAlpha, false, FailBit, 0);

    const char* savedFalse = numpunct->falsename;
    const char* savedTrue = numpunct->truename;
    numpunct->falsename = "no";
    numpunct->truename = "nope";
    CheckBool(bench, "the longer name wins", "nope", BoolAlpha, true, EofBit, 4);
    CheckBool(bench, "the shorter name", "no", BoolAlpha, false, EofBit, 2);
    CheckBool(bench, "a partial longer name fails", "nop", BoolAlpha, false, FailBit | EofBit, 3);
    CheckBool(bench, "the shorter name then more", "nox", BoolAlpha, false, 0, 2);
    numpunct->falsename = "";
    CheckBool(bench, "an empty name matches nothing read", "x", BoolAlpha, false, 0, 0);
    numpunct->falsename = savedFalse;
    numpunct->truename = savedTrue;

    const char* savedGrouping = numpunct->grouping;
    const char savedSeparator = numpunct->thousandsSep;
    numpunct->thousandsSep = ',';
    numpunct->grouping = "\3";
    CheckLongLong(bench, "grouped", "1,234,567", Dec, 1234567, EofBit, 9);
    CheckLongLong(bench, "a short first group", "12,345", Dec, 12345, EofBit, 6);
    CheckLongLong(bench, "a long first group", "1234,567", Dec, 1234567, FailBit | EofBit, 8);
    CheckLongLong(bench, "a wrong inner group", "12,34", Dec, 1234, FailBit | EofBit, 5);
    CheckLongLong(bench, "two separators", "1,,234", Dec, 1234, FailBit | EofBit, 6);
    CheckLongLong(bench, "a trailing separator", "1,234,", Dec, 1234, FailBit | EofBit, 6);
    CheckLongLong(bench, "a leading separator", ",123", Dec, 123, FailBit | EofBit, 4);
    CheckLongLong(bench, "a separator after the sign", "-,123", Dec, -123, FailBit | EofBit, 5);
    CheckLongLong(bench, "a separator before the sign", ",-12", Dec, -12, FailBit | EofBit, 4);
    CheckLongLong(bench, "a group stops counting at 127", std::string(130, '0') + ",000", Dec, 0, FailBit | EofBit, 134);
    CheckLongLong(bench, "no separators", "1234567", Dec, 1234567, EofBit, 7);
    CheckLongLong(bench, "a leading zero opens the first group", "0,123", Dec, 123, EofBit, 5);
    CheckLongLong(bench, "a period is not a separator", "1.234", Dec, 1, 0, 1);
    numpunct->grouping = "\3\2";
    CheckLongLong(bench, "the last size repeats", "12,34,567", Dec, 1234567, EofBit, 9);
    CheckLongLong(bench, "the second size", "1,234,567", Dec, 1234567, FailBit | EofBit, 9);
    numpunct->grouping = "\177";
    CheckLongLong(bench, "a size of 127 checks nothing", "1,2,3", Dec, 123, EofBit, 5);
    numpunct->grouping = "\2\177";
    CheckLongLong(bench, "127 skips the groups before it", "1,234,56", Dec, 123456, EofBit, 8);
    CheckLongLong(bench, "the size before 127 still applies", "1,234,567", Dec, 1234567, FailBit | EofBit, 9);
    numpunct->grouping = "";
    CheckLongLong(bench, "no grouping has no separator", "1,234", Dec, 1, 0, 1);
    numpunct->grouping = "\3";
    CheckShort(bench, "unsigned short keeps the value on bad groups", "12,34", 1234, FailBit | EofBit);
    CheckInt(bench, "unsigned int drops the value on bad groups", "12,34", 0, FailBit | EofBit);
    CheckBool(bench, "bool on bad groups", "0,0", Dec, true, FailBit | EofBit, 3);
    CheckState(bench, "pointer keeps the value on bad groups", 14, "1,23", Dec, 0x123, FailBit | EofBit);
    numpunct->grouping = savedGrouping;
    numpunct->thousandsSep = savedSeparator;

    const auto split = Run<std::int64_t>(bench, 9, "12345", Dec, 2);
    Require(split.value == 12345 && split.consumed == 5 && bench.source.count == 0 && bench.source.uflows == 3 && bench.source.underflows == 4,
        "the count limits direct reads, the rest goes through underflow and uflow: " + std::to_string(bench.source.uflows) + " " + std::to_string(bench.source.underflows));

    Load(bench.source, "89", 256);
    std::int32_t state = 0;
    std::int64_t value = 0;
    auto cached = Slot<Get<std::int64_t>>(bench.facet, 9)(bench.facet, {&bench.source.buffer, true, '7'}, {nullptr, false, 0}, &bench.stream, &state, &value);
    Require(value == 79 && state == EofBit && cached.buffer == nullptr && cached.got, "a held character is read first and its advance consumes the buffer's");

    Load(bench.source, "123", 256);
    state = 0;
    value = 5;
    auto same = Slot<Get<std::int64_t>>(bench.facet, 9)(bench.facet, {&bench.source.buffer, false, 0}, {&bench.source.buffer, false, 0}, &bench.stream, &state, &value);
    Require(value == 0 && state == (FailBit | EofBit) && Consumed(bench.source) == 0 && same.buffer == &bench.source.buffer, "two iterators on one buffer compare equal");

    state = 0;
    value = 5;
    auto none = Slot<Get<std::int64_t>>(bench.facet, 9)(bench.facet, {nullptr, false, 0}, {nullptr, false, 0}, &bench.stream, &state, &value);
    Require(value == 0 && state == (FailBit | EofBit) && none.buffer == nullptr && none.got, "a null buffer is the end");

    Load(bench.source, "", 0);
    bench.source.next = nullptr;
    bench.source.count = 5;
    bench.source.text = "64";
    state = 0;
    value = 0;
    Slot<Get<std::int64_t>>(bench.facet, 9)(bench.facet, {&bench.source.buffer, false, 0}, {nullptr, false, 0}, &bench.stream, &state, &value);
    Require(value == 64 && bench.source.count == 5 && bench.source.uflows == 2, "no get area reads through the virtuals");

    void** vtable = *static_cast<void***>(const_cast<void*>(bench.facet));
    void* savedLongLong = vtable[9];
    std::array<void*, 15> replaced{};
    std::memcpy(replaced.data(), vtable, sizeof(replaced));
    replaced[9] = reinterpret_cast<void*>(&RejectLongLong);
    *static_cast<void***>(const_cast<void*>(bench.facet)) = replaced.data();
    const auto direct = Run<std::int64_t>(bench, 7, "11", Dec);
    Require(direct.value == 11 && direct.state == EofBit, "long reads without the long long virtual");
    *static_cast<void***>(const_cast<void*>(bench.facet)) = vtable;
    Require(vtable[9] == savedLongLong, "vtable restored");

    void** ctypeVtable = *static_cast<void***>(ctype);
    std::array<void*, 12> wideningCtype{};
    std::memcpy(wideningCtype.data(), ctypeVtable, sizeof(wideningCtype));
    wideningCtype[9] = reinterpret_cast<void*>(&WidenOneToI);
    *static_cast<void***>(ctype) = wideningCtype.data();
    CheckLongLong(bench, "digits come from the ctype facet", "I21", Dec, 12, 0, 2);
    *static_cast<void***>(ctype) = ctypeVtable;

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
