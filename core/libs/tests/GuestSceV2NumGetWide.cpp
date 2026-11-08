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
extern std::uint64_t _ZNSt8__sce_v27num_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28numpunctIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v25ctypeIwE2idE_nid_postfix;
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
constexpr std::int32_t Hex = 0x800;
constexpr std::int32_t BoolAlpha = 0x4000;
constexpr std::int32_t EofBit = 0x1;
constexpr std::int32_t FailBit = 0x2;

static unsigned failures = 0;

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

struct InputIterator {
    GuestLocale::Streambuf* buffer;
    bool got;
    char16_t value;
};

static_assert(sizeof(InputIterator) == 16);

struct InputVtable {
    const void* reserved[7];
    int (APS5_VABI *underflow)(GuestLocale::Streambuf* self);
    int (APS5_VABI *uflow)(GuestLocale::Streambuf* self);
};

struct Source {
    GuestLocale::Streambuf buffer{};
    InputVtable vtable{};
    std::array<char16_t, 512> storage{};
    std::u16string text;
    char* next = nullptr;
    std::int32_t count = 0;
    std::size_t direct = 0;
    std::size_t slow = 0;
    int uflows = 0;
};

static Source* g_source = nullptr;

static int APS5_VABI SourceUnderflow(GuestLocale::Streambuf*) {
    return g_source->slow < g_source->text.size() ? g_source->text[g_source->slow] : -1;
}

static int APS5_VABI SourceUflow(GuestLocale::Streambuf*) {
    ++g_source->uflows;
    return g_source->slow < g_source->text.size() ? g_source->text[g_source->slow++] : -1;
}

static void Load(Source& source, const std::u16string& text, std::size_t direct) {
    source.vtable.underflow = SourceUnderflow;
    source.vtable.uflow = SourceUflow;
    source.buffer.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(&source.vtable);
    source.text = text;
    std::memcpy(source.storage.data(), text.data(), text.size() * sizeof(char16_t));
    source.direct = direct < text.size() ? direct : text.size();
    source.next = reinterpret_cast<char*>(source.storage.data());
    source.count = static_cast<std::int32_t>(source.direct);
    source.slow = source.direct;
    source.uflows = 0;
    source.buffer.reserved[6] = &source.next;
    source.buffer.reservedCounts[1] = &source.count;
    g_source = &source;
}

static std::size_t Consumed(const Source& source) {
    return static_cast<std::size_t>(reinterpret_cast<const char16_t*>(source.next) - source.storage.data()) + source.slow - source.direct;
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

template<typename Value> static Outcome<Value> Run(Bench& bench, int slot, const std::u16string& text, std::int32_t flags, std::size_t direct = 512) {
    Load(bench.source, text, direct);
    bench.stream.flags = flags;
    Outcome<Value> outcome{};
    outcome.out = Slot<Get<Value>>(bench.facet, slot)(bench.facet, {&bench.source.buffer, false, 0}, {nullptr, false, 0}, &bench.stream, &outcome.state, &outcome.value);
    outcome.consumed = Consumed(bench.source);
    return outcome;
}

static void CheckLongLong(Bench& bench, const std::string& name, const std::u16string& text, std::int32_t flags, std::int64_t value, std::int32_t state, std::size_t consumed, std::size_t direct = 512) {
    const auto outcome = Run<std::int64_t>(bench, 9, text, flags, direct);
    Require(outcome.value == value, name + ": value " + std::to_string(outcome.value) + ", expected " + std::to_string(value));
    Require(outcome.state == state, name + ": state " + std::to_string(outcome.state) + ", expected " + std::to_string(state));
    Require(outcome.consumed == consumed, name + ": consumed " + std::to_string(outcome.consumed) + ", expected " + std::to_string(consumed));
    const bool ended = consumed == text.size();
    Require(outcome.out.got && (outcome.out.buffer == nullptr) == ended, name + ": returned iterator");
    if (!ended) Require(outcome.out.value == text[consumed], name + ": the returned iterator holds the next character");
}

static void CheckBool(Bench& bench, const std::string& name, const std::u16string& text, std::int32_t flags, bool value, std::int32_t state, std::size_t consumed) {
    const auto outcome = Run<bool>(bench, 4, text, flags);
    Require(outcome.value == value && outcome.state == state && outcome.consumed == consumed,
        name + ": value " + std::to_string(outcome.value) + " state " + std::to_string(outcome.state) + " consumed " + std::to_string(outcome.consumed));
}

static const char* APS5_VABI WidenToArabicIndic(const void*, const char* first, const char* last, char16_t* destination) {
    for (; first != last; ++first, ++destination) *destination = *first >= '0' && *first <= '9' ? static_cast<char16_t>(u'٠' + (*first - '0')) : static_cast<unsigned char>(*first);
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
    auto* numpunct = reinterpret_cast<GuestLocale::WideNumpunctFacet*>(locale->facets[_ZNSt8__sce_v28numpunctIwE2idE_nid_postfix]);
    void* ctype = locale->facets[_ZNSt8__sce_v25ctypeIwE2idE_nid_postfix];

    static Bench bench{};
    bench.facet = locale->facets[_ZNSt8__sce_v27num_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix];
    bench.stream.locale = &locale;
    Require(bench.facet != nullptr && numpunct != nullptr && ctype != nullptr, "_Makeloc installs the wide num_get, numpunct and ctype");

    CheckLongLong(bench, "decimal to the end", u"123", Dec, 123, EofBit, 3);
    CheckLongLong(bench, "negative stops at a letter", u"-42x", Dec, -42, 0, 3);
    CheckLongLong(bench, "hex prefix", u"0XfF", 0, 255, EofBit, 4);
    CheckLongLong(bench, "automatic octal", u"017", 0, 15, EofBit, 3);
    CheckLongLong(bench, "a high byte is not a digit", u"1Ĳ", Dec, 1, 0, 1);
    CheckLongLong(bench, "a digit with a high byte is not a digit", u"1ı", Dec, 1, 0, 1);
    CheckLongLong(bench, "U+FFFF is a character, not the end", u"7￿", Dec, 7, 0, 1);
    CheckLongLong(bench, "the count limits direct reads", u"98765", Dec, 98765, EofBit, 5, 2);
    Require(bench.source.uflows == 3 && bench.source.count == 0, "the rest goes through uflow");
    CheckLongLong(bench, "empty input", u"", Dec, 0, FailBit | EofBit, 0);
    CheckLongLong(bench, "underflow keeps the high byte", u"1Ĳ", Dec, 1, 0, 1, 0);
    CheckLongLong(bench, "underflow returns U+FFFF as a character", u"7￿", Dec, 7, 0, 1, 0);
    CheckLongLong(bench, "a high byte over plus is not a sign", u"ī5", Dec, 0, FailBit, 0);

    Load(bench.source, u"89", 512);
    std::int32_t state = 0;
    std::int64_t value = 0;
    auto held = Slot<Get<std::int64_t>>(bench.facet, 9)(bench.facet, {&bench.source.buffer, true, u'7'}, {nullptr, false, 0}, &bench.stream, &state, &value);
    Require(value == 79 && state == EofBit && held.buffer == nullptr, "a held character is read first");

    const auto pointer = Run<std::uint64_t>(bench, 14, u"1f", Dec);
    Require(pointer.value == 31 && pointer.state == EofBit, "pointer is hex");
    const auto shortValue = Run<std::uint16_t>(bench, 5, u"-1", Dec);
    Require(shortValue.value == 0xffff && shortValue.state == EofBit, "unsigned short negates");
    const auto intValue = Run<std::uint32_t>(bench, 6, u"4294967295", Dec);
    Require(intValue.value == 4294967295u && intValue.state == EofBit, "unsigned int");
    const auto longValue = Run<std::int64_t>(bench, 7, u"-5", Dec);
    Require(longValue.value == -5 && longValue.state == EofBit, "long");
    const auto unsignedLong = Run<std::uint64_t>(bench, 10, u"ff", Hex);
    Require(unsignedLong.value == 255 && unsignedLong.state == EofBit, "unsigned long long");

    const auto floatValue = Run<float>(bench, 11, u"1.5x", Dec);
    Require(floatValue.value == 1.5f && floatValue.state == 0 && floatValue.consumed == 3, "float");
    const auto doubleHex = Run<double>(bench, 12, u"0x1.8p1", Dec);
    Require(doubleHex.value == 3.0 && doubleHex.state == EofBit, "double reads hex");
    const auto highExponent = Run<double>(bench, 12, u"2ť", Dec);
    Require(highExponent.value == 2.0 && highExponent.state == 0 && highExponent.consumed == 1, "a high byte over e is not an exponent");
    const auto highPoint = Run<double>(bench, 12, u"2Į5", Dec);
    Require(highPoint.value == 2.0 && highPoint.state == 0 && highPoint.consumed == 1, "a high byte over the point is not the point");
    const auto longFloat = Run<long double>(bench, 13, u"2.25e1", Dec);
    Require(longFloat.value == 22.5L && longFloat.state == EofBit, "long double");
    const auto longHex = Run<long double>(bench, 13, u"0x1p4", 0x3000);
    Require(longHex.value == 16.0L && longHex.state == EofBit, "long double hexfloat");
    const auto highDigit = Run<double>(bench, 12, u"2ĳ", Dec);
    Require(highDigit.value == 2.0 && highDigit.state == 0 && highDigit.consumed == 1, "a high byte over a digit is not a digit");
    const auto longHighPoint = Run<long double>(bench, 13, u"2Į5", Dec);
    Require(longHighPoint.value == 2.0L && longHighPoint.state == 0 && longHighPoint.consumed == 1, "a high byte over the point ends the long double");
    const auto longHigh = Run<long double>(bench, 13, u"3Ņ", Dec);
    Require(longHigh.value == 3.0L && longHigh.state == 0 && longHigh.consumed == 1, "a high byte over E ends the long double");
    CheckBool(bench, "bool one", u"1", Dec, true, EofBit, 1);
    CheckBool(bench, "bool true", u"true", BoolAlpha, true, EofBit, 4);
    CheckBool(bench, "bool false", u"false", BoolAlpha, false, EofBit, 5);
    CheckBool(bench, "bool mismatch", u"trap", BoolAlpha, false, FailBit, 2);

    const std::uint16_t* savedFalse = numpunct->falsename;
    const std::uint16_t* savedTrue = numpunct->truename;
    static const std::uint16_t falseName[] {0x043d, 0x0435, 0x0442, 0};
    static const std::uint16_t trueName[] {0x0434, 0x0430, 0};
    numpunct->falsename = falseName;
    numpunct->truename = trueName;
    CheckBool(bench, "non-ASCII true name", u"да", BoolAlpha, true, EofBit, 2);
    CheckBool(bench, "non-ASCII false name", u"нет!", BoolAlpha, false, 0, 3);
    CheckBool(bench, "a low byte match is not a match", u"40", BoolAlpha, false, FailBit, 0);
    numpunct->falsename = savedFalse;
    numpunct->truename = savedTrue;

    const char* savedGrouping = numpunct->grouping;
    const std::uint16_t savedSeparator = numpunct->thousandsSep;
    numpunct->grouping = "\3";
    numpunct->thousandsSep = 0x202f;
    CheckLongLong(bench, "a non-ASCII separator", u"1 234", Dec, 1234, EofBit, 5);
    CheckLongLong(bench, "the separator's low byte is not a separator", u"1/234", Dec, 1, 0, 1);
    CheckLongLong(bench, "a wrong group", u"12 34", Dec, 1234, FailBit | EofBit, 5);
    numpunct->decimalPoint = 0x066b;
    const auto widePoint = Run<double>(bench, 12, u"1 234٫5", Dec);
    Require(widePoint.value == 1234.5 && widePoint.state == EofBit, "a non-ASCII separator and decimal point");
    const auto widePointLong = Run<long double>(bench, 13, u"1 234٫5", Dec);
    Require(widePointLong.value == 1234.5L && widePointLong.state == EofBit, "a non-ASCII separator and decimal point for long double");
    numpunct->decimalPoint = u'.';
    numpunct->grouping = savedGrouping;
    numpunct->thousandsSep = savedSeparator;

    void** ctypeVtable = *static_cast<void***>(ctype);
    std::array<void*, 16> widening{};
    std::memcpy(widening.data(), ctypeVtable, sizeof(widening));
    widening[13] = reinterpret_cast<void*>(&WidenToArabicIndic);
    *static_cast<void***>(ctype) = widening.data();
    CheckLongLong(bench, "digits come from the wide ctype facet", u"١٢٠3", Dec, 120, 0, 3);
    *static_cast<void***>(ctype) = ctypeVtable;

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
