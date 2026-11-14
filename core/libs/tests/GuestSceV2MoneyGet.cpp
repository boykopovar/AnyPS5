#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern std::uint64_t _ZNSt8__sce_v29money_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v29money_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix;
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
constexpr std::int32_t EofBit = 0x1;
constexpr std::int32_t FailBit = 0x2;

static unsigned failures = 0;

static void Require(bool value, const std::string& what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

template<typename T> static T Slot(const void* facet, int index) {
    return reinterpret_cast<T>((*static_cast<void* const* const*>(facet))[index]);
}

template<typename Text> struct InputIterator {
    GuestLocale::Streambuf* buffer;
    bool got;
    Text value;
};

struct InputVtable {
    const void* reserved[7];
    int (APS5_VABI *underflow)(GuestLocale::Streambuf* self);
    int (APS5_VABI *uflow)(GuestLocale::Streambuf* self);
};

template<typename Text> struct GuestString {
    union {
        Text buffer[16 / sizeof(Text)];
        Text* pointer;
    };
    std::uint64_t size;
    std::uint64_t capacity;
};

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

template<typename Text> struct Source {
    GuestLocale::Streambuf buffer{};
    InputVtable vtable{};
    std::array<Text, 512> storage{};
    std::basic_string<Text> text;
    Text* next = nullptr;
    std::int32_t count = 0;
    std::size_t direct = 0;
    std::size_t slow = 0;
};

template<typename Text> static Source<Text>* g_source = nullptr;

template<typename Text> static int APS5_VABI SourceUnderflow(GuestLocale::Streambuf*) {
    const auto& source = *g_source<Text>;
    return source.slow < source.text.size() ? static_cast<int>(static_cast<std::make_unsigned_t<Text>>(source.text[source.slow])) : -1;
}

template<typename Text> static int APS5_VABI SourceUflow(GuestLocale::Streambuf*) {
    auto& source = *g_source<Text>;
    return source.slow < source.text.size() ? static_cast<int>(static_cast<std::make_unsigned_t<Text>>(source.text[source.slow++])) : -1;
}

template<typename Text> struct Bench {
    const void* facet = nullptr;
    GuestLocale::SceV2IosBase stream{};
    Source<Text> source;
};

template<typename Text> using GetLongDouble = InputIterator<Text> (APS5_VABI *)(const void*, InputIterator<Text>, InputIterator<Text>, std::int32_t, GuestLocale::SceV2IosBase*, std::int32_t*, long double*);
template<typename Text> using GetDigits = InputIterator<Text> (APS5_VABI *)(const void*, InputIterator<Text>, InputIterator<Text>, std::int32_t, GuestLocale::SceV2IosBase*, std::int32_t*, GuestString<Text>*);

template<typename Text> static void Load(Bench<Text>& bench, const std::basic_string<Text>& text, std::size_t direct, std::int32_t flags) {
    auto& source = bench.source;
    source.vtable.underflow = SourceUnderflow<Text>;
    source.vtable.uflow = SourceUflow<Text>;
    source.buffer.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(&source.vtable);
    source.text = text;
    std::memcpy(source.storage.data(), text.data(), text.size() * sizeof(Text));
    source.direct = direct < text.size() ? direct : text.size();
    source.next = source.storage.data();
    source.count = static_cast<std::int32_t>(source.direct);
    source.slow = source.direct;
    source.buffer.reserved[6] = &source.next;
    source.buffer.reservedCounts[1] = &source.count;
    g_source<Text> = &source;
    bench.stream.flags = flags;
}

template<typename Text> static std::size_t Consumed(const Source<Text>& source) {
    return static_cast<std::size_t>(source.next - source.storage.data()) + source.slow - source.direct;
}

template<typename Text> static const Text* Data(const GuestString<Text>& value) {
    return value.capacity > 16 / sizeof(Text) - 1 ? value.pointer : value.buffer;
}

template<typename Text> static void CheckDigits(Bench<Text>& bench, const std::string& name, std::int32_t international, std::int32_t flags, const std::basic_string<Text>& text, const std::basic_string<Text>& expected, std::int32_t state, std::size_t consumed, std::size_t direct = 512) {
    Load(bench, text, direct, flags);
    GuestString<Text> value{};
    value.buffer[0] = Text('?');
    value.size = 1;
    value.capacity = 16 / sizeof(Text) - 1;
    std::int32_t outState = 0;
    const auto out = Slot<GetDigits<Text>>(bench.facet, 5)(bench.facet, {&bench.source.buffer, false, 0}, {nullptr, false, 0}, international, &bench.stream, &outState, &value);
    const std::basic_string<Text> got(Data(value), value.size);
    Require(got == expected, name + ": other digits");
    Require(Data(value)[value.size] == Text{}, name + ": digits stay terminated");
    Require(outState == state, name + ": state " + std::to_string(outState) + ", expected " + std::to_string(state));
    Require(Consumed(bench.source) == consumed, name + ": consumed " + std::to_string(Consumed(bench.source)) + ", expected " + std::to_string(consumed));
    Require(out.got && (out.buffer == nullptr) == (consumed == text.size()), name + ": returned iterator");
    if (consumed < text.size()) Require(out.value == text[consumed], name + ": the returned iterator holds the next character");
    if (value.capacity > 16 / sizeof(Text) - 1) GuestStringStorage::Deallocate(value.pointer, (value.capacity + 1) * sizeof(Text));
}

template<typename Text> static void CheckLongDouble(Bench<Text>& bench, const std::string& name, const std::basic_string<Text>& text, long double expected, std::int32_t state) {
    Load(bench, text, 512, 0);
    long double value = -7.0L;
    std::int32_t outState = 0;
    *__error_nid_postfix() = 33;
    Slot<GetLongDouble<Text>>(bench.facet, 4)(bench.facet, {&bench.source.buffer, false, 0}, {nullptr, false, 0}, 0, &bench.stream, &outState, &value);
    Require(value == expected, name + ": other value");
    Require(outState == state, name + ": state " + std::to_string(outState) + ", expected " + std::to_string(state));
    Require(*__error_nid_postfix() == 33, name + ": errno is kept");
}

template<typename Text> static void SetFields(void* facet, const char* grouping, Text decimal, Text thousands, const Text* currency, const Text* positive, const Text* negative, std::int32_t digits, const char* negativePattern) {
    auto* fields = static_cast<MoneypunctFields<Text>*>(facet);
    fields->grouping = grouping;
    fields->decimalPoint = decimal;
    fields->thousandsSep = thousands;
    fields->currency = currency;
    fields->positiveSign = positive;
    fields->negativeSign = negative;
    fields->fractionDigits = digits;
    std::memcpy(fields->positivePattern, "xxxx", 4);
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
    narrow.facet = locale->facets[_ZNSt8__sce_v29money_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix];
    narrow.stream.locale = &locale;
    static Bench<char16_t> wide;
    wide.facet = locale->facets[_ZNSt8__sce_v29money_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix];
    wide.stream.locale = &locale;
    void* local = locale->facets[_ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix];
    void* international = locale->facets[_ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix];
    void* wideLocal = locale->facets[_ZNSt8__sce_v210moneypunctIwLb0EE2idE_nid_postfix];
    Require(narrow.facet != nullptr && wide.facet != nullptr && local != nullptr && international != nullptr && wideLocal != nullptr, "_Makeloc installs money_get and moneypunct");
    if (failures != 0) std::_Exit(1);

    static const char grouping[] = "\3";
    SetFields<char>(local, grouping, '.', ',', "EUR", "", "()", 2, "+v$x");
    CheckDigits<char>(narrow, "sign, grouped value, symbol and the rest of the sign", 0, 0, "(1,234.56EUR)", "-123456", EofBit, 13);
    CheckDigits<char>(narrow, "the symbol is optional without showbase", 0, 0, "(1,234.56)", "-123456", EofBit, 10);
    CheckDigits<char>(narrow, "showbase requires the symbol", 0, ShowBase, "(1,234.56)", "?", FailBit, 9);
    CheckDigits<char>(narrow, "a group of the wrong size fails", 0, 0, "(12,34.56EUR)", "?", FailBit, 6);
    CheckDigits<char>(narrow, "a trailing separator fails", 0, 0, "(1,.5)", "?", FailBit, 3);
    CheckDigits<char>(narrow, "a short fraction fails", 0, 0, "(1.5)", "?", FailBit, 4);
    CheckDigits<char>(narrow, "missing fraction digits are zeros", 0, 0, "(7)", "-700", EofBit, 3);
    CheckDigits<char>(narrow, "no sign is positive", 0, 0, "12 rest", "1200", 0, 2);
    CheckDigits<char>(narrow, "a missing rest of the sign fails", 0, 0, "(12", "?", EofBit | FailBit, 3);
    CheckDigits<char>(narrow, "no digits fail", 0, 0, "(EUR)", "?", FailBit, 1);
    CheckDigits<char>(narrow, "the buffer and the slow path read alike", 0, 0, "(1,234.56EUR)", "-123456", EofBit, 13, 3);
    CheckDigits<char>(narrow, "a value longer than the inline buffer moves to the heap", 0, 0, "12345678901234567890 ", "1234567890123456789000", 0, 20);
    CheckDigits<char>(narrow, "the rest of the sign must follow", 0, 0, "(12EUR", "?", EofBit | FailBit, 6);
    CheckDigits<char>(narrow, "a group longer than 127 digits counts as 127", 0, 0, "(" + std::string(200, '7') + ",123)", "?", FailBit, 205);
    CheckLongDouble<char>(narrow, "long double counts the smallest units", "(12,345.67)", -1234567.0L, EofBit);
    CheckLongDouble<char>(narrow, "long double fails without digits", "()", -7.0L, FailBit);

    SetFields<char>(local, "", '.', ',', "EUR", "+", "", 0, "$+vx");
    CheckDigits<char>(narrow, "an absent sign is negative when only the positive sign is set", 0, 0, "5", "-5", EofBit, 1);
    CheckDigits<char>(narrow, "the positive sign", 0, 0, "+5", "5", EofBit, 2);

    SetFields<char>(local, "", '.', ',', "EUR", "", "-", -2, "$ v+");
    CheckDigits<char>(narrow, "a space field needs a space", 0, ShowBase, "EUR12", "?", FailBit, 3);
    CheckDigits<char>(narrow, "a space field takes every space", 0, ShowBase, "EUR \t\n12-", "-12", EofBit, 9);
    CheckDigits<char>(narrow, "negative fraction digits take no fraction", 0, 0, " 12.34", "12", 0, 4);

    SetFields<char>(local, "", '.', ',', "EUR", "", "-", 0, "v?x+");
    CheckDigits<char>(narrow, "unknown pattern bytes are skipped", 0, 0, "12-", "-12", EofBit, 3);
    SetFields<char>(local, "", '.', ',', "EUR", "", "-", 0, "+vx$");
    CheckDigits<char>(narrow, "a last symbol is optional", 0, ShowBase, "-12", "-12", EofBit, 3);

    static const char groupingLimit[] = "\x7f";
    SetFields<char>(local, groupingLimit, '.', ',', "EUR", "", "-", 0, "$+vx");
    CheckDigits<char>(narrow, "a first group of CHAR_MAX turns grouping off", 0, 0, "1,234", "1", 0, 1);
    SetFields<char>(local, grouping, '\0', '\0', "EUR", "", "-", 2, "$+vx");
    CheckDigits<char>(narrow, "a null decimal point matches nothing", 0, 0, std::string("12\0" "34", 5), "1200", 0, 2);
    CheckDigits<char>(narrow, "a null separator turns grouping off", 0, 0, std::string("1\0" "234", 5), "100", 0, 1);

    SetFields<char>(local, "", '.', ',', "EUR", "", "-", 0, "$+vx");
    Load(narrow, std::string(48, '5'), 512, 0);
    GuestString<char> grown{};
    grown.capacity = 47;
    grown.pointer = static_cast<char*>(GuestLocale::AllocateStringStorage(48));
    grown.pointer[0] = '\0';
    std::int32_t grownState = 0;
    Slot<GetDigits<char>>(narrow.facet, 5)(narrow.facet, {&narrow.source.buffer, false, 0}, {nullptr, false, 0}, 0, &narrow.stream, &grownState, &grown);
    Require(grown.size == 48 && grown.capacity == 70 && std::string(grown.pointer, 48) == std::string(48, '5'), "a heap string grows by half its capacity");
    GuestStringStorage::Deallocate(grown.pointer, 71);

    SetFields<char>(international, "", '.', ' ', "USD ", "", "-", 2, "+$xv");
    CheckDigits<char>(narrow, "any nonzero international flag picks the international facet", 2, ShowBase, "-USD 12.34", "-1234", EofBit, 10);

    static const char wideGrouping[] = "\2";
    static const char16_t euro[] = u"\x20ac";
    static const char16_t empty[] = u"";
    static const char16_t minus[] = u"\x2212";
    SetFields<char16_t>(wideLocal, wideGrouping, u'\x066b', u'\x066c', euro, empty, minus, 1, "+v $");
    CheckDigits<char16_t>(wide, "wide grouping, separators and symbol", 0, ShowBase, u"\x2212" u"1\x066c" u"23\x066b" u"4 \x20ac", u"-1234", EofBit, 9);
    CheckDigits<char16_t>(wide, "a wide character equal in its low byte is not a digit", 0, 0, u"12\x0133", u"?", FailBit, 2);
    CheckLongDouble<char16_t>(wide, "wide long double", u"12\x066b" u"5 ", 125.0L, EofBit);

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
