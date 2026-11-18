#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

using Thrower = void (APS5_VABI *)(const void*);

extern "C" {
extern const TypeRecord _ZTISt9bad_alloc_nid_postfix;
extern const TypeRecord _ZTISt13runtime_error_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v25ctypeIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v25ctypeIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v25ctypeItE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28numpunctIcE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28numpunctIwE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v27codecvtIwc9_MbstatetE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v27codecvtItc9_MbstatetE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v210moneypunctIwLb0EE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28time_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28time_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix;
extern std::uint64_t _ZNSt8__sce_v28time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

static std::map<void*, std::size_t> gLive;
static std::set<void*> gCallocBlocks;
static std::size_t gFailCalloc = 0;
static std::size_t gCallocSeen = 0;

void* APS5_VABI TestHeapAllocate(std::size_t bytes) {
    void* pointer = std::malloc(bytes);
    gLive[pointer] = bytes;
    return pointer;
}
void APS5_VABI TestHeapFree(void* pointer) {
    gLive.erase(pointer);
    gCallocBlocks.erase(pointer);
    std::free(pointer);
}
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) {
    if (++gCallocSeen == gFailCalloc) return nullptr;
    void* pointer = std::calloc(count, bytes);
    gLive[pointer] = count * bytes;
    gCallocBlocks.insert(pointer);
    return pointer;
}
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) {
    gLive.erase(pointer);
    void* grown = std::realloc(pointer, bytes);
    gLive[grown] = bytes;
    return grown;
}
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer != nullptr ? 0 : -1;
}

static unsigned failures = 0;
static char caught[128];

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* vtable = *static_cast<const void* const* const*>(object);
    std::snprintf(caught, sizeof(caught), "%s", static_cast<const TypeRecord*>(vtable[-1])->name);
}

static std::size_t Size(const void* pointer) {
    const auto found = gLive.find(const_cast<void*>(pointer));
    return found == gLive.end() ? 0 : found->second;
}

template<typename T> static T Field(const void* facet, std::size_t offset) {
    T value;
    std::memcpy(&value, static_cast<const char*>(facet) + offset, sizeof(T));
    return value;
}

static std::u16string Wide(const void* pointer) {
    return std::u16string(static_cast<const char16_t*>(pointer));
}

static std::string Text(const void* pointer) {
    return static_cast<const char*>(pointer);
}

static const GuestLocale::SceV2Locinfo kInfo{0, "previous", "C"};

using Make = GuestLocale::Implementation* (*)(std::int32_t, GuestLocale::Implementation*, const GuestLocaleFacade*);

static GuestLocale::Implementation* MakeAll(std::int32_t category, GuestLocale::Implementation* self, const GuestLocaleFacade* locale) {
    return _ZNSt8__sce_v26locale7_Locimp8_MakelocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix(&kInfo, category, self, locale);
}

static std::size_t InstalledFacets(const GuestLocale::Implementation* locale) {
    std::size_t count = 0;
    for (std::uint64_t index = 0; index < locale->facetCount; ++index) count += locale->facets[index] != nullptr;
    return count;
}

static void DropLocale(GuestLocale::Implementation* locale) {
    while (locale->base.vtable->release(&locale->base) == nullptr) {}
    locale->base.vtable->deleteObject(&locale->base);
}

static void TestCategories() {
    const std::array<std::pair<std::int32_t, std::size_t>, 7> expected{{{0x1, 3}, {0x2, 6}, {0x4, 12}, {0x8, 9}, {0x10, 6}, {0x20, 3}, {0x3f, 39}}};
    for (const auto& [category, count] : expected) {
        auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
        Require(MakeAll(category, locale, nullptr) == locale, "_Makeloc returns the locale");
        Require(locale->category == static_cast<std::uint32_t>(category), "_Makeloc sets the category");
        Require(std::strcmp(locale->name, "C") == 0, "_Makeloc names the locale after the _Locinfo");
        Require(InstalledFacets(locale) == count, "_Makeloc installs the facets of the category");
        for (std::uint64_t index = 0; index < locale->facetCount; ++index) {
            const GuestLocale::Facet* facet = locale->facets[index];
            if (facet == nullptr) continue;
            Require(facet->references == 1, "installed facets hold one reference");
            Require(reinterpret_cast<const void* const*>(facet->vtable)[4] != nullptr, "the first virtual slot is filled");
        }
        DropLocale(locale);
    }
    auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    locale->category = 0x40;
    MakeAll(0, locale, nullptr);
    Require(locale->category == 0x40 && InstalledFacets(locale) == 0, "category zero adds nothing and keeps the mask");
    MakeAll(0x8, locale, nullptr);
    Require(locale->category == 0x48, "categories accumulate");
    DropLocale(locale);
}

static void TestSingleFunctions() {
    struct Case { void (APS5_VABI *call)(const GuestLocale::SceV2Locinfo*, std::int32_t, GuestLocale::Implementation*, const GuestLocaleFacade*); std::size_t all; };
    const std::array<Case, 3> cases{{
        {_ZNSt8__sce_v26locale7_Locimp9_MakexlocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix, 8},
        {_ZNSt8__sce_v26locale7_Locimp9_MakewlocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix, 13},
        {_ZNSt8__sce_v26locale7_Locimp11_MakeushlocERKNS_8_LocinfoEiPS1_PKS0__nid_postfix, 13},
    }};
    for (const Case& test : cases) {
        auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
        test.call(&kInfo, 0x3f, locale, nullptr);
        Require(InstalledFacets(locale) == test.all, "each _Make function installs its own facets");
        Require(locale->category == 0 && std::strcmp(locale->name, "*") == 0, "only _Makeloc updates the mask and the name");
        DropLocale(locale);
    }
}

static void TestFields() {
    auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x3f, locale, nullptr);
    const auto at = [&](std::uint64_t id) -> const void* { return locale->facets[id]; };

    const void* ctype = at(_ZNSt8__sce_v25ctypeIcE2idE_nid_postfix);
    Require(Size(ctype) == 0x20, "ctype<char> size");
    const short* table = Field<const short*>(ctype, 0x10);
    Require(table['A'] == 3 && table['z'] == 0x10 && table['\t'] == 0x4c0 && table[0x80] == 0, "ctype<char> table");
    Require(Field<std::int32_t>(ctype, 0x18) == 0, "ctype<char> owns no table");

    for (const std::uint64_t id : {_ZNSt8__sce_v25ctypeIwE2idE_nid_postfix, _ZNSt8__sce_v25ctypeItE2idE_nid_postfix}) {
        Require(Size(at(id)) == 0x48, "wide ctype size");
        Require(Field<std::uint32_t>(at(id), 0x20) == 1 && Field<std::uint32_t>(at(id), 0x24) == 1 && Field<std::uint64_t>(at(id), 0x40) == 0, "wide ctype conversion data");
    }
    for (const std::uint64_t id : {_ZNSt8__sce_v27codecvtIwc9_MbstatetE2idE_nid_postfix, _ZNSt8__sce_v27codecvtItc9_MbstatetE2idE_nid_postfix}) {
        Require(Size(at(id)) == 0x38, "wide codecvt size");
        Require(Field<std::uint32_t>(at(id), 0xc) == 1 && Field<std::uint32_t>(at(id), 0x10) == 1 && Field<std::uint32_t>(at(id), 0x30) == 0, "wide codecvt conversion data");
    }

    const void* punct = at(_ZNSt8__sce_v28numpunctIcE2idE_nid_postfix);
    Require(Size(punct) == 0x30, "numpunct<char> size");
    Require(Text(Field<const char*>(punct, 0x10)).empty() && Text(Field<const char*>(punct, 0x20)) == "false" && Text(Field<const char*>(punct, 0x28)) == "true", "numpunct<char> strings");
    Require(Field<char>(punct, 0x18) == '.' && Field<char>(punct, 0x19) == 0, "numpunct<char> follows localeconv");

    const void* widePunct = at(_ZNSt8__sce_v28numpunctIwE2idE_nid_postfix);
    Require(Wide(Field<const void*>(widePunct, 0x20)) == u"false" && Wide(Field<const void*>(widePunct, 0x28)) == u"true", "numpunct<wchar_t> strings");
    Require(Field<std::uint16_t>(widePunct, 0x18) == '.' && Field<std::uint16_t>(widePunct, 0x1a) == 0, "numpunct<wchar_t> follows localeconv");

    const void* money = at(_ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix);
    const void* moneyIntl = at(_ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix);
    Require(Size(money) == 0x70 && Size(moneyIntl) == 0x70, "moneypunct size");
    Require(Field<bool>(money, 0x44) == false && Field<bool>(moneyIntl, 0x44) == true, "moneypunct international flag");
    Require(Text(Field<const char*>(money, 0x30)) == "-" && Text(Field<const char*>(money, 0x28)).empty(), "moneypunct signs");
    Require(Field<std::int32_t>(money, 0x38) == 0, "moneypunct fraction digits for the C locale");
    Require(std::memcmp(static_cast<const char*>(money) + 0x3c, "$+xv", 4) == 0 && std::memcmp(static_cast<const char*>(money) + 0x40, "$+xv", 4) == 0, "moneypunct patterns for the C locale");
    Require(Field<std::uint32_t>(money, 0x48) == 1, "moneypunct conversion data");
    Require(Wide(Field<const void*>(at(_ZNSt8__sce_v210moneypunctIwLb0EE2idE_nid_postfix), 0x30)) == u"-", "moneypunct<wchar_t> minus sign");

    const void* timeGet = at(_ZNSt8__sce_v28time_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix);
    Require(Size(timeGet) == 0x58, "time_get<char> size");
    Require(Text(Field<const char*>(timeGet, 0x20)) == ":AM:am:PM:pm" && Text(Field<const char*>(timeGet, 0x10)).rfind(":Sun:Sunday", 0) == 0 && Text(Field<const char*>(timeGet, 0x18)).rfind(":Jan:January", 0) == 0, "time_get<char> names");
    Require(Field<std::int32_t>(timeGet, 0x28) == 2 && Field<std::uint32_t>(timeGet, 0x2c) == 1, "time_get<char> date order and conversion data");
    const void* wideTimeGet = at(_ZNSt8__sce_v28time_getIwNS_19istreambuf_iteratorIwNS_11char_traitsIwEEEEE2idE_nid_postfix);
    Require(Wide(Field<const void*>(wideTimeGet, 0x20)) == u":AM:am:PM:pm", "time_get<wchar_t> names");

    const void* timePut = at(_ZNSt8__sce_v28time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix);
    Require(Size(timePut) == 0x18 && Field<void*>(timePut, 0x10) == nullptr, "time_put<char> layout");
    DropLocale(locale);
}

static void TestOwnershipAndLeaks() {
    const auto before = gLive.size();
    auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x3f, locale, nullptr);
    Require(gLive.size() == before + 2 + 39 + 3 + 8 + 3, "the locale, its vector, 39 facets and the unsigned short strings are allocated");
    auto* copy = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpERKS1__nid_postfix(locale);
    const GuestLocale::Facet* shared = locale->facets[_ZNSt8__sce_v25ctypeIcE2idE_nid_postfix];
    Require(shared->references == 2, "a copy retains the facets");
    DropLocale(locale);
    Require(shared->references == 1, "the original releases its reference");
    DropLocale(copy);
    Require(gLive.size() == before, "the last release frees the facets and their strings");
}

static void TestCtypeDeleteModes() {
    auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x2, locale, nullptr);
    const auto before = gLive.size();
    auto* narrow = locale->facets[_ZNSt8__sce_v25ctypeIcE2idE_nid_postfix];
    void* table = TestHeapAllocate(16);
    std::memcpy(reinterpret_cast<char*>(narrow) + 0x10, &table, sizeof(table));
    const std::int32_t arrayOwned = -1;
    std::memcpy(reinterpret_cast<char*>(narrow) + 0x18, &arrayOwned, sizeof(arrayOwned));
    auto* wide = locale->facets[_ZNSt8__sce_v25ctypeIwE2idE_nid_postfix];
    void* wideTable = TestHeapAllocate(16);
    std::memcpy(reinterpret_cast<char*>(wide) + 0x10, &wideTable, sizeof(wideTable));
    std::memcpy(reinterpret_cast<char*>(wide) + 0x18, &arrayOwned, sizeof(arrayOwned));
    Require(gLive.size() == before + 2, "test tables allocated");
    DropLocale(locale);
    Require(Size(table) == 0 && Size(wideTable) == 0, "a ctype facet frees the table it owns");
}

static void TestLocaleSource() {
    auto* source = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x2, source, nullptr);
    GuestLocaleFacade facade{source};
    auto* target = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x2, target, &facade);
    for (const std::uint64_t id : {_ZNSt8__sce_v25ctypeIcE2idE_nid_postfix, _ZNSt8__sce_v25ctypeIwE2idE_nid_postfix}) {
        Require(target->facets[id] == source->facets[id], "a facet of the source locale is shared");
        Require(target->facets[id]->references == 2, "the target retains the shared facet");
    }
    DropLocale(target);
    DropLocale(source);
}

static void APS5_VABI MakeWithLocale(const void* argument) {
    MakeAll(0x8, _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false), static_cast<const GuestLocaleFacade*>(argument));
}

static void TestDefaultFacets() {
    GuestLocale::Implementation bad{{nullptr, 1, 0}, nullptr, 0, 0x3f, false, "nowhere"};
    GuestLocaleFacade badFacade{&bad};
    const auto before = gLive.size();
    SceV2CatchType = &_ZTISt13runtime_error_nid_postfix;
    SceV2OtherType = nullptr;
    SceV2Selector = 0;
    caught[0] = 0;
    SceV2Catch(MakeWithLocale, &badFacade);
    Require(caught[0] != 0, "a facet missing from a locale with a bad name throws runtime_error");
    Require(gLive.size() == before + 1, "the failed facet is freed and only the target locale stays");

    GuestLocale::Implementation blank{{nullptr, 1, 0}, nullptr, 0, 0x3f, false, "C"};
    GuestLocaleFacade blankFacade{&blank};
    auto* target = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x8 | 0x4, target, &blankFacade);
    const void* punct = target->facets[_ZNSt8__sce_v28numpunctIcE2idE_nid_postfix];
    Require(Field<char>(punct, 0x18) == '.' && Field<char>(punct, 0x19) == ',', "a default numpunct<char> uses '.' and ','");
    const void* widePunct = target->facets[_ZNSt8__sce_v28numpunctIwE2idE_nid_postfix];
    Require(Field<std::uint16_t>(widePunct, 0x18) == '.' && Field<std::uint16_t>(widePunct, 0x1a) == ',', "a default numpunct<wchar_t> uses '.' and ','");
    Require(reinterpret_cast<const GuestLocale::Facet*>(punct)->references == 2, "the default facet keeps a reference of its own");

    auto* again = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x8, again, &blankFacade);
    Require(again->facets[_ZNSt8__sce_v28numpunctIcE2idE_nid_postfix] == punct, "the default facet is reused");
    Require(reinterpret_cast<const GuestLocale::Facet*>(punct)->references == 3, "reuse takes another reference");
    DropLocale(again);
    DropLocale(target);
    Require(reinterpret_cast<const GuestLocale::Facet*>(punct)->references == 1, "the cached reference survives the locales");

    GuestLocale::Implementation badCached{{nullptr, 1, 0}, nullptr, 0, 0x3f, false, "nowhere"};
    GuestLocaleFacade badCachedFacade{&badCached};
    auto* cachedTarget = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x8, cachedTarget, &badCachedFacade);
    Require(cachedTarget->facets[_ZNSt8__sce_v28numpunctIcE2idE_nid_postfix] == punct, "a cached facet needs no valid name");
    DropLocale(cachedTarget);

    auto* global = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x10, global, nullptr);
    auto* previous = _ZNSt8__sce_v26locale16_GetgloballocaleEv_nid_postfix();
    _ZNSt8__sce_v26locale16_SetgloballocaleEPv_nid_postfix(global);
    GuestLocale::Implementation transparent{{nullptr, 1, 0}, nullptr, 0, 0x3f, true, "C"};
    GuestLocaleFacade transparentFacade{&transparent};
    auto* viaGlobal = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x10, viaGlobal, &transparentFacade);
    const std::uint64_t timeGetId = _ZNSt8__sce_v28time_getIcNS_19istreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE_nid_postfix;
    Require(viaGlobal->facets[timeGetId] == global->facets[timeGetId], "a transparent locale takes the global facet");
    _ZNSt8__sce_v26locale16_SetgloballocaleEPv_nid_postfix(previous);
    DropLocale(viaGlobal);
    DropLocale(global);
}

static void TestConventions() {
    SonyLconv* conventions = localeconv_nid_postfix();
    const SonyLconv saved = *conventions;
    char grouping[] = "\3";
    char currency[] = "EUR";
    char international[] = "USD";
    char positive[] = "+";
    char negative[] = "(";
    char monetaryPoint[] = ",";
    char monetarySeparator[] = ".";
    conventions->monetary[0] = currency;
    conventions->monetary[1] = international;
    conventions->monetary[2] = monetaryPoint;
    conventions->monetary[3] = grouping;
    conventions->monetary[4] = monetarySeparator;
    conventions->monetary[5] = negative;
    conventions->monetary[6] = positive;
    conventions->chars[0] = 2;
    conventions->chars[1] = 0;
    conventions->chars[2] = 1;
    conventions->chars[3] = 2;
    conventions->chars[4] = 1;
    conventions->chars[5] = 0;
    conventions->chars[6] = 1;
    conventions->chars[7] = 3;

    auto* locale = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    MakeAll(0x4, locale, nullptr);
    const void* money = locale->facets[_ZNSt8__sce_v210moneypunctIcLb0EE2idE_nid_postfix];
    const void* moneyIntl = locale->facets[_ZNSt8__sce_v210moneypunctIcLb1EE2idE_nid_postfix];
    Require(Field<std::int32_t>(money, 0x38) == 2 && Field<std::int32_t>(moneyIntl, 0x38) == 3, "moneypunct fraction digits follow localeconv");
    Require(std::memcmp(static_cast<const char*>(money) + 0x3c, "+$vx", 4) == 0, "moneypunct positive pattern");
    Require(std::memcmp(static_cast<const char*>(money) + 0x40, "v $+", 4) == 0, "moneypunct negative pattern");
    Require(std::memcmp(static_cast<const char*>(moneyIntl) + 0x3c, "$+xv", 4) == 0, "international moneypunct keeps the default pattern");
    Require(Field<char>(money, 0x18) == ',' && Field<char>(money, 0x19) == '.', "moneypunct separators follow localeconv");
    Require(Text(Field<const char*>(money, 0x20)).empty(), "moneypunct<char> keeps an empty currency");

    std::vector<const void*> ushort;
    for (std::uint64_t index = 0; index < locale->facetCount; ++index) {
        if (locale->facets[index] != nullptr && Size(locale->facets[index]) == 0x70 && Size(Field<const void*>(locale->facets[index], 0x20)) != 0) ushort.push_back(locale->facets[index]);
    }
    Require(ushort.size() == 2, "the unsigned short moneypunct facets own their strings");
    if (ushort.size() == 2) {
        Require(Wide(Field<const void*>(ushort[0], 0x20)) == u"EUR" && Wide(Field<const void*>(ushort[1], 0x20)) == u"USD", "unsigned short currency follows localeconv");
        Require(Wide(Field<const void*>(ushort[0], 0x28)) == u"+" && Wide(Field<const void*>(ushort[0], 0x30)) == u"(", "unsigned short signs follow localeconv");
        Require(Text(Field<const char*>(ushort[0], 0x10)) == "\3", "unsigned short grouping follows localeconv");
    }
    DropLocale(locale);
    *conventions = saved;
}

static void APS5_VABI MakeUshortFacets(const void* argument) {
    MakeAll(*static_cast<const std::int32_t*>(argument), _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false), nullptr);
}

static void TestAllocationFailures() {
    const std::int32_t categories[] = {0x8, 0x4, 0x10};
    const std::size_t callocs[] = {3, 8, 3};
    const std::size_t firstFacet[] = {3, 4, 3};
    for (std::size_t test = 0; test < 3; ++test) {
        for (std::size_t failing = 1; failing <= callocs[test]; ++failing) {
            const std::size_t before = gCallocBlocks.size();
            gCallocSeen = 0;
            gFailCalloc = failing;
            SceV2CatchType = &_ZTISt9bad_alloc_nid_postfix;
            SceV2OtherType = nullptr;
            SceV2Selector = 0;
            caught[0] = 0;
            SceV2Catch(MakeUshortFacets, &categories[test]);
            gFailCalloc = 0;
            Require(caught[0] != 0, "a failed string allocation throws bad_alloc");
            const std::size_t kept = failing > firstFacet[test] ? firstFacet[test] : 0;
            Require(gCallocBlocks.size() == before + kept, "a failed facet frees the strings it already built");
        }
    }
}

int main() {
    const std::array<void*, 7> heap{reinterpret_cast<void*>(TestHeapAllocate), reinterpret_cast<void*>(TestHeapFree), reinterpret_cast<void*>(TestHeapCalloc),
                                    reinterpret_cast<void*>(TestHeapReallocate), reinterpret_cast<void*>(TestHeapAlign), reinterpret_cast<void*>(TestHeapRealign),
                                    reinterpret_cast<void*>(TestHeapPosixAlign)};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    TestCategories();
    TestSingleFunctions();
    TestFields();
    TestOwnershipAndLeaks();
    TestCtypeDeleteModes();
    TestLocaleSource();
    TestConventions();
    TestDefaultFacets();
    TestAllocationFailures();
    if (failures != 0) return 1;
    std::puts("guest_sce_v2_makeloc passed");
    return 0;
}
