#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <ios>
#include <mutex>
#include <atomic>
#include <cstring>
#include <vector>
#include <algorithm>
#include <array>
#include <limits>
#include <cerrno>

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

// FreeBSD's MB_CUR_MAX reads this exported int. The only supported guest
// locale is C, whose multibyte characters occupy one byte.
int __mb_cur_max_nid_postfix = 1;

// The runtime currently exposes the classic C locale. Keep byte classification
// independent of any locale selected by host-side libraries.
// Guest FreeBSD locale categories are LC_ALL=0 through LC_MESSAGES=6.
// The empty locale selects the runtime default, which is currently C.
char* APS5_VABI setlocale_nid_postfix(int category, const char* locale) {
    if (category < 0 || category > 6) return nullptr;
    if (locale != nullptr && locale[0] != '\0' && std::strcmp(locale, "C") != 0 &&
        std::strcmp(locale, "POSIX") != 0) return nullptr;
    static char classicName[] = "C";
    return classicName;
}

// The classic locale collates by code-unit value; transformed keys can be
// compared with strcmp / guest-width wcscmp in exactly the same order.
int APS5_VABI strcoll_nid_postfix(const char* left, const char* right) {
    return std::strcmp(left, right);
}

std::size_t APS5_VABI strxfrm_nid_postfix(char* destination, const char* source,
                                          std::size_t capacity) {
    const auto length = std::strlen(source);
    if (capacity != 0) {
        const auto copied = length < capacity ? length + 1 : capacity;
        std::memcpy(destination, source, copied);
    }
    return length;
}

int APS5_VABI wcscoll_nid_postfix(const std::uint16_t* left, const std::uint16_t* right) {
    while (*left != 0 && *left == *right) { ++left; ++right; }
    return *left < *right ? -1 : *left > *right ? 1 : 0;
}

std::size_t APS5_VABI wcsxfrm_nid_postfix(std::uint16_t* destination,
                                           const std::uint16_t* source, std::size_t capacity) {
    std::size_t length = 0;
    while (source[length] != 0) ++length;
    if (capacity != 0) {
        const auto copied = length < capacity ? length + 1 : capacity;
        std::memcpy(destination, source, copied * sizeof(*source));
    }
    return length;
}

// The guest SDK uses 16-bit wchar_t. Only the classic C locale is modeled;
// its multibyte representation is single-byte ASCII, not the host CRT locale.
std::size_t APS5_VABI wcstombs_nid_postfix(char* destination, const std::uint16_t* source,
                                           std::size_t capacity) {
    if (source == nullptr) { errno = 22; return static_cast<std::size_t>(-1); }
    std::size_t converted = 0;
    for (;;) {
        // A null destination requests the full required length, irrespective of capacity.
        if (destination != nullptr && converted == capacity) return converted;
        const auto character = source[converted];
        if (character == 0) {
            if (destination != nullptr) destination[converted] = '\0';
            return converted;
        }
        if (character > 0x7f) { errno = 86; return static_cast<std::size_t>(-1); }
        if (destination != nullptr) destination[converted] = static_cast<char>(character);
        ++converted;
    }
}

// The C locale is stateless. Keep the guest mbstate_t opaque: a host mbstate_t
// can have a different layout and must not be used to interpret guest memory.
std::size_t APS5_VABI mbrtowc_nid_postfix(std::uint16_t* destination, const char* source,
                                          std::size_t count, void* state) {
    (void)state;
    if (source == nullptr) return 0;
    if (count == 0) return static_cast<std::size_t>(-2);
    const auto character = static_cast<unsigned char>(*source);
    if (character > 0x7f) { errno = 86; return static_cast<std::size_t>(-1); }
    if (destination != nullptr) *destination = character;
    return character == 0 ? 0 : 1;
}

std::size_t APS5_VABI mbrlen_nid_postfix(const char* source, std::size_t count, void* state) {
    return mbrtowc_nid_postfix(nullptr, source, count, state);
}

int APS5_VABI mbtowc_nid_postfix(std::uint16_t* destination, const char* source,
                                 std::size_t count) {
    if (source == nullptr) return 0; // The C locale has no shift state.
    const auto result = mbrtowc_nid_postfix(destination, source, count, nullptr);
    if (result == static_cast<std::size_t>(-2)) { errno = 86; return -1; }
    if (result == static_cast<std::size_t>(-1)) return -1;
    return static_cast<int>(result);
}

std::size_t APS5_VABI mbsrtowcs_nid_postfix(std::uint16_t* destination, const char** source,
                                             std::size_t capacity, void* state) {
    (void)state;
    if (source == nullptr || *source == nullptr) { errno = 22; return static_cast<std::size_t>(-1); }
    const char* cursor = *source;
    std::size_t converted = 0;
    for (;;) {
        if (destination != nullptr && converted == capacity) {
            *source = cursor;
            return converted;
        }
        const auto character = static_cast<unsigned char>(*cursor);
        if (character > 0x7f) {
            if (destination != nullptr) *source = cursor;
            errno = 86;
            return static_cast<std::size_t>(-1);
        }
        if (character == 0) {
            if (destination != nullptr) {
                destination[converted] = 0;
                *source = nullptr;
            }
            return converted;
        }
        if (destination != nullptr) destination[converted] = character;
        ++cursor;
        ++converted;
    }
}

std::size_t APS5_VABI wcrtomb_nid_postfix(char* destination, std::uint16_t character,
                                           void* state) {
    (void)state;
    if (destination == nullptr) return 1; // Encoding the null character resets the C locale.
    if (character > 0x7f) { errno = 86; return static_cast<std::size_t>(-1); }
    *destination = static_cast<char>(character);
    return 1;
}

int APS5_VABI isupper_nid_postfix(int c) { return c >= 'A' && c <= 'Z'; }
int APS5_VABI islower_nid_postfix(int c) { return c >= 'a' && c <= 'z'; }
int APS5_VABI isalpha_nid_postfix(int c) { return isupper_nid_postfix(c) || islower_nid_postfix(c); }
int APS5_VABI isdigit_nid_postfix(int c) { return c >= '0' && c <= '9'; }
int APS5_VABI isalnum_nid_postfix(int c) { return isalpha_nid_postfix(c) || isdigit_nid_postfix(c); }
int APS5_VABI isspace_nid_postfix(int c) { return c == ' ' || (c >= '\t' && c <= '\r'); }
int APS5_VABI isblank_nid_postfix(int c) { return c == ' ' || c == '\t'; }
int APS5_VABI iscntrl_nid_postfix(int c) { return (c >= 0 && c < 32) || c == 127; }
int APS5_VABI isprint_nid_postfix(int c) { return c >= 32 && c <= 126; }
int APS5_VABI isgraph_nid_postfix(int c) { return c >= 33 && c <= 126; }
int APS5_VABI ispunct_nid_postfix(int c) { return isgraph_nid_postfix(c) && !isalnum_nid_postfix(c); }
int APS5_VABI isxdigit_nid_postfix(int c) {
    return isdigit_nid_postfix(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
int APS5_VABI toupper_nid_postfix(int c) { return islower_nid_postfix(c) ? c - ('a' - 'A') : c; }
int APS5_VABI tolower_nid_postfix(int c) { return isupper_nid_postfix(c) ? c + ('a' - 'A') : c; }

std::uint64_t _ZNSt5ctypeIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt7collateIwE2idE_nid_postfix = 0;
std::uint64_t _ZNSt7collateIcE2idE_nid_postfix = 0;
std::uint64_t _ZNSt7codecvtIcc9_MbstatetE2idE_nid_postfix = 0;
std::uintptr_t _ZTVSt7codecvtIcc9_MbstatetE_nid_postfix[16] {};
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

void APS5_VABI _ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet**, const GuestLocale::Implementation*) {
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
