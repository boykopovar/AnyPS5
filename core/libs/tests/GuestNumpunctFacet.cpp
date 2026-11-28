#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <source_location>
#include <stdexcept>

extern "C" std::uintptr_t _ZTVSt8numpunctIcE_nid_postfix[11];

namespace {

using Facet = GuestLocale::NumpunctFacet;

std::array<void*, 8> allocated{};
std::size_t allocatedCount = 0;
std::array<void*, 8> released{};
std::size_t releasedCount = 0;
std::size_t failingAllocation = 0;

void ResetHeapRecord() {
    allocated.fill(nullptr);
    allocatedCount = 0;
    released.fill(nullptr);
    releasedCount = 0;
    failingAllocation = 0;
}

void* APS5_VABI HeapAllocate(std::size_t bytes) {
    const std::size_t index = allocatedCount++;
    if (failingAllocation != 0 && allocatedCount == failingAllocation) return nullptr;
    void* pointer = std::malloc(bytes);
    if (index < allocated.size()) allocated[index] = pointer;
    return pointer;
}
void APS5_VABI HeapRelease(void* pointer) {
    if (releasedCount < released.size()) released[releasedCount] = pointer;
    ++releasedCount;
    std::free(pointer);
}
void* APS5_VABI HeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI HeapReallocate(void* pointer, std::size_t bytes) { return std::realloc(pointer, bytes); }
void* APS5_VABI HeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI HeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI HeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer == nullptr ? 12 : 0;
}

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "Numpunct facet check failed at line %u\n", location.line());
    std::abort();
}

template<class Exception, class Function>
void RequireThrows(Function function) {
    bool thrown = false;
    try {
        function();
    } catch (const Exception&) {
        thrown = true;
    }
    Require(thrown);
}

const GuestLocale::FacetVtable* NumpunctVtable() {
    return reinterpret_cast<const GuestLocale::FacetVtable*>(&_ZTVSt8numpunctIcE_nid_postfix[2]);
}

void RequireOwnedStrings(const Facet& facet, std::size_t firstAllocation) {
    Require(facet.grouping == allocated[firstAllocation] && std::strcmp(facet.grouping, "") == 0);
    Require(facet.falsename == allocated[firstAllocation + 1] && std::strcmp(facet.falsename, "false") == 0);
    Require(facet.truename == allocated[firstAllocation + 2] && std::strcmp(facet.truename, "true") == 0);
}

void RequireReleasedStrings(const Facet& facet) {
    Require(releasedCount >= 3);
    Require(released[0] == facet.grouping && released[1] == facet.falsename && released[2] == facet.truename);
}

struct Locinfo {
    Locinfo() { _ZNSt8_LocinfoC1EPKc_nid_postfix(&storage, "C"); }
    ~Locinfo() { _ZNSt8_LocinfoD1Ev_nid_postfix(&storage); }
    GuestLocale::LocinfoStorage storage{};
};

void CheckInit() {
    const Locinfo locinfo;
    const SonyLconv* conventions = localeconv_nid_postfix();
    using Init = void (APS5_VABI *)(Facet*, const GuestLocale::LocinfoStorage*, bool);
    for (const Init init : {static_cast<Init>(_ZNSt8numpunctIcE5_InitERKSt8_Locinfob_nid_postfix), static_cast<Init>(NumpunctInit_nid_no_patch)}) {
        for (const bool isDefault : {false, true}) {
            Facet facet{};
            facet.base.references = 0x55;
            facet.decimalPoint = 'x';
            facet.thousandsSep = 'y';
            ResetHeapRecord();
            init(&facet, &locinfo.storage, isDefault);
            Require(allocatedCount == 3 && releasedCount == 0);
            RequireOwnedStrings(facet, 0);
            Require(facet.grouping != conventions->grouping);
            Require(facet.base.vtable == nullptr && facet.base.references == 0x55);
            Require(facet.decimalPoint == '.');
            Require(facet.thousandsSep == (isDefault ? ',' : conventions->thousands_sep[0]));
            Require(facet.thousandsSep == (isDefault ? ',' : '\0'));
            const Facet before = facet;
            _ZNSt8numpunctIcE5_TidyEv_nid_postfix(&facet);
            Require(releasedCount == 3);
            RequireReleasedStrings(before);
            Require(facet.grouping == before.grouping && facet.falsename == before.falsename && facet.truename == before.truename);
        }
    }
}

void CheckInitFailure() {
    const Locinfo locinfo;
    for (std::size_t failing = 1; failing <= 3; ++failing) {
        Facet facet{};
        facet.grouping = "stale";
        facet.falsename = "stale";
        facet.truename = "stale";
        ResetHeapRecord();
        failingAllocation = failing;
        RequireThrows<std::bad_alloc>([&] { _ZNSt8numpunctIcE5_InitERKSt8_Locinfob_nid_postfix(&facet, &locinfo.storage, false); });
        Require(allocatedCount == failing && releasedCount == failing - 1);
        for (std::size_t index = 0; index + 1 < failing; ++index) Require(released[index] == allocated[index]);
        Require(facet.truename == nullptr);
        Require((failing == 1) == (facet.grouping == nullptr));
        Require((failing <= 2) == (facet.falsename == nullptr));
    }
    ResetHeapRecord();
}

using CurrentConstructor = void (APS5_VABI *)(Facet*, std::size_t);
using LocinfoConstructor = void (APS5_VABI *)(Facet*, const GuestLocale::LocinfoStorage*, std::size_t, bool);
using NamedConstructor = void (APS5_VABI *)(Facet*, const char*, std::size_t, bool);

void CheckCurrentLocaleConstructors() {
    for (const CurrentConstructor construct : {static_cast<CurrentConstructor>(_ZNSt8numpunctIcEC1Em_nid_postfix), static_cast<CurrentConstructor>(_ZNSt8numpunctIcEC2Em_nid_postfix), static_cast<CurrentConstructor>(NumpunctConstruct_nid_no_patch)}) {
        for (const std::size_t references : {std::size_t{0}, std::size_t{7}, std::size_t{0x100000005}}) {
            Facet facet{};
            ResetHeapRecord();
            construct(&facet, references);
            Require(facet.base.vtable == NumpunctVtable());
            Require(facet.base.references == static_cast<std::uint32_t>(references));
            Require(allocatedCount == 3 && releasedCount == 0);
            RequireOwnedStrings(facet, 0);
            Require(facet.decimalPoint == '.' && facet.thousandsSep == ',');
            _ZNSt8numpunctIcED1Ev_nid_postfix(&facet);
        }
    }
}

void CheckLocinfoConstructors() {
    const Locinfo locinfo;
    for (const LocinfoConstructor construct : {static_cast<LocinfoConstructor>(_ZNSt8numpunctIcEC1ERKSt8_Locinfomb_nid_postfix), static_cast<LocinfoConstructor>(NumpunctConstructFromLocinfo_nid_no_patch)}) {
        for (const bool isDefault : {false, true}) {
            Facet facet{};
            ResetHeapRecord();
            construct(&facet, &locinfo.storage, 3, isDefault);
            Require(facet.base.vtable == NumpunctVtable() && facet.base.references == 3);
            Require(allocatedCount == 3 && releasedCount == 0);
            RequireOwnedStrings(facet, 0);
            Require(facet.decimalPoint == '.' && facet.thousandsSep == (isDefault ? ',' : '\0'));
            _ZNSt8numpunctIcED1Ev_nid_postfix(&facet);
        }
    }
}

void CheckNamedConstructors() {
    for (const NamedConstructor construct : {static_cast<NamedConstructor>(NumpunctConstructNamed_nid_no_patch), static_cast<NamedConstructor>(_ZNSt8numpunctIcEC2EPKcmb_nid_postfix)}) {
        for (const bool isDefault : {false, true}) {
            Facet facet{};
            ResetHeapRecord();
            construct(&facet, "C", 2, isDefault);
            Require(facet.base.vtable == NumpunctVtable() && facet.base.references == 2);
            Require(allocatedCount == 3 && releasedCount == 0);
            RequireOwnedStrings(facet, 0);
            Require(facet.decimalPoint == '.' && facet.thousandsSep == (isDefault ? ',' : '\0'));
            _ZNSt8numpunctIcED1Ev_nid_postfix(&facet);
        }
        Facet facet{};
        ResetHeapRecord();
        RequireThrows<std::invalid_argument>([&] { construct(&facet, "fr_FR", 0, false); });
        Require(allocatedCount == 0 && releasedCount == 0);
    }
}

void CheckDestructors() {
    using Destructor = void (APS5_VABI *)(Facet*);
    const auto* vtable = NumpunctVtable();
    const auto slotDestroy = reinterpret_cast<Destructor>(vtable->destroy);
    const auto slotDelete = reinterpret_cast<Destructor>(vtable->deleteObject);
    for (const Destructor destroy : {static_cast<Destructor>(_ZNSt8numpunctIcED1Ev_nid_postfix), static_cast<Destructor>(NumpunctDestroy_nid_no_patch), slotDestroy}) {
        Facet facet{};
        _ZNSt8numpunctIcEC2Em_nid_postfix(&facet, 0);
        const Facet before = facet;
        ResetHeapRecord();
        destroy(&facet);
        Require(allocatedCount == 0 && releasedCount == 3);
        RequireReleasedStrings(before);
    }
    for (const Destructor deleteObject : {static_cast<Destructor>(NumpunctDelete_nid_no_patch), slotDelete}) {
        ResetHeapRecord();
        auto* facet = static_cast<Facet*>(_Znwm_nid_postfix(sizeof(Facet)));
        _ZNSt8numpunctIcEC1Em_nid_postfix(facet, 0);
        Require(allocatedCount == 4 && allocated[0] == facet);
        RequireOwnedStrings(*facet, 1);
        const Facet before = *facet;
        releasedCount = 0;
        deleteObject(facet);
        Require(releasedCount == 4);
        RequireReleasedStrings(before);
        Require(released[3] == allocated[0]);
    }
}

void CheckNullFacet() {
    const Locinfo locinfo;
    ResetHeapRecord();
    RequireThrows<std::invalid_argument>([] { _ZNSt8numpunctIcE5_TidyEv_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([&] { _ZNSt8numpunctIcE5_InitERKSt8_Locinfob_nid_postfix(nullptr, &locinfo.storage, false); });
    RequireThrows<std::invalid_argument>([] { _ZNSt8numpunctIcEC1Em_nid_postfix(nullptr, 0); });
    RequireThrows<std::invalid_argument>([] { _ZNSt8numpunctIcEC2Em_nid_postfix(nullptr, 0); });
    RequireThrows<std::invalid_argument>([&] { _ZNSt8numpunctIcEC1ERKSt8_Locinfomb_nid_postfix(nullptr, &locinfo.storage, 0, false); });
    RequireThrows<std::invalid_argument>([&] { NumpunctConstructFromLocinfo_nid_no_patch(nullptr, &locinfo.storage, 0, false); });
    RequireThrows<std::invalid_argument>([] { NumpunctConstructNamed_nid_no_patch(nullptr, "C", 0, false); });
    RequireThrows<std::invalid_argument>([] { _ZNSt8numpunctIcEC2EPKcmb_nid_postfix(nullptr, "C", 0, false); });
    RequireThrows<std::invalid_argument>([] { _ZNSt8numpunctIcED1Ev_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { NumpunctDestroy_nid_no_patch(nullptr); });
    RequireThrows<std::invalid_argument>([] { NumpunctDelete_nid_no_patch(nullptr); });
    Require(allocatedCount == 0 && releasedCount == 0);
}

}

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(&HeapAllocate);
    heapApi[1] = reinterpret_cast<void*>(&HeapRelease);
    heapApi[2] = reinterpret_cast<void*>(&HeapCalloc);
    heapApi[3] = reinterpret_cast<void*>(&HeapReallocate);
    heapApi[4] = reinterpret_cast<void*>(&HeapAlign);
    heapApi[5] = reinterpret_cast<void*>(&HeapRealign);
    heapApi[6] = reinterpret_cast<void*>(&HeapPosixAlign);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    CheckInit();
    CheckInitFailure();
    CheckCurrentLocaleConstructors();
    CheckLocinfoConstructors();
    CheckNamedConstructors();
    CheckDestructors();
    CheckNullFacet();
    std::puts("Numpunct facet checks passed");
}
