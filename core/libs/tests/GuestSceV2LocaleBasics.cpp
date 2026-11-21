#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

using GuestWhat = const char* (APS5_VABI *)(const LibcException::ExceptionObject*);
using Thrower = void (APS5_VABI *)(const void*);

extern "C" {
extern const TypeRecord _ZTISt9exception_nid_postfix;
extern GuestLocale::Implementation* _ZNSt8__sce_v26locale7_Locimp8_ClocptrE_nid_postfix;
extern GuestLocale::Implementation* _ZNSt8__sce_v221_sceLibcClassicLocaleE_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

void* APS5_VABI TestHeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
void APS5_VABI TestHeapFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer != nullptr ? 0 : -1;
}

static unsigned failures = 0;
static std::string inspected;

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* exception = static_cast<const LibcException::ExceptionObject*>(object);
    const auto* vtable = static_cast<const void* const*>(exception->vtable);
    inspected = static_cast<const TypeRecord*>(vtable[-1])->name;
    inspected += '|';
    inspected += reinterpret_cast<GuestWhat>(const_cast<void*>(vtable[2]))(exception);
}

enum class LocinfoCall { Check, Construct, Addcats };

struct LocinfoArguments { LocinfoCall call; GuestLocale::SceV2Locinfo* self; int category; const char* name; };

extern "C" void APS5_VABI SceV2CallLocinfo(const void* argument) {
    const auto* arguments = static_cast<const LocinfoArguments*>(argument);
    if (arguments->call == LocinfoCall::Check) _ZNSt8__sce_v215_sceLibcLocinfoEPKc_nid_postfix(arguments->name);
    if (arguments->call == LocinfoCall::Construct) _ZNSt8__sce_v28_Locinfo13_Locinfo_ctorEPS0_iPKc_nid_postfix(arguments->self, arguments->category, arguments->name);
    if (arguments->call == LocinfoCall::Addcats) _ZNSt8__sce_v28_Locinfo16_Locinfo_AddcatsEPS0_iPKc_nid_postfix(arguments->self, arguments->category, arguments->name);
}

static bool ThrowsBadLocaleName(const LocinfoArguments& arguments) {
    const TypeRecord runtimeError {_ZTISt9exception_nid_postfix.vtable, "NSt8__sce_v213runtime_errorE", nullptr};
    const TypeRecord logicError {_ZTISt9exception_nid_postfix.vtable, "NSt8__sce_v211logic_errorE", nullptr};
    SceV2CatchType = &runtimeError;
    SceV2OtherType = &logicError;
    SceV2Selector = 0;
    inspected.clear();
    SceV2Catch(reinterpret_cast<Thrower>(SceV2CallLocinfo), &arguments);
    return SceV2Selector == 2 && inspected == "NSt8__sce_v213runtime_errorE|bad locale name";
}

static bool IsEmptyLocimp(const GuestLocale::Implementation* locimp, bool transparent) {
    return locimp != nullptr && locimp->base.vtable != nullptr && locimp->base.references == 1 && locimp->facets == nullptr
        && locimp->facetCount == 0 && locimp->category == 0 && locimp->transparent == transparent && locimp->name != nullptr
        && std::strcmp(locimp->name, "*") == 0;
}

static void CheckClassic() {
    auto* classic = _ZNSt8__sce_v221_sceLibcClassicLocaleE_nid_postfix;
    Require(_ZNSt8__sce_v26locale7_Locimp8_ClocptrE_nid_postfix == classic, "_Clocptr points at the classic _Locimp");
    auto* facade = _ZNSt8__sce_v26locale7classicEv_nid_postfix();
    Require(facade != nullptr && facade->implementation == classic, "classic() holds the classic _Locimp");
    Require(_ZNSt8__sce_v26locale7classicEv_nid_postfix() == facade, "classic() returns the same object");
    Require(_ZNSt8__sce_v26locale16_GetgloballocaleEv_nid_postfix() == classic, "the global locale starts as the classic one");
    Require(_ZNSt8__sce_v26locale5_InitEb_nid_postfix(false) == classic, "_Init returns the global locale");
}

static void CheckEmpty() {
    GuestLocaleFacade first {};
    GuestLocaleFacade second {};
    Require(_ZNSt8__sce_v26locale5emptyEv_nid_postfix(&first) == &first, "empty() returns its result");
    Require(IsEmptyLocimp(first.implementation, true), "empty() builds a transparent _Locimp named *");
    _ZNSt8__sce_v26locale5emptyEv_nid_postfix(&second);
    Require(second.implementation != first.implementation, "empty() builds a new _Locimp each time");
    Require(IsEmptyLocimp(_ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false), false), "_New_Locimp(false) builds an opaque _Locimp");
    Require(IsEmptyLocimp(_ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(true), true), "_New_Locimp(true) builds a transparent _Locimp");
}

static void CheckGlobal() {
    auto* classic = _ZNSt8__sce_v221_sceLibcClassicLocaleE_nid_postfix;
    const auto classicReferences = classic->base.references;
    GuestLocaleFacade target {};
    _ZNSt8__sce_v26locale5emptyEv_nid_postfix(&target);
    auto* locimp = target.implementation;

    GuestLocaleFacade previous {};
    Require(_ZNSt8__sce_v26locale6globalERKS0__nid_postfix(&previous, &target) == &previous, "global() returns its result");
    Require(previous.implementation == classic, "global() returns the previous global locale");
    Require(classic->base.references == classicReferences, "global() releases the reference the old global held");
    Require(_ZNSt8__sce_v26locale16_GetgloballocaleEv_nid_postfix() == locimp, "global() installs the new locale");
    Require(locimp->base.references == 2, "global() retains the new global locale");
    Require(_ZNSt8__sce_v26locale5_InitEb_nid_postfix(false) == locimp, "_Init returns the new global locale");
    Require(locimp->base.references == 2, "_Init(false) takes no reference");

    GuestLocaleFacade same {};
    _ZNSt8__sce_v26locale6globalERKS0__nid_postfix(&same, &target);
    Require(same.implementation == locimp, "global() with the current locale returns it");
    Require(locimp->base.references == 3, "global() with the current locale only takes the result's reference");

    GuestLocaleFacade back {classic};
    GuestLocaleFacade old {};
    _ZNSt8__sce_v26locale6globalERKS0__nid_postfix(&old, &back);
    Require(old.implementation == locimp, "global() returns the replaced locale");
    Require(locimp->base.references == 3, "the replaced locale keeps the result's reference only");
    Require(classic->base.references == classicReferences + 1, "the classic locale is retained again as the global");
    Require(_ZNSt8__sce_v26locale16_GetgloballocaleEv_nid_postfix() == classic, "global() restores the classic locale");

    auto* other = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(false);
    _ZNSt8__sce_v26locale16_SetgloballocaleEPv_nid_postfix(other);
    Require(_ZNSt8__sce_v26locale16_GetgloballocaleEv_nid_postfix() == other, "_Setgloballocale installs the locale");
    Require(other->base.references == 1, "_Setgloballocale takes no reference");
    _ZNSt8__sce_v26locale16_SetgloballocaleEPv_nid_postfix(classic);
    bool threw = false;
    try {
        _ZNSt8__sce_v26locale16_SetgloballocaleEPv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "_Setgloballocale(null) throws");
    Require(_ZNSt8__sce_v26locale16_GetgloballocaleEv_nid_postfix() == classic, "a rejected _Setgloballocale keeps the global locale");
}

static void CheckLocinfo() {
    const char untouched[] = "untouched";
    const char* const accepted[] {"", "C"};
    const char* const rejected[] {nullptr, "POSIX", "c", "C.UTF-8", "CX", "en_US.UTF-8"};
    const int categories[] {0, 1, 0x3f, -1};

    for (const char* name : accepted) {
        LocinfoArguments check {LocinfoCall::Check, nullptr, 0, name};
        SceV2CallLocinfo(&check);
        for (int category : categories) {
            GuestLocale::SceV2Locinfo locinfo {0x5a5a5a5a5a5a5a5aull, nullptr, nullptr};
            _ZNSt8__sce_v28_Locinfo13_Locinfo_ctorEPS0_iPKc_nid_postfix(&locinfo, category, name);
            Require(locinfo.name != nullptr && std::strcmp(locinfo.name, "C") == 0, "_Locinfo(int) stores the current name");
            Require(locinfo.classicName != nullptr && std::strcmp(locinfo.classicName, "C") == 0, "_Locinfo(int) stores C at offset 0x10");
            Require(locinfo.reserved == 0x5a5a5a5a5a5a5a5aull, "_Locinfo(int) preserves the caller word");

            GuestLocale::SceV2Locinfo added {0x5a5a5a5a5a5a5a5aull, untouched, untouched};
            Require(_ZNSt8__sce_v28_Locinfo16_Locinfo_AddcatsEPS0_iPKc_nid_postfix(&added, category, name) == &added, "_Addcats returns its object");
            Require(added.classicName != nullptr && std::strcmp(added.classicName, "C") == 0, "_Addcats stores C at offset 0x10");
            Require(added.name == untouched && added.reserved == 0x5a5a5a5a5a5a5a5aull, "_Addcats leaves the other fields");
        }
    }

    for (const char* name : rejected) {
        Require(ThrowsBadLocaleName({LocinfoCall::Check, nullptr, 0, name}), "_sceLibcLocinfo rejects the name");

        GuestLocale::SceV2Locinfo locinfo {0x5a5a5a5a5a5a5a5aull, nullptr, untouched};
        Require(ThrowsBadLocaleName({LocinfoCall::Construct, &locinfo, 0x3f, name}), "_Locinfo(int) rejects the name");
        Require(locinfo.name != nullptr && std::strcmp(locinfo.name, "C") == 0, "_Locinfo(int) stores the current name before rejecting");
        Require(locinfo.classicName == untouched, "_Locinfo(int) leaves offset 0x10 when rejecting");

        GuestLocale::SceV2Locinfo added {0x5a5a5a5a5a5a5a5aull, untouched, untouched};
        Require(ThrowsBadLocaleName({LocinfoCall::Addcats, &added, 0x3f, name}), "_Addcats rejects the name");
        Require(added.name == untouched && added.classicName == untouched, "_Addcats changes nothing when rejecting");
    }
}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&TestHeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&TestHeapFree);
    heapApi[2] = reinterpret_cast<void*>(&TestHeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&TestHeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&TestHeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&TestHeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&TestHeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());

    CheckClassic();
    CheckEmpty();
    CheckGlobal();
    CheckLocinfo();
    if (failures != 0) return 1;
    std::puts("std::__sce_v2 locale basics passed");
    return 0;
}
