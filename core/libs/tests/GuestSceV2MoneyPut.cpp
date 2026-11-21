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
extern std::uint64_t _ZNSt8__sce_v29money_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v29money_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v210moneypunctIwLb0EE2idE_nid_postfix;
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

constexpr std::int32_t ShowBase = 0x8;
constexpr std::int32_t Left = 0x40;
constexpr std::int32_t Internal = 0x100;

static unsigned failures = 0;

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

template<typename T> static T Slot(const void* facet, int index) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(facet))[index]);
}

template<typename Text> struct GuestString {
    union {
        Text buffer[16 / sizeof(Text)];
        Text* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

template<typename Text> static GuestString<Text> MakeString(const std::basic_string<Text>& text) {
    GuestString<Text> value{};
    constexpr std::uint64_t InlineCapacity = 16 / sizeof(Text) - 1;
    Require(text.size() <= InlineCapacity, "test strings stay inline");
    std::memcpy(value.buffer, text.data(), text.size() * sizeof(Text));
    value.size = text.size();
    value.capacity = InlineCapacity;
    return value;
}

template<typename Text> struct MoneypunctFields {
    const void* vtable;
    std::uint32_t references;
    std::uint32_t reserved;
    const char* grouping;
    Text decimalPoint;
    Text thousandsSep;
    const Text* currency;
    const Text* positiveSign;
    const Text* negativeSign;
    std::int32_t fractionDigits;
    char positivePattern[4];
    char negativePattern[4];
};

template<typename Text> struct Sink {
    GuestLocale::Streambuf buffer{};
    GuestLocale::StreambufVtable vtable{};
    std::array<Text, 8192> storage{};
    Text* next = nullptr;
    Text* end = nullptr;
    std::int32_t count = 0;
};

template<typename Text> struct Bench {
    const void* facet = nullptr;
    GuestLocale::SceV2IosBase stream{};
    Sink<Text> sink;
};

template<typename Text> using PutDigits = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, bool, GuestLocale::SceV2IosBase*, Text, const GuestString<Text>*);
template<typename Text> using PutLongDouble = GuestLocale::OutputIterator (APS5_VABI *)(const void*, GuestLocale::OutputIterator, bool, GuestLocale::SceV2IosBase*, Text, long double);

template<typename Text> static void Arm(Bench<Text>& bench, std::int32_t flags, std::int64_t width) {
    bench.stream.flags = flags;
    bench.stream.width = width;
    bench.sink.buffer.vtable = &bench.sink.vtable;
    bench.sink.next = bench.sink.storage.data();
    bench.sink.end = bench.sink.storage.data() + bench.sink.storage.size();
    bench.sink.count = static_cast<std::int32_t>(bench.sink.storage.size());
    bench.sink.buffer.putNext = reinterpret_cast<char**>(&bench.sink.next);
    bench.sink.buffer.putEnd = reinterpret_cast<char**>(&bench.sink.end);
    bench.sink.buffer.reservedCounts[2] = &bench.sink.count;
}

template<typename Text> static void Finish(Bench<Text>& bench, const std::string& name, const std::basic_string<Text>& expected, GuestLocale::OutputIterator out) {
    const std::basic_string<Text> written(bench.sink.storage.data(), bench.sink.next);
    Require(written == expected, name + ": wrote other text");
    Require(!out.failed && out.buffer == &bench.sink.buffer, name + ": iterator stays good");
    Require(bench.stream.width == 0, name + ": width is consumed");
}

template<typename Text> static void CheckDigits(Bench<Text>& bench, const std::string& name, bool international, std::int32_t flags, std::int64_t width, Text fill, const std::basic_string<Text>& digits, const std::basic_string<Text>& expected) {
    Arm(bench, flags, width);
    const GuestString<Text> value = MakeString(digits);
    Finish(bench, name, expected, Slot<PutDigits<Text>>(bench.facet, 5)(bench.facet, {false, &bench.sink.buffer}, international, &bench.stream, fill, &value));
}

template<typename Text> static void CheckLongDouble(Bench<Text>& bench, const std::string& name, long double value, const std::basic_string<Text>& expected) {
    Arm(bench, 0, 0);
    Finish(bench, name, expected, Slot<PutLongDouble<Text>>(bench.facet, 4)(bench.facet, {false, &bench.sink.buffer}, false, &bench.stream, Text('*'), value));
}

template<typename Text> static void SetFields(void* facet, const char* grouping, Text decimal, Text thousands, const Text* currency, const Text* positive, const Text* negative, std::int32_t digits, const char* positivePattern, const char* negativePattern) {
    auto* fields = static_cast<MoneypunctFields<Text>*>(facet);
    fields->grouping = grouping;
    fields->decimalPoint = decimal;
    fields->thousandsSep = thousands;
    fields->currency = currency;
    fields->positiveSign = positive;
    fields->negativeSign = negative;
    fields->fractionDigits = digits;
    std::memcpy(fields->positivePattern, positivePattern, 4);
    std::memcpy(fields->negativePattern, negativePattern, 4);
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
    static auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    _ZNSt8__sce_v26locale7_Locimp8_MakelocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(&info, 0x3f, locale, nullptr);

    static Bench<char> narrow;
    narrow.facet = locale->facets[_ZNSt8__sce_v29money_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix];
    narrow.stream.locale = &locale;
    static Bench<char16_t> wide;
    wide.facet = locale->facets[_ZNSt8__sce_v29money_putIwNS_19ostreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix];
    wide.stream.locale = &locale;
    void* local = locale->facets[_ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix];
    void* international = locale->facets[_ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix];
    void* wideLocal = locale->facets[_ZNSt8__sce_v210moneypunctIwLb0EE2idE_nid_postfix];
    Require(narrow.facet != nullptr && wide.facet != nullptr && local != nullptr && international != nullptr && wideLocal != nullptr, "_Makeloc installs money_put and moneypunct");
    if (failures != 0) std::_Exit(1);

    CheckDigits<char>(narrow, "C locale digits", false, 0, 0, '*', "1234", "1234");
    CheckDigits<char>(narrow, "C locale negative", false, 0, 0, '*', "-56", "-56");
    CheckDigits<char>(narrow, "empty digits print a zero", false, 0, 0, '*', "", "0");
    CheckDigits<char>(narrow, "digits stop at a non-digit", false, 0, 0, '*', "12a34", "12");
    CheckDigits<char>(narrow, "a lone minus prints a negative zero", false, 0, 0, '*', "-", "-0");
    CheckDigits<char>(narrow, "right fill", false, 0, 6, '*', "-56", "***-56");
    CheckDigits<char>(narrow, "internal fill after the sign", false, Internal, 6, '*', "-56", "-***56");
    CheckDigits<char>(narrow, "left fill", false, Left, 6, '*', "-56", "-56***");
    CheckLongDouble<char>(narrow, "long double rounds a half away from zero", 2.5L, "3");
    CheckLongDouble<char>(narrow, "long double negative", -1234.6L, "-1235");
    CheckLongDouble<char>(narrow, "a negative zero keeps its minus among the digits", -0.0L, "-0");
    CheckDigits<char>(narrow, "a minus among the digits ends them", false, 0, 0, '*', "12-3", "12");
    Arm(narrow, 0, 0);
    const auto infinite = Slot<PutLongDouble<char>>(narrow.facet, 4)(narrow.facet, {false, &narrow.sink.buffer}, false, &narrow.stream, '*', __builtin_infl());
    const std::string infiniteText(narrow.sink.storage.data(), narrow.sink.next);
    Require(!infinite.failed && infiniteText.size() == 5003 && infiniteText.find_first_not_of('0', 3) == std::string::npos && infiniteText[0] != '0', "infinity stops after 500 divisions and gets 5000 zeros");

    static const char grouping[] = "\3";
    SetFields<char>(local, grouping, '.', ',', "EUR", "", "()", 2, "$+ v", "+v$x");
    SetFields<char>(international, "", '.', ' ', "USD ", "", "-", 2, "$+xv", "+$xv");
    CheckDigits<char>(narrow, "grouping and fraction", false, 0, 0, '*', "1234567", "*12,345.67");
    CheckDigits<char>(narrow, "a space field puts the fill", false, ShowBase, 0, '*', "1234567", "EUR*12,345.67");
    CheckDigits<char>(narrow, "a short value gets leading zeros", false, 0, 0, '*', "5", "*0.05");
    CheckDigits<char>(narrow, "the rest of the sign goes last", false, ShowBase, 0, '*', "-5", "(0.05EUR)");
    CheckDigits<char>(narrow, "international facet", true, ShowBase, 0, '*', "-1234", "-USD 12.34");
    SetFields<char>(local, grouping, '.', ',', "EUR", "", "-", -3, "v$x+", "v$x+");
    CheckDigits<char>(narrow, "negative fraction digits count as positive", false, 0, 0, '*', "-1", "0.001-");
    static const char changingGroups[] = "";
    SetFields<char>(local, changingGroups, '.', ',', "EUR", "", "-", 0, "$+vx", "$+vx");
    CheckDigits<char>(narrow, "groups follow the grouping bytes", false, 0, 0, '*', "123456", "1,23,45,6");
    SetFields<char>(local, "", '.', ',', "EUR", "", "-", 0, "$+vx", "$+vx");
    CheckDigits<char>(narrow, "a last none field holds no internal fill", false, Internal, 8, '*', "12", "******12");

    static const char wideGrouping[] = "\2";
    static const char16_t euro[] = u"\x20ac";
    static const char16_t empty[] = u"";
    static const char16_t minus[] = u"\x2212";
    SetFields<char16_t>(wideLocal, wideGrouping, u'\x066b', u'\x066c', euro, empty, minus, 1, "$+xv", "+v $");
    CheckDigits<char16_t>(wide, "wide grouping", false, ShowBase, 0, u'_', u"12345", u"\x20ac" u"12\x066c" u"34\x066b" u"5");
    CheckDigits<char16_t>(wide, "wide negative with a space field", false, ShowBase, 0, u'_', u"-7", u"\x2212" u"0\x066b" u"7_\x20ac");
    CheckLongDouble<char16_t>(wide, "wide long double", 42.0L, u"4\x066b" u"2");

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
