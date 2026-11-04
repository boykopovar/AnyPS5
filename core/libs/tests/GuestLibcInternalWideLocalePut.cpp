#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
}

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal wide locale put: %s\n", message);
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

GuestLocale::String* AssignShortString(GuestLocale::String* result, const char* text) {
    *result = {};
    result->size = std::strlen(text);
    result->capacity = 15;
    std::memcpy(result->buffer, text, result->size);
    return result;
}

void APS5_VABI TestFacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI TestFacetRelease(GuestLocale::Facet*) { return nullptr; }
std::uint16_t APS5_VABI TestDecimalPoint(const GuestLocale::WideNumpunctFacet*) { return 0x066b; }
std::uint16_t APS5_VABI TestThousandsSep(const GuestLocale::WideNumpunctFacet*) { return 0x066c; }
GuestLocale::String* APS5_VABI TestGrouping(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return AssignShortString(result, "\1\2"); }
GuestLocale::String* APS5_VABI TestName(GuestLocale::String*, const GuestLocale::WideNumpunctFacet*) { throw std::logic_error("num_put<wchar_t> floating and pointer output reads no bool names"); }
const GuestLocale::WideNumpunctVtable g_testNumpunctVtable{{TestFacetNop, TestFacetNop, TestFacetNop, TestFacetRelease}, TestDecimalPoint, TestThousandsSep, TestGrouping, TestName, TestName};

template<typename TPut>
std::u16string Put(TPut put, GuestLocale::Implementation** locale, std::int32_t flags, std::int32_t precision, std::int32_t width) {
    char16_t storage[64];
    char16_t* next = storage;
    char16_t* end = storage + 64;
    TestStreambuf stream{{&g_testStreambufVtable, {}, &next, {}, &end}, {}, 0, 0};
    GuestLocale::IosBase ios{};
    ios.flags = flags;
    ios.precision = precision;
    ios.width = width;
    ios.locale = locale;
    const GuestLocale::WideOutputIterator out = put(GuestLocale::WideOutputIterator{false, &stream.buffer}, &ios);
    Require(!out.failed && out.buffer == &stream.buffer, "the iterator keeps its buffer and does not fail");
    Require(ios.width == 0 && stream.offered == 0, "the width is consumed and the put area takes every character");
    return std::u16string(storage, next);
}

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void CheckClassicNumPut() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    const GuestLocale::Facet facet{nullptr, 1, 0};
    const double infinity = std::numeric_limits<double>::infinity();
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, out, ios, u'*', 1234567.891); }, classic, 0, 6, 0) == u"1.23457e+06", "double in general notation");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, out, ios, u'*', 1234567.891); }, classic, 0x1000, 2, 0) == u"1234567.89", "double in fixed notation");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, out, ios, u'\u2014', -0.000123); }, classic, 0xa00, 3, 12) == u"\u2014\u2014-1.230E-04", "double in uppercase scientific notation with a wide fill");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, out, ios, u'*', -1.5); }, classic, 0x1808, 6, 12) == u"-***0x1.8p+0", "double in hexadecimal notation with internal fill");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, out, ios, u'*', 2.5); }, classic, 0x502, 2, 7) == u"+2.5***", "double with showpos, showpoint and left fill");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, out, ios, u'*', -infinity); }, classic, 0x400, 6, 0) == u"-inf", "negative infinity");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLongDouble_nid_no_patch(&facet, out, ios, u'*', -2.25L); }, classic, 0, 3, 0) == u"-2.25", "long double in general notation");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLongDouble_nid_no_patch(&facet, out, ios, u'*', 0.5L); }, classic, 0x1000, 2, 0) == u"0.50", "long double in fixed notation");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutPointer_nid_no_patch(&facet, out, ios, u'*', reinterpret_cast<const void*>(0x1234)); }, classic, 0, 6, 6) == u"**1234", "pointer right adjusted");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutPointer_nid_no_patch(&facet, out, ios, u'*', reinterpret_cast<const void*>(0xabc)); }, classic, 0x2, 6, 5) == u"abc**", "pointer left adjusted");
    Require(Throws([&] { Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, out, ios, u'*', 1.0); }, classic, 0x1000, -45, 0); }), "a precision without room for the value throws");
}

void CheckCustomNumpunct() {
    GuestLocale::Facet punct{&g_testNumpunctVtable.facet, 1, 0};
    std::vector<GuestLocale::Facet*> facets(_ZNSt8numpunctIwE2idE_nid_postfix + 1);
    Require(_ZNSt8numpunctIwE2idE_nid_postfix != 0, "the wide numpunct id is assigned by the first lookup");
    facets.back() = &punct;
    GuestLocale::Implementation custom{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
    GuestLocale::Implementation* customPointer = &custom;
    const GuestLocale::Facet facet{nullptr, 1, 0};
    const double infinity = std::numeric_limits<double>::infinity();
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, out, ios, u'*', 1234567.5); }, &customPointer, 0x1000, 1, 0) == u"12\u066c34\u066c56\u066c7\u066b5", "fixed double takes the wide grouping, separator and decimal point");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutLongDouble_nid_no_patch(&facet, out, ios, u'*', -1234.0L); }, &customPointer, 0x800, 2, 0) == u"-1\u066b23e+03", "scientific long double takes the wide decimal point");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, out, ios, u'*', infinity); }, &customPointer, 0x1000, 1, 0) == u"inf", "infinity is not grouped");
    Require(Put([&](auto out, auto* ios) { return sceLibcInternalWideNumPutDoPutPointer_nid_no_patch(&facet, out, ios, u'*', reinterpret_cast<const void*>(0x12345)); }, &customPointer, 0, 6, 0) == u"12\u066c34\u066c5", "pointer digits are grouped");
}

void CheckOverflow() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    const GuestLocale::Facet facet{nullptr, 1, 0};
    char16_t storage[3];
    char16_t* next = storage;
    char16_t* end = storage + 3;
    TestStreambuf stream{{&g_testStreambufVtable, {}, &next, {}, &end}, {}, 0, 3};
    GuestLocale::IosBase ios{};
    ios.precision = 6;
    ios.locale = classic;
    GuestLocale::WideOutputIterator out = sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, {false, &stream.buffer}, &ios, u'*', 12.25);
    Require(!out.failed && next == end && std::u16string(storage, 3) == u"12." && stream.accepted == u"25", "characters past the put area go to overflow");
    out = sceLibcInternalWideNumPutDoPutPointer_nid_no_patch(&facet, out, &ios, u'*', reinterpret_cast<const void*>(0x1f));
    Require(out.failed && stream.offered == 4 && stream.accepted == u"251", "a refused character fails the iterator");
    out = sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(&facet, {false, nullptr}, &ios, u'*', 1.0);
    Require(out.failed && out.buffer == nullptr, "a null buffer fails the iterator");
}

void CheckTimePut() {
    const GuestLocale::Facet facet{nullptr, 1, 0};
    GuestTm time{};
    time.tm_year = 126;
    time.tm_mon = 9;
    time.tm_mday = 2;
    time.tm_hour = 13;
    time.tm_min = 5;
    time.tm_sec = 9;
    time.tm_wday = 5;
    time.tm_yday = 274;
    char16_t storage[8];
    char16_t* next = storage;
    char16_t* end = storage + 8;
    TestStreambuf stream{{&g_testStreambufVtable, {}, &next, {}, &end}, {}, 0, 100};
    GuestLocale::WideOutputIterator out = sceLibcInternalWideTimePutDoPut_nid_no_patch(&facet, {false, &stream.buffer}, nullptr, u'*', &time, 'Y', 0);
    Require(!out.failed && out.buffer == &stream.buffer && next == storage + 4 && std::u16string(storage, 4) == u"2026", "%Y fills the put area");
    out = sceLibcInternalWideTimePutDoPut_nid_no_patch(&facet, out, nullptr, u'*', &time, 'c', 0);
    Require(!out.failed && next == end && std::u16string(storage + 4, 4) == u"Fri " && stream.accepted == u"Oct  2 13:05:09 2026", "%c overflows into the stream buffer");
    stream.accepted.clear();
    out = sceLibcInternalWideTimePutDoPut_nid_no_patch(&facet, out, nullptr, u'*', &time, 'Y', 'E');
    out = sceLibcInternalWideTimePutDoPut_nid_no_patch(&facet, out, nullptr, u'*', &time, 'd', 'O');
    out = sceLibcInternalWideTimePutDoPut_nid_no_patch(&facet, out, nullptr, u'*', &time, 'j', 0);
    Require(!out.failed && stream.accepted == u"202602275", "%EY and %Od use the C locale forms");
    stream.capacity = static_cast<int>(stream.accepted.size()) + 1;
    stream.offered = 0;
    out = sceLibcInternalWideTimePutDoPut_nid_no_patch(&facet, out, nullptr, u'*', &time, 'T', 0);
    Require(out.failed && stream.offered == 8 && stream.accepted == u"2026022751", "a refused character fails the iterator");
    out = sceLibcInternalWideTimePutDoPut_nid_no_patch(&facet, {false, nullptr}, nullptr, u'*', &time, 'H', 0);
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

    CheckClassicNumPut();
    CheckCustomNumpunct();
    CheckOverflow();
    CheckTimePut();

    std::puts("libc internal wide locale put checks passed");
    return 0;
}
