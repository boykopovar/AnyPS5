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
extern std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix;
}

namespace {

constexpr std::int32_t Left = 0x2;
constexpr std::int32_t Internal = 0x8;
constexpr std::int32_t HexFloat = 0x1800;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "char num_put float field: %s\n", message);
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

struct NarrowStream {
    GuestLocale::Streambuf buffer{};
    char* putNext = nullptr;
    char* putEnd = nullptr;
    std::string accepted;
    bool fail = false;
};

int APS5_VABI NarrowOverflow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<NarrowStream*>(self);
    if (stream->fail) return -1;
    stream->accepted.push_back(static_cast<char>(character));
    return character & 0xff;
}

const GuestLocale::StreambufVtable g_narrowVtable{{}, NarrowOverflow};

NarrowStream MakeNarrow(bool fail = false) {
    NarrowStream stream{};
    stream.buffer.vtable = &g_narrowVtable;
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

char APS5_VABI GroupDecimalPoint(const GuestLocale::NumpunctFacet*) { return ','; }
char APS5_VABI GroupThousandsSep(const GuestLocale::NumpunctFacet*) { return '\''; }
GuestLocale::String* APS5_VABI GroupGrouping(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, g_grouping.c_str()); }
GuestLocale::String* APS5_VABI GroupFalse(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, "false"); }
GuestLocale::String* APS5_VABI GroupTrue(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return LibcAssignCString_nid_no_patch(result, "true"); }

const GuestLocale::NumpunctVtable g_groupVtable{{FacetNop, FacetDelete, FacetRetain, FacetRelease}, GroupDecimalPoint, GroupThousandsSep, GroupGrouping, GroupFalse, GroupTrue};

using Fput = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, GuestLocale::IosBase*, char, const char*, std::size_t);
const Fput FloatPut = _ZNKSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_FputES3_RSt8ios_basecPKcm_nid_postfix;

std::string PutFloat(GuestLocale::Implementation** locale, std::int32_t flags, std::int32_t width, const char* digits) {
    GuestLocale::IosBase ios{};
    ios.flags = flags;
    ios.width = width;
    ios.locale = locale;
    auto stream = MakeNarrow();
    const GuestLocale::OutputIterator out = FloatPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '*', digits, std::strlen(digits));
    Require(!out.failed && out.buffer == &stream.buffer, "char _Fput returns the iterator it wrote through");
    Require(ios.width == 0, "char _Fput resets stream width");
    return stream.accepted;
}

void CheckFormatting() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(PutFloat(classic, 0, 8, "-1.5") == "****-1.5", "char _Fput pads on the left by default");
    Require(PutFloat(classic, Internal, 8, "-1.5") == "-****1.5", "char _Fput pads after sign for internal");
    Require(PutFloat(classic, Left, 8, "-1.5") == "-1.5****", "char _Fput pads on the right for left");
    Require(PutFloat(classic, HexFloat | Internal, 10, "-0x1.8p+0") == "-0x*1.8p+0", "char _Fput keeps sign and hex prefix before internal fill");
    Require(PutFloat(classic, 0, 6, "inf") == "***inf", "char _Fput formats inf without grouping");
    Require(PutFloat(classic, 0, 6, "-nan") == "**-nan", "char _Fput formats -nan without grouping");
}

void CheckPunctuation() {
    const std::uint64_t savedId = _ZNSt8numpunctIcE2idE_nid_postfix;
    if (_ZNSt8numpunctIcE2idE_nid_postfix == 0) _ZNSt8numpunctIcE2idE_nid_postfix = 1;
    const std::uint64_t id = _ZNSt8numpunctIcE2idE_nid_postfix;
    GuestLocale::NumpunctFacet punct{{&g_groupVtable.facet, 1, 0}, nullptr, ',', '\'', nullptr, nullptr};
    std::vector<GuestLocale::Facet*> facets(id + 1, nullptr);
    facets[id] = &punct.base;
    GuestLocale::Implementation implementation{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
    GuestLocale::Implementation* locale = &implementation;
    g_grouping = "\3";
    Require(PutFloat(&locale, 0, 0, "12345.67") == "12'345,67", "char _Fput replaces decimal point and groups integer part");
    Require(PutFloat(&locale, 0, 0, "-1234e+05") == "-1'234e+05", "char _Fput stops grouping at exponent");
    Require(PutFloat(&locale, 0, 0, "inf") == "inf", "char _Fput does not group inf");
    g_grouping = "";
    _ZNSt8numpunctIcE2idE_nid_postfix = savedId;
}

void CheckErrors() {
    GuestLocale::Implementation** classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::IosBase ios{};
    ios.locale = classic;
    auto stream = MakeNarrow(true);
    const char digits[] = "1.5";
    const GuestLocale::OutputIterator out = FloatPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '*', digits, 3);
    Require(out.failed, "char _Fput marks iterator failed when overflow fails");
    Require(Throws([&] { FloatPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, nullptr, '*', digits, 3); }), "char _Fput rejects null stream");
    GuestLocale::IosBase noLocale{};
    Require(Throws([&] { FloatPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &noLocale, '*', digits, 3); }), "char _Fput rejects stream without locale");
    Require(Throws([&] { FloatPut(nullptr, GuestLocale::OutputIterator{false, &stream.buffer}, &ios, '*', nullptr, 1); }), "char _Fput rejects null digits with positive count");
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
    CheckPunctuation();
    CheckErrors();
    std::puts("libc internal char num_put float field checks passed");
    return 0;
}
