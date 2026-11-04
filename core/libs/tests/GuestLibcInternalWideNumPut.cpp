#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
}

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal wide num_put: %s\n", message);
    std::abort();
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

struct TestStreambuf {
    GuestLocale::WideStreambuf buffer;
    std::u16string accepted;
    int offered;
    int capacity;
};

int APS5_VABI TestOverflow(GuestLocale::WideStreambuf* self, int character) {
    auto* stream = reinterpret_cast<TestStreambuf*>(self);
    ++stream->offered;
    if (static_cast<int>(stream->accepted.size()) >= stream->capacity) return -1;
    stream->accepted += static_cast<char16_t>(character);
    return character;
}

const GuestLocale::WideStreambufVtable g_testStreambufVtable{{}, TestOverflow};

template<typename TPut>
std::u16string Put(GuestLocale::Implementation** locale, TPut put, std::int32_t flags, std::int32_t width) {
    std::uint16_t storage[64];
    std::uint16_t* next = storage;
    std::uint16_t* end = storage + 64;
    TestStreambuf stream{{&g_testStreambufVtable, {}, &next, {}, &end}, {}, 0, 0};
    GuestLocale::IosBase ios{};
    ios.flags = flags;
    ios.precision = 6;
    ios.width = width;
    ios.locale = locale;
    const GuestLocale::WideOutputIterator out = put(GuestLocale::WideOutputIterator{false, &stream.buffer}, &ios);
    Require(!out.failed && out.buffer == &stream.buffer, "the iterator keeps its buffer and does not fail");
    Require(ios.width == 0 && stream.offered == 0, "the width is consumed and the put area takes every character");
    std::u16string text;
    for (const std::uint16_t* character = storage; character != next; ++character) text += static_cast<char16_t>(*character);
    return text;
}

GuestLocale::String* AssignWide(GuestLocale::String* result, const char16_t* text) {
    *result = {};
    std::size_t length = 0;
    while (text[length] != 0) ++length;
    char* data = result->buffer;
    std::size_t capacity = 7;
    if (length > 7) {
        capacity = length | 7;
        data = static_cast<char*>(_Znwm_nid_postfix((capacity + 1) * 2));
        result->pointer = data;
    }
    std::memcpy(data, text, (length + 1) * 2);
    result->size = length;
    result->capacity = capacity;
    return result;
}

void APS5_VABI FacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI FacetRelease(GuestLocale::Facet*) { return nullptr; }
std::uint16_t APS5_VABI GroupedDecimalPoint(const GuestLocale::WideNumpunctFacet*) { return u','; }
std::uint16_t APS5_VABI GroupedThousandsSep(const GuestLocale::WideNumpunctFacet*) { return 0x2009; }
GuestLocale::String* APS5_VABI GroupedGrouping(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) {
    *result = {};
    result->size = 2;
    result->capacity = 15;
    std::memcpy(result->buffer, "\3\2", 3);
    return result;
}
GuestLocale::String* APS5_VABI GroupedFalsename(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return AssignWide(result, u"\u043b\u043e\u0436\u043d\u044b\u0439 \u0437\u043d\u0430\u0447\u0435\u043d\u0438\u0435"); }
GuestLocale::String* APS5_VABI GroupedTruename(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return AssignWide(result, u"\u0434\u0430"); }
const GuestLocale::WideNumpunctVtable g_groupedNumpunctVtable{{FacetNop, FacetNop, FacetNop, FacetRelease}, GroupedDecimalPoint, GroupedThousandsSep, GroupedGrouping, GroupedFalsename, GroupedTruename};

const char* APS5_VABI FullwidthWidenRange(const GuestLocale::Facet*, const char* first, const char* last, std::uint16_t* destination) {
    for (; first != last; ++first, ++destination) *destination = *first >= '0' && *first <= '9' ? static_cast<std::uint16_t>(0xff10 + (*first - '0')) : static_cast<std::uint16_t>(static_cast<unsigned char>(*first));
    return last;
}
GuestLocale::WideCtypeVtable g_fullwidthCtypeVtable{{FacetNop, FacetNop, FacetNop, FacetRelease}, {}, nullptr, FullwidthWidenRange};

GuestLocale::WideOutputIterator APS5_VABI DerivedPutLong(const GuestLocale::Facet*, GuestLocale::WideOutputIterator out, GuestLocale::IosBase* stream, std::uint16_t, std::int64_t value) {
    std::uint16_t* next = *out.buffer->putNext;
    *next = value == 1 ? 0x2713 : 0x2717;
    *out.buffer->putNext = next + 1;
    stream->width = 0;
    return out;
}

void CheckClassicLocale() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    const GuestLocale::Facet facet{nullptr, 1, 0};
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLong_nid_no_patch(&facet, out, ios, u'*', -42); }, 0x408, 8) == u"-*****42", "long with showpos and internal fill");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLong_nid_no_patch(&facet, out, ios, u'*', 255); }, 0x2c8, 10) == u"0X******FF", "long in uppercase hex with showbase and internal fill");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLong_nid_no_patch(&facet, out, ios, 0x3000, 7); }, 0, 3) == u"\u3000\u30007", "long right adjusted with a wide fill");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutUnsignedLong_nid_no_patch(&facet, out, ios, u'*', std::numeric_limits<std::uint64_t>::max()); }, 0x20, 0) == u"1777777777777777777777", "unsigned long in octal");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutUnsignedLong_nid_no_patch(&facet, out, ios, u'*', 9); }, 0xa0, 0) == u"011", "unsigned long in octal with showbase");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLongLong_nid_no_patch(&facet, out, ios, u'*', std::numeric_limits<std::int64_t>::min()); }, 0x2, 25) == u"-9223372036854775808*****", "long long left adjusted");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLongLong_nid_no_patch(&facet, out, ios, u'*', 5); }, 0x400, 0) == u"+5", "long long with showpos");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutUnsignedLongLong_nid_no_patch(&facet, out, ios, u'*', 0xabcdefULL); }, 0x40, 0) == u"abcdef", "unsigned long long in hex");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutBool_nid_no_patch(&facet, out, ios, u'*', true); }, 0x8002, 7) == u"true***", "true with boolalpha left adjusted");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutBool_nid_no_patch(&facet, out, ios, u'*', false); }, 0x8000, 7) == u"**false", "false with boolalpha right adjusted");
}

void CheckDerivedFacet() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::WideNumPutVtable base{{FacetNop, FacetNop, FacetNop, FacetRelease}, LibcWideNumPutBool_nid_no_patch, LibcWideNumPutLong_nid_no_patch, LibcWideNumPutUnsignedLong_nid_no_patch, LibcWideNumPutLongLong_nid_no_patch, LibcWideNumPutUnsignedLongLong_nid_no_patch, nullptr, nullptr, nullptr};
    const GuestLocale::Facet baseFacet{&base.facet, 1, 0};
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutBool_nid_no_patch(&baseFacet, out, ios, u'*', true); }, 0, 3) == u"**1", "true without boolalpha goes through do_put(long)");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutBool_nid_no_patch(&baseFacet, out, ios, u'*', false); }, 0, 0) == u"0", "false without boolalpha goes through do_put(long)");
    GuestLocale::WideNumPutVtable derived = base;
    derived.putLong = DerivedPutLong;
    const GuestLocale::Facet derivedFacet{&derived.facet, 1, 0};
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutBool_nid_no_patch(&derivedFacet, out, ios, u'*', true); }, 0, 0) == u"\u2713", "true without boolalpha calls the derived do_put(long)");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutBool_nid_no_patch(&derivedFacet, out, ios, u'*', false); }, 0, 0) == u"\u2717", "false without boolalpha calls the derived do_put(long)");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutBool_nid_no_patch(&derivedFacet, out, ios, u'*', true); }, 0x8000, 0) == u"true", "bool with boolalpha uses numpunct<wchar_t>");
    Require(Put(classic, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLong_nid_no_patch(&derivedFacet, out, ios, u'*', 7); }, 0, 0) == u"7", "the base do_put(long) does not dispatch to the override");
}

void CheckLocaleFacets() {
    if (_ZNSt8numpunctIwE2idE_nid_postfix == 0) _ZNSt8numpunctIwE2idE_nid_postfix = 60;
    if (_ZNSt5ctypeIwE2idE_nid_postfix == 0) _ZNSt5ctypeIwE2idE_nid_postfix = 61;
    const GuestLocale::WideNumpunctFacet punct{{&g_groupedNumpunctVtable.facet, 1, 0}, "", u'.', u',', nullptr, nullptr};
    const GuestLocale::Facet ctype{&g_fullwidthCtypeVtable.facet, 1, 0};
    std::array<GuestLocale::Facet*, 64> facets{};
    facets[_ZNSt8numpunctIwE2idE_nid_postfix] = const_cast<GuestLocale::Facet*>(&punct.base);
    GuestLocale::Implementation grouped{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "grouped"};
    GuestLocale::Implementation* groupedLocale = &grouped;
    const GuestLocale::Facet facet{nullptr, 1, 0};
    Require(Put(&groupedLocale, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLong_nid_no_patch(&facet, out, ios, u'*', -1234567); }, 0, 0) == u"-12\u200934\u2009567", "grouping repeats its last group with a wide separator");
    Require(Put(&groupedLocale, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutUnsignedLongLong_nid_no_patch(&facet, out, ios, u'*', 0x1234567ULL); }, 0xc0, 14) == u"***0x12\u200934\u2009567", "hex grouping skips the base prefix");
    Require(Put(&groupedLocale, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLong_nid_no_patch(&facet, out, ios, u'*', 999); }, 0, 0) == u"999", "a short number is not grouped");
    Require(Put(&groupedLocale, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutBool_nid_no_patch(&facet, out, ios, u'*', true); }, 0x8000, 3) == u"*\u0434\u0430", "truename comes from the stream's numpunct<wchar_t>");
    Require(Put(&groupedLocale, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutBool_nid_no_patch(&facet, out, ios, u'*', false); }, 0x8000, 0) == u"\u043b\u043e\u0436\u043d\u044b\u0439 \u0437\u043d\u0430\u0447\u0435\u043d\u0438\u0435", "a long falsename is read from heap storage");
    facets[_ZNSt5ctypeIwE2idE_nid_postfix] = const_cast<GuestLocale::Facet*>(&ctype);
    Require(Put(&groupedLocale, [&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLongLong_nid_no_patch(&facet, out, ios, u'*', -1234); }, 0x408, 8) == u"-**\uff11\u2009\uff12\uff13\uff14", "digits are widened through ctype<wchar_t>");
}

void CheckStreambuf() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    const GuestLocale::Facet facet{nullptr, 1, 0};
    GuestLocale::IosBase ios{};
    ios.locale = classic;
    ios.width = 6;
    std::uint16_t storage[2];
    std::uint16_t* next = storage;
    std::uint16_t* end = storage + 2;
    TestStreambuf stream{{&g_testStreambufVtable, {}, &next, {}, &end}, {}, 0, 100};
    GuestLocale::WideOutputIterator out = sceLibcInternalWideNumPutDoPutLong_nid_no_patch(&facet, {false, &stream.buffer}, &ios, 0xff0a, 42);
    Require(!out.failed && next == end && storage[0] == 0xff0a && storage[1] == 0xff0a, "the put area takes the first characters");
    Require(stream.offered == 4 && stream.accepted == u"\uff0a\uff0a42", "overflow receives the rest as zero-extended code units");
    stream.capacity = static_cast<int>(stream.accepted.size()) + 1;
    stream.offered = 0;
    out = sceLibcInternalWideNumPutDoPutUnsignedLong_nid_no_patch(&facet, out, &ios, u'*', 987);
    Require(out.failed && stream.offered == 3 && stream.accepted == u"\uff0a\uff0a429", "a refused character fails the iterator");
    out = sceLibcInternalWideNumPutDoPutLong_nid_no_patch(&facet, {false, nullptr}, &ios, u'*', 1);
    Require(out.failed && out.buffer == nullptr, "a null buffer fails the iterator");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckClassicLocale();
    CheckDerivedFacet();
    CheckLocaleFacets();
    CheckStreambuf();

    std::puts("libc internal wide num_put checks passed");
    return 0;
}
