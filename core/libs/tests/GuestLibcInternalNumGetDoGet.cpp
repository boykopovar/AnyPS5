#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
extern std::int32_t _ZNSt6locale2id7_Id_cntE_nid_postfix;
extern std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8numpunctIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix;
}

namespace {

constexpr std::int32_t EofBit = 0x1;
constexpr std::int32_t FailBit = 0x2;
constexpr std::int32_t DecFlag = 0x10;
constexpr std::int32_t OctFlag = 0x20;
constexpr std::int32_t HexFlag = 0x40;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "num_get do_get: %s\n", message);
    std::exit(1);
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

#ifdef _WIN32
HMODULE g_library = nullptr;
#else
void* g_library = nullptr;
#endif

template<typename TFunction>
TFunction Export(const char* nid) {
    const std::string name = std::string(nid) + "_nid_no_patch_cut";
#ifdef _WIN32
    auto* address = reinterpret_cast<void*>(GetProcAddress(g_library, name.c_str()));
#else
    void* address = dlsym(g_library, name.c_str());
#endif
    Require(address != nullptr, "missing export");
    return reinterpret_cast<TFunction>(address);
}

template<typename TValue>
using NumGetExport = GuestLocale::InputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::InputIterator, GuestLocale::InputIterator, GuestLocale::IosBase*, std::int32_t*, TValue*);

struct GuestStreambufVtable {
    void* destroy;
    void* deleteObject;
    void* lock;
    void* unlock;
    void* overflow;
    void* pbackfail;
    void* showmanyc;
    int (APS5_VABI *underflow)(GuestLocale::Streambuf* self);
    int (APS5_VABI *uflow)(GuestLocale::Streambuf* self);
};

struct CharTestStreambuf {
    GuestLocale::Streambuf base;
    std::string storage;
    const char* data;
    std::size_t size;
    std::size_t pos;
    char* gnext;
    char* gend;
    bool useBuffer;
};

int APS5_VABI CharUnderflow(GuestLocale::Streambuf* self) {
    auto* sb = reinterpret_cast<CharTestStreambuf*>(self);
    if (sb->useBuffer || sb->pos >= sb->size) return -1;
    return static_cast<unsigned char>(sb->data[sb->pos]);
}

int APS5_VABI CharUflow(GuestLocale::Streambuf* self) {
    auto* sb = reinterpret_cast<CharTestStreambuf*>(self);
    if (sb->useBuffer || sb->pos >= sb->size) return -1;
    return static_cast<unsigned char>(sb->data[sb->pos++]);
}

const GuestStreambufVtable g_charStreambufVtable{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, CharUnderflow, CharUflow};

CharTestStreambuf MakeCharStreambuf(std::string input, bool buffered) {
    CharTestStreambuf sb{};
    sb.base.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(&g_charStreambufVtable);
    sb.storage = std::move(input);
    sb.pos = 0;
    sb.useBuffer = buffered;
    return sb;
}

void AttachCharStreambuf(CharTestStreambuf& sb) {
    sb.data = sb.storage.data();
    sb.size = sb.storage.size();
    if (sb.useBuffer) {
        sb.gnext = const_cast<char*>(sb.data);
        sb.gend = const_cast<char*>(sb.data + sb.size);
    }
    sb.base.reserved[6] = &sb.gnext;
    sb.base.reservedCounts[2] = &sb.gend;
}

struct WideTestStreambuf {
    GuestLocale::Streambuf base;
    std::u16string storage;
    const std::uint16_t* data;
    std::size_t size;
    std::size_t pos;
    char* gnext;
    char* gend;
    bool useBuffer;
};

int APS5_VABI WideUnderflow(GuestLocale::Streambuf* self) {
    auto* sb = reinterpret_cast<WideTestStreambuf*>(self);
    if (sb->useBuffer || sb->pos >= sb->size) return -1;
    return static_cast<int>(sb->data[sb->pos]);
}

int APS5_VABI WideUflow(GuestLocale::Streambuf* self) {
    auto* sb = reinterpret_cast<WideTestStreambuf*>(self);
    if (sb->useBuffer || sb->pos >= sb->size) return -1;
    return static_cast<int>(sb->data[sb->pos++]);
}

const GuestStreambufVtable g_wideStreambufVtable{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, WideUnderflow, WideUflow};

WideTestStreambuf MakeWideStreambuf(std::u16string input, bool buffered) {
    WideTestStreambuf sb{};
    sb.base.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(&g_wideStreambufVtable);
    sb.storage = std::move(input);
    sb.pos = 0;
    sb.useBuffer = buffered;
    return sb;
}

void AttachWideStreambuf(WideTestStreambuf& sb) {
    sb.data = reinterpret_cast<const std::uint16_t*>(sb.storage.data());
    sb.size = sb.storage.size();
    if (sb.useBuffer) {
        sb.gnext = reinterpret_cast<char*>(const_cast<char16_t*>(sb.storage.data()));
        sb.gend = reinterpret_cast<char*>(const_cast<char16_t*>(sb.storage.data() + sb.size));
    }
    sb.base.reserved[6] = &sb.gnext;
    sb.base.reservedCounts[2] = &sb.gend;
}

struct ChunkedCharStreambuf {
    GuestLocale::Streambuf base;
    const char* part1;
    std::size_t len1;
    const char* part2;
    std::size_t len2;
    char* gnext;
    char* gend;
    int stage;
};

int APS5_VABI ChunkedUnderflow(GuestLocale::Streambuf* self) {
    auto* sb = reinterpret_cast<ChunkedCharStreambuf*>(self);
    if (sb->stage == 0 && sb->len2 > 0) {
        sb->gnext = const_cast<char*>(sb->part2);
        sb->gend = const_cast<char*>(sb->part2 + sb->len2);
        sb->stage = 1;
        return static_cast<unsigned char>(sb->gnext[0]);
    }
    return -1;
}

int APS5_VABI ChunkedUflow(GuestLocale::Streambuf* self) {
    const int ch = ChunkedUnderflow(self);
    if (ch == -1) return -1;
    auto* sb = reinterpret_cast<ChunkedCharStreambuf*>(self);
    ++sb->gnext;
    return ch;
}

const GuestStreambufVtable g_chunkedStreambufVtable{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, ChunkedUnderflow, ChunkedUflow};

GuestLocale::InputIterator BeginIterator(GuestLocale::Streambuf* sb) {
    GuestLocale::InputIterator it{};
    it.words[0] = reinterpret_cast<std::uintptr_t>(sb);
    return it;
}

GuestLocale::InputIterator EndIterator() {
    GuestLocale::InputIterator it{};
    it.words[1] = 1;
    return it;
}

template<typename TChar>
struct IteratorView {
    GuestLocale::Streambuf* buffer;
    bool got;
    TChar value;
};

template<typename TChar>
IteratorView<TChar> ViewOf(GuestLocale::InputIterator it) {
    IteratorView<TChar> view{};
    std::memcpy(&view, &it, sizeof(view));
    return view;
}

void APS5_VABI TestFacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI TestFacetRelease(GuestLocale::Facet*) { return nullptr; }
const GuestLocale::FacetVtable g_testFacetVtable{TestFacetNop, TestFacetNop, TestFacetNop, TestFacetRelease};

struct CustomCharNumpunct {
    GuestLocale::NumpunctFacet base;
};

char APS5_VABI CustomCharDecimal(const GuestLocale::NumpunctFacet* self) { return self->decimalPoint; }
char APS5_VABI CustomCharThousands(const GuestLocale::NumpunctFacet* self) { return self->thousandsSep; }
GuestLocale::String* APS5_VABI CustomCharString(GuestLocale::String* result, const char* text) {
    return LibcAssignCString_nid_no_patch(result, text);
}
GuestLocale::String* APS5_VABI CustomCharGrouping(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return CustomCharString(result, self->grouping);
}
GuestLocale::String* APS5_VABI CustomCharFalsename(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return CustomCharString(result, self->falsename);
}
GuestLocale::String* APS5_VABI CustomCharTruename(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return CustomCharString(result, self->truename);
}

const GuestLocale::NumpunctVtable g_customCharNumpunctVtable{
    g_testFacetVtable, CustomCharDecimal, CustomCharThousands, CustomCharGrouping, CustomCharFalsename, CustomCharTruename};

struct CustomWideNumpunct {
    GuestLocale::WideNumpunctFacet base;
};

std::uint16_t APS5_VABI CustomWideDecimal(const GuestLocale::WideNumpunctFacet* self) { return self->decimalPoint; }
std::uint16_t APS5_VABI CustomWideThousands(const GuestLocale::WideNumpunctFacet* self) { return self->thousandsSep; }
GuestLocale::String* APS5_VABI CustomWideGrouping(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return LibcAssignCString_nid_no_patch(result, self->grouping);
}
GuestLocale::String* APS5_VABI CustomWideFalsename(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return LibcAssignWideCString_nid_no_patch(result, self->falsename);
}
GuestLocale::String* APS5_VABI CustomWideTruename(GuestLocale::String* result, const GuestLocale::WideNumpunctFacet* self) {
    return LibcAssignWideCString_nid_no_patch(result, self->truename);
}

const GuestLocale::WideNumpunctVtable g_customWideNumpunctVtable{
    g_testFacetVtable, CustomWideDecimal, CustomWideThousands, CustomWideGrouping, CustomWideFalsename, CustomWideTruename};

std::uint16_t APS5_VABI CustomWideCtypeWiden(const GuestLocale::Facet*, char ch) {
    return static_cast<std::uint16_t>(0x4000u + static_cast<unsigned char>(ch));
}

const char* APS5_VABI CustomWideCtypeWidenRange(const GuestLocale::Facet*, const char* first, const char* last, std::uint16_t* dest) {
    for (; first != last; ++first, ++dest) *dest = static_cast<std::uint16_t>(0x4000u + static_cast<unsigned char>(*first));
    return first;
}

GuestLocale::IosBase MakeIos(std::int32_t flags, GuestLocale::Implementation** locale) {
    GuestLocale::IosBase ios{};
    ios.flags = flags;
    ios.locale = locale;
    return ios;
}

template<typename TValue, typename TFn>
GuestLocale::InputIterator RunCharGet(TFn fn, const std::string& text, std::int32_t flags, GuestLocale::Implementation** locale, std::int32_t& state, TValue& value, bool buffered = true) {
    auto sb = MakeCharStreambuf(text, buffered);
    AttachCharStreambuf(sb);
    auto ios = MakeIos(flags, locale);
    GuestLocale::Facet dummy{};
    return fn(&dummy, BeginIterator(&sb.base), EndIterator(), &ios, &state, &value);
}

template<typename TValue, typename TFn>
GuestLocale::InputIterator RunWideGet(TFn fn, const std::u16string& text, std::int32_t flags, GuestLocale::Implementation** locale, std::int32_t& state, TValue& value, bool buffered = true) {
    auto sb = MakeWideStreambuf(text, buffered);
    AttachWideStreambuf(sb);
    auto ios = MakeIos(flags, locale);
    GuestLocale::Facet dummy{};
    return fn(&dummy, BeginIterator(&sb.base), EndIterator(), &ios, &state, &value);
}

void CheckCharIntegers() {
    auto* const classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;

    {
        std::int32_t state = 0;
        std::uint16_t val = 99;
        auto it = RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
            "65535", DecFlag, classic, state, val, true);
        Require(val == 65535 && state == EofBit && ViewOf<char>(it).buffer == nullptr, "ushort max buffered");

        state = 0;
        val = 99;
        it = RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
            "65535", DecFlag, classic, state, val, false);
        Require(val == 65535 && state == EofBit && ViewOf<char>(it).buffer == nullptr, "ushort max unbuffered");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
            "-1", DecFlag, classic, state, val);
        Require(val == 65535 && state == EofBit, "ushort negative -1");

        state = 0;
        val = 123;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
            "65536", DecFlag, classic, state, val);
        Require(val == 65535 && state == (FailBit | EofBit), "ushort overflow");

        state = 0;
        val = 123;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
            "-65536", DecFlag, classic, state, val);
        Require(val == 65535 && state == (FailBit | EofBit), "ushort negative overflow");

        state = 0;
        val = 77;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
            "", DecFlag, classic, state, val);
        Require(val == 0 && state == (FailBit | EofBit), "ushort empty");
    }

    {
        std::int32_t state = 0;
        std::uint32_t val = 0;
        auto it = RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            "0xdeadbeefZ", 0, classic, state, val);
        Require(val == 0xdeadbeefu && state == 0 && ViewOf<char>(it).buffer != nullptr && ViewOf<char>(it).value == 'Z', "uint auto hex prefix stops at Z");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            "4294967296", DecFlag, classic, state, val);
        Require(val == std::numeric_limits<std::uint32_t>::max() && state == (FailBit | EofBit), "uint 32-bit overflow");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            "0777", 0, classic, state, val);
        Require(val == 0777u && state == EofBit, "uint auto octal");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            "ff", HexFlag, classic, state, val);
        Require(val == 0xffu && state == EofBit, "uint hex without 0x");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            "377", OctFlag, classic, state, val);
        Require(val == 0377u && state == EofBit, "uint oct");
    }

    {
        std::int32_t state = 0;
        std::int64_t val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "9223372036854775807", DecFlag, classic, state, val);
        Require(val == std::numeric_limits<std::int64_t>::max() && state == EofBit, "long max 64-bit");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "-9223372036854775808", DecFlag, classic, state, val);
        Require(val == std::numeric_limits<std::int64_t>::min() && state == EofBit, "long min 64-bit");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "9223372036854775808", DecFlag, classic, state, val);
        Require(val == std::numeric_limits<std::int64_t>::max() && state == (FailBit | EofBit), "long overflow");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "-9223372036854775809", DecFlag, classic, state, val);
        Require(val == std::numeric_limits<std::int64_t>::min() && state == (FailBit | EofBit), "long underflow");

        state = 0;
        val = 42;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "+", DecFlag, classic, state, val);
        Require(val == 0 && state == (FailBit | EofBit), "long bare sign");
    }

    {
        std::int32_t state = 0;
        std::uint64_t val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm_nid_postfix,
            "18446744073709551615", DecFlag, classic, state, val);
        Require(val == std::numeric_limits<std::uint64_t>::max() && state == EofBit, "ulong max");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm_nid_postfix,
            "18446744073709551616", DecFlag, classic, state, val);
        Require(val == std::numeric_limits<std::uint64_t>::max() && state == (FailBit | EofBit), "ulong overflow");
    }

    {
        std::int32_t state = 0;
        std::int64_t val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx_nid_postfix,
            "+0x7fffffffffffffff", 0, classic, state, val);
        Require(val == std::numeric_limits<std::int64_t>::max() && state == EofBit, "llong auto +0x max");

        std::uint64_t uval = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy_nid_postfix,
            "0xffffffffffffffff", 0, classic, state, uval);
        Require(uval == std::numeric_limits<std::uint64_t>::max() && state == EofBit, "ullong auto 0x max");
    }

    {
        CustomCharNumpunct punct{{{&g_customCharNumpunctVtable.facet, 1, 0}, "\3", '.', ',', "false", "true"}};
        std::int32_t initState = 0;
        std::int64_t initVal = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "0", DecFlag, classic, initState, initVal);
        Require(_ZNSt8numpunctIcE2idE_nid_postfix != 0, "numpunct<char>::id initialized");

        std::vector<GuestLocale::Facet*> facets(_ZNSt8numpunctIcE2idE_nid_postfix + 1);
        facets[_ZNSt8numpunctIcE2idE_nid_postfix] = &punct.base.base;
        GuestLocale::Implementation customLoc{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
        GuestLocale::Implementation* customPtr = &customLoc;

        std::int32_t state = 0;
        std::int64_t val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1,234,567", DecFlag, &customPtr, state, val);
        Require(val == 1234567 && state == EofBit, "valid char grouping");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1,23,456", DecFlag, &customPtr, state, val);
        Require(val == 123456 && state == (FailBit | EofBit), "bad char grouping sets failbit and keeps value");

        state = 0;
        val = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1,,234", DecFlag, &customPtr, state, val);
        Require(val == 1234 && state == (FailBit | EofBit), "adjacent char separators fail");
    }
}

void CheckCharFloats() {
    auto* const classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;

    {
        std::int32_t state = 0;
        float fval = 0.0f;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
            "1e10", 0, classic, state, fval);
        Require(fval == 1e10f && state == EofBit, "float 1e10");

        state = 0;
        fval = 0.0f;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
            ".5", 0, classic, state, fval);
        Require(fval == 0.5f && state == EofBit, "float .5");

        state = 0;
        fval = 1.0f;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
            "-0.0", 0, classic, state, fval);
        Require(fval == 0.0f && std::signbit(fval) && state == EofBit, "float -0.0 preserves signbit");

        state = 0;
        fval = 0.0f;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
            "1e50", 0, classic, state, fval);
        Require(std::isinf(fval) && fval > 0.0f && state == (FailBit | EofBit), "float overflow");

        state = 0;
        fval = 1.0f;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
            "1e-50", 0, classic, state, fval);
        Require(fval == 0.0f && state == (FailBit | EofBit), "float underflow");

        state = 0;
        fval = 5.0f;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
            "1e+", 0, classic, state, fval);
        Require(fval == 0.0f && state == (FailBit | EofBit), "float incomplete exponent");
    }

    {
        std::int32_t state = 0;
        double dval = 0.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "0x1.8p2", 0, classic, state, dval);
        Require(dval == 6.0 && state == EofBit, "double hex float");

        state = 0;
        dval = 9.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "0x.0p0", 0, classic, state, dval);
        Require(dval == 0.0 && state == EofBit, "double hex 0x.0p0");

        state = 0;
        dval = 0.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "1e400", 0, classic, state, dval);
        Require(std::isinf(dval) && state == (FailBit | EofBit), "double overflow");

        state = 0;
        dval = 1.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "1e-400", 0, classic, state, dval);
        Require(dval == 0.0 && state == (FailBit | EofBit), "double underflow");
    }

    {
        std::int32_t state = 0;
        long double ldval = 0.0L;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix,
            "1.234567890123456789L", 0, classic, state, ldval);
        Require(std::fabs(ldval - 1.234567890123456789L) <= 1e-18L && state == 0, "long double 80-bit precision");

        state = 0;
        ldval = 0.0L;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix,
            "1e2000", 0, classic, state, ldval);
        Require(std::isfinite(ldval) && std::fabs(ldval - 1e2000L) / 1e2000L < 1e-17L && state == EofBit, "long double 1e2000 exceeds double range without overflow");

        state = 0;
        ldval = 0.0L;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix,
            "1e5000", 0, classic, state, ldval);
        Require(std::isinf(ldval) && state == (FailBit | EofBit), "long double overflow at 1e5000");

        state = 0;
        ldval = 1.0L;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix,
            "-1e-5000", 0, classic, state, ldval);
        Require(ldval == 0.0L && std::signbit(ldval) && state == (FailBit | EofBit), "long double underflow at -1e-5000");

        state = 0;
        ldval = 1.0L;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix,
            "0x1p-20000", 0, classic, state, ldval);
        Require(ldval == 0.0L && state == (FailBit | EofBit), "long double hex underflow");

        state = 0;
        ldval = 0.0L;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix,
            "0x1p20000", 0, classic, state, ldval);
        Require(std::isinf(ldval) && state == (FailBit | EofBit), "long double hex overflow");
    }

    {
        CustomCharNumpunct punct{{{&g_customCharNumpunctVtable.facet, 1, 0}, "\3", ',', '.', "false", "true"}};
        std::vector<GuestLocale::Facet*> facets(_ZNSt8numpunctIcE2idE_nid_postfix + 1);
        facets[_ZNSt8numpunctIcE2idE_nid_postfix] = &punct.base.base;
        GuestLocale::Implementation customLoc{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
        GuestLocale::Implementation* customPtr = &customLoc;

        std::int32_t state = 0;
        double dval = 0.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "1.234,5", 0, &customPtr, state, dval);
        Require(dval == 1234.5 && state == EofBit, "double custom decimal comma and grouping dot");

        state = 0;
        dval = 0.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "0.123,5", 0, &customPtr, state, dval);
        Require(dval == 123.5 && state == EofBit, "double leading 0 with valid grouping");

        state = 0;
        dval = 0.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "1.23,5", 0, &customPtr, state, dval);
        Require(dval == 123.5 && state == (FailBit | EofBit), "double bad grouping sets failbit and converts");
    }

    {
        auto sb = MakeCharStreambuf("0x1.8p2", true);
        AttachCharStreambuf(sb);
        auto first = BeginIterator(&sb.base);
        auto last = EndIterator();
        auto ios = MakeIos(0, classic);
        GuestLocale::Facet dummy{};
        char buf[800]{};
        std::int32_t hexExp = 1000000000;
        const int pten = _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(
            &dummy, buf, &first, &last, &ios, &hexExp);
        Require(pten == 0 && hexExp == 0 && std::strcmp(buf, "0x1.8p2") == 0, "char _Getffld V2 hex");

        auto sbBad = MakeCharStreambuf("+", true);
        AttachCharStreambuf(sbBad);
        first = BeginIterator(&sbBad.base);
        hexExp = 0;
        buf[0] = 'X';
        _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(
            &dummy, buf, &first, &last, &ios, &hexExp);
        Require(buf[0] == '\0', "char _Getffld clears buffer on invalid field");
    }

    {
        ChunkedCharStreambuf sb{};
        sb.base.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(&g_chunkedStreambufVtable);
        sb.part1 = "123";
        sb.len1 = 3;
        sb.part2 = "456";
        sb.len2 = 3;
        sb.gnext = const_cast<char*>(sb.part1);
        sb.gend = const_cast<char*>(sb.part1 + sb.len1);
        sb.base.reserved[6] = &sb.gnext;
        sb.base.reservedCounts[2] = &sb.gend;
        auto ios = MakeIos(DecFlag, classic);
        GuestLocale::Facet dummy{};
        std::int32_t state = 0x4;
        std::int64_t val = 0;
        _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix(
            &dummy, BeginIterator(&sb.base), EndIterator(), &ios, &state, &val);
        Require(val == 123456 && state == (0x4 | EofBit), "chunked refill preserves badbit on success and hits eof");
    }
}

void CheckWideNumbers() {
    auto* const classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;

    {
        std::int32_t state = 0;
        std::uint16_t u16 = 0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
            u"-1", DecFlag, classic, state, u16, true);
        Require(u16 == 65535 && state == EofBit, "wide ushort negative -1 buffered");

        state = 0;
        u16 = 0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
            u"65536", DecFlag, classic, state, u16, false);
        Require(u16 == 65535 && state == (FailBit | EofBit), "wide ushort overflow unbuffered");
    }

    {
        std::int32_t state = 0;
        std::uint32_t u32 = 0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            u"0x1234abcd", 0, classic, state, u32);
        Require(u32 == 0x1234abcdu && state == EofBit, "wide uint auto hex");

        state = 0;
        u32 = 0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            u"4294967296", DecFlag, classic, state, u32);
        Require(u32 == std::numeric_limits<std::uint32_t>::max() && state == (FailBit | EofBit), "wide uint overflow");
    }

    {
        std::int32_t state = 0;
        std::int64_t lval = 0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            u"-9223372036854775808", DecFlag, classic, state, lval);
        Require(lval == std::numeric_limits<std::int64_t>::min() && state == EofBit, "wide long min");

        state = 0;
        std::uint64_t ulval = 0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm_nid_postfix,
            u"01777777777777777777777", OctFlag, classic, state, ulval);
        Require(ulval == std::numeric_limits<std::uint64_t>::max() && state == EofBit, "wide ulong oct max");

        state = 0;
        std::int64_t llval = 0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx_nid_postfix,
            u"9223372036854775808", DecFlag, classic, state, llval);
        Require(llval == std::numeric_limits<std::int64_t>::max() && state == (FailBit | EofBit), "wide llong overflow");

        state = 0;
        std::uint64_t ullval = 0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy_nid_postfix,
            u"", DecFlag, classic, state, ullval);
        Require(ullval == 0 && state == (FailBit | EofBit), "wide ullong empty");
    }

    {
        std::int32_t state = 0;
        float fval = 0.0f;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
            u"1e10", 0, classic, state, fval);
        Require(fval == 1e10f && state == EofBit, "wide float 1e10");

        state = 0;
        double dval = 0.0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            u"-0.0", 0, classic, state, dval);
        Require(dval == 0.0 && std::signbit(dval) && state == EofBit, "wide double -0.0");

        state = 0;
        long double ldval = 0.0L;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix,
            u"1e2000", 0, classic, state, ldval);
        Require(std::isfinite(ldval) && std::fabs(ldval - 1e2000L) / 1e2000L < 1e-17L && state == EofBit, "wide long double 1e2000");

        state = 0;
        ldval = 0.0L;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix,
            u"1e5000", 0, classic, state, ldval);
        Require(std::isinf(ldval) && state == (FailBit | EofBit), "wide long double overflow");
    }

    {
        GuestLocale::WideCtypeVtable ctypeVtable{};
        ctypeVtable.facet = g_testFacetVtable;
        ctypeVtable.widenMethods[0] = reinterpret_cast<const void*>(CustomWideCtypeWiden);
        ctypeVtable.widenMethods[1] = reinterpret_cast<const void*>(CustomWideCtypeWidenRange);
        GuestLocale::Facet customCtype{&ctypeVtable.facet, 1, 0};

        static const std::uint16_t kFalseName[] = {'f', 'a', 'l', 's', 'e', 0};
        static const std::uint16_t kTrueName[] = {'t', 'r', 'u', 'e', 0};
        CustomWideNumpunct punct{{{&g_customWideNumpunctVtable.facet, 1, 0}, "\3", u',', u'_', kFalseName, kTrueName}};

        const std::size_t maxId = std::max(_ZNSt8numpunctIwE2idE_nid_postfix, _ZNSt5ctypeIwE2idE_nid_postfix);
        std::vector<GuestLocale::Facet*> facets(maxId + 1);
        facets[_ZNSt8numpunctIwE2idE_nid_postfix] = &punct.base.base;
        facets[_ZNSt5ctypeIwE2idE_nid_postfix] = &customCtype;
        GuestLocale::Implementation customLoc{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
        GuestLocale::Implementation* customPtr = &customLoc;

        std::u16string shiftedNum;
        for (char c : std::string("-1_234,5e1")) {
            shiftedNum.push_back(c == '_' || c == ',' ? static_cast<char16_t>(c) : static_cast<char16_t>(0x4000u + static_cast<unsigned char>(c)));
        }
        std::int32_t state = 0;
        double dval = 0.0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            shiftedNum, 0, &customPtr, state, dval);
        Require(dval == -12345.0 && state == EofBit, "wide custom ctype widen and numpunct grouping/decimal");

        std::u16string badGrouped;
        for (char c : std::string("12_34")) {
            badGrouped.push_back(c == '_' ? u'_' : static_cast<char16_t>(0x4000u + static_cast<unsigned char>(c)));
        }
        state = 0;
        std::int64_t lval = 0;
        RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            badGrouped, DecFlag, &customPtr, state, lval);
        Require(lval == 1234 && state == (FailBit | EofBit), "wide custom ctype bad grouping");
    }

    {
        auto sb = MakeWideStreambuf(u"2.5e-1", true);
        AttachWideStreambuf(sb);
        auto first = BeginIterator(&sb.base);
        auto last = EndIterator();
        auto ios = MakeIos(0, classic);
        GuestLocale::Facet dummy{};
        char buf[800]{};
        std::int32_t hexExp = 1000000000;
        const int pten = _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(
            &dummy, buf, &first, &last, &ios, &hexExp);
        Require(pten == 0 && std::strcmp(buf, "02.5e-1") == 0, "wide _Getffld V2 decimal");
    }
}

void CheckGetcat() {
    auto* const classic = _ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(nullptr, nullptr) == 4, "_Getcat category with null args");

    GuestLocale::Facet sentinel{};
    GuestLocale::Facet* existing = &sentinel;
    Require(_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&existing, &classic) == 4 && existing == &sentinel, "_Getcat leaves non-null facet untouched");

    GuestLocale::Facet* facet = nullptr;
    Require(_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&facet, &classic) == 4 && facet != nullptr, "_Getcat allocates facet");
    Require(facet->references == 0 && facet->vtable != nullptr, "_Getcat initializes references to 0 and installs vtable");

    facet->vtable->destroy(facet);
    facet->vtable->retain(facet);
    Require(facet->references == 1, "num_get facet retain increments references");
    facet->vtable->retain(facet);
    Require(facet->references == 2, "num_get facet second retain increments references");
    Require(facet->vtable->release(facet) == nullptr && facet->references == 1, "num_get facet release returns nullptr while references > 0");
    Require(facet->vtable->release(facet) == facet && facet->references == 0, "num_get facet release returns facet on 0");

    GuestLocale::Implementation copy{};
    _ZNSt6locale7_LocimpC1ERKS0__nid_postfix(&copy, classic);
    if (_ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix == 0) {
        _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix =
            static_cast<std::uint64_t>(++_ZNSt6locale2id7_Id_cntE_nid_postfix);
    }
    const std::size_t id = _ZNSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix;
    _ZNSt6locale7_Locimp7_AddfacEPNS_5facetEm_nid_postfix(&copy, facet, id);
    Require(copy.facetCount > id && copy.facets[id] == facet, "facet registered in locale _Locimp");

    GuestLocale::Implementation* copyPtr = &copy;
    auto ios = MakeIos(DecFlag, &copyPtr);
    {
        auto sb = MakeCharStreambuf("65535", true);
        AttachCharStreambuf(sb);
        std::int32_t state = 0;
        std::uint16_t u16 = 0;
        Export<NumGetExport<std::uint16_t>>("gozsp4urvq8")(facet, BeginIterator(&sb.base), EndIterator(), &ios, &state, &u16);
        Require(u16 == 65535 && state == EofBit, "num_get::get(ushort) dispatches through _Getcat facet");
    }
    {
        auto sb = MakeCharStreambuf("4294967295", true);
        AttachCharStreambuf(sb);
        std::int32_t state = 0;
        std::uint32_t u32 = 0;
        Export<NumGetExport<std::uint32_t>>("N1VqUWz2OEI")(facet, BeginIterator(&sb.base), EndIterator(), &ios, &state, &u32);
        Require(u32 == 4294967295u && state == EofBit, "num_get::get(uint) dispatches through _Getcat facet");
    }
    {
        auto sb = MakeCharStreambuf("-1234567890123", true);
        AttachCharStreambuf(sb);
        std::int32_t state = 0;
        std::int64_t lval = 0;
        Export<NumGetExport<std::int64_t>>("I2UzwkwwEPs")(facet, BeginIterator(&sb.base), EndIterator(), &ios, &state, &lval);
        Require(lval == -1234567890123ll && state == EofBit, "num_get::get(long) dispatches through _Getcat facet");
    }
    {
        auto sb = MakeCharStreambuf("18446744073709551615", true);
        AttachCharStreambuf(sb);
        std::int32_t state = 0;
        std::uint64_t ulval = 0;
        Export<NumGetExport<std::uint64_t>>("2bfL3yIBi5k")(facet, BeginIterator(&sb.base), EndIterator(), &ios, &state, &ulval);
        Require(ulval == std::numeric_limits<std::uint64_t>::max() && state == EofBit, "num_get::get(ulong) dispatches through _Getcat facet");
    }
    {
        auto sb = MakeCharStreambuf("-99", true);
        AttachCharStreambuf(sb);
        std::int32_t state = 0;
        std::int64_t llval = 0;
        Export<NumGetExport<std::int64_t>>("4hiQK82QuLc")(facet, BeginIterator(&sb.base), EndIterator(), &ios, &state, &llval);
        Require(llval == -99 && state == EofBit, "num_get::get(llong) dispatches through _Getcat facet");
    }
    {
        auto sb = MakeCharStreambuf("99", true);
        AttachCharStreambuf(sb);
        std::int32_t state = 0;
        std::uint64_t ullval = 0;
        Export<NumGetExport<std::uint64_t>>("eZfFLyWCkvg")(facet, BeginIterator(&sb.base), EndIterator(), &ios, &state, &ullval);
        Require(ullval == 99 && state == EofBit, "num_get::get(ullong) dispatches through _Getcat facet");
    }
    {
        auto sb = MakeCharStreambuf("1.5", true);
        AttachCharStreambuf(sb);
        std::int32_t state = 0;
        float fval = 0.0f;
        Export<NumGetExport<float>>("6RGkooTERsE")(facet, BeginIterator(&sb.base), EndIterator(), &ios, &state, &fval);
        Require(fval == 1.5f && state == EofBit, "num_get::get(float) dispatches through _Getcat facet");
    }
    {
        auto sb = MakeCharStreambuf("2.25", true);
        AttachCharStreambuf(sb);
        std::int32_t state = 0;
        double dval = 0.0;
        Export<NumGetExport<double>>("mAwXCpkWaYc")(facet, BeginIterator(&sb.base), EndIterator(), &ios, &state, &dval);
        Require(dval == 2.25 && state == EofBit, "num_get::get(double) dispatches through _Getcat facet");
    }
    {
        auto sb = MakeCharStreambuf("1e2000", true);
        AttachCharStreambuf(sb);
        std::int32_t state = 0;
        long double ldval = 0.0L;
        Export<NumGetExport<long double>>("wUCRGap1j0U")(facet, BeginIterator(&sb.base), EndIterator(), &ios, &state, &ldval);
        Require(std::isfinite(ldval) && std::fabs(ldval - 1e2000L) / 1e2000L < 1e-17L && state == EofBit, "num_get::get(long double) dispatches through _Getcat facet");
    }

    facet->vtable->deleteObject(facet);
}

void CheckAdditionalEdgeCases() {
    auto* const classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;

    {
        std::int32_t state = 0;
        std::int64_t lval = 0;
        auto it = RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1,234", DecFlag, classic, state, lval);
        Require(lval == 1 && state == 0 && ViewOf<char>(it).value == ',', "classic C locale stops before comma in integer");

        state = 0;
        lval = 0;
        auto wit = RunWideGet(_ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            u"1,234", DecFlag, classic, state, lval);
        Require(lval == 1 && state == 0 && ViewOf<std::uint16_t>(wit).value == u',', "classic C locale stops before wide comma in integer");

        double dval = 0.0;
        state = 0;
        auto fit = RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "1,234", 0, classic, state, dval);
        Require(dval == 1.0 && state == 0 && ViewOf<char>(fit).value == ',', "classic C locale stops before comma in float");
    }

    {
        CustomCharNumpunct punct{{{&g_customCharNumpunctVtable.facet, 1, 0}, "\3", '.', ',', "false", "true"}};
        std::vector<GuestLocale::Facet*> facets(_ZNSt8numpunctIcE2idE_nid_postfix + 1);
        facets[_ZNSt8numpunctIcE2idE_nid_postfix] = &punct.base.base;
        GuestLocale::Implementation customLoc{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
        GuestLocale::Implementation* customPtr = &customLoc;

        std::int32_t state = 0;
        std::int64_t lval = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            ",123", DecFlag, &customPtr, state, lval);
        Require(lval == 123 && state == (FailBit | EofBit), "leading separator before sign fails");

        state = 0;
        lval = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "+,123", DecFlag, &customPtr, state, lval);
        Require(lval == 123 && state == (FailBit | EofBit), "leading separator after sign fails");

        state = 0;
        lval = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1,234,", DecFlag, &customPtr, state, lval);
        Require(lval == 1234 && state == (FailBit | EofBit), "trailing separator at eof fails");

        state = 0;
        lval = 0;
        auto it = RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1,234,Z", DecFlag, &customPtr, state, lval);
        Require(lval == 1234 && state == FailBit && ViewOf<char>(it).value == 'Z', "trailing separator stops at Z");

        state = 0;
        lval = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1234,567", DecFlag, &customPtr, state, lval);
        Require(lval == 1234567 && state == (FailBit | EofBit), "first group exceeding 3 fails");

        std::uint16_t u16 = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
            "12,34", DecFlag, &customPtr, state, u16);
        Require(u16 == 1234 && state == (FailBit | EofBit), "ushort bad grouping");

        std::uint32_t u32 = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            "12,34", DecFlag, &customPtr, state, u32);
        Require(u32 == 1234 && state == (FailBit | EofBit), "uint bad grouping");

        std::uint64_t ul = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm_nid_postfix,
            "12,34", DecFlag, &customPtr, state, ul);
        Require(ul == 1234 && state == (FailBit | EofBit), "ulong bad grouping");

        std::int64_t ll = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERx_nid_postfix,
            "12,34", DecFlag, &customPtr, state, ll);
        Require(ll == 1234 && state == (FailBit | EofBit), "llong bad grouping");

        std::uint64_t ull = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy_nid_postfix,
            "12,34", DecFlag, &customPtr, state, ull);
        Require(ull == 1234 && state == (FailBit | EofBit), "ullong bad grouping");

        auto sb = MakeCharStreambuf("12,34", true);
        AttachCharStreambuf(sb);
        auto first = BeginIterator(&sb.base);
        auto last = EndIterator();
        auto ios = MakeIos(0, &customPtr);
        GuestLocale::Facet dummy{};
        char buf[800]{};
        std::int32_t hexExp = 1000000000;
        buf[0] = 'X';
        _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(
            &dummy, buf, &first, &last, &ios, &hexExp);
        Require(buf[0] == '\0', "char _Getffld clears buffer on bad grouping");
    }

    {
        const std::string longGrouping(24, '\3');
        CustomCharNumpunct pMulti{{{&g_customCharNumpunctVtable.facet, 1, 0}, "\3\2", '.', ',', "false", "true"}};
        CustomCharNumpunct pOpen{{{&g_customCharNumpunctVtable.facet, 1, 0}, "\3\xff", '.', ',', "false", "true"}};
        CustomCharNumpunct pMax{{{&g_customCharNumpunctVtable.facet, 1, 0}, "\3\x7f", '.', ',', "false", "true"}};
        CustomCharNumpunct pHeap{{{&g_customCharNumpunctVtable.facet, 1, 0}, longGrouping.c_str(), '.', ',', "false", "true"}};

        std::vector<GuestLocale::Facet*> facets(_ZNSt8numpunctIcE2idE_nid_postfix + 1);
        GuestLocale::Implementation customLoc{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
        GuestLocale::Implementation* customPtr = &customLoc;

        facets[_ZNSt8numpunctIcE2idE_nid_postfix] = &pMulti.base.base;
        std::int32_t state = 0;
        std::int64_t lval = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1,23,456", DecFlag, &customPtr, state, lval);
        Require(lval == 123456 && state == EofBit, "multi-byte grouping 3,2");

        facets[_ZNSt8numpunctIcE2idE_nid_postfix] = &pOpen.base.base;
        state = 0;
        lval = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1234,567", DecFlag, &customPtr, state, lval);
        Require(lval == 1234567 && state == EofBit, "grouping terminated by <= 0 permits ungrouped upper digits");

        facets[_ZNSt8numpunctIcE2idE_nid_postfix] = &pMax.base.base;
        state = 0;
        lval = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1234,567", DecFlag, &customPtr, state, lval);
        Require(lval == 1234567 && state == EofBit, "CHAR_MAX grouping disables upper separators");

        facets[_ZNSt8numpunctIcE2idE_nid_postfix] = &pHeap.base.base;
        state = 0;
        lval = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "1,234,567", DecFlag, &customPtr, state, lval);
        Require(lval == 1234567 && state == EofBit, "heap-allocated grouping string");
    }

    {
        std::int32_t state = 0;
        std::uint16_t u16 = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERt_nid_postfix,
            "18446744073709551616", DecFlag, classic, state, u16);
        Require(u16 == 65535 && state == (FailBit | EofBit), "ushort uint64 overflow");

        std::uint32_t u32 = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            "18446744073709551616", DecFlag, classic, state, u32);
        Require(u32 == std::numeric_limits<std::uint32_t>::max() && state == (FailBit | EofBit), "uint uint64 overflow");

        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            "-1", DecFlag, classic, state, u32);
        Require(u32 == std::numeric_limits<std::uint32_t>::max() && state == EofBit, "uint -1 wraps mod 2^32");

        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERj_nid_postfix,
            "-4294967296", DecFlag, classic, state, u32);
        Require(u32 == std::numeric_limits<std::uint32_t>::max() && state == (FailBit | EofBit), "uint -4294967296 overflows");

        std::uint64_t ul = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERm_nid_postfix,
            "-1", DecFlag, classic, state, ul);
        Require(ul == std::numeric_limits<std::uint64_t>::max() && state == EofBit, "ulong -1 wraps");

        std::uint64_t ull = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERy_nid_postfix,
            "-1", DecFlag, classic, state, ull);
        Require(ull == std::numeric_limits<std::uint64_t>::max() && state == EofBit, "ullong -1 wraps");

        std::int64_t lval = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "0X1A", 0, classic, state, lval);
        Require(lval == 26 && state == EofBit, "long uppercase 0X prefix");

        lval = 77;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "0x", 0, classic, state, lval);
        Require(lval == 0 && state == (FailBit | EofBit), "bare 0x sets failbit and zeroes value");

        lval = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "0777", DecFlag, classic, state, lval);
        Require(lval == 777 && state == EofBit, "explicit dec ignores leading zero");

        lval = 99;
        state = 0;
        auto it = RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "0x10", DecFlag, classic, state, lval);
        Require(lval == 0 && state == 0 && ViewOf<char>(it).value == 'x', "explicit dec stops at x");

        lval = 0;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERl_nid_postfix,
            "111111111111111111111111111111111", DecFlag, classic, state, lval);
        Require(lval == std::numeric_limits<std::int64_t>::max() && state == (FailBit | EofBit), "over 31 nonzero int digits fails");
    }

    {
        std::int32_t state = 0;
        double dval = 42.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "+1.5", 0, classic, state, dval);
        Require(dval == 1.5 && state == EofBit, "double leading plus");

        state = 0;
        dval = 42.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "0", 0, classic, state, dval);
        Require(dval == 0.0 && state == EofBit, "double bare 0 at eof");

        for (const char* bad : {"", ".", "0x.", "1eZ"}) {
            state = 0;
            dval = 42.0;
            RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
                bad, 0, classic, state, dval);
            Require((state & FailBit) != 0 && dval == 0.0, "invalid float sets failbit and zeroes value");
        }

        state = 0;
        dval = 0.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "0X1.8P2", 0, classic, state, dval);
        Require(dval == 6.0 && state == EofBit, "double uppercase 0X...P2");

        state = 0;
        dval = 0.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "0x1.8", 0, classic, state, dval);
        Require(dval == 1.5 && state == EofBit, "double hex float without exponent");

        state = 0;
        dval = 0.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            ".0001", 0, classic, state, dval);
        Require(std::fabs(dval - 1e-4) < 1e-12 && state == EofBit, "double leading fraction zeros");

        const std::string v2Sticky = "1." + std::string(770, '0') + "1";
        state = 0;
        dval = 0.0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            v2Sticky, 0, classic, state, dval);
        Require(dval == 1.0 && state == EofBit, "double sticky digit >768 digits");

        float fval = 0.0f;
        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
            "0x1p200", 0, classic, state, fval);
        Require(std::isinf(fval) && state == (FailBit | EofBit), "float hex overflow");

        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
            "0x1p-200", 0, classic, state, fval);
        Require(fval == 0.0f && state == (FailBit | EofBit), "float hex underflow");

        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERf_nid_postfix,
            "1e-40", 0, classic, state, fval);
        Require(fval > 0.0f && fval < 1e-37f && state == EofBit, "float nonzero subnormal succeeds without failbit");

        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "0x1p2000", 0, classic, state, dval);
        Require(std::isinf(dval) && state == (FailBit | EofBit), "double hex overflow");

        state = 0;
        RunCharGet(_ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateERd_nid_postfix,
            "0x1p-2000", 0, classic, state, dval);
        Require(dval == 0.0 && state == (FailBit | EofBit), "double hex underflow");
    }

    {
        auto sb = MakeCharStreambuf("1.25e2", true);
        AttachCharStreambuf(sb);
        auto first = BeginIterator(&sb.base);
        auto last = EndIterator();
        auto ios = MakeIos(0, classic);
        GuestLocale::Facet dummy{};
        char buf[64]{};
        std::int32_t hexExp = 0;
        const int pten = _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(
            &dummy, buf, &first, &last, &ios, &hexExp);
        Require(pten == 0 && std::strcmp(buf, "01.25e2") == 0, "char _Getffld V1");

        auto sbLong = MakeCharStreambuf("1." + std::string(40, '5'), true);
        AttachCharStreambuf(sbLong);
        first = BeginIterator(&sbLong.base);
        hexExp = 0;
        _ZNKSt7num_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(
            &dummy, buf, &first, &last, &ios, &hexExp);
        Require(std::strlen(buf) == 38 && buf[37] == '6', "char _Getffld V1 rounds at 36 digits");
    }

    {
        auto sbHex = MakeWideStreambuf(u"0x1.8p2", true);
        AttachWideStreambuf(sbHex);
        auto first = BeginIterator(&sbHex.base);
        auto last = EndIterator();
        auto ios = MakeIos(0, classic);
        GuestLocale::Facet dummy{};
        char buf[800]{};
        std::int32_t hexExp = 1000000000;
        const int pten = _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(
            &dummy, buf, &first, &last, &ios, &hexExp);
        Require(pten == 0 && hexExp == 0 && std::strcmp(buf, "0x1.8p2") == 0, "wide _Getffld V2 hex");

        auto sbV1 = MakeWideStreambuf(u"1.25e2", true);
        AttachWideStreambuf(sbV1);
        first = BeginIterator(&sbV1.base);
        hexExp = 0;
        _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(
            &dummy, buf, &first, &last, &ios, &hexExp);
        Require(std::strcmp(buf, "01.25e2") == 0, "wide _Getffld V1 decimal");

        auto sbEmpty = MakeWideStreambuf(u"", true);
        AttachWideStreambuf(sbEmpty);
        first = BeginIterator(&sbEmpty.base);
        hexExp = 1000000000;
        buf[0] = 'X';
        _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(
            &dummy, buf, &first, &last, &ios, &hexExp);
        Require(buf[0] == '\0', "wide _Getffld empty clears buffer");

        static const std::uint16_t kFalseName[] = {'f', 'a', 'l', 's', 'e', 0};
        static const std::uint16_t kTrueName[] = {'t', 'r', 'u', 'e', 0};
        CustomWideNumpunct wp{{{&g_customWideNumpunctVtable.facet, 1, 0}, "\3", u'.', u',', kFalseName, kTrueName}};
        std::vector<GuestLocale::Facet*> facets(_ZNSt8numpunctIwE2idE_nid_postfix + 1);
        facets[_ZNSt8numpunctIwE2idE_nid_postfix] = &wp.base.base;
        GuestLocale::Implementation customLoc{{nullptr, 1, 0}, facets.data(), facets.size(), 0, false, "C"};
        GuestLocale::Implementation* customPtr = &customLoc;
        auto sbBadGroup = MakeWideStreambuf(u"12,34", true);
        AttachWideStreambuf(sbBadGroup);
        first = BeginIterator(&sbBadGroup.base);
        auto iosGroup = MakeIos(0, &customPtr);
        buf[0] = 'X';
        _ZNKSt7num_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE8_GetffldEPcRS3_S6_RSt8ios_basePi_nid_postfix(
            &dummy, buf, &first, &last, &iosGroup, &hexExp);
        Require(buf[0] == '\0', "wide _Getffld bad grouping clears buffer");
    }
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "expected libSceLibcInternal path");
#ifdef _WIN32
    g_library = LoadLibraryA(argv[1]);
#else
    g_library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
#endif
    Require(g_library != nullptr, "load libSceLibcInternal");
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckCharIntegers();
    CheckCharFloats();
    CheckWideNumbers();
    CheckGetcat();
    CheckAdditionalEdgeCases();
    std::puts("libc internal num_get do_get checks passed");
    return 0;
}
