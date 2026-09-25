#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <ios>
#include <mutex>
#include <atomic>
#include <cstring>
#include <vector>
#include <algorithm>
#include <array>
#include <limits>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"

namespace {

std::mutex g_localeInitMutex;
bool g_localeInitialized = false;
std::vector<GuestLocale::Facet*> g_registeredFacets;

void APS5_VABI DestroyClassicLocale(GuestLocale::Facet*) {
    throw std::runtime_error("Cannot destroy the classic locale");
}

void APS5_VABI RetainLocale(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale retain: null facet");
    std::atomic_ref<std::uint32_t> references(self->references);
    auto count = references.load();
    do {
        if (count == 0 || count == UINT32_MAX) throw std::runtime_error("locale retain: invalid reference count");
    } while (!references.compare_exchange_weak(count, count + 1));
}

GuestLocale::Facet* APS5_VABI ReleaseLocale(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale release: null facet");
    std::atomic_ref<std::uint32_t> references(self->references);
    auto count = references.load();
    // The classic locale is immortal: a release that would drop it to zero keeps its last reference.
    do {
        if (count <= 1) return nullptr;
    } while (!references.compare_exchange_weak(count, count - 1));
    return nullptr;
}

// Copies of the classic locale (see _Locimp::_Locimp(const _Locimp&)) count down to zero but are never
// freed either, since their facets are shared with the classic locale.
void APS5_VABI DestroyCopiedLocale(GuestLocale::Facet*) {}

void APS5_VABI RetainCopiedLocale(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale retain: null facet");
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI ReleaseCopiedLocale(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("locale release: null facet");
    std::atomic_ref<std::uint32_t> references(self->references);
    auto count = references.load();
    while (count != 0 && !references.compare_exchange_weak(count, count - 1)) {}
    return nullptr;
}

const GuestLocale::FacetVtable g_localeVtable{DestroyClassicLocale, DestroyClassicLocale, RetainLocale, ReleaseLocale};
const GuestLocale::FacetVtable g_copiedLocaleVtable{DestroyCopiedLocale, DestroyCopiedLocale, RetainCopiedLocale, ReleaseCopiedLocale};
GuestLocale::Facet* g_classicFacets[1]{};
GuestLocale::Implementation g_classicLocale{{&g_localeVtable, 1, 0}, g_classicFacets, 1, 0, false, "C"};

constexpr std::array<short, 257> MakeClassificationTable() {
    std::array<short, 257> table{};
    for (unsigned int value = 0; value < 256; ++value) {
        short mask = 0;
        if (value < 32 || value == 127) mask |= 0x80;
        if (value == ' ') mask |= 0x04;
        if (value >= '\t' && value <= '\r') mask |= 0x40;
        if (value >= 'A' && value <= 'Z') mask |= 0x02;
        if (value >= 'a' && value <= 'z') mask |= 0x10;
        if (value >= '0' && value <= '9') mask |= 0x20;
        if ((value >= '0' && value <= '9') || (value >= 'A' && value <= 'F') || (value >= 'a' && value <= 'f')) mask |= 0x01;
        if (value >= 33 && value <= 126 && (mask & 0x232) == 0) mask |= 0x08;
        table[value + 1] = mask;
    }
    return table;
}

constexpr std::array<short, 257> MakeCaseTable(bool upper) {
    std::array<short, 257> table{};
    table[0] = -1;
    for (unsigned int value = 0; value < 256; ++value) {
        auto converted = value;
        if (upper && value >= 'a' && value <= 'z') converted -= 'a' - 'A';
        if (!upper && value >= 'A' && value <= 'Z') converted += 'a' - 'A';
        table[value + 1] = static_cast<short>(converted);
    }
    return table;
}

constexpr auto g_classificationTable = MakeClassificationTable();
constexpr auto g_lowerTable = MakeCaseTable(false);
constexpr auto g_upperTable = MakeCaseTable(true);

}

extern "C" {

std::uint64_t _ZNSt5ctypeIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt7collateIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE_nid_postfix[12] {};

std::streamoff _ZSt7_BADOFF_nid_postfix = -1;
std::fpos_t _ZSt4_Fpz_nid_postfix {};
std::int32_t _ZNSt6locale2id7_Id_cntE_nid_postfix = 0;

GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix = &g_classicLocale;

void APS5_VABI _ZNSt8ios_baseD2Ev_nid_postfix(GuestLocale::IosBase* self) {
    if (self == nullptr) throw std::invalid_argument("ios_base destructor: null object");
    if (self->standardStream != 0 || self->storage != nullptr || self->callbacks != nullptr) throw std::runtime_error("ios_base destructor: unsupported stream storage or callbacks");
    if (self->locale != &_ZSt21_sceLibcClassicLocale_nid_postfix) throw std::runtime_error("ios_base destructor: unsupported locale ownership");
    self->locale = nullptr;
}

// Dinkumware's static locale::_Init(bool) returns the global _Locimp, which the caller copies.
GuestLocale::Implementation* APS5_VABI _ZNSt6locale5_InitEv_nid_postfix() {
    std::lock_guard<std::mutex> lock(g_localeInitMutex);
    if (!g_localeInitialized) {
        g_localeInitialized = true;
    }
    return &g_classicLocale;
}

void APS5_VABI _ZNSt6locale5facet9_RegisterEv_nid_postfix(GuestLocale::Facet* self) {
    if (self == nullptr || self->vtable == nullptr) throw std::invalid_argument("locale register: invalid facet");
    std::lock_guard<std::mutex> lock(g_localeInitMutex);
    for (const auto* facet : g_registeredFacets) {
        if (facet == self) throw std::runtime_error("locale register: duplicate facet");
    }
    g_registeredFacets.push_back(self);
}

GuestLocale::Implementation* APS5_VABI _ZNSt6locale16_GetgloballocaleEv_nid_postfix() {
    return &g_classicLocale;
}

void APS5_VABI _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet**, const GuestLocale::Implementation*) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZNSt8_LocinfoC1EPKc_nid_postfix(GuestLocale::LocinfoStorage* self, const char* localeName) {
    if (self == nullptr || localeName == nullptr || std::strcmp(localeName, "C") != 0) throw std::invalid_argument("_Locinfo: only the C locale is supported");
    new (self) GuestLocale::LocinfoStorage{};
}

void APS5_VABI _ZNSt8_LocinfoD1Ev_nid_postfix(GuestLocale::LocinfoStorage* self) {
    if (self == nullptr) throw std::invalid_argument("_Locinfo destructor: null object");
}

int APS5_VABI _Mbtowcx_nid_postfix(std::uint16_t* dst, const char* src, std::size_t count, mbstate_t* st) {
    if (dst == nullptr || src == nullptr || st == nullptr || count == 0) throw std::invalid_argument("_Mbtowcx: invalid conversion arguments");
    wchar_t converted{};
    const auto result = std::mbrtowc(&converted, src, count, st);
    if (result == static_cast<std::size_t>(-1)) throw std::runtime_error("_Mbtowcx: invalid multibyte character");
    if (result == static_cast<std::size_t>(-2)) throw std::runtime_error("_Mbtowcx: incomplete multibyte character");
    if (result > static_cast<std::size_t>(std::numeric_limits<int>::max()) || static_cast<std::uint32_t>(converted) > 0xffff) throw std::runtime_error("_Mbtowcx: conversion exceeds guest character limits");
    *dst = static_cast<std::uint16_t>(converted);
    return static_cast<int>(result);
}

int APS5_VABI _Wctombx_nid_postfix(char* dst, std::uint16_t src, mbstate_t* st) {
    if (dst == nullptr || st == nullptr) throw std::invalid_argument("_Wctombx: invalid conversion arguments");
    const auto result = std::wcrtomb(dst, static_cast<wchar_t>(src), st);
    if (result == static_cast<std::size_t>(-1)) throw std::runtime_error("_Wctombx: invalid wide character");
    if (result > static_cast<std::size_t>(std::numeric_limits<int>::max())) throw std::runtime_error("_Wctombx: conversion size exceeds guest limits");
    return static_cast<int>(result);
}

const short* APS5_VABI _Getpctype_nid_postfix() {
    return g_classificationTable.data() + 1;
}

const short* APS5_VABI _Getptolower_nid_postfix() {
    return g_lowerTable.data() + 1;
}

const short* APS5_VABI _Getptoupper_nid_postfix() {
    return g_upperTable.data() + 1;
}

mbstate_t* APS5_VABI _Getpmbstate_nid_postfix() {
    thread_local mbstate_t state {};
    return &state;
}

mbstate_t* APS5_VABI _Getpwcstate_nid_postfix() {
    thread_local mbstate_t state {};
    return &state;
}

wint_t APS5_VABI _Towctrans_nid_postfix(wint_t c, wctrans_t desc) {
    return std::towctrans(c, desc);
}

// Locale ids and stream objects the title imports besides the ones above; the streams are zeroed
// storage (the guest constructs Dinkumware streams itself) so address-taking code links and only a
// use of them would misbehave.
std::uint64_t _ZNSt7codecvtIwc9_MbstatetE2idE_nid_postfix = 0;
alignas(16) unsigned char _ZSt4cout_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt4cerr_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt3cin_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt5wcout_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt5wcerr_nid_postfix[0x400] {};
alignas(16) unsigned char _ZSt4wcin_nid_postfix[0x400] {};

// Facet vectors are read by inlined guest code, so they come from the guest application heap.
GuestLocale::Facet** AllocateFacetVector(std::size_t count) {
    auto* vector = static_cast<GuestLocale::Facet**>(ApplicationHeapAllocate_nid_no_patch(count * sizeof(GuestLocale::Facet*)));
    if (vector == nullptr) throw std::runtime_error("locale: facet vector allocation failed");
    std::memset(vector, 0, count * sizeof(GuestLocale::Facet*));
    return vector;
}

// _Locimp::_Locimp(const _Locimp&): the copy shares the source's facets; guest locales are never freed.
void APS5_VABI _ZNSt6locale7_LocimpC1ERKS0__nid_postfix(GuestLocale::Implementation* self, const GuestLocale::Implementation* source) {
    if (self == nullptr || source == nullptr) throw std::invalid_argument("_Locimp copy: null object");
    std::lock_guard<std::mutex> lock(g_localeInitMutex);
    *self = *source;
    self->base.vtable = &g_copiedLocaleVtable;
    self->base.references = 1;
    if (source->facetCount != 0) {
        self->facets = AllocateFacetVector(source->facetCount);
        std::memcpy(self->facets, source->facets, source->facetCount * sizeof(GuestLocale::Facet*));
    }
}

// _Locimp::_Addfac(facet*, size_t id): installs a facet at its id, growing the vector as needed.
void APS5_VABI _ZNSt6locale7_Locimp7_AddfacEPNS_5facetEm_nid_postfix(GuestLocale::Implementation* self, GuestLocale::Facet* facet, std::size_t id) {
    if (self == nullptr) throw std::invalid_argument("_Locimp::_Addfac: null object");
    std::lock_guard<std::mutex> lock(g_localeInitMutex);
    if (id >= self->facetCount) {
        const std::size_t count = std::max<std::size_t>(id + 1, 40);
        auto* grown = AllocateFacetVector(count);
        if (self->facetCount != 0) std::memcpy(grown, self->facets, self->facetCount * sizeof(GuestLocale::Facet*));
        self->facets = grown;
        self->facetCount = count;
    }
    self->facets[id] = facet;
}

void APS5_VABI _init_env_nid_postfix() {
    ApplicationHeapInitialize_nid_no_patch(ApplicationProcessParameters_nid_no_patch());
}

}
