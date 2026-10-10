#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <ios>
#include <locale>
#include <mutex>
#include <atomic>
#include <cstring>
#include <vector>
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"

extern "C" {
void* APS5_VABI _Znwm_nid_postfix(std::size_t size);
void APS5_VABI _ZdlPv_nid_postfix(void* pointer);
}

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

constexpr std::size_t GuestStringInlineCapacity = 15;
constexpr std::size_t WideStringInlineCapacity = GuestStringInlineCapacity - 1;
constexpr std::size_t GuestStringLargeAllocation = 0x1000;

void ValidateCharacterRange(const char* first, const char* last, const char* function) {
    if ((first == nullptr) != (last == nullptr) || last < first) throw std::invalid_argument(std::string(function) + ": invalid character range");
}

void ValidateWideCharacterRange(const char16_t* first, const char16_t* last, const char* function) {
    if ((first == nullptr) != (last == nullptr) || last < first) throw std::invalid_argument(std::string(function) + ": invalid wide character range");
}

void APS5_VABI DestroyCollate(GuestLocale::Facet*) {}

void APS5_VABI DeleteCollate(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("collate delete: null facet");
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI RetainFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("facet retain: null facet");
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI ReleaseFacet(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("facet release: null facet");
    return std::atomic_ref<std::uint32_t>(self->references).fetch_sub(1) == 1 ? self : nullptr;
}

int APS5_VABI CollateCompare(const GuestLocale::CollateFacet*, const char* first1, const char* last1, const char* first2, const char* last2) {
    ValidateCharacterRange(first1, last1, "collate::do_compare");
    ValidateCharacterRange(first2, last2, "collate::do_compare");
    const auto length1 = static_cast<std::size_t>(last1 - first1);
    const auto length2 = static_cast<std::size_t>(last2 - first2);
    const auto common = std::min(length1, length2);
    const int order = common == 0 ? 0 : std::memcmp(first1, first2, common);
    if (order != 0) return order < 0 ? -1 : 1;
    if (length1 == length2) return 0;
    return length1 < length2 ? -1 : 1;
}

GuestLocale::String* APS5_VABI CollateTransform(GuestLocale::String* result, const GuestLocale::CollateFacet*, const char* first, const char* last) {
    if (result == nullptr) throw std::invalid_argument("collate::do_transform: null result");
    ValidateCharacterRange(first, last, "collate::do_transform");
    const auto length = static_cast<std::size_t>(last - first);
    if (length != 0 && std::memchr(first, 0, length) != nullptr) throw std::runtime_error("collate::do_transform: embedded null characters are not supported");
    if (length + 1 >= GuestStringLargeAllocation) throw std::runtime_error("collate::do_transform: results of 4095 characters or more are not supported");
    char* data = result->buffer;
    std::size_t capacity = GuestStringInlineCapacity;
    if (length > GuestStringInlineCapacity) {
        capacity = length;
        data = static_cast<char*>(_Znwm_nid_postfix(capacity + 1));
    }
    if (length != 0) std::memcpy(data, first, length);
    data[length] = 0;
    if (data != result->buffer) result->pointer = data;
    result->size = length;
    result->capacity = capacity;
    return result;
}

std::int64_t APS5_VABI CollateHash(const GuestLocale::CollateFacet*, const char* first, const char* last) {
    ValidateCharacterRange(first, last, "collate::do_hash");
    std::uint64_t hash = 0xcbf29ce484222325;
    for (; first != last; ++first) hash = (hash ^ static_cast<unsigned char>(*first)) * 0x100000001b3;
    return static_cast<std::int64_t>(hash);
}

const GuestLocale::CollateVtable g_collateVtable{{DestroyCollate, DeleteCollate, RetainFacet, ReleaseFacet}, CollateCompare, CollateTransform, CollateHash};

// std::collate<wchar_t> is the C locale as wcscoll and wcsxfrm model it: the order of the guest's 16-bit
// code units, the shorter range first when one is a prefix of the other. The hash folds the same bytes the
// order compares, low byte first, so it agrees with compare().
int APS5_VABI CollateWideCompare(const GuestLocale::CollateFacet*, const char16_t* first1, const char16_t* last1, const char16_t* first2, const char16_t* last2) {
    ValidateWideCharacterRange(first1, last1, "collate::do_compare");
    ValidateWideCharacterRange(first2, last2, "collate::do_compare");
    const auto length1 = static_cast<std::size_t>(last1 - first1);
    const auto length2 = static_cast<std::size_t>(last2 - first2);
    const auto common = std::min(length1, length2);
    for (std::size_t index = 0; index < common; ++index) {
        if (first1[index] != first2[index]) return first1[index] < first2[index] ? -1 : 1;
    }
    if (length1 == length2) return 0;
    return length1 < length2 ? -1 : 1;
}

GuestLocale::String* APS5_VABI CollateWideTransform(GuestLocale::String* result, const GuestLocale::CollateFacet*, const char16_t* first, const char16_t* last) {
    if (result == nullptr) throw std::invalid_argument("collate::do_transform: null result");
    ValidateWideCharacterRange(first, last, "collate::do_transform");
    const auto length = static_cast<std::size_t>(last - first);
    if (length != 0 && std::find(first, last, u'\0') != last) throw std::runtime_error("collate::do_transform: embedded null characters are not supported");
    const auto bytes = length * sizeof(char16_t);
    if (bytes + sizeof(char16_t) >= GuestStringLargeAllocation) throw std::runtime_error("collate::do_transform: results of 2047 characters or more are not supported");
    char* data = result->buffer;
    std::size_t capacity = WideStringInlineCapacity;
    if (bytes > capacity) {
        capacity = bytes;
        data = static_cast<char*>(_Znwm_nid_postfix(capacity + sizeof(char16_t)));
    }
    if (length != 0) std::memcpy(data, first, bytes);
    std::memset(data + bytes, 0, sizeof(char16_t));
    if (data != result->buffer) result->pointer = data;
    result->size = length;
    result->capacity = capacity;
    return result;
}

std::int64_t APS5_VABI CollateWideHash(const GuestLocale::CollateFacet*, const char16_t* first, const char16_t* last) {
    ValidateWideCharacterRange(first, last, "collate::do_hash");
    std::uint64_t hash = 0xcbf29ce484222325;
    for (; first != last; ++first) {
        const auto unit = static_cast<unsigned char>(*first);
        hash = (hash ^ unit) * 0x100000001b3;
        hash = (hash ^ static_cast<unsigned char>(*first >> 8)) * 0x100000001b3;
    }
    return static_cast<std::int64_t>(hash);
}

const GuestLocale::WideCollateVtable g_wideCollateVtable{{DestroyCollate, DeleteCollate, RetainFacet, ReleaseFacet}, CollateWideCompare, CollateWideTransform, CollateWideHash};

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

void ValidateCharacter(int value) {
    if (value != EOF && (value < 0 || value > UCHAR_MAX)) throw std::invalid_argument("Invalid character value");
}

int ClassifyCharacter(int value, std::ctype_base::mask mask) {
    ValidateCharacter(value);
    if (value == EOF) return 0;
    return std::use_facet<std::ctype<char>>(std::locale::classic()).is(mask, static_cast<char>(value));
}

int ConvertCharacter(int value, bool upper) {
    ValidateCharacter(value);
    if (value == EOF) return EOF;
    const auto& facet = std::use_facet<std::ctype<char>>(std::locale::classic());
    const auto character = static_cast<char>(value);
    return static_cast<unsigned char>(upper ? facet.toupper(character) : facet.tolower(character));
}

}

extern "C" {

int APS5_VABI isupper_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::upper); }
int APS5_VABI islower_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::lower); }
int APS5_VABI isalpha_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::alpha); }
int APS5_VABI isdigit_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::digit); }
int APS5_VABI isalnum_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::alnum); }
int APS5_VABI isspace_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::space); }
int APS5_VABI isblank_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::blank); }
int APS5_VABI iscntrl_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::cntrl); }
int APS5_VABI isprint_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::print); }
int APS5_VABI isgraph_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::graph); }
int APS5_VABI ispunct_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::punct); }
int APS5_VABI isxdigit_nid_postfix(int c) { return ClassifyCharacter(c, std::ctype_base::xdigit); }
int APS5_VABI toupper_nid_postfix(int c) { return ConvertCharacter(c, true); }
int APS5_VABI tolower_nid_postfix(int c) { return ConvertCharacter(c, false); }

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

std::size_t APS5_VABI _ZNSt7collateIwE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    if (facet != nullptr && *facet == nullptr) {
        if (locale == nullptr || *locale == nullptr || (*locale)->name == nullptr || std::strcmp((*locale)->name, "C") != 0) throw std::invalid_argument("collate::_Getcat: only the C locale is supported");
        auto* collate = static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet)));
        *collate = {{&g_wideCollateVtable.facet, 0, 0}, nullptr, nullptr};
        *facet = &collate->base;
    }
    return 1;
}

std::size_t APS5_VABI _ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    if (facet != nullptr && *facet == nullptr) {
        if (locale == nullptr || *locale == nullptr || (*locale)->name == nullptr || std::strcmp((*locale)->name, "C") != 0) throw std::invalid_argument("collate::_Getcat: only the C locale is supported");
        auto* collate = static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet)));
        *collate = {{&g_collateVtable.facet, 0, 0}, nullptr, nullptr};
        *facet = &collate->base;
    }
    return 1;
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

void APS5_VABI init_env_nid_postfix(const InitEnvParams* params) {
    (void)params;
    _init_env_nid_postfix();
}

}
