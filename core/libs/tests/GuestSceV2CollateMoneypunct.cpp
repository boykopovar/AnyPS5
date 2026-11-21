#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
extern std::uint64_t _ZNSt8__sce_v27collateIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v27collateIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v210moneypunctIwLb0EE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v210moneypunctIwLb1EE2idE_nid_postfix;
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

    const Text* data() const { return capacity > 16 / sizeof(Text) - 1 ? pointer : buffer; }
    std::basic_string<Text> text() const { return std::basic_string<Text>(data(), size); }
};

static_assert(sizeof(GuestString<char>) == 0x20);
static_assert(sizeof(GuestString<char16_t>) == 0x20);

template<typename Text> using Compare = int (APS5_VABI *)(const void*, const Text*, const Text*, const Text*, const Text*);
template<typename Text> using Transform = GuestString<Text>* (APS5_VABI *)(GuestString<Text>*, const void*, const Text*, const Text*);
template<typename Text> using Hash = std::int64_t (APS5_VABI *)(const void*, const Text*, const Text*);

template<typename Text> static int RunCompare(const void* facet, const std::basic_string<Text>& left, const std::basic_string<Text>& right) {
    return Slot<Compare<Text>>(facet, 4)(facet, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
}

template<typename Text> static GuestString<Text> RunTransform(const void* facet, const Text* first, const Text* last) {
    GuestString<Text> result;
    std::memset(&result, 0xa5, sizeof(result));
    Require(Slot<Transform<Text>>(facet, 5)(&result, facet, first, last) == &result, "transform returns its result");
    return result;
}

template<typename Text> static std::uint64_t RunHash(const void* facet, const std::basic_string<Text>& text) {
    return static_cast<std::uint64_t>(Slot<Hash<Text>>(facet, 6)(facet, text.data(), text.data() + text.size()));
}

static void CheckCollateChar(const void* facet) {
    using namespace std::string_literals;
    Require(RunCompare<char>(facet, "abc"s, "abc"s) == 0, "compare equal");
    Require(RunCompare<char>(facet, "a"s, "z"s) == -1, "compare gives -1, not the byte difference");
    Require(RunCompare<char>(facet, "z"s, "a"s) == 1, "compare gives 1, not the byte difference");
    Require(RunCompare<char>(facet, "ab"s, "abc"s) == -1, "a prefix orders first");
    Require(RunCompare<char>(facet, "abc"s, "ab"s) == 1, "a longer string orders last");
    Require(RunCompare<char>(facet, "\x80"s, "a"s) == 1, "bytes compare unsigned");
    Require(RunCompare<char>(facet, "a\0b"s, "a\0c"s) == -1, "compare continues past a null byte");
    Require(RunCompare<char>(facet, ""s, ""s) == 0, "compare empty ranges");

    const std::string embedded = "x\0y"s;
    const auto short_ = RunTransform<char>(facet, embedded.data(), embedded.data() + embedded.size());
    Require(short_.size == 3 && short_.capacity == 15 && short_.text() == embedded && short_.buffer[3] == 0, "transform copies a short range with its null bytes");
    Require(std::all_of(short_.buffer + 4, short_.buffer + 16, [](char c) { return c == 0; }), "transform clears the rest of the inline buffer");
    const std::string longText(40, 'q');
    const auto long_ = RunTransform<char>(facet, longText.data(), longText.data() + longText.size());
    Require(long_.size == 40 && long_.capacity == 47 && long_.text() == longText && long_.data()[40] == 0, "transform allocates a long range");
    const auto empty = RunTransform<char>(facet, nullptr, nullptr);
    Require(empty.size == 0 && empty.capacity == 15 && empty.buffer[0] == 0, "transform an empty range");

    Require(RunHash<char>(facet, ""s) == 0xcbf29ce484222325, "hash of an empty range is the FNV-1a basis");
    Require(RunHash<char>(facet, "a"s) == 0xaf63dc4c8601ec8c, "hash of a");
    Require(RunHash<char>(facet, "a\0b"s) == 0xe5d29919042666b2, "hash covers null bytes");
}

static void CheckCollateWide(const void* facet) {
    using namespace std::string_literals;
    Require(RunCompare<char16_t>(facet, u"abc"s, u"abc"s) == 0, "wide compare equal");
    Require(RunCompare<char16_t>(facet, u"a"s, u"z"s) == -1, "wide compare less");
    Require(RunCompare<char16_t>(facet, u"\xffff"s, u"a"s) == 1, "wide characters compare unsigned");
    Require(RunCompare<char16_t>(facet, u"ab"s, u"a"s) == 1, "wide longer string orders last");
    Require(RunCompare<char16_t>(facet, u"a\0b"s, u"a\0a"s) == 1, "wide compare continues past a null character");

    const std::u16string embedded = u"x\0y"s;
    const auto short_ = RunTransform<char16_t>(facet, embedded.data(), embedded.data() + embedded.size());
    Require(short_.size == 3 && short_.capacity == 7 && short_.text() == embedded && short_.buffer[3] == 0, "wide transform copies a short range with its null characters");
    const std::u16string longText(20, u'\x20ac');
    const auto long_ = RunTransform<char16_t>(facet, longText.data(), longText.data() + longText.size());
    Require(long_.size == 20 && long_.capacity == 23 && long_.text() == longText && long_.data()[20] == 0, "wide transform allocates a long range");
    const auto* odd = reinterpret_cast<const char16_t*>(reinterpret_cast<const char*>(longText.data()) + 3);
    const auto zeros = RunTransform<char16_t>(facet, longText.data(), odd);
    Require(zeros.size == 1 && zeros.capacity == 7 && zeros.buffer[0] == 0 && zeros.buffer[1] == 0, "wide transform of an odd byte count leaves zeros");

    Require(RunHash<char16_t>(facet, u""s) == 0xcbf29ce484222325, "wide hash of an empty range");
    Require(RunHash<char16_t>(facet, u"a"s) == 0x089be207b544f1e4, "wide hash runs over the bytes");
    Require(RunHash<char16_t>(facet, u"a\x20ac"s) == 0xaa6e43820c8db2e8, "wide hash of two characters");
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
    bool international;
    unsigned char conversion[0x28];
};

static_assert(sizeof(MoneypunctFields<char>) == 0x70);
static_assert(sizeof(MoneypunctFields<char16_t>) == 0x70);

template<typename Text> using CharGet = Text (APS5_VABI *)(const void*);
template<typename Text> using StringGet = GuestString<Text>* (APS5_VABI *)(GuestString<Text>*, const void*);
using IntGet = int (APS5_VABI *)(const void*);
using PatternGet = std::uint32_t (APS5_VABI *)(const void*);

template<typename Text> static std::basic_string<Text> RunString(const void* facet, int slot) {
    GuestString<Text> result;
    std::memset(&result, 0xa5, sizeof(result));
    Require(Slot<StringGet<Text>>(facet, slot)(&result, facet) == &result, "moneypunct string returns its result");
    Require(result.data()[result.size] == 0, "moneypunct string is terminated");
    return result.text();
}

static std::string Pattern(const void* facet, int slot) {
    const std::uint32_t pattern = Slot<PatternGet>(facet, slot)(facet);
    std::string text(4, '\0');
    std::memcpy(text.data(), &pattern, sizeof(pattern));
    return text;
}

template<typename Text> static void CheckMoneypunctDefaults(const void* facet, const std::string& name) {
    Require(Slot<CharGet<Text>>(facet, 4)(facet) == 0, name + " decimal_point of the C locale");
    Require(Slot<CharGet<Text>>(facet, 5)(facet) == 0, name + " thousands_sep of the C locale");
    Require(RunString<char>(facet, 6).empty(), name + " grouping of the C locale");
    Require(RunString<Text>(facet, 7).empty(), name + " curr_symbol of the C locale");
    Require(RunString<Text>(facet, 8).empty(), name + " positive_sign of the C locale");
    Require(RunString<Text>(facet, 9) == std::basic_string<Text>(1, Text('-')), name + " negative_sign of the C locale");
    Require(Slot<IntGet>(facet, 10)(facet) == 0, name + " frac_digits of the C locale");
    Require(Pattern(facet, 11) == "$+xv", name + " pos_format of the C locale");
    Require(Pattern(facet, 12) == "$+xv", name + " neg_format of the C locale");
}

template<typename Text> static void CheckMoneypunctFields(const void* facet, const std::string& name) {
    MoneypunctFields<Text> fields;
    std::memcpy(&fields, facet, sizeof(fields));
    const std::basic_string<Text> currency(30, Text('C'));
    const std::basic_string<Text> positive(2, Text('+'));
    const std::basic_string<Text> negative(1, Text(0xe9));
    fields.grouping = "\3\2";
    fields.decimalPoint = Text(',');
    fields.thousandsSep = Text(0xa0);
    fields.currency = currency.c_str();
    fields.positiveSign = positive.c_str();
    fields.negativeSign = negative.c_str();
    fields.fractionDigits = -7;
    std::memcpy(fields.positivePattern, "\2\3\1\4", 4);
    std::memcpy(fields.negativePattern, "\3\4\0\2", 4);
    Require(Slot<CharGet<Text>>(&fields, 4)(&fields) == Text(','), name + " decimal_point reads the facet");
    Require(Slot<CharGet<Text>>(&fields, 5)(&fields) == Text(0xa0), name + " thousands_sep reads the facet");
    Require(RunString<char>(&fields, 6) == "\3\2", name + " grouping copies the facet's bytes");
    Require(RunString<Text>(&fields, 7) == currency, name + " curr_symbol allocates a long string");
    Require(RunString<Text>(&fields, 8) == positive, name + " positive_sign");
    Require(RunString<Text>(&fields, 9) == negative, name + " negative_sign");
    Require(Slot<IntGet>(&fields, 10)(&fields) == -7, name + " frac_digits reads the facet");
    Require(Pattern(&fields, 11) == std::string("\2\3\1\4", 4), name + " pos_format reads the facet");
    Require(Pattern(&fields, 12) == std::string("\3\4\0\2", 4), name + " neg_format reads the facet");
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

    const void* collateChar = locale->facets[_ZNSt8__sce_v27collateIcE2idE_nid_postfix];
    const void* collateWide = locale->facets[_ZNSt8__sce_v27collateIwE2idE_nid_postfix];
    Require(collateChar != nullptr && collateWide != nullptr, "_Makeloc installs both collate facets");
    if (collateChar != nullptr) CheckCollateChar(collateChar);
    if (collateWide != nullptr) CheckCollateWide(collateWide);

    const std::array<std::uint64_t, 2> charIds{_ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix, _ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix};
    const std::array<std::uint64_t, 2> wideIds{_ZNSt8__sce_v210moneypunctIwLb0EE2idE_nid_postfix, _ZNSt8__sce_v210moneypunctIwLb1EE2idE_nid_postfix};
    for (std::size_t international = 0; international < 2; ++international) {
        const std::string suffix = international != 0 ? ", true>" : ", false>";
        const void* narrow = locale->facets[charIds[international]];
        const void* wide = locale->facets[wideIds[international]];
        Require(narrow != nullptr && wide != nullptr, "_Makeloc installs moneypunct" + suffix);
        if (narrow != nullptr) {
            CheckMoneypunctDefaults<char>(narrow, "moneypunct<char" + suffix);
            CheckMoneypunctFields<char>(narrow, "moneypunct<char" + suffix);
        }
        if (wide != nullptr) {
            CheckMoneypunctDefaults<char16_t>(wide, "moneypunct<wchar_t" + suffix);
            CheckMoneypunctFields<char16_t>(wide, "moneypunct<wchar_t" + suffix);
        }
    }

    if (failures != 0) std::fprintf(stderr, "%u failures\n", failures);
    std::fflush(stderr);
    std::_Exit(failures == 0 ? 0 : 1);
}
