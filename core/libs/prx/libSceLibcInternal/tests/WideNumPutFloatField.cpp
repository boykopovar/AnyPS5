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
constexpr std::int32_t HexFloat = 0x1800;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "wide num_put float field: %s\n", message);
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

std::uint16_t APS5_VABI GroupDecimalPoint(const GuestLocale::WideNumpunctFacet*) { return u','; }
std::uint16_t APS5_VABI GroupThousandsSep(const GuestLocale::WideNumpunctFacet*) { return u'\''; }
GuestLocale::String* APS5_VABI GroupGrouping(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, g_grouping.c_str()); }
GuestLocale::String* APS5_VABI GroupFalse(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, "false"); }
GuestLocale::String* APS5_VABI GroupTrue(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, "true"); }

const GuestLocale::WideNumpunctVtable g_wideGroupVtable{{FacetNop, FacetDelete, FacetRetain, FacetRelease}, GroupDecimalPoint, GroupThousandsSep, GroupGrouping, GroupFalse, GroupTrue};

const char* APS5_VABI CustomWidenRange(const GuestLocale::Facet*, const char* first, const char* last, std::uint16_t* destination) {
    for (; first != last; ++first, ++destination) {
        *destination = static_cast<std::uint16_t>(*first == 'e' ? u'E' : static_cast<unsigned char>(*first));
    }
    return last;
}

using WideFput = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, std::uint16_t, const char*, std::size_t);
const WideFput WideFloatPut = _ZNKSt7num_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_FputES3_RSt8ios_basewPKcm_nid_postfix;

std::u16string PutWideFloat(GuestLocale::Implementation** locale, std::int32_t flags, std::int32_t width, const char* digits) {
    GuestLocale::IosBase ios{};
    ios.flags = flags;
    ios.width = width;
    ios.locale = locale;
    auto stream = MakeWide();
    const GuestLocale::OutputIterator out = WideFloatPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, u'*', digits, std::strlen(digits));
    Require(!out.failed && out.buffer == &stream.buffer, "wide _Fput returns the iterator it wrote through");
    Require(ios.width == 0, "wide _Fput resets stream width");
    return stream.accepted;
}

void CheckFormatting() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(PutWideFloat(classic, 0, 8, "-1.5") == u"****-1.5", "wide _Fput pads on the left by default");
    Require(PutWideFloat(classic, Internal, 8, "-1.5") == u"-****1.5", "wide _Fput pads after sign for internal");
    Require(PutWideFloat(classic, Left, 8, "-1.5") == u"-1.5****", "wide _Fput pads on the right for left");
    Require(PutWideFloat(classic, HexFloat | Internal, 10, "-0x1.8p+0") == u"-0x*1.8p+0", "wide _Fput keeps sign and hex prefix before internal fill");
    Require(PutWideFloat(classic, 0, 6, "inf") == u"***inf", "wide _Fput formats inf without grouping");
    Require(PutWideFloat(classic, 0, 6, "-nan") == u"**-nan", "wide _Fput formats -nan without grouping");
}

void CheckPunctuationAndWiden() {
    const std::uint64_t savedPunctId = _ZNSt8numpunctIwE2idE_nid_postfix;
    const std::uint64_t savedCtypeId = _ZNSt5ctypeIwE2idE_nid_postfix;
    if (_ZNSt8numpunctIwE2idE_nid_postfix == 0) _ZNSt8numpunctIwE2idE_nid_postfix = 1;
    if (_ZNSt5ctypeIwE2idE_nid_postfix == 0) _ZNSt5ctypeIwE2idE_nid_postfix = 2;
    const std::uint64_t punctId = _ZNSt8numpunctIwE2idE_nid_postfix;
    const std::uint64_t ctypeId = _ZNSt5ctypeIwE2idE_nid_postfix;
    GuestLocale::WideNumpunctFacet punct{{&g_wideGroupVtable.facet, 1, 0}, nullptr, u',', u'\'', nullptr, nullptr};
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
    Require(PutWideFloat(&locale, 0, 0, "12345.67") == u"12'345,67", "wide _Fput replaces decimal point and groups integer part");
    Require(PutWideFloat(&locale, 0, 0, "-1234e+05") == u"-1'234E+05", "wide _Fput widens through ctype<wchar_t> and stops grouping at exponent");
    Require(PutWideFloat(&locale, 0, 0, "inf") == u"inf", "wide _Fput does not group inf");
    g_grouping = "";
    Require(PutWideFloat(&locale, 0, 0, "12345.67") == u"12345,67", "wide _Fput handles empty grouping pattern with decimal point");
    Require(PutWideFloat(&locale, 0, 0, "12345") == u"12345", "wide _Fput handles empty grouping pattern without decimal point");
    _ZNSt8numpunctIwE2idE_nid_postfix = savedPunctId;
    _ZNSt5ctypeIwE2idE_nid_postfix = savedCtypeId;
}

void CheckErrors() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::IosBase ios{};
    ios.locale = classic;
    auto stream = MakeWide(true);
    const char digits[] = "1.5";
    const GuestLocale::OutputIterator out = WideFloatPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, u'*', digits, 3);
    Require(out.failed, "wide _Fput marks iterator failed when overflow fails");
    Require(Throws([&] { WideFloatPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, nullptr, u'*', digits, 3); }), "wide _Fput rejects null stream");
    GuestLocale::IosBase noLocale{};
    Require(Throws([&] { WideFloatPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &noLocale, u'*', digits, 3); }), "wide _Fput rejects stream without locale");
    Require(Throws([&] { WideFloatPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, u'*', nullptr, 1); }), "wide _Fput rejects null digits with positive count");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckFormatting();
    CheckPunctuationAndWiden();
    CheckErrors();
    std::puts("libc internal wide num_put float field checks passed");
    return 0;
}
