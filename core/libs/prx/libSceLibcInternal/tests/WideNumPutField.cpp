#include "prx/libSceLibcInternal/include/ErrExports.hpp"
#include "prx/libc/include/Api.hpp"
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
extern std::uint64_t _ZNSt8numpunctIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
}

namespace {

constexpr std::int32_t Left = 0x2;
constexpr std::int32_t Internal = 0x8;
constexpr std::int32_t Hex = 0x40;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "wide num_put field: %s\n", message);
    std::abort();
}

template <typename Function>
bool Throws(Function&& function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct WideOutStreambuf {
    GuestLocale::Streambuf buffer{};
    char* putNext = nullptr;
    char* putEnd = nullptr;
    std::u16string accepted;
    bool fail = false;
};

int APS5_VABI WideOverflow(GuestLocale::Streambuf* buffer, int value) {
    auto* stream = reinterpret_cast<WideOutStreambuf*>(buffer);
    if (stream->fail) return -1;
    stream->accepted.push_back(static_cast<char16_t>(value));
    return value & 0xffff;
}

const GuestLocale::StreambufVtable g_wideOutVtable{{}, WideOverflow};

WideOutStreambuf MakeWide(bool fail = false) {
    WideOutStreambuf stream{};
    stream.buffer.vtable = &g_wideOutVtable;
    stream.buffer.putNext = &stream.putNext;
    stream.buffer.putEnd = &stream.putEnd;
    stream.fail = fail;
    return stream;
}

void APS5_VABI FacetNop(GuestLocale::Facet*) {}
void APS5_VABI FacetDelete(GuestLocale::Facet*) {}
void APS5_VABI FacetRetain(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI FacetRelease(GuestLocale::Facet*) { return nullptr; }

std::string g_grouping;

std::uint16_t APS5_VABI GroupDecimalPoint(const GuestLocale::WideNumpunctFacet*) { return u'.'; }
std::uint16_t APS5_VABI GroupThousandsSep(const GuestLocale::WideNumpunctFacet*) { return u'\''; }
GuestLocale::String* APS5_VABI GroupGrouping(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, g_grouping.c_str()); }
GuestLocale::String* APS5_VABI GroupFalse(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, "false"); }
GuestLocale::String* APS5_VABI GroupTrue(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, "true"); }

const GuestLocale::WideNumpunctVtable g_wideGroupVtable{{FacetNop, FacetDelete, FacetRetain, FacetRelease}, GroupDecimalPoint, GroupThousandsSep, GroupGrouping, GroupFalse, GroupTrue};

const char* APS5_VABI CustomWidenRange(const GuestLocale::Facet*, const char* first, const char* last, std::uint16_t* destination) {
    for (; first != last; ++first, ++destination) {
        *destination = static_cast<std::uint16_t>(*first == 'a' ? u'A' : static_cast<unsigned char>(*first));
    }
    return last;
}

using WideIput = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, char*, std::size_t);
const WideIput WideIntegerPut = _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_IputES3_RSt8ios_basewPcm_nid_postfix;

std::u16string PutWideInteger(GuestLocale::Implementation** locale, std::int32_t flags, std::int32_t width, const char* digits) {
    GuestLocale::IosBase ios{};
    ios.flags = flags;
    ios.width = width;
    ios.locale = locale;
    auto stream = MakeWide();
    std::string buffer(digits);
    const GuestLocale::OutputIterator out = WideIntegerPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, u'*', buffer.data(), buffer.size());
    Require(!out.failed && out.buffer == &stream.buffer, "wide _Iput returns the iterator it wrote through");
    Require(ios.width == 0, "wide _Iput resets the stream width");
    return stream.accepted;
}

void CheckAdjustment() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(PutWideInteger(classic, 0, 6, "-42") == u"***-42", "wide _Iput pads on the left by default");
    Require(PutWideInteger(classic, Internal, 6, "-42") == u"-***42", "wide _Iput pads after the sign for internal");
    Require(PutWideInteger(classic, Left, 6, "-42") == u"-42***", "wide _Iput pads on the right for left");
    Require(PutWideInteger(classic, Left | Internal, 6, "+7") == u"****+7", "wide _Iput treats a mixed adjustfield as right");
    Require(PutWideInteger(classic, Hex | Internal, 8, "0x1f") == u"0x****1f", "wide _Iput keeps the hex prefix before internal fill");
    Require(PutWideInteger(classic, Hex | Internal, 8, "-0X1F") == u"-0X***1F", "wide _Iput keeps the sign and hex prefix before internal fill");
    Require(PutWideInteger(classic, Internal, 6, "0x1f") == u"**0x1f", "wide _Iput sees no hex prefix without the hex base");
    Require(PutWideInteger(classic, 0, 2, "12345") == u"12345", "wide _Iput does not truncate to the width");
    Require(PutWideInteger(classic, 0, -3, "9") == u"9", "wide _Iput ignores a negative width");
    Require(PutWideInteger(classic, 0, 3, "") == u"***", "wide _Iput fills an empty field");
}

void CheckGroupingAndWiden() {
    const std::uint64_t savedPunctId = _ZNSt8numpunctIwE2idE_nid_postfix;
    const std::uint64_t savedCtypeId = _ZNSt5ctypeIwE2idE_nid_postfix;
    if (_ZNSt8numpunctIwE2idE_nid_postfix == 0) _ZNSt8numpunctIwE2idE_nid_postfix = 1;
    if (_ZNSt5ctypeIwE2idE_nid_postfix == 0) _ZNSt5ctypeIwE2idE_nid_postfix = 2;
    const std::uint64_t punctId = _ZNSt8numpunctIwE2idE_nid_postfix;
    const std::uint64_t ctypeId = _ZNSt5ctypeIwE2idE_nid_postfix;
    GuestLocale::WideNumpunctFacet punct{{&g_wideGroupVtable.facet, 1, 0}, nullptr, u'.', u'\'', nullptr, nullptr};
    GuestLocale::WideCtypeVtable ctypeVtable{};
    ctypeVtable.facet = {FacetNop, FacetDelete, FacetRetain, FacetRelease};
    ctypeVtable.widenMethods[1] = reinterpret_cast<const void*>(CustomWidenRange);
    GuestLocale::Facet ctypeFacet{&ctypeVtable.facet, 1, 0};
    const std::size_t maxId = punctId > ctypeId ? punctId : ctypeId;
    std::vector<GuestLocale::Facet*> facets(maxId + 1, nullptr);
    facets[punctId] = &punct.base;
    facets[ctypeId] = &ctypeFacet;
    GuestLocale::Implementation implementation{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
    GuestLocale::Implementation* locale = &implementation;
    g_grouping = "\3";
    Require(PutWideInteger(&locale, 0, 0, "1234567") == u"1'234'567", "wide _Iput repeats the last group");
    Require(PutWideInteger(&locale, 0, 0, "-1234") == u"-1'234", "wide _Iput does not group the sign");
    Require(PutWideInteger(&locale, 0, 0, "123") == u"123", "wide _Iput inserts no separator in a single group");
    Require(PutWideInteger(&locale, Hex, 0, "0x12a45") == u"0x12'A45", "wide _Iput widens through ctype<wchar_t> and does not group hex prefix");
    Require(PutWideInteger(&locale, Internal, 10, "-12345") == u"-***12'345", "wide _Iput pads the grouped field");
    g_grouping = "\1\2";
    Require(PutWideInteger(&locale, 0, 0, "12345") == u"12'34'5", "wide _Iput follows each group in turn");
    g_grouping = "\2\x7f";
    Require(PutWideInteger(&locale, 0, 0, "12345") == u"123'45", "wide _Iput stops at CHAR_MAX");
    g_grouping = "\x7f";
    Require(PutWideInteger(&locale, 0, 0, "12345") == u"12345", "wide _Iput does not group when first group is CHAR_MAX");
    g_grouping = "";
    _ZNSt8numpunctIwE2idE_nid_postfix = savedPunctId;
    _ZNSt5ctypeIwE2idE_nid_postfix = savedCtypeId;
}

void CheckErrors() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::IosBase ios{};
    ios.locale = classic;
    auto stream = MakeWide(true);
    char digit = '7';
    const GuestLocale::OutputIterator out = WideIntegerPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, u'*', &digit, 1);
    Require(out.failed, "wide _Iput marks iterator failed when overflow fails");
    Require(Throws([&] { WideIntegerPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, nullptr, u'*', &digit, 1); }), "wide _Iput rejects null stream");
    GuestLocale::IosBase noLocale{};
    Require(Throws([&] { WideIntegerPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &noLocale, u'*', &digit, 1); }), "wide _Iput rejects stream without locale");
    Require(Throws([&] { WideIntegerPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, u'*', nullptr, 1); }), "wide _Iput rejects null digits with positive count");
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

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckAdjustment();
    CheckGroupingAndWiden();
    CheckErrors();
    std::puts("libc internal wide num_put field checks passed");
    return 0;
}
