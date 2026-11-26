#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libSceLibcInternal/include/TimeGetFields.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/GuestTime.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "tests/CommandLineArgument.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
extern std::uint64_t _ZNSt5ctypeIwE2idE_nid_postfix;
void APS5_VABI sceLibcInternalUnknown6_110_IuFImJ5pkTk_nid_no_patch(void* self, const void* info, std::size_t refs);
}

namespace {

struct WideTimeGetView {
    GuestLocale::Facet base;
    const char16_t* days;
    const char16_t* months;
    const char16_t* amPm;
    std::int32_t dateOrder;
    std::uint32_t reserved;
};

struct WideTimeGetStorage {
    GuestLocale::Facet base;
    const char16_t* days;
    const char16_t* months;
    const char16_t* amPm;
    std::int32_t dateOrder;
    std::uint32_t reserved;
    std::array<std::byte, 48> untouchedSuffix;
};

struct TimePutCharStorage {
    GuestLocale::Facet base;
    GuestLocale::TimeInfo names;
};

static_assert(sizeof(WideTimeGetView) == 0x30);
static_assert(sizeof(WideTimeGetStorage) == 0x60);
static_assert(sizeof(TimePutCharStorage) == 0xb8);

struct OutputStreambuf {
    GuestLocale::Streambuf buffer;
    char* next = nullptr;
    char* end = nullptr;
    std::string narrowOut;
    std::u16string wideOut;
};

int APS5_VABI OverflowNarrow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<OutputStreambuf*>(self);
    stream->narrowOut.push_back(static_cast<char>(character));
    return character;
}

int APS5_VABI OverflowWide(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<OutputStreambuf*>(self);
    stream->wideOut.push_back(static_cast<char16_t>(character));
    return character;
}

const GuestLocale::StreambufVtable g_narrowOutVtable{{}, OverflowNarrow};
const GuestLocale::StreambufVtable g_wideOutVtable{{}, OverflowWide};

void* g_lastFreed = nullptr;
std::size_t g_freeCount = 0;
#ifdef _WIN32
HMODULE g_library = nullptr;
#else
void* g_library = nullptr;
#endif

void* APS5_VABI TestHeapAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestHeapFree(void* pointer) {
    g_lastFreed = pointer;
    ++g_freeCount;
    std::free(pointer);
}
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer != nullptr ? 0 : 12;
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal time_get_fields: %s\n", message);
    std::exit(1);
}

template<typename TFunction>
TFunction ResolveExport(const char* symbol) {
#ifdef _WIN32
    auto* address = reinterpret_cast<void*>(GetProcAddress(g_library, symbol));
#else
    void* address = dlsym(g_library, symbol);
#endif
    Require(address != nullptr, "missing exported symbol");
    return reinterpret_cast<TFunction>(address);
}

template<typename TCall>
bool Throws(TCall&& call) {
    try {
        call();
        return false;
    } catch (...) {
        return true;
    }
}

void CheckB2AdditionalCtorsAndGetcat(bool hasLibrary) {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    GuestLocale::LocinfoStorage info{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&info, "C");
    const GuestLocale::TimeInfo* expectedTimes = _Getptimes_nid_postfix();

    const std::size_t largeRefs = (std::size_t{0x23456789u} << 32) | 9u;
    for (const bool complete : {true, false}) {
        WideTimeGetStorage storage{};
        storage.untouchedSuffix.fill(std::byte{0x5a});
        const auto expectedSuffix = storage.untouchedSuffix;
        if (complete) {
            _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(&storage.base, largeRefs);
        } else {
            _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(&storage.base, largeRefs);
        }
        Require(storage.base.vtable != nullptr && storage.base.references == 9 && storage.base.reserved == 0x23456789u, "wide time_get refs ctor stores full 64-bit refs");
        Require(storage.dateOrder == 2 && storage.reserved == 0 && storage.untouchedSuffix == expectedSuffix, "wide time_get refs ctor sets 0x30 layout without suffix write");
    }

    for (const bool complete : {true, false}) {
        WideTimeGetStorage storage{};
        storage.untouchedSuffix.fill(std::byte{0x3c});
        const auto expectedSuffix = storage.untouchedSuffix;
        if (complete) {
            _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(&storage.base, &info, largeRefs);
        } else {
            _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(&storage.base, &info, largeRefs);
        }
        Require(storage.base.vtable != nullptr && storage.base.references == 9 && storage.base.reserved == 0x23456789u, "wide time_get locinfo ctor stores full 64-bit refs");
        Require(storage.dateOrder == 2 && storage.reserved == 0 && storage.untouchedSuffix == expectedSuffix, "wide time_get locinfo ctor sets 0x30 layout");
    }

    {
        auto* heapWideGet = static_cast<WideTimeGetStorage*>(_Znwm_nid_postfix(sizeof(WideTimeGetStorage)));
        std::memset(heapWideGet, 0, sizeof(*heapWideGet));
        _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(&heapWideGet->base, 1);
        const std::size_t freesBefore = g_freeCount;
        g_lastFreed = nullptr;
        _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED0Ev_nid_postfix(&heapWideGet->base);
        Require(g_freeCount == freesBefore + 1 && g_lastFreed == heapWideGet, "wide time_get D0 frees object");
    }

    {
        auto* heapCharGet = static_cast<GuestLocale::Facet*>(_Znwm_nid_postfix(sizeof(GuestLocale::Facet)));
        std::memset(heapCharGet, 0, sizeof(*heapCharGet));
        const std::size_t freesBefore = g_freeCount;
        g_lastFreed = nullptr;
        libSceLibcInternalInventory_183_nid_no_patch(heapCharGet);
        Require(g_freeCount == freesBefore + 1 && g_lastFreed == heapCharGet, "char time_get D0 frees object");

        GuestLocale::Facet charFacet{};
        charFacet.references = 3;
        const std::size_t freesBeforeD1 = g_freeCount;
        libSceLibcInternalInventory_184_nid_no_patch(&charFacet);
        Require(charFacet.vtable == nullptr && charFacet.references == 3 && charFacet.reserved == 0 && g_freeCount == freesBeforeD1, "char time_get D1 leaves the facet and frees nothing");

        WideTimeGetStorage wideStorage{};
        wideStorage.untouchedSuffix.fill(std::byte{0x6b});
        const auto expectedSuffix = wideStorage.untouchedSuffix;
        _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(&wideStorage.base, 4);
        const void* const wideVtable = wideStorage.base.vtable;
        const char16_t* const wideDays = wideStorage.days;
        libSceLibcInternalInventory_192_nid_no_patch(&wideStorage.base);
        Require(wideStorage.base.vtable == wideVtable && wideStorage.base.references == 4 && wideStorage.days == wideDays && wideStorage.untouchedSuffix == expectedSuffix
                && g_freeCount == freesBeforeD1,
            "wide time_get D1 leaves the facet and frees nothing");
    }

    {
        GuestLocale::Implementation classicLoc{{nullptr, 1, 0}, nullptr, 0, 5, false, "C"};
        GuestLocale::Implementation* classicPtr = &classicLoc;
        GuestLocale::Facet* catFacet = nullptr;
        const std::size_t catMask = _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&catFacet, &classicPtr);
        Require(catMask == 5 && catFacet != nullptr && catFacet->references == 0, "wide time_get _Getcat allocates C facet with 0 refs");
        GuestLocale::Facet* const savedCatFacet = catFacet;
        const std::size_t freesBeforeReuse = g_freeCount;
        Require(_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&catFacet, &classicPtr) == 5, "wide time_get _Getcat preserves existing non-null facet");
        Require(catFacet == savedCatFacet && g_freeCount == freesBeforeReuse, "wide time_get _Getcat leaves existing pointer untouched");
        Require(_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(nullptr, nullptr) == 5, "wide time_get _Getcat returns mask when facet pointer is null");
        const auto* wideView = reinterpret_cast<const WideTimeGetView*>(catFacet);
        Require(wideView->dateOrder == 2 && std::u16string_view(wideView->days).starts_with(u":Sun:"), "wide time_get _Getcat initializes fields");
        catFacet->vtable->deleteObject(catFacet);

        GuestLocale::Implementation badLoc{{nullptr, 1, 0}, nullptr, 0, 5, false, "fr_FR"};
        GuestLocale::Implementation* badPtr = &badLoc;
        GuestLocale::Facet* badFacet = nullptr;
        const std::size_t freesBefore = g_freeCount;
        Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&badFacet, &badPtr); }), "wide time_get _Getcat rejects non-C locale");
        Require(badFacet == nullptr && g_freeCount == freesBefore + 1, "wide time_get _Getcat frees allocation on error without writing output");
    }

    {
        auto* heapPutChar = static_cast<TimePutCharStorage*>(_Znwm_nid_postfix(sizeof(TimePutCharStorage)));
        std::memset(heapPutChar, 0, sizeof(*heapPutChar));
        _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(&heapPutChar->base, 7);
        Require(heapPutChar->base.vtable != nullptr && heapPutChar->base.references == 7, "time_put<char> C1Em initializes facet header");
        Require(std::memcmp(&heapPutChar->names, expectedTimes, sizeof(*expectedTimes)) == 0, "time_put<char> C1Em initializes TimeInfo");

        const void* savedVtable = heapPutChar->base.vtable;
        heapPutChar->base.references = 42;
        std::memset(&heapPutChar->names, 0, sizeof(heapPutChar->names));
        _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo_nid_postfix(&heapPutChar->base, &info);
        Require(heapPutChar->base.vtable == savedVtable && heapPutChar->base.references == 42, "time_put<char> _Init preserves vtable and refs");
        Require(std::memcmp(&heapPutChar->names, expectedTimes, sizeof(*expectedTimes)) == 0, "time_put<char> _Init populates TimeInfo");

        const std::size_t freesBefore = g_freeCount;
        g_lastFreed = nullptr;
        _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED0Ev_nid_postfix(&heapPutChar->base);
        Require(g_freeCount == freesBefore + 1 && g_lastFreed == heapPutChar, "time_put<char> D0 frees object");
    }

    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(nullptr, 0); }), "wide time_get C1Em null throws");
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix(nullptr, 0); }), "wide time_get C2Em null throws");
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(nullptr, &info, 0); }), "wide time_get C1Locinfo null self throws");
    WideTimeGetStorage tmpWide{};
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix(&tmpWide.base, nullptr, 0); }), "wide time_get C1Locinfo null locinfo throws");
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix(&tmpWide.base, nullptr, 0); }), "wide time_get C2Locinfo null locinfo throws");
    Require(Throws([] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED0Ev_nid_postfix(nullptr); }), "wide time_get D0 null throws");
    Require(Throws([] { libSceLibcInternalInventory_183_nid_no_patch(nullptr); }), "char time_get D0 null throws");
    Require(Throws([] { libSceLibcInternalInventory_184_nid_no_patch(nullptr); }), "char time_get D1 null throws");
    Require(Throws([] { libSceLibcInternalInventory_192_nid_no_patch(nullptr); }), "wide time_get D1 null throws");
    GuestLocale::Facet* nullCat = nullptr;
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&nullCat, nullptr); }), "wide time_get _Getcat null locale throws");
    Require(Throws([] { _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix(nullptr, 0); }), "char time_put C1Em null throws");
    TimePutCharStorage tmpPutChar{};
    Require(Throws([&] { _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo_nid_postfix(nullptr, &info); }), "char time_put _Init null self throws");
    Require(Throws([&] { _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo_nid_postfix(&tmpPutChar.base, nullptr); }), "char time_put _Init null locinfo throws");
    Require(Throws([] { _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED0Ev_nid_postfix(nullptr); }), "char time_put D0 null throws");
    Require(Throws([] { libSceLibcInternalInventory_197_nid_no_patch(); }), "unsupported wide time_put constructor remains throwing");
    Require(Throws([] { libSceLibcInternalInventory_198_nid_no_patch(); }), "unsupported wide time_put constructor remains throwing");
    Require(NotImplementedCalls_nid_no_patch() > before, "B2 error paths reported");

    if (hasLibrary) {
        using GetcatFn = std::size_t (APS5_VABI*)(GuestLocale::Facet**, const GuestLocale::Implementation* const*);
        using RefsCtorFn = void (APS5_VABI*)(GuestLocale::Facet*, std::size_t);
        using LocinfoCtorFn = void (APS5_VABI*)(GuestLocale::Facet*, const GuestLocale::LocinfoStorage*, std::size_t);
        using InitFn = void (APS5_VABI*)(GuestLocale::Facet*, const GuestLocale::LocinfoStorage*);
        using DtorFn = void (APS5_VABI*)(GuestLocale::Facet*);

        const auto nidGetcat = ResolveExport<GetcatFn>("_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix");
        const auto nidGetC1Loc = ResolveExport<LocinfoCtorFn>("_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1ERKSt8_Locinfom_nid_postfix");
        const auto nidGetC1Refs = ResolveExport<RefsCtorFn>("_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix");
        const auto nidGetC2Loc = ResolveExport<LocinfoCtorFn>("_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2ERKSt8_Locinfom_nid_postfix");
        const auto nidGetC2Refs = ResolveExport<RefsCtorFn>("_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2Em_nid_postfix");
        const auto nidGetD0 = ResolveExport<DtorFn>("_ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED0Ev_nid_postfix");
        const auto nidPutInit = ResolveExport<InitFn>("_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE5_InitERKSt8_Locinfo_nid_postfix");
        const auto nidPutC1Refs = ResolveExport<RefsCtorFn>("_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1Em_nid_postfix");
        const auto nidPutD0 = ResolveExport<DtorFn>("_ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEED0Ev_nid_postfix");
        const auto nidWidePutInit = ResolveExport<InitFn>("_ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo_nid_postfix");

        GuestLocale::Implementation classicLoc{{nullptr, 1, 0}, nullptr, 0, 5, false, "C"};
        GuestLocale::Implementation* classicPtr = &classicLoc;
        GuestLocale::Facet* facet = nullptr;
        Require(nidGetcat(&facet, &classicPtr) == 5 && facet != nullptr, "NID _Getcat works");
        nidGetD0(facet);

        for (const auto fn : {nidGetC1Refs, nidGetC2Refs}) {
            auto* obj = static_cast<WideTimeGetStorage*>(_Znwm_nid_postfix(sizeof(WideTimeGetStorage)));
            fn(&obj->base, 2);
            Require(obj->dateOrder == 2 && obj->base.references == 2, "NID wide time_get refs ctor");
            nidGetD0(&obj->base);
        }
        for (const auto fn : {nidGetC1Loc, nidGetC2Loc}) {
            auto* obj = static_cast<WideTimeGetStorage*>(_Znwm_nid_postfix(sizeof(WideTimeGetStorage)));
            fn(&obj->base, &info, 3);
            Require(obj->dateOrder == 2 && obj->base.references == 3, "NID wide time_get locinfo ctor");
            nidGetD0(&obj->base);
        }

        auto* charPut = static_cast<TimePutCharStorage*>(_Znwm_nid_postfix(sizeof(TimePutCharStorage)));
        nidPutC1Refs(&charPut->base, 4);
        std::memset(&charPut->names, 0, sizeof(charPut->names));
        nidPutInit(&charPut->base, &info);
        Require(std::memcmp(&charPut->names, expectedTimes, sizeof(*expectedTimes)) == 0, "NID time_put<char> _Init");
        nidPutD0(&charPut->base);

        GuestLocale::Facet widePutFacet{nullptr, 5, 0};
        nidWidePutInit(&widePutFacet, &info);
        Require(widePutFacet.vtable != nullptr && widePutFacet.references == 5, "NID ZolDcuDSD0Q installs wide time_put vtable");
    }

    _ZNSt8_LocinfoD1Ev_nid_postfix(&info);
}

void CheckGroup1ConstructionAndLifetime() {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();

    for (const bool complete : {true, false}) {
        WideTimeGetStorage storage{};
        storage.untouchedSuffix.fill(std::byte{0x5a});
        const auto expectedSuffix = storage.untouchedSuffix;
        if (complete) {
            _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&storage.base, "C", 2);
        } else {
            _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2EPKcm_nid_postfix(&storage.base, "C", 2);
        }
        Require(storage.base.vtable != nullptr && storage.base.references == 2 && storage.base.reserved == 0, "wide time_get ctor sets facet header");
        Require(storage.days != nullptr && std::u16string_view(storage.days) == u":Sun:Sunday:Mon:Monday:Tue:Tuesday:Wed:Wednesday:Thu:Thursday:Fri:Friday:Sat:Saturday", "wide time_get days table");
        Require(storage.months != nullptr && std::u16string_view(storage.months) == u":Jan:January:Feb:February:Mar:March:Apr:April:May:May:Jun:June:Jul:July:Aug:August:Sep:September:Oct:October:Nov:November:Dec:December", "wide time_get months table");
        Require(storage.amPm != nullptr && std::u16string_view(storage.amPm) == u":AM:am:PM:pm", "wide time_get am/pm table");
        Require(storage.dateOrder == 2 && storage.reserved == 0, "wide time_get dateorder is mdy");
        Require(storage.untouchedSuffix == expectedSuffix, "wide time_get ctor does not touch bytes past 0x30");
        const auto* vtable = reinterpret_cast<const GuestLocale::TimeGetVtable*>(storage.base.vtable);
        Require(vtable->dateOrder(&storage.base) == 2, "wide time_get vtable dateOrder works");
        _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev_nid_postfix(&storage.base);
        Require(storage.base.references == 2 && storage.untouchedSuffix == expectedSuffix, "wide time_get D2 preserves fields");
    }

    auto* heapWideGet = static_cast<WideTimeGetStorage*>(_Znwm_nid_postfix(sizeof(WideTimeGetStorage)));
    std::memset(heapWideGet, 0, sizeof(*heapWideGet));
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&heapWideGet->base, "C", 1);
    heapWideGet->base.vtable->retain(&heapWideGet->base);
    Require(heapWideGet->base.references == 2, "wide time_get retain increments refs");
    Require(heapWideGet->base.vtable->release(&heapWideGet->base) == nullptr && heapWideGet->base.references == 1, "first release returns null");
    auto* toDelete = heapWideGet->base.vtable->release(&heapWideGet->base);
    Require(toDelete == &heapWideGet->base, "last release returns facet pointer");
    toDelete->vtable->deleteObject(toDelete);

    GuestLocale::Facet charFacet{};
    _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev_nid_postfix(&charFacet);

    GuestLocale::LocinfoStorage info{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&info, "C");
    const GuestLocale::TimeInfo* expectedTimes = _Getptimes_nid_postfix();

    TimePutCharStorage putFromLocinfo{};
    _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(&putFromLocinfo.base, &info, 3);
    Require(putFromLocinfo.base.vtable != nullptr && putFromLocinfo.base.references == 3, "time_put<char> C1 locinfo ctor");
    Require(std::memcmp(&putFromLocinfo.names, expectedTimes, sizeof(*expectedTimes)) == 0, "time_put<char> C1 populates TimeInfo");

    TimePutCharStorage putDefault{};
    _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(&putDefault.base, 1);
    Require(putDefault.base.vtable != nullptr && putDefault.base.references == 1, "time_put<char> C2 default ctor");
    Require(std::memcmp(&putDefault.names, expectedTimes, sizeof(*expectedTimes)) == 0, "time_put<char> C2 populates TimeInfo");

    TimePutCharStorage putBaseFromLocinfo{};
    sceLibcInternalUnknown6_110_IuFImJ5pkTk_nid_no_patch(&putBaseFromLocinfo.base, &info, 4);
    Require(putBaseFromLocinfo.base.vtable == putFromLocinfo.base.vtable && putBaseFromLocinfo.base.references == 4 && putBaseFromLocinfo.base.reserved == 0,
        "time_put<char> C2 locinfo ctor sets the facet header as C1 does");
    Require(std::memcmp(&putBaseFromLocinfo.names, expectedTimes, sizeof(*expectedTimes)) == 0, "time_put<char> C2 locinfo ctor populates TimeInfo");
    Require(Throws([&] { sceLibcInternalUnknown6_110_IuFImJ5pkTk_nid_no_patch(nullptr, &info, 0); }), "time_put<char> C2 locinfo ctor null self throws");
    Require(Throws([&] { sceLibcInternalUnknown6_110_IuFImJ5pkTk_nid_no_patch(&putBaseFromLocinfo.base, nullptr, 0); }), "time_put<char> C2 locinfo ctor null locinfo throws");

    GuestTm tm{};
    tm.tm_year = 126;
    tm.tm_mon = 10;
    tm.tm_mday = 16;
    tm.tm_hour = 9;
    tm.tm_min = 4;
    tm.tm_sec = 5;
    OutputStreambuf narrowStream{};
    narrowStream.buffer = {&g_narrowOutVtable, {}, &narrowStream.next, {}, &narrowStream.end};
    const auto* putCharVtable = reinterpret_cast<const GuestLocale::TimePutVtable*>(putDefault.base.vtable);
    auto narrowOut = putCharVtable->put(&putDefault.base, {false, &narrowStream.buffer}, nullptr, ' ', &tm, 'Y', 0);
    Require(!narrowOut.failed && narrowStream.narrowOut == "2026", "time_put<char> vtable put formats %Y");

    struct WideTimePutGuarded {
        GuestLocale::Facet base;
        std::array<std::byte, 16> guard;
    } putWideGuarded{};
    putWideGuarded.guard.fill(std::byte{0xa5});
    const auto expectedGuard = putWideGuarded.guard;
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(&putWideGuarded.base, 4);
    Require(putWideGuarded.base.vtable != nullptr && putWideGuarded.base.references == 4 && putWideGuarded.guard == expectedGuard, "time_put<wchar_t> C1 writes only 0x10-byte Facet header");

    OutputStreambuf wideStream{};
    wideStream.buffer = {&g_wideOutVtable, {}, &wideStream.next, {}, &wideStream.end};
    const auto* putWideVtable = reinterpret_cast<const GuestLocale::WideTimePutVtable*>(putWideGuarded.base.vtable);
    auto wideOut = putWideVtable->put(&putWideGuarded.base, {false, &wideStream.buffer}, nullptr, u' ', &tm, 'Y', 0);
    Require(!wideOut.failed && wideStream.wideOut == u"2026", "time_put<wchar_t> vtable put formats %Y");
    _ZNSt8_LocinfoD1Ev_nid_postfix(&info);

    WideTimeGetStorage invalidStorage{};
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(nullptr, "C", 0); }), "wide time_get C1 null self throws");
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2EPKcm_nid_postfix(nullptr, "C", 0); }), "wide time_get C2 null self throws");
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&invalidStorage.base, nullptr, 0); }), "wide time_get C1 null name throws");
    Require(Throws([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC2EPKcm_nid_postfix(&invalidStorage.base, "en_US", 0); }), "wide time_get C2 non-C name throws");
    Require(Throws([] { _ZNSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEED2Ev_nid_postfix(nullptr); }), "char time_get D2 null throws");
    Require(Throws([] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEED2Ev_nid_postfix(nullptr); }), "wide time_get D2 null throws");
    Require(Throws([&] { _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(nullptr, &info, 0); }), "char time_put C1 null self throws");
    Require(Throws([&] { _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC1ERKSt8_Locinfom_nid_postfix(&putFromLocinfo.base, nullptr, 0); }), "char time_put C1 null locinfo throws");
    Require(Throws([] { _ZNSt8time_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEEC2Em_nid_postfix(nullptr, 0); }), "char time_put C2 null self throws");
    Require(Throws([] { _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(nullptr, 0); }), "wide time_put C1 null self throws");
    GuestLocale::Facet reinitWideFacet{nullptr, 4, 0};
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo_nid_postfix(&reinitWideFacet, &info);
    Require(reinitWideFacet.vtable == &putWideVtable->facet && reinitWideFacet.references == 4, "wide time_put _Init installs vtable without touching references");
    _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo_nid_postfix(&putWideGuarded.base, &info);
    Require(putWideGuarded.base.vtable != nullptr && putWideGuarded.base.references == 4 && putWideGuarded.guard == expectedGuard, "wide time_put _Init preserves guarded storage");
    Require(Throws([&] { _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo_nid_postfix(nullptr, &info); }), "wide time_put _Init null self throws");
    Require(Throws([&] { _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE5_InitERKSt8_Locinfo_nid_postfix(&putWideGuarded.base, nullptr); }), "wide time_put _Init null locinfo throws");
    Require(NotImplementedCalls_nid_no_patch() > before, "error paths reported via NotImplemented");
}

template<typename TChar>
struct InputStreambuf {
    GuestLocale::Streambuf buffer{};
    char* next = nullptr;
    char* end = nullptr;
    std::basic_string<TChar> storage;
    std::size_t index = 0;
    int underflows = 0;
    int uflows = 0;

    void Setup(std::basic_string<TChar> text, bool buffered);
    GuestLocale::InputIterator Begin() {
        return GuestLocale::InputIterator{{reinterpret_cast<std::uint64_t>(&buffer), 0}};
    }
    TChar RemainingFirst() const {
        TChar value{};
        if (next != nullptr && next < end) {
            std::memcpy(&value, next, sizeof(TChar));
            return value;
        }
        return index < storage.size() ? storage[index] : value;
    }
};

template<typename TChar>
int APS5_VABI InputUnderflow(GuestLocale::Streambuf* self) {
    auto* stream = reinterpret_cast<InputStreambuf<TChar>*>(self);
    ++stream->underflows;
    if (stream->index >= stream->storage.size()) return -1;
    if constexpr (sizeof(TChar) == 1) {
        return static_cast<unsigned char>(stream->storage[stream->index]);
    } else {
        return static_cast<std::uint16_t>(stream->storage[stream->index]);
    }
}

template<typename TChar>
int APS5_VABI InputUflow(GuestLocale::Streambuf* self) {
    auto* stream = reinterpret_cast<InputStreambuf<TChar>*>(self);
    ++stream->uflows;
    if (stream->index >= stream->storage.size()) return -1;
    const auto element = stream->storage[stream->index++];
    if constexpr (sizeof(TChar) == 1) {
        return static_cast<unsigned char>(element);
    } else {
        return static_cast<std::uint16_t>(element);
    }
}

struct InputStreambufVtable {
    std::array<std::uintptr_t, 9> slots{};
};

template<typename TChar>
const InputStreambufVtable& GetInputVtable() {
    static const InputStreambufVtable vt = [] {
        InputStreambufVtable table{};
        table.slots[7] = reinterpret_cast<std::uintptr_t>(&InputUnderflow<TChar>);
        table.slots[8] = reinterpret_cast<std::uintptr_t>(&InputUflow<TChar>);
        return table;
    }();
    return vt;
}

template<typename TChar>
void InputStreambuf<TChar>::Setup(std::basic_string<TChar> text, bool buffered) {
    storage = std::move(text);
    index = 0;
    underflows = 0;
    uflows = 0;
    next = nullptr;
    end = nullptr;
    buffer = {};
    buffer.vtable = reinterpret_cast<const GuestLocale::StreambufVtable*>(GetInputVtable<TChar>().slots.data());
    if (buffered && !storage.empty()) {
        next = reinterpret_cast<char*>(storage.data());
        end = next + storage.size() * sizeof(TChar);
        index = storage.size();
        buffer.reserved[6] = &next;
        buffer.reservedCounts[2] = &end;
    }
}

GuestLocale::InputIterator EndIter() {
    return GuestLocale::InputIterator{{0, 1}};
}

bool APS5_VABI CustomWideIs(const GuestLocale::Facet*, short mask, std::uint16_t ch) {
    if (mask == 0x44) return ch == u' ' || ch == u'\t';
    if (mask == 0x20) return ch >= u'0' && ch <= u'9';
    return false;
}

char APS5_VABI CustomWideNarrow(const GuestLocale::Facet*, std::uint16_t ch, char fallback) {
    return ch <= 0x7f ? static_cast<char>(ch) : fallback;
}

std::uint16_t APS5_VABI CustomWideToLower(const GuestLocale::Facet*, std::uint16_t ch) {
    return (ch >= u'A' && ch <= u'Z') ? static_cast<std::uint16_t>(ch - u'A' + u'a') : ch;
}

struct WideCtypeFixture {
    GuestLocale::Facet base;
    std::array<std::uintptr_t, 16> vt{};
};

WideCtypeFixture MakeWideCtype() {
    WideCtypeFixture f{};
    f.vt[4] = reinterpret_cast<std::uintptr_t>(&CustomWideIs);
    f.vt[8] = reinterpret_cast<std::uintptr_t>(&CustomWideToLower);
    f.vt[14] = reinterpret_cast<std::uintptr_t>(&CustomWideNarrow);
    f.base.vtable = reinterpret_cast<const GuestLocale::FacetVtable*>(f.vt.data());
    return f;
}

void CheckGroup2GetInt() {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    WideTimeGetStorage facet{};
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&facet.base, "C", 0);
    WideCtypeFixture ctype = MakeWideCtype();

    for (const bool buffered : {true, false}) {
        InputStreambuf<char16_t> sb{};
        {
            sb.Setup(u"42abc", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = -1;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 99, &val, &ctype.base);
            Require(state == 0 && val == 42, "valid integer without eof");
        }
        {
            sb.Setup(u"0007", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = -1;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 9999, &val, &ctype.base);
            Require(state == 1 && val == 7, "leading zeros collapsed and eofbit set");
        }
        {
            sb.Setup(u" 5", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = -1;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 1, 31, &val, &ctype.base);
            Require(state == 1 && val == 5, "leading space allowed in _Getint_v2");
        }
        {
            sb.Setup(u"1234", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = -1;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 59, &val, &ctype.base);
            Require(state == 0 && val == 12, "max width respects _Hi digits");
        }
        {
            sb.Setup(u"65", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = 777;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 59, &val, &ctype.base);
            Require(state == 3 && val == 777, "out of range sets failbit+eofbit and leaves val unchanged");
        }
        {
            sb.Setup(u"xyz", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = 888;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 59, &val, &ctype.base);
            Require(state == 2 && val == 888, "non-digit sets failbit without eofbit");
        }
        {
            sb.Setup(u"+7", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = -1;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 59, &val, &ctype.base);
            Require(state == 1 && val == 7, "plus sign accepted");
        }
        {
            sb.Setup(u"-5", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = 0;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, -9, 9, &val, &ctype.base);
            Require(state == 1 && val == -5, "minus sign accepted inside range");
        }
        {
            sb.Setup(u"-5", buffered);
            auto first = sb.Begin();
            auto last = EndIter();
            int val = 666;
            const auto state = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &first, &last, 0, 59, &val, &ctype.base);
            Require(state == 3 && val == 666, "negative value below range fails");
        }
    }

    InputStreambuf<char16_t> emptySb{};
    emptySb.Setup(u"", false);
    auto emptyFirst = emptySb.Begin();
    auto last = EndIter();
    int val = 999;
    Require(_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &emptyFirst, &last, 0, 59, &val, &ctype.base) == 3 && val == 999, "empty input returns eofbit|failbit");

    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(nullptr, &emptyFirst, &last, 0, 59, &val, &ctype.base); }), "_Getint null self throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, nullptr, &last, 0, 59, &val, &ctype.base); }), "_Getint null first throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix(&facet.base, &emptyFirst, &last, 0, 59, &val, nullptr); }), "_Getint null ctype throws");
    Require(NotImplementedCalls_nid_no_patch() > before, "_Getint error paths reported");
}

template<typename TChar>
std::basic_string<TChar> Ascii(const char* text) {
    std::basic_string<TChar> out;
    for (const char* p = text; *p != '\0'; ++p) out.push_back(static_cast<TChar>(static_cast<unsigned char>(*p)));
    return out;
}

template<typename TChar, typename TGetTimeFn, typename TGetYearFn, typename TGetMonthFn>
void CheckGroup3Typed(const GuestLocale::Facet* facet, TGetTimeFn getTime, TGetYearFn getYear, TGetMonthFn getMonth) {
    GuestLocale::IosBase ios{};
    GuestLocale::Implementation* classic = _ZSt21_sceLibcClassicLocale_nid_postfix;
    ios.locale = &classic;
    InputStreambuf<TChar> sb{};

    for (const bool buffered : {true, false}) {
        {
            sb.Setup(Ascii<TChar>("23:59:60"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getTime(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 1 && tm.tm_hour == 23 && tm.tm_min == 59 && tm.tm_sec == 60, "do_get_time parses leap second to eof");
        }
        {
            sb.Setup(Ascii<TChar>("07:08:09Z"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getTime(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_hour == 7 && tm.tm_min == 8 && tm.tm_sec == 9 && sb.RemainingFirst() == 'Z', "do_get_time leaves trailing character unconsumed");
        }
        {
            sb.Setup(Ascii<TChar>("12-34:56"), buffered);
            GuestTm tm{};
            tm.tm_min = 44;
            std::int32_t state = 0;
            getTime(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 2 && tm.tm_hour == 12 && tm.tm_min == 44, "do_get_time rejects non-colon separator");
        }
        {
            sb.Setup(Ascii<TChar>("12:34"), buffered);
            GuestTm tm{};
            tm.tm_sec = 55;
            std::int32_t state = 0;
            getTime(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 3 && tm.tm_sec == 55, "do_get_time truncated at second separator sets eofbit|failbit");
        }
        {
            sb.Setup(Ascii<TChar>("12:34:56"), buffered);
            GuestTm tm{};
            tm.tm_hour = 5;
            tm.tm_min = 44;
            tm.tm_sec = 33;
            std::int32_t state = 2;
            getTime(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 2 && tm.tm_hour == 12 && tm.tm_min == 44 && tm.tm_sec == 33 && sb.RemainingFirst() == ':', "do_get_time with incoming failbit stores the hour and stops at the first separator");
        }

        const struct {
            const char* text;
            int expectedYear;
            std::int32_t expectedState;
        } yearCases[] = {
            {"2026", 126, 1},
            {"68", 168, 1},
            {"69", 69, 1},
            {"0069", -1831, 1},
            {"99", 99, 1},
            {"00", 100, 1},
            {"2025x", 125, 0},
        };
        for (const auto& c : yearCases) {
            sb.Setup(Ascii<TChar>(c.text), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getYear(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == c.expectedState && tm.tm_year == c.expectedYear, "do_get_year conversion");
        }

        {
            sb.Setup(Ascii<TChar>("nOvEmBeR"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getMonth(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_mon == 10, "do_get_monthname case-insensitive full name");
        }
        {
            sb.Setup(Ascii<TChar>("jAnX"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getMonth(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_mon == 0 && sb.RemainingFirst() == 'X', "do_get_monthname stops cleanly at abbreviated match");
        }
        {
            sb.Setup(Ascii<TChar>("May"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getMonth(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_mon == 4, "do_get_monthname resolves May");
        }
        {
            sb.Setup(Ascii<TChar>("JanuaX"), buffered);
            GuestTm tm{};
            tm.tm_mon = 9;
            std::int32_t state = 0;
            getMonth(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 2 && tm.tm_mon == 9, "do_get_monthname fails partial long name mismatch");
        }
        {
            sb.Setup(Ascii<TChar>(""), buffered);
            GuestTm tm{};
            tm.tm_mon = 9;
            std::int32_t state = 0;
            getMonth(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 2 && tm.tm_mon == 9, "do_get_monthname fails empty input");
        }
    }
}

void CheckGroup3Fields() {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    GuestLocale::Facet charFacet{};
    CheckGroup3Typed<char>(
        &charFacet,
        &_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
        &_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
        &_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix);

    WideTimeGetStorage wideFacet{};
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&wideFacet.base, "C", 0);
    const auto* vt = reinterpret_cast<const GuestLocale::TimeGetVtable*>(wideFacet.base.vtable);
    CheckGroup3Typed<char16_t>(&wideFacet.base, vt->getTime, vt->getYear, vt->getMonthname);

    GuestTm tm{};
    std::int32_t state = 0;
    GuestLocale::IosBase ios{};
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(nullptr, EndIter(), EndIter(), &ios, &state, &tm); }), "char do_get_time null self throws");
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(&charFacet, EndIter(), EndIter(), &ios, nullptr, &tm); }), "char do_get_year null state throws");
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(&charFacet, EndIter(), EndIter(), &ios, &state, nullptr); }), "char do_get_monthname null tm throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(&wideFacet.base, EndIter(), EndIter(), &ios, &state, nullptr); }), "wide do_get_time null tm throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(nullptr, EndIter(), EndIter(), &ios, &state, &tm); }), "wide do_get_year null self throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(&wideFacet.base, EndIter(), EndIter(), &ios, nullptr, &tm); }), "wide do_get_monthname null state throws");
    Require(NotImplementedCalls_nid_no_patch() > before, "Group 3 error paths reported");
}

struct DispatchEntry {
    char specifier;
    char modifier;
};

struct DispatchFacet {
    GuestLocale::Facet base;
    std::vector<DispatchEntry> calls;
    bool setFailOnNext = false;
};

GuestLocale::InputIterator APS5_VABI MockDoGet(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, char specifier, char modifier) {
    auto* facet = const_cast<DispatchFacet*>(reinterpret_cast<const DispatchFacet*>(self));
    facet->calls.push_back({specifier, modifier});
    if (facet->setFailOnNext) {
        *state |= 2;
        return first;
    }
    if (specifier == 'H') {
        return _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(self, first, last, stream, state, time);
    }
    if (specifier == 'Y') {
        return _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(self, first, last, stream, state, time);
    }
    *state |= 2;
    return first;
}

const GuestLocale::TimeGetVtable g_mockWideTimeGetVtable{
    {nullptr, nullptr, nullptr, nullptr},
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    MockDoGet,
};

void CheckGroup4GetAndGetFmt() {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    WideTimeGetStorage realFacet{};
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&realFacet.base, "C", 0);

    GuestLocale::IosBase ios{};
    GuestLocale::Implementation* classic = _ZSt21_sceLibcClassicLocale_nid_postfix;
    ios.locale = &classic;

    InputStreambuf<char16_t> sb{};
    {
        sb.Setup(u"AbC   2026", true);
        GuestTm tm{};
        std::int32_t state = 2;
        const std::u16string fmt = u"aBc 2026";
        _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
            &realFacet.base, sb.Begin(), EndIter(), &ios, &state, &tm,
            reinterpret_cast<const std::uint16_t*>(fmt.data()),
            reinterpret_cast<const std::uint16_t*>(fmt.data() + fmt.size()));
        Require(state == 0, "get resets state, matches case-insensitive literals, and skips multiple spaces");
    }
    {
        sb.Setup(u"12:34:56", false);
        GuestTm tm{};
        std::int32_t state = 0;
        const std::u16string fmt = u"%H";
        _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
            &realFacet.base, sb.Begin(), EndIter(), &ios, &state, &tm,
            reinterpret_cast<const std::uint16_t*>(fmt.data()),
            reinterpret_cast<const std::uint16_t*>(fmt.data() + fmt.size()));
        Require(state == 0 && tm.tm_hour == 12 && sb.RemainingFirst() == ':', "C1-constructed wide time_get dispatches %H through g_wideTimeGetVtable::get");
    }

    DispatchFacet dispatch{{&g_mockWideTimeGetVtable.facet, 1, 0}, {}, false};
    Require(_ZNSt5ctypeIwE2idE_nid_postfix != 0, "ctype<wchar_t>::id initialized after FindFacet");
    std::vector<GuestLocale::Facet*> facets(_ZNSt5ctypeIwE2idE_nid_postfix + 1, nullptr);
    WideCtypeFixture customCtype = MakeWideCtype();
    facets[_ZNSt5ctypeIwE2idE_nid_postfix] = &customCtype.base;
    GuestLocale::Implementation customLoc{{nullptr, 1, 0}, facets.data(), facets.size(), 5, false, "C"};
    GuestLocale::Implementation* customPtr = &customLoc;
    GuestLocale::IosBase customIos{};
    customIos.locale = &customPtr;

    for (const bool buffered : {true, false}) {
        {
            dispatch.calls.clear();
            dispatch.setFailOnNext = false;
            sb.Setup(u"2026 /  09:10:11!", buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            const std::u16string fmt = u"%EY / %OH";
            _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
                &dispatch.base, sb.Begin(), EndIter(), &customIos, &state, &tm,
                reinterpret_cast<const std::uint16_t*>(fmt.data()),
                reinterpret_cast<const std::uint16_t*>(fmt.data() + fmt.size()));
            Require(state == 0 && tm.tm_year == 126 && tm.tm_hour == 9 && tm.tm_min == 10 && tm.tm_sec == 11, "get dispatches modifiers E/O and parses through do_get");
            Require(dispatch.calls.size() == 2 && dispatch.calls[0].specifier == 'Y' && dispatch.calls[0].modifier == 'E' && dispatch.calls[1].specifier == 'H' && dispatch.calls[1].modifier == 'O', "get forwards exact specifier/modifier pairs");
            Require(sb.RemainingFirst() == '!', "get leaves unconsumed trailing char");
        }
        {
            dispatch.calls.clear();
            dispatch.setFailOnNext = true;
            sb.Setup(u"12:34:56", buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            const std::u16string fmt = u"%H %Y";
            _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
                &dispatch.base, sb.Begin(), EndIter(), &customIos, &state, &tm,
                reinterpret_cast<const std::uint16_t*>(fmt.data()),
                reinterpret_cast<const std::uint16_t*>(fmt.data() + fmt.size()));
            Require(state == 2 && dispatch.calls.size() == 1, "get stops immediately once state becomes non-goodbit");
        }
        {
            dispatch.calls.clear();
            dispatch.setFailOnNext = false;
            sb.Setup(u"2026-09:10:11", buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(
                &dispatch.base, sb.Begin(), EndIter(), &customIos, &state, &tm, "%Y-%H");
            Require(state == 1 && tm.tm_year == 126 && tm.tm_hour == 9 && tm.tm_sec == 11, "_Getfmt dispatches format specifiers and sets eofbit");
            Require(dispatch.calls.size() == 2 && dispatch.calls[0].modifier == '\0' && dispatch.calls[1].modifier == '\0', "_Getfmt passes zero modifier");
        }
    }

    {
        sb.Setup(u"a", true);
        GuestTm tm{};
        std::int32_t state = 0;
        const std::u16string trailingPct = u"a%";
        _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
            &dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm,
            reinterpret_cast<const std::uint16_t*>(trailingPct.data()),
            reinterpret_cast<const std::uint16_t*>(trailingPct.data() + trailingPct.size()));
        Require(state == 3, "get trailing % after input end sets eofbit|failbit");

        sb.Setup(u"ab", true);
        state = 0;
        const std::u16string trailingMod = u"a%E";
        _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(
            &dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm,
            reinterpret_cast<const std::uint16_t*>(trailingMod.data()),
            reinterpret_cast<const std::uint16_t*>(trailingMod.data() + trailingMod.size()));
        Require(state == 2, "get trailing modifier sets failbit");

        sb.Setup(u"a", true);
        state = 0;
        _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(
            &dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, "a%");
        Require(state == 3, "_Getfmt trailing % sets failbit|eofbit");
    }

    GuestTm tm{};
    std::int32_t state = 0;
    const std::u16string emptyFmt;
    const auto* fmtPtr = reinterpret_cast<const std::uint16_t*>(emptyFmt.data());
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(nullptr, EndIter(), EndIter(), &ios, &state, &tm, fmtPtr, fmtPtr); }), "get null self throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(&dispatch.base, EndIter(), EndIter(), &ios, nullptr, &tm, fmtPtr, fmtPtr); }), "get null state throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix(&dispatch.base, EndIter(), EndIter(), &ios, &state, &tm, fmtPtr + 1, fmtPtr); }), "get inverted format range throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(nullptr, EndIter(), EndIter(), &ios, &state, &tm, ""); }), "_Getfmt null self throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(&dispatch.base, EndIter(), EndIter(), &ios, &state, &tm, nullptr); }), "_Getfmt null fmt throws");
    Require(NotImplementedCalls_nid_no_patch() > before, "Group 4 error paths reported");
}

struct NarrowFacetVtable {
    GuestLocale::TimeGetVtable timeGet;
};

struct CustomDateFacet {
    GuestLocale::Facet base;
    std::int32_t order;
    bool narrow;
};

std::int32_t APS5_VABI CustomDateOrder(const GuestLocale::Facet* self) {
    return reinterpret_cast<const CustomDateFacet*>(self)->order;
}

GuestLocale::InputIterator APS5_VABI CustomGetMonth(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    if (reinterpret_cast<const CustomDateFacet*>(self)->narrow) {
        return _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(self, first, last, stream, state, time);
    }
    return _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(self, first, last, stream, state, time);
}

GuestLocale::InputIterator APS5_VABI CustomGetYear(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    if (reinterpret_cast<const CustomDateFacet*>(self)->narrow) {
        return _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(self, first, last, stream, state, time);
    }
    return _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(self, first, last, stream, state, time);
}

GuestLocale::InputIterator APS5_VABI CustomGetWeekday(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    if (reinterpret_cast<const CustomDateFacet*>(self)->narrow) {
        return _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(self, first, last, stream, state, time);
    }
    return _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(self, first, last, stream, state, time);
}

GuestLocale::InputIterator APS5_VABI CustomDoGet(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, char specifier, char modifier) {
    if (reinterpret_cast<const CustomDateFacet*>(self)->narrow) {
        return _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix(self, first, last, stream, state, time, specifier, modifier);
    }
    return _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix(self, first, last, stream, state, time, specifier, modifier);
}

const GuestLocale::TimeGetVtable g_customDateVtable{
    {nullptr, nullptr, nullptr, nullptr},
    CustomDateOrder,
    nullptr,
    nullptr,
    CustomGetWeekday,
    CustomGetMonth,
    CustomGetYear,
    CustomDoGet,
};

template<typename TChar>
std::basic_string<TChar> FromAscii(const char* text) {
    std::basic_string<TChar> out;
    for (const char* p = text; *p != '\0'; ++p) out.push_back(static_cast<TChar>(static_cast<unsigned char>(*p)));
    return out;
}

template<typename TChar, typename TGetDate, typename TGetWeekday, typename TDoGet, typename TGetFmt>
void CheckRemainingTyped(GuestLocale::Facet* facet, bool narrow, TGetDate getDate, TGetWeekday getWeekday, TDoGet doGet, TGetFmt getFmt) {
    GuestLocale::Implementation* classic = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::IosBase ios{};
    ios.locale = &classic;
    InputStreambuf<TChar> sb{};

    for (const bool buffered : {true, false}) {
        {
            sb.Setup(FromAscii<TChar>("thu."), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getWeekday(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_wday == 4 && sb.RemainingFirst() == '.', "do_get_weekday parses case-insensitive short name");
        }
        {
            sb.Setup(FromAscii<TChar>("Wednesday"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getWeekday(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_wday == 3, "do_get_weekday parses full name");
        }
        {
            sb.Setup(FromAscii<TChar>("S"), buffered);
            GuestTm tm{};
            tm.tm_wday = 6;
            std::int32_t state = 0;
            getWeekday(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 2 && tm.tm_wday == 6, "do_get_weekday rejects ambiguous prefix without touching eofbit");
        }
        {
            sb.Setup(FromAscii<TChar>(""), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getWeekday(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 2, "do_get_weekday rejects empty input");
        }

        {
            sb.Setup(FromAscii<TChar>("11/17/2026!"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getDate(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 0 && tm.tm_mon == 10 && tm.tm_mday == 17 && tm.tm_year == 126 && sb.RemainingFirst() == '!', "do_get_date parses numeric mdy");
        }
        {
            sb.Setup(FromAscii<TChar>("Nov 7, 26"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getDate(facet, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 1 && tm.tm_mon == 10 && tm.tm_mday == 7 && tm.tm_year == 126, "do_get_date parses leading month name and comma separator to EOF");
        }

        {
            CustomDateFacet custom{{&g_customDateVtable.facet, 1, 0}, 1, narrow};
            sb.Setup(FromAscii<TChar>("17 / 11 : 2026"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getDate(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 1 && tm.tm_mday == 17 && tm.tm_mon == 10 && tm.tm_year == 126, "do_get_date supports dmy");

            custom.order = 3;
            sb.Setup(FromAscii<TChar>("2026, Nov 17"), buffered);
            tm = {};
            state = 0;
            getDate(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 1 && tm.tm_year == 126 && tm.tm_mon == 10 && tm.tm_mday == 17, "do_get_date supports ymd with middle month name");

            custom.order = 4;
            sb.Setup(FromAscii<TChar>("2026/17/Nov"), buffered);
            tm = {};
            state = 0;
            getDate(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 1 && tm.tm_year == 126 && tm.tm_mday == 17 && tm.tm_mon == 10, "do_get_date supports ydm with trailing month name");

            sb.Setup(FromAscii<TChar>("2026/Nov 17"), buffered);
            tm = {};
            state = 0;
            getDate(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 1 && tm.tm_year == 126 && tm.tm_mon == 10 && tm.tm_mday == 17, "do_get_date switches ydm with middle month name to ymd");

            custom.order = 0;
            sb.Setup(FromAscii<TChar>("02/28/2025"), buffered);
            tm = {};
            state = 0;
            getDate(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 1 && tm.tm_mon == 1 && tm.tm_mday == 28 && tm.tm_year == 125, "do_get_date treats no_order as mdy");
        }

        {
            CustomDateFacet custom{{&g_customDateVtable.facet, 1, 0}, 2, narrow};
            sb.Setup(FromAscii<TChar>("Nov/Dec/2026"), buffered);
            GuestTm tm{};
            std::int32_t state = 0;
            getDate(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require((state & 2) != 0, "do_get_date rejects duplicate month name in mdy");

            sb.Setup(FromAscii<TChar>("11/17"), buffered);
            tm = {};
            state = 0;
            getDate(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 3, "do_get_date fails with eofbit on missing third component");

            sb.Setup(FromAscii<TChar>(""), buffered);
            tm = {};
            state = 0;
            getDate(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm);
            Require(state == 3, "do_get_date fails empty input");
        }

        {
            CustomDateFacet custom{{&g_customDateVtable.facet, 1, 0}, 2, narrow};
            sb.Setup(FromAscii<TChar>("Wed Nov 17 09:10:60 2026"), buffered);
            GuestTm tm{};
            std::int32_t state = 2;
            doGet(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm, 'c', '\0');
            Require(state == 1 && tm.tm_wday == 3 && tm.tm_mon == 10 && tm.tm_mday == 17 && tm.tm_hour == 9 && tm.tm_min == 10 && tm.tm_sec == 60 && tm.tm_year == 126, "do_get %c parses full timestamp and resets prior state");

            sb.Setup(FromAscii<TChar>("12:30:45 pm"), buffered);
            tm = {};
            state = 0;
            doGet(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm, 'r', '\0');
            Require(state == 1 && tm.tm_hour == 12 && tm.tm_min == 30 && tm.tm_sec == 45, "do_get %r maps 12 PM to 12");

            sb.Setup(FromAscii<TChar>("12:30:45 AM"), buffered);
            tm = {};
            state = 0;
            doGet(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm, 'r', '\0');
            Require(state == 1 && tm.tm_hour == 0 && tm.tm_min == 30 && tm.tm_sec == 45, "do_get %r maps 12 AM to 0");

            sb.Setup(FromAscii<TChar>("20 68 69 2026 365 42 5"), buffered);
            tm = {};
            state = 0;
            getFmt(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm, "%C %y %U %Y %j %W %w");
            Require(state == 1 && tm.tm_year == 126 && tm.tm_yday == 42 && tm.tm_wday == 5, "_Getfmt dispatches %C %y %U %Y %j %W %w");

            sb.Setup(FromAscii<TChar>("12/31/68"), buffered);
            tm = {};
            state = 0;
            doGet(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm, 'D', '\0');
            Require(state == 1 && tm.tm_mon == 11 && tm.tm_mday == 31 && tm.tm_year == 168, "do_get %D parses %m/%d/%y");

            sb.Setup(FromAscii<TChar>("%"), buffered);
            tm = {};
            state = 0;
            doGet(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm, '%', '\0');
            Require(state == 1, "do_get %% consumes percent literal");

            sb.Setup(FromAscii<TChar>("x"), buffered);
            tm = {};
            state = 0;
            doGet(&custom.base, sb.Begin(), EndIter(), &ios, &state, &tm, 'Z', '\0');
            Require(state == 2 && sb.RemainingFirst() == 'x', "do_get unknown specifier sets failbit");
        }
    }
}

void CheckGroup5RemainingTimeGet(bool hasLibrary) {
    const std::uint64_t before = NotImplementedCalls_nid_no_patch();
    GuestLocale::Implementation* classic = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::IosBase ios{};
    ios.locale = &classic;

    GuestLocale::CtypeFacet charCtype{{nullptr, 1, 0}, _Getpctype_nid_postfix(), nullptr, nullptr, 0, 0};
    GuestLocale::Facet charFacet{};
    {
        InputStreambuf<char> sb{};
        sb.Setup("  42x", true);
        int val = -1;
        GuestLocale::InputIterator first = sb.Begin();
        GuestLocale::InputIterator last = EndIter();
        const std::int32_t st = _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetintERS3_S5_iiRiRKSt5ctypeIcE_nid_postfix(
            &charFacet, &first, &last, 0, 9999, &val, &charCtype.base);
        Require(st == 0 && val == 42 && sb.RemainingFirst() == 'x', "char _Getint skips spaces and parses bounded integer");
    }

    {
        CustomDateFacet custom{{&g_customDateVtable.facet, 1, 0}, 2, true};
        CheckRemainingTyped<char>(
            &custom.base, true,
            &_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
            &_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix,
            &_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix,
            &_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix);
    }

    {
        WideTimeGetStorage wideFacet{};
        _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&wideFacet.base, "C", 0);
        const auto* vt = reinterpret_cast<const GuestLocale::TimeGetVtable*>(wideFacet.base.vtable);
        Require(vt->getDate != nullptr && vt->getWeekday != nullptr && vt->get != nullptr, "wide vtable remaining slots non-null");
#ifdef _WIN32
        if (HMODULE loadedLib = GetModuleHandleA("libSceLibcInternal.prx"); loadedLib != nullptr) {
            Require(reinterpret_cast<void*>(vt->getDate) == reinterpret_cast<void*>(GetProcAddress(loadedLib, "_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix")), "wide vtable getDate wired");
            Require(reinterpret_cast<void*>(vt->getWeekday) == reinterpret_cast<void*>(GetProcAddress(loadedLib, "_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix")), "wide vtable getWeekday wired");
            Require(reinterpret_cast<void*>(vt->get) == reinterpret_cast<void*>(GetProcAddress(loadedLib, "_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix")), "wide vtable get wired");
        }
#else
        Require(vt->getDate == &_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix, "wide vtable getDate wired");
        Require(vt->getWeekday == &_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix, "wide vtable getWeekday wired");
        Require(vt->get == &_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix, "wide vtable get wired");
#endif
        CheckRemainingTyped<char16_t>(
            &wideFacet.base, false, vt->getDate, vt->getWeekday, vt->get,
            &_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix);
    }

    if (hasLibrary) {
        using GetintCharFn = std::int32_t (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::InputIterator*, GuestLocale::InputIterator*, int, int, int*, const GuestLocale::Facet*);
        using DateFn = GuestLocale::InputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::InputIterator, GuestLocale::InputIterator, GuestLocale::IosBase*, std::int32_t*, GuestTm*);
        using DoGetFn = GuestLocale::InputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::InputIterator, GuestLocale::InputIterator, GuestLocale::IosBase*, std::int32_t*, GuestTm*, char, char);
        using GetFmtFn = GuestLocale::InputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::InputIterator, GuestLocale::InputIterator, GuestLocale::IosBase*, std::int32_t*, GuestTm*, const char*);

        const auto nidCharDate = ResolveExport<DateFn>("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix");
        const auto nidCharWday = ResolveExport<DateFn>("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix");
        const auto nidCharDoGet = ResolveExport<DoGetFn>("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix");
        const auto nidCharGetFmt = ResolveExport<GetFmtFn>("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix");
        const auto nidCharGetInt = ResolveExport<GetintCharFn>("_ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetintERS3_S5_iiRiRKSt5ctypeIcE_nid_postfix");
        const auto nidWideDate = ResolveExport<DateFn>("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix");
        const auto nidWideWday = ResolveExport<DateFn>("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix");
        const auto nidWideDoGet = ResolveExport<DoGetFn>("_ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix");

        CustomDateFacet narrowFacet{{&g_customDateVtable.facet, 1, 0}, 2, true};
        GuestTm tm{};
        std::int32_t state = 0;
        InputStreambuf<char> cSb{};
        cSb.Setup("75", true);
        GuestLocale::InputIterator cFirst = cSb.Begin();
        GuestLocale::InputIterator cLast = EndIter();
        int iv = 0;
        Require(nidCharGetInt(&narrowFacet.base, &cFirst, &cLast, 0, 99, &iv, &charCtype.base) == 1 && iv == 75, "NID 8raXTYQ11cg");
        cSb.Setup("05/12/2026", true);
        state = 0;
        nidCharDate(&narrowFacet.base, cSb.Begin(), EndIter(), &ios, &state, &tm);
        Require(state == 1 && tm.tm_mon == 4 && tm.tm_mday == 12 && tm.tm_year == 126, "NID T85u2sPrKOo");
        cSb.Setup("Friday", true);
        state = 0;
        nidCharWday(&narrowFacet.base, cSb.Begin(), EndIter(), &ios, &state, &tm);
        Require(state == 0 && tm.tm_wday == 5, "NID xzYpD5d24aA");
        cSb.Setup("20:15", true);
        state = 0;
        nidCharGetFmt(&narrowFacet.base, cSb.Begin(), EndIter(), &ios, &state, &tm, "%R");
        Require(state == 1 && tm.tm_hour == 20 && tm.tm_min == 15, "NID E7UermPZVcw");
        cSb.Setup("23", true);
        state = 0;
        nidCharDoGet(&narrowFacet.base, cSb.Begin(), EndIter(), &ios, &state, &tm, 'H', '\0');
        Require(state == 1 && tm.tm_hour == 23, "NID Vw03kdKZUN0");

        WideTimeGetStorage wideFacet{};
        _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&wideFacet.base, "C", 0);
        InputStreambuf<char16_t> wSb{};
        wSb.Setup(u"08/09/2024", true);
        tm = {};
        state = 0;
        nidWideDate(&wideFacet.base, wSb.Begin(), EndIter(), &ios, &state, &tm);
        Require(state == 1 && tm.tm_mon == 7 && tm.tm_mday == 9 && tm.tm_year == 124, "NID SYCwZXKZQ08");
        wSb.Setup(u"Mon", true);
        state = 0;
        nidWideWday(&wideFacet.base, wSb.Begin(), EndIter(), &ios, &state, &tm);
        Require(state == 0 && tm.tm_wday == 1, "NID hGlkh5YpcKw");
        wSb.Setup(u"59", true);
        state = 0;
        nidWideDoGet(&wideFacet.base, wSb.Begin(), EndIter(), &ios, &state, &tm, 'S', '\0');
        Require(state == 1 && tm.tm_sec == 59, "NID 0MzJAexrlr4");
    }

    GuestTm tm{};
    std::int32_t state = 0;
    int intVal = 0;
    GuestLocale::InputIterator dummyFirst = EndIter();
    GuestLocale::InputIterator dummyLast = EndIter();
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetintERS3_S5_iiRiRKSt5ctypeIcE_nid_postfix(nullptr, &dummyFirst, &dummyLast, 0, 99, &intVal, &charCtype.base); }), "char _Getint null self throws");
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(nullptr, EndIter(), EndIter(), &ios, &state, &tm); }), "char do_get_date null self throws");
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(&charFacet, EndIter(), EndIter(), &ios, nullptr, &tm); }), "char do_get_weekday null state throws");
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix(&charFacet, EndIter(), EndIter(), &ios, &state, nullptr, 'H', '\0'); }), "char do_get null tm throws");
    Require(Throws([&] { _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix(&charFacet, EndIter(), EndIter(), &ios, &state, &tm, nullptr); }), "char _Getfmt null fmt throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(nullptr, EndIter(), EndIter(), &ios, &state, &tm); }), "wide do_get_date null self throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix(&charFacet, EndIter(), EndIter(), &ios, nullptr, &tm); }), "wide do_get_weekday null state throws");
    Require(Throws([&] { _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix(&charFacet, EndIter(), EndIter(), &ios, &state, nullptr, 'H', '\0'); }), "wide do_get null tm throws");
    Require(NotImplementedCalls_nid_no_patch() > before, "Group 5 error paths reported");
}

template<typename TCall>
void RequireInvalid(TCall call, const char* text) {
    bool threw = false;
    errno = 71;
    try {
        call();
    } catch (const std::invalid_argument& error) {
        threw = std::strstr(error.what(), text) != nullptr;
    }
    Require(threw && errno == 71, text);
}

class GuardedPages {
public:
    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<std::byte*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr, "guarded allocation succeeds");
        DWORD previous = 0;
        Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0, "guard page protection succeeds");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "host page size is available");
        pageSize = static_cast<std::size_t>(size);
        void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED, "guarded allocation succeeds");
        memory = static_cast<std::byte*>(allocation);
        Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0, "guard page protection succeeds");
#endif
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, pageSize * 2);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    template<typename TElement>
    TElement* Last(std::size_t count) { return reinterpret_cast<TElement*>(memory + pageSize) - count; }

    void ReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(memory, pageSize, PAGE_READONLY, &previous) != 0, "read-only protection succeeds");
#else
        Require(mprotect(memory, pageSize, PROT_READ) == 0, "read-only protection succeeds");
#endif
    }

private:
    std::byte* memory = nullptr;
    std::size_t pageSize = 0;
};

template<typename TPointer>
TPointer Inaccessible() {
    return reinterpret_cast<TPointer>(std::uintptr_t{0x10});
}

void CheckGroup6GuestMemory() {
    constexpr auto getInt = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetintERS3_S5_iiRiRKSt5ctypeIwE_nid_postfix;
    constexpr auto getTime = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_timeES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix;
    constexpr auto getYear = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_yearES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix;
    constexpr auto getMonth = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE16do_get_monthnameES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix;
    constexpr auto get = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKwSE__nid_postfix;
    constexpr auto getFmt = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE7_GetfmtES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmPKc_nid_postfix;
    WideTimeGetStorage facet{};
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&facet.base, "C", 0);
    WideCtypeFixture ctype = MakeWideCtype();
    InputStreambuf<char16_t> sb{};
    GuestLocale::IosBase ios{};
    GuestTm tm{};
    std::int32_t state = 0;
    int val = -1;

    sb.Setup(u"42", true);
    auto first = sb.Begin();
    auto last = EndIter();
    RequireInvalid([&] { getInt(&facet.base, Inaccessible<GuestLocale::InputIterator*>(), &last, 0, 99, &val, &ctype.base); }, "unreadable iterator");
    RequireInvalid([&] { getInt(&facet.base, &first, Inaccessible<GuestLocale::InputIterator*>(), 0, 99, &val, &ctype.base); }, "unreadable iterator");
    RequireInvalid([&] { getInt(&facet.base, &first, &last, 0, 99, Inaccessible<int*>(), &ctype.base); }, "unwritable value");
    sb.Setup(u"xyz", true);
    first = sb.Begin();
    Require(getInt(&facet.base, &first, &last, 0, 99, Inaccessible<int*>(), &ctype.base) == 2, "_Getint leaves an unwritable value alone when it fails");
    GuardedPages iteratorPages;
    auto* guardedLast = iteratorPages.Last<GuestLocale::InputIterator>(1);
    *guardedLast = EndIter();
    iteratorPages.ReadOnly();
    sb.Setup(u"42", true);
    first = sb.Begin();
    Require(getInt(&facet.base, &first, guardedLast, 0, 99, &val, &ctype.base) == 1 && val == 42, "_Getint accepts an end iterator it does not change in read-only memory");
    sb.Setup(u"42", true);
    first = sb.Begin();
    val = 5;
    Require(getInt(&facet.base, &first, &first, 0, 99, &val, &ctype.base) == 3 && val == 5, "_Getint treats an aliased first and last as one iterator");

    sb.Setup(u"12:34:56", true);
    RequireInvalid([&] { getTime(&facet.base, sb.Begin(), EndIter(), &ios, Inaccessible<std::int32_t*>(), &tm); }, "unreadable state");
    sb.Setup(u"12:34:56", true);
    RequireInvalid([&] { getTime(&facet.base, sb.Begin(), EndIter(), &ios, &state, Inaccessible<GuestTm*>()); }, "unwritable time");
    sb.Setup(u"xx", true);
    state = 0;
    getTime(&facet.base, sb.Begin(), EndIter(), &ios, &state, Inaccessible<GuestTm*>());
    Require(state == 2, "do_get_time leaves an unwritable tm alone when the hour fails");
    GuardedPages statePages;
    auto* readOnlyState = statePages.Last<std::int32_t>(1);
    statePages.ReadOnly();
    sb.Setup(u"2026", true);
    RequireInvalid([&] { getYear(&facet.base, sb.Begin(), EndIter(), &ios, readOnlyState, &tm); }, "unwritable state");
    sb.Setup(u"Feb", true);
    RequireInvalid([&] { getMonth(Inaccessible<GuestLocale::Facet*>(), sb.Begin(), EndIter(), &ios, &state, &tm); }, "unreadable facet vtable");
    sb.Setup(u"Feb", true);
    RequireInvalid([&] { getMonth(&facet.base, sb.Begin(), EndIter(), &ios, &state, Inaccessible<GuestTm*>()); }, "unwritable time");
    sb.Setup(u"Feb", true);
    tm = {};
    Require(getMonth(&facet.base, sb.Begin(), EndIter(), &ios, &state, &tm).words[0] == 0 && tm.tm_mon == 1, "do_get_monthname reads the months table of a constructed facet");

    DispatchFacet dispatch{{&g_mockWideTimeGetVtable.facet, 1, 0}, {}, false};
    GuardedPages formatPages;
    auto* format = formatPages.Last<std::uint16_t>(2);
    format[0] = u'a';
    format[1] = u'b';
    sb.Setup(u"ab", true);
    state = 0;
    get(&dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, format, format + 2);
    Require(state == 0, "get reads a format that ends at a guard page");
    sb.Setup(u"abc", true);
    RequireInvalid([&] { get(&dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, format, format + 3); }, "unreadable format");
    sb.Setup(u"a", true);
    state = 0;
    get(&dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, format, format + (std::size_t{1} << 40));
    Require(state == 3, "get with an oversized format reads only what it matches");
    sb.Setup(u"ab", true);
    RequireInvalid([&] { get(Inaccessible<GuestLocale::Facet*>(), sb.Begin(), EndIter(), &ios, &state, &tm, format, format + 2); }, "unreadable facet vtable");
    RequireInvalid([&] { get(&dispatch.base, sb.Begin(), EndIter(), &ios, Inaccessible<std::int32_t*>(), &tm, format, format + 2); }, "unwritable state");
    const GuestLocale::Facet broken{Inaccessible<const GuestLocale::FacetVtable*>(), 1, 0};
    const std::u16string directive = u"%H";
    sb.Setup(u"12", true);
    RequireInvalid([&] { get(&broken, sb.Begin(), EndIter(), &ios, &state, &tm, reinterpret_cast<const std::uint16_t*>(directive.data()), reinterpret_cast<const std::uint16_t*>(directive.data() + directive.size())); }, "unreadable facet vtable");

    auto* narrow = formatPages.Last<char>(2);
    narrow[0] = 'a';
    narrow[1] = '\0';
    sb.Setup(u"a", true);
    state = 0;
    getFmt(&dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, narrow);
    Require(state == 1, "_Getfmt reads a format whose terminator ends at a guard page");
    narrow[1] = 'b';
    sb.Setup(u"ab", true);
    RequireInvalid([&] { getFmt(&dispatch.base, sb.Begin(), EndIter(), &ios, &state, &tm, narrow); }, "unreadable format");
    sb.Setup(u"x", true);
    RequireInvalid([&] { getFmt(&dispatch.base, sb.Begin(), EndIter(), &ios, Inaccessible<std::int32_t*>(), &tm, "a"); }, "unreadable state");
    sb.Setup(u"a", true);
    state = 0;
    getFmt(&broken, sb.Begin(), EndIter(), &ios, &state, &tm, "a");
    Require(state == 1, "_Getfmt reads no vtable slot for a literal format");
    sb.Setup(u"12", true);
    RequireInvalid([&] { getFmt(&broken, sb.Begin(), EndIter(), &ios, &state, &tm, "%H"); }, "unreadable facet vtable");

    RequireInvalid([&] { _ZNSt8time_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEEC1Em_nid_postfix(Inaccessible<GuestLocale::Facet*>(), 1); }, "unwritable facet");

    constexpr auto charGetInt = _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE7_GetintERS3_S5_iiRiRKSt5ctypeIcE_nid_postfix;
    constexpr auto charWeekday = _ZNKSt8time_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE14do_get_weekdayES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix;
    constexpr auto wideDate = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE11do_get_dateES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tm_nid_postfix;
    constexpr auto wideDoGet = _ZNKSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE6do_getES3_S3_RSt8ios_baseRNSt5_IosbIiE8_IostateEP2tmcc_nid_postfix;
    GuestLocale::CtypeFacet charCtype{{nullptr, 1, 0}, _Getpctype_nid_postfix(), nullptr, nullptr, 0, 0};
    InputStreambuf<char> narrowSb{};
    narrowSb.Setup("42", true);
    auto narrowFirst = narrowSb.Begin();
    RequireInvalid([&] { charGetInt(&facet.base, Inaccessible<GuestLocale::InputIterator*>(), &last, 0, 99, &val, &charCtype.base); }, "unreadable iterator");
    RequireInvalid([&] { charGetInt(&facet.base, &narrowFirst, &last, 0, 99, Inaccessible<int*>(), &charCtype.base); }, "unwritable value");
    narrowSb.Setup("Tue", true);
    RequireInvalid([&] { charWeekday(&facet.base, narrowSb.Begin(), EndIter(), &ios, &state, Inaccessible<GuestTm*>()); }, "unwritable time");
    narrowSb.Setup("Xyz", true);
    RequireInvalid([&] { charWeekday(&facet.base, narrowSb.Begin(), EndIter(), &ios, Inaccessible<std::int32_t*>(), &tm); }, "unreadable state");

    sb.Setup(u"2/3/2026", true);
    RequireInvalid([&] { wideDate(&facet.base, sb.Begin(), EndIter(), &ios, &state, Inaccessible<GuestTm*>()); }, "unreadable time");
    sb.Setup(u"2/3/2026", true);
    RequireInvalid([&] { wideDate(&facet.base, sb.Begin(), EndIter(), &ios, Inaccessible<std::int32_t*>(), &tm); }, "unreadable state");
    sb.Setup(u"2/3/2026", true);
    RequireInvalid([&] { wideDate(&broken, sb.Begin(), EndIter(), &ios, &state, &tm); }, "unreadable facet vtable");
    sb.Setup(u"2/3/2026", true);
    tm = {};
    state = 0;
    wideDate(&facet.base, sb.Begin(), EndIter(), &ios, &state, &tm);
    Require(state == 1 && tm.tm_mon == 1 && tm.tm_mday == 3 && tm.tm_year == 126, "do_get_date parses through the guest-memory helpers");

    sb.Setup(u"12", true);
    RequireInvalid([&] { wideDoGet(&facet.base, sb.Begin(), EndIter(), &ios, &state, Inaccessible<GuestTm*>(), 'H', '\0'); }, "unwritable time");
    sb.Setup(u"12", true);
    RequireInvalid([&] { wideDoGet(&facet.base, sb.Begin(), EndIter(), &ios, Inaccessible<std::int32_t*>(), &tm, 'H', '\0'); }, "unwritable state");
    sb.Setup(u"PM", true);
    RequireInvalid([&] { wideDoGet(&facet.base, sb.Begin(), EndIter(), &ios, &state, Inaccessible<GuestTm*>(), 'p', '\0'); }, "unreadable time");
    sb.Setup(u"Feb", true);
    RequireInvalid([&] { wideDoGet(&broken, sb.Begin(), EndIter(), &ios, &state, &tm, 'b', '\0'); }, "unreadable facet vtable");

    WideTimeGetStorage unreadableTables{};
    _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(&unreadableTables.base, "C", 0);
    unreadableTables.months = Inaccessible<const char16_t*>();
    sb.Setup(u"Feb", true);
    RequireInvalid([&] { getMonth(&unreadableTables.base, sb.Begin(), EndIter(), &ios, &state, &tm); }, "unreadable facet table");
    RequireInvalid([&] { _ZNSt8time_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEC1EPKcm_nid_postfix(Inaccessible<GuestLocale::Facet*>(), "C", 0); }, "unwritable facet");
}

void WarmUpHostExceptionRuntime() {
    try {
        throw std::runtime_error("warm-up");
    } catch (const std::runtime_error&) {
    }
}

void CheckGroup7Getloctxt(bool hasLibrary) {
    using GetloctxtFn = int (APS5_VABI*)(GuestLocale::InputIterator*, GuestLocale::InputIterator*, std::size_t, const char*);
    GetloctxtFn fn = &_ZSt10_GetloctxtIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEiRT0_S5_mPKT__nid_postfix;
    if (hasLibrary) {
        auto resolved = ResolveExport<GetloctxtFn>("_ZSt10_GetloctxtIcSt19istreambuf_iteratorIcSt11char_traitsIcEEEiRT0_S5_mPKT__nid_postfix");
        if (resolved != nullptr) {
            fn = resolved;
        }
    }

    const char* monthsTable = ":Jan:January:Feb:February";
    GuestLocale::InputIterator endIter = EndIter();

    {
        InputStreambuf<char> sb{};
        sb.Setup("Feb", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == 2, "_Getloctxt Feb -> 2");
        Require((first.words[1] & 0xffu) == 0, "Feb cache flag 0");
        Require(first.words[0] == 0, "Feb iterator at eof");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("feb", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == 2, "_Getloctxt feb -> 2");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("FEB", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == 2, "_Getloctxt FEB -> 2");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("February", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == 3, "_Getloctxt February -> 3");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Febx", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == 2, "_Getloctxt Febx -> 2");
        Require((first.words[1] & 0xffu) != 0, "Febx cached flag set");
        Require(static_cast<char>((first.words[1] >> 8) & 0xffu) == 'x', "Febx cached x");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Feb 3", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == 2, "_Getloctxt Feb 3 -> 2");
        Require((first.words[1] & 0xffu) != 0, "Feb 3 cached flag set");
        Require(static_cast<char>((first.words[1] >> 8) & 0xffu) == ' ', "Feb 3 cached space");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Febru", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == -1, "_Getloctxt Febru -> -1");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Janua", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == -1, "_Getloctxt Janua -> -1");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Ja", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == -1, "_Getloctxt Ja -> -1");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Xyz", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == -2, "_Getloctxt Xyz -> -2");
        Require((first.words[1] & 0xffu) != 0, "Xyz cached flag set");
        Require(static_cast<char>((first.words[1] >> 8) & 0xffu) == 'X', "Xyz cached X");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, monthsTable);
        Require(res == -2, "_Getloctxt empty buffer -> -2");
    }

    {
        GuestLocale::InputIterator nullIter{{0, 0}};
        int res = fn(&nullIter, &endIter, 0, monthsTable);
        Require(res == -2, "_Getloctxt null buffer -> -2");
        Require(nullIter.words[0] == 0 && nullIter.words[1] == 0, "null buffer unchanged");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("mar", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, ":MAR:Apr");
        Require(res == 0, "_Getloctxt mar -> 0");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("pm", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, ":AM:PM");
        Require(res == 1, "_Getloctxt pm -> 1");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("p", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, ":AM:PM");
        Require(res == -1, "_Getloctxt p -> -1");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("ab", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, ",a,ab,abc");
        Require(res == 1, "_Getloctxt ab -> 1");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("abc", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, ",a,ab,abc");
        Require(res == 2, "_Getloctxt abc -> 2");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("abd", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, ",a,ab,abc");
        Require(res == 1, "_Getloctxt abd -> 1");
        Require((first.words[1] & 0xffu) != 0, "abd cached flag set");
        Require(static_cast<char>((first.words[1] >> 8) & 0xffu) == 'd', "abd cached d");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("a", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, ",a,ab,abc");
        Require(res == 0, "_Getloctxt a -> 0");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("\xc9t\xe9", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, ":\xe9t\xe9:\xc9t\xe9");
        Require(res == 1, "_Getloctxt accented match -> 1");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("\xe9t\xe9", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, ":\xc9t\xe9");
        Require(res == -2, "_Getloctxt accented mismatch -> -2");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Feb", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, "");
        Require(res == -2, "_Getloctxt empty table -> -2");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Feb", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, ":");
        Require(res == 0, "_Getloctxt table : -> 0");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Jan", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 0, "Jan:Feb");
        Require(res == -2, "_Getloctxt Jan:Feb separator J -> -2");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Jan", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 2, ":Jan:January");
        Require(res == 0, "_Getloctxt count 2 -> 0");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Jan", true);
        GuestLocale::InputIterator first = sb.Begin();
        int res = fn(&first, &endIter, 5, ":Jan:January");
        Require(res == 0, "_Getloctxt count 5 -> 0");
    }

    {
        static constexpr char boolTable[] = "\0false\0true";
        static constexpr char emptyFirst[] = "\0\0on";
        struct MarkedCase {
            const char* table;
            std::size_t count;
            const char* input;
            int result;
            char next;
        };
        for (const MarkedCase& item : {MarkedCase{boolTable, 2, "false", 0, '\0'}, MarkedCase{boolTable, 2, "true", 1, '\0'}, MarkedCase{boolTable, 2, "TRUE", 1, '\0'},
                 MarkedCase{boolTable, 2, "False", 0, '\0'}, MarkedCase{boolTable, 2, "truex", 1, 'x'}, MarkedCase{boolTable, 2, "false 1", 0, ' '},
                 MarkedCase{boolTable, 2, "tru", -1, '\0'}, MarkedCase{boolTable, 2, "f", -1, '\0'}, MarkedCase{boolTable, 2, "trux", -1, 'x'},
                 MarkedCase{boolTable, 2, "yes", -2, 'y'}, MarkedCase{boolTable, 0, "true", -2, 't'}, MarkedCase{boolTable, 1, "true", -2, 't'},
                 MarkedCase{boolTable, 1, "false", 0, '\0'}, MarkedCase{emptyFirst, 2, "x", 0, 'x'}, MarkedCase{emptyFirst, 2, "on", 1, '\0'},
                 MarkedCase{emptyFirst, 2, "o", -1, '\0'}}) {
            InputStreambuf<char> sb{};
            sb.Setup(item.input, true);
            GuestLocale::InputIterator first = sb.Begin();
            Require(fn(&first, &endIter, item.count, item.table) == item.result, "_Getloctxt NUL-marked table result");
            Require(sb.RemainingFirst() == item.next, "_Getloctxt NUL-marked table consumption");
        }
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Jan", true);
        GuestLocale::InputIterator first = sb.Begin();
        Require(fn(nullptr, &endIter, 0, monthsTable) == -2, "null first -> -2");
        Require(fn(&first, &endIter, 0, nullptr) == -2, "null ptr -> -2");
        Require(fn(&first, nullptr, 0, monthsTable) == -2, "null last -> -2");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Jan", true);
        GuestLocale::InputIterator first = sb.Begin();
        Require(Throws([&] { fn(&first, &endIter, SIZE_MAX, ":Jan"); }), "SIZE_MAX throws length_error");
    }

    {
        InputStreambuf<char> sb{};
        sb.Setup("Jan", true);
        RequireInvalid([&] { fn(Inaccessible<GuestLocale::InputIterator*>(), &endIter, 0, monthsTable); }, "unreadable iterator");
        GuestLocale::InputIterator first = sb.Begin();
        RequireInvalid([&] { fn(&first, Inaccessible<GuestLocale::InputIterator*>(), 0, monthsTable); }, "unreadable iterator");
        GuardedPages iteratorPages;
        auto* readOnlyFirst = iteratorPages.Last<GuestLocale::InputIterator>(1);
        *readOnlyFirst = sb.Begin();
        iteratorPages.ReadOnly();
        RequireInvalid([&] { fn(readOnlyFirst, &endIter, 0, monthsTable); }, "unwritable iterator");
    }

    {
        GuardedPages tablePages;
        constexpr std::string_view terminatedTable{":Jan:Feb", 9};
        char* tableAtGuard = tablePages.Last<char>(terminatedTable.size());
        std::memcpy(tableAtGuard, terminatedTable.data(), terminatedTable.size());
        InputStreambuf<char> sb{};
        sb.Setup("Feb", true);
        GuestLocale::InputIterator first = sb.Begin();
        Require(fn(&first, &endIter, 0, tableAtGuard) == 1, "_Getloctxt reads a table that ends at a guard page");
        char* unterminatedTable = tablePages.Last<char>(4);
        std::memcpy(unterminatedTable, ":Jan", 4);
        sb.Setup("Jan", true);
        first = sb.Begin();
        RequireInvalid([&] { fn(&first, &endIter, 0, unterminatedTable); }, "unreadable facet table");
        constexpr std::string_view markedTable{"\0false\0true", 12};
        char* markedAtGuard = tablePages.Last<char>(markedTable.size());
        std::memcpy(markedAtGuard, markedTable.data(), markedTable.size());
        sb.Setup("true", true);
        first = sb.Begin();
        Require(fn(&first, &endIter, 2, markedAtGuard) == 1, "_Getloctxt reads a NUL-marked table that ends at a guard page");
        char* unterminatedMarked = tablePages.Last<char>(10);
        std::memcpy(unterminatedMarked, markedTable.data(), 10);
        sb.Setup("true", true);
        first = sb.Begin();
        RequireInvalid([&] { fn(&first, &endIter, 2, unterminatedMarked); }, "unreadable facet table");
    }
}

void CheckGroup6WideGetloctxt(bool hasLibrary) {
    using WideGetloctxtFn = int (APS5_VABI*)(GuestLocale::InputIterator*, GuestLocale::InputIterator*, std::size_t, const std::uint16_t*);
    using MixedGetloctxtFn = int (APS5_VABI*)(GuestLocale::InputIterator*, GuestLocale::InputIterator*, std::size_t, const char*);
    WideGetloctxtFn wide = &_ZSt10_GetloctxtIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEiRT0_S5_mPKT__nid_postfix;
    MixedGetloctxtFn mixed = &_ZSt10_GetloctxtIcSt19istreambuf_iteratorIwSt11char_traitsIwEEEiRT0_S5_mPKT__nid_postfix;
    if (hasLibrary) {
        if (auto resolved = ResolveExport<WideGetloctxtFn>("_ZSt10_GetloctxtIwSt19istreambuf_iteratorIwSt11char_traitsIwEEEiRT0_S5_mPKT__nid_postfix")) wide = resolved;
        if (auto resolved = ResolveExport<MixedGetloctxtFn>("_ZSt10_GetloctxtIcSt19istreambuf_iteratorIwSt11char_traitsIwEEEiRT0_S5_mPKT__nid_postfix")) mixed = resolved;
    }
    const auto units = [](const char16_t* text) { return reinterpret_cast<const std::uint16_t*>(text); };
    GuestLocale::InputIterator endIter = EndIter();

    static constexpr char16_t wideBoolTable[] = u"\0false\0true";
    struct WideCase {
        const char16_t* table;
        std::size_t count;
        const char16_t* input;
        int result;
        char16_t next;
    };
    for (const WideCase& item : {WideCase{u":Jan:Feb:Mar", 0, u"Feb", 1, u'\0'}, WideCase{u":Jan:Feb:Mar", 0, u"feb", 1, u'\0'},
             WideCase{u":Jan:Feb:Mar", 0, u"MAR", 2, u'\0'}, WideCase{u":Jan:Feb:Mar", 0, u"Febx", 1, u'x'},
             WideCase{u":Jan:Feb:Mar", 0, u"Ma", -1, u'\0'}, WideCase{u":Jan:Feb:Mar", 0, u"Xyz", -2, u'X'},
             WideCase{u":Jan:Feb:Mar", 0, u"", -2, u'\0'}, WideCase{u",a,ab,abc", 0, u"abd", 1, u'd'},
             WideCase{u":\u00e9t\u00e9:\u0416", 0, u"\u0416", 1, u'\0'}, WideCase{u":\u00e9t\u00e9", 0, u"\u00c9t\u00e9", -2, u'\u00c9'},
             WideCase{u":Feb", 0, u"\u0146eb", -2, u'\u0146'}, WideCase{u"", 0, u"Feb", -2, u'F'},
             WideCase{u"Jan:Feb", 0, u"Jan", -2, u'J'}, WideCase{u":Jan:January", 5, u"Jan", 0, u'\0'},
             WideCase{wideBoolTable, 2, u"true", 1, u'\0'}, WideCase{wideBoolTable, 2, u"tru", -1, u'\0'},
             WideCase{wideBoolTable, 0, u"true", -2, u't'}}) {
        for (bool buffered : {true, false}) {
            InputStreambuf<char16_t> sb{};
            sb.Setup(item.input, buffered);
            GuestLocale::InputIterator first = sb.Begin();
            Require(wide(&first, &endIter, item.count, units(item.table)) == item.result, "wide _Getloctxt result");
            Require(sb.RemainingFirst() == item.next, "wide _Getloctxt consumption");
        }
    }

    static constexpr char charBoolTable[] = "\0false\0true";
    struct MixedCase {
        const char* table;
        std::size_t count;
        const char16_t* input;
        int result;
        char16_t next;
    };
    for (const MixedCase& item : {MixedCase{":Jan:Feb:Mar", 0, u"Feb", 1, u'\0'}, MixedCase{":Jan:Feb:Mar", 0, u"FEB", 1, u'\0'},
             MixedCase{":Jan:Feb:Mar", 0, u"Febx", 1, u'x'}, MixedCase{":Jan:Feb:Mar", 0, u"Ma", -1, u'\0'},
             MixedCase{":Jan:Feb:Mar", 0, u"Xyz", -2, u'X'}, MixedCase{":Feb", 0, u"\u0146eb", -2, u'\u0146'},
             MixedCase{":\xe9t\xe9", 0, u"\u00e9t\u00e9", -2, u'\u00e9'}, MixedCase{":AM:PM", 0, u"pm", 1, u'\0'},
             MixedCase{charBoolTable, 2, u"false", 0, u'\0'}, MixedCase{charBoolTable, 2, u"TRUEx", 1, u'x'},
             MixedCase{"", 0, u"Feb", -2, u'F'}}) {
        for (bool buffered : {true, false}) {
            InputStreambuf<char16_t> sb{};
            sb.Setup(item.input, buffered);
            GuestLocale::InputIterator first = sb.Begin();
            Require(mixed(&first, &endIter, item.count, item.table) == item.result, "mixed _Getloctxt result");
            Require(sb.RemainingFirst() == item.next, "mixed _Getloctxt consumption");
        }
    }

    {
        InputStreambuf<char16_t> sb{};
        sb.Setup(u"Jan", true);
        GuestLocale::InputIterator first = sb.Begin();
        GuestLocale::InputIterator nullIter{{0, 0}};
        Require(wide(nullptr, &endIter, 0, units(u":Jan")) == -2, "wide null first -> -2");
        Require(wide(&first, nullptr, 0, units(u":Jan")) == -2, "wide null last -> -2");
        Require(wide(&first, &endIter, 0, nullptr) == -2, "wide null ptr -> -2");
        Require(wide(&nullIter, &endIter, 0, units(u":Jan")) == -2, "wide null buffer -> -2");
        Require(mixed(nullptr, &endIter, 0, ":Jan") == -2, "mixed null first -> -2");
        Require(mixed(&first, nullptr, 0, ":Jan") == -2, "mixed null last -> -2");
        Require(mixed(&first, &endIter, 0, nullptr) == -2, "mixed null ptr -> -2");
        Require(mixed(&nullIter, &endIter, 0, ":Jan") == -2, "mixed null buffer -> -2");
        Require(sb.RemainingFirst() == u'J', "null arguments consume nothing");
        Require(Throws([&] { wide(&first, &endIter, SIZE_MAX, units(u":Jan")); }), "wide SIZE_MAX throws length_error");
        Require(Throws([&] { mixed(&first, &endIter, SIZE_MAX, ":Jan"); }), "mixed SIZE_MAX throws length_error");
    }
}

} // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    bool hasLibrary = false;
    if (argc >= 2) {
#ifdef _WIN32
        g_library = LoadLibraryW(ArgumentPath(argv, 1).c_str());
#else
        g_library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
#endif
        Require(g_library != nullptr, "load libSceLibcInternal");
        hasLibrary = true;
    }
    std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestHeapAllocate),
        reinterpret_cast<void*>(&TestHeapFree),
        reinterpret_cast<void*>(&TestHeapCalloc),
        reinterpret_cast<void*>(&TestHeapReallocate),
        reinterpret_cast<void*>(&TestHeapAlign),
        reinterpret_cast<void*>(&TestHeapRealign),
        reinterpret_cast<void*>(&TestHeapPosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckGroup1ConstructionAndLifetime();
    CheckB2AdditionalCtorsAndGetcat(hasLibrary);
    CheckGroup2GetInt();
    CheckGroup3Fields();
    CheckGroup4GetAndGetFmt();
    CheckGroup5RemainingTimeGet(hasLibrary);
    WarmUpHostExceptionRuntime();
    CheckGroup6GuestMemory();
    CheckGroup7Getloctxt(hasLibrary);
    CheckGroup6WideGetloctxt(hasLibrary);
    return 0;
}
