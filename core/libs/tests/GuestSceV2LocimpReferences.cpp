#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

using Thrower = void (APS5_VABI *)(const void*);

extern "C" {
extern const TypeRecord _ZTISt9bad_alloc_nid_postfix;
extern const TypeRecord _ZTISt8bad_cast_nid_postfix;

void APS5_VABI SceV2Catch(Thrower thrower, const void* argument);

const void* SceV2CatchType = nullptr;
const void* SceV2OtherType = nullptr;
int SceV2Selector = 0;
}

static std::size_t gFailBytes = 0;
static void* gLastFreed = nullptr;
static void* gLastObject = nullptr;

void* APS5_VABI TestHeapAllocate(std::size_t bytes) {
    if (bytes == gFailBytes) return nullptr;
    void* pointer = std::malloc(bytes);
    if (bytes == sizeof(GuestLocale::Implementation)) gLastObject = pointer;
    return pointer;
}
void APS5_VABI TestHeapFree(void* pointer) {
    gLastFreed = pointer;
    std::free(pointer);
}
void* APS5_VABI TestHeapCalloc(std::size_t count, std::size_t bytes) { return std::calloc(count, bytes); }
void* APS5_VABI TestHeapReallocate(void* pointer, std::size_t bytes) { return bytes == gFailBytes ? nullptr : std::realloc(pointer, bytes); }
void* APS5_VABI TestHeapAlign(std::size_t, std::size_t bytes) { return std::malloc(bytes); }
void* APS5_VABI TestHeapRealign(void* pointer, std::size_t bytes, std::size_t) { return std::realloc(pointer, bytes); }
int APS5_VABI TestHeapPosixAlign(void** pointer, std::size_t, std::size_t bytes) {
    *pointer = std::malloc(bytes);
    return *pointer != nullptr ? 0 : -1;
}

static char caught[128];

extern "C" void APS5_VABI SceV2Inspect(void* object) {
    const auto* vtable = *static_cast<const void* const* const*>(object);
    std::snprintf(caught, sizeof(caught), "%s", static_cast<const TypeRecord*>(vtable[-1])->name);
}

struct CountingFacet {
    GuestLocale::Facet base;
    int retains;
    int releases;
    int deletes;
};

static void APS5_VABI CountingDestroy(GuestLocale::Facet*) {}

static void APS5_VABI CountingDelete(GuestLocale::Facet* self) {
    ++reinterpret_cast<CountingFacet*>(self)->deletes;
}

static void APS5_VABI CountingRetain(GuestLocale::Facet* self) {
    ++reinterpret_cast<CountingFacet*>(self)->retains;
    ++self->references;
}

static GuestLocale::Facet* APS5_VABI CountingRelease(GuestLocale::Facet* self) {
    ++reinterpret_cast<CountingFacet*>(self)->releases;
    return --self->references == 0 ? self : nullptr;
}

static const GuestLocale::FacetVtable gCountingVtable{CountingDestroy, CountingDelete, CountingRetain, CountingRelease};

static CountingFacet MakeFacet() { return {{&gCountingVtable, 0, 0}, 0, 0, 0}; }

static bool Counts(const CountingFacet& facet, std::uint32_t references, int retains, int releases, int deletes) {
    return facet.base.references == references && facet.retains == retains && facet.releases == releases && facet.deletes == deletes;
}

struct AddfacArguments { GuestLocale::Implementation* self; GuestLocale::Facet* facet; std::size_t id; };
struct CtorArguments { GuestLocale::Implementation* self; const GuestLocale::Implementation* source; };

extern "C" void APS5_VABI CallAddfac(const void* argument) {
    const auto* arguments = static_cast<const AddfacArguments*>(argument);
    _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(arguments->self, arguments->facet, arguments->id);
}

extern "C" void APS5_VABI CallCtor(const void* argument) {
    const auto* arguments = static_cast<const CtorArguments*>(argument);
    _ZNSt8__sce_v26locale7_Locimp12_Locimp_ctorEPS1_RKS1__nid_postfix(arguments->self, arguments->source);
}

extern "C" void APS5_VABI CallNewLocimp(const void* argument) {
    _ZNSt8__sce_v26locale7_Locimp11_New_LocimpERKS1__nid_postfix(static_cast<const GuestLocale::Implementation*>(argument));
}

static bool ThrowsBadAlloc(Thrower thrower, const void* argument) {
    SceV2CatchType = &_ZTISt9bad_alloc_nid_postfix;
    SceV2OtherType = &_ZTISt8bad_cast_nid_postfix;
    SceV2Selector = 0;
    caught[0] = 0;
    SceV2Catch(thrower, argument);
    return SceV2Selector == 2 && std::strcmp(caught, "St9bad_alloc") == 0;
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

    auto* classic = _ZNSt8__sce_v26locale5_InitEb_nid_postfix(false);
    Expect(classic != nullptr, "the classic locale exists");

    auto* copy = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpERKS1__nid_postfix(classic);
    Expect(copy != nullptr && copy != classic, "_New_Locimp returns a new object");
    Expect(copy->base.vtable != nullptr && copy->base.vtable != classic->base.vtable, "the copy has the versioned _Locimp vtable");
    Expect(copy->base.references == 1, "the copy starts with one reference");
    Expect(copy->facetCount == classic->facetCount, "the copy keeps the facet count");
    Expect(copy->category == classic->category && copy->transparent == classic->transparent, "the copy keeps category and transparency");
    Expect(copy->name == classic->name, "the copy shares the name pointer");
    Expect(copy->facets != nullptr && copy->facets != classic->facets, "the copy owns its facet vector");

    CountingFacet first = MakeFacet();
    CountingFacet second = MakeFacet();
    CountingFacet third = MakeFacet();
    _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(copy, &first.base, 3);
    Expect(copy->facetCount == 40, "Addfac grows to 40 slots");
    Expect(copy->facets[3] == &first.base && copy->facets[39] == nullptr, "Addfac stores the facet and clears new slots");
    Expect(Counts(first, 1, 1, 0, 0), "Addfac retains the new facet");
    _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(copy, &second.base, 3);
    Expect(copy->facets[3] == &second.base, "Addfac replaces the facet");
    Expect(Counts(first, 0, 1, 1, 1), "Addfac releases and deletes the replaced facet");
    Expect(Counts(second, 1, 1, 0, 0), "Addfac retains the replacement");
    _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(copy, &second.base, 3);
    Expect(Counts(second, 1, 2, 1, 0), "re-adding the same facet keeps it alive");
    _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(copy, &third.base, 45);
    Expect(copy->facetCount == 46, "Addfac grows past 40 to id + 1");
    Expect(copy->facets[3] == &second.base && copy->facets[45] == &third.base && copy->facets[44] == nullptr, "growing keeps the old facets");

    auto* nested = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpERKS1__nid_postfix(copy);
    Expect(nested->facetCount == 46 && nested->facets[3] == &second.base && nested->facets[45] == &third.base, "the nested copy has the same facets");
    Expect(second.base.references == 2 && third.base.references == 2, "the nested copy retains its facets");
    copy->base.vtable->retain(&copy->base);
    Expect(copy->base.references == 2, "the versioned vtable retains the copy");
    Expect(copy->base.vtable->release(&copy->base) == nullptr, "a release above zero keeps the copy");
    auto* released = copy->base.vtable->release(&copy->base);
    Expect(released == &copy->base, "the last release returns the copy");
    const auto* const locimpVtable = copy->base.vtable;
    released->vtable->deleteObject(released);
    Expect(gLastFreed == copy, "deleteObject frees the copy");
    Expect(Counts(second, 1, 3, 2, 0) && Counts(third, 1, 2, 1, 0), "destroying the copy releases its facets");

    GuestLocale::Facet* sentinel[1]{};
    GuestLocale::Implementation manual{{nullptr, 1, 0}, sentinel, 0, 0, false, "manual"};
    _ZNSt8__sce_v26locale7_Locimp12_Locimp_ctorEPS1_RKS1__nid_postfix(&manual, nested);
    Expect(manual.facets == sentinel && manual.facetCount == 0, "a zero count keeps the vector");
    manual.facetCount = 4;
    _ZNSt8__sce_v26locale7_Locimp12_Locimp_ctorEPS1_RKS1__nid_postfix(&manual, nested);
    Expect(manual.facets != sentinel && manual.facets != nested->facets, "the ctor allocates a vector");
    Expect(manual.facets[3] == &second.base && manual.facets[0] == nullptr && manual.facetCount == 4, "the ctor copies only the first count facets");
    Expect(Counts(second, 2, 4, 2, 0) && Counts(third, 1, 2, 1, 0), "the ctor retains only the copied facets");
    _ZNSt8__sce_v26locale7_Locimp12_Locimp_dtorEPS1__nid_postfix(&manual);
    Expect(gLastFreed == manual.facets, "the dtor frees the vector");
    Expect(Counts(second, 1, 4, 3, 0), "the dtor releases the facets");

    _ZNSt8__sce_v26locale7_Locimp12_Locimp_dtorEPS1__nid_postfix(nested);
    Expect(Counts(second, 0, 4, 4, 1) && Counts(third, 0, 2, 2, 1), "the dtor deletes the last references");
    Expect(gLastFreed == nested->facets, "the dtor frees the nested vector");
    _ZdlPv_nid_postfix(nested);

    GuestLocale::Implementation borrowed = *classic;
    CountingFacet fourth = MakeFacet();
    gLastFreed = nullptr;
    _ZNSt8__sce_v26locale7_Locimp12_Locimp_dtorEPS1__nid_postfix(&borrowed);
    Expect(gLastFreed == nullptr, "the dtor keeps the classic vector");
    _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(&borrowed, &fourth.base, 2);
    Expect(borrowed.facets != classic->facets && borrowed.facetCount == 40, "Addfac copies the classic vector");
    Expect(classic->facetCount == 1 && classic->facets[0] == nullptr, "the classic vector is unchanged");
    Expect(Counts(fourth, 1, 1, 0, 0), "Addfac retains the facet added to the copied vector");
    _ZNSt8__sce_v26locale7_Locimp12_Locimp_dtorEPS1__nid_postfix(&borrowed);
    Expect(Counts(fourth, 0, 1, 1, 1), "the dtor releases the facet added to the copied vector");

    auto* owner = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpERKS1__nid_postfix(classic);
    CountingFacet fifth = MakeFacet();
    _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(owner, &fifth.base, 7);
    auto** before = owner->facets;
    CountingFacet sixth = MakeFacet();
    gFailBytes = 100 * sizeof(GuestLocale::Facet*);
    const AddfacArguments failingAddfac{owner, &sixth.base, 99};
    Expect(ThrowsBadAlloc(CallAddfac, &failingAddfac), "a failed Addfac growth throws bad_alloc");
    Expect(owner->facets == before && owner->facetCount == 40, "a failed Addfac growth keeps the vector");
    Expect(Counts(sixth, 0, 0, 0, 0) && Counts(fifth, 1, 1, 0, 0), "a failed Addfac growth retains nothing");

    gFailBytes = 40 * sizeof(GuestLocale::Facet*);
    GuestLocale::Implementation target{{nullptr, 1, 0}, sentinel, 40, 0, false, "target"};
    const CtorArguments failingCtor{&target, owner};
    Expect(ThrowsBadAlloc(CallCtor, &failingCtor), "a failed ctor allocation throws bad_alloc");
    Expect(target.facets == sentinel && Counts(fifth, 1, 1, 0, 0), "a failed ctor allocation retains nothing");

    gLastFreed = nullptr;
    gLastObject = nullptr;
    Expect(ThrowsBadAlloc(CallNewLocimp, owner), "a failed _New_Locimp vector allocation throws bad_alloc");
    Expect(gLastObject != nullptr && gLastFreed == gLastObject, "a failed _New_Locimp frees the new object");
    Expect(Counts(fifth, 1, 1, 0, 0), "a failed _New_Locimp retains nothing");
    gFailBytes = 0;

    CountingFacet seventh = MakeFacet();
    std::thread([&] { _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(owner, &seventh.base, 99); }).join();
    Expect(owner->facetCount == 100 && owner->facets[99] == &seventh.base && owner->facets[7] == &fifth.base, "Addfac works after the failures");
    Expect(owner->base.vtable->release(&owner->base) == &owner->base, "the owner drops to zero");
    owner->base.vtable->deleteObject(&owner->base);
    Expect(Counts(fifth, 0, 1, 1, 1) && Counts(seventh, 0, 1, 1, 1), "deleting the owner deletes its facets");

    auto* fresh = _ZNSt8__sce_v26locale7_Locimp11_New_LocimpEb_nid_postfix(true);
    Expect(fresh->base.vtable == locimpVtable && fresh->facets == nullptr && fresh->facetCount == 0, "_New_Locimp(bool) uses the owning vtable");
    CountingFacet eighth = MakeFacet();
    _ZNSt8__sce_v26locale7_Locimp14_Locimp_AddfacEPS1_PNS0_5facetEm_nid_postfix(fresh, &eighth.base, 0);
    Expect(fresh->facetCount == 40 && fresh->facets[0] == &eighth.base, "Addfac grows an empty vector");
    Expect(fresh->base.vtable->release(&fresh->base) == &fresh->base, "the fresh locale drops to zero");
    fresh->base.vtable->deleteObject(&fresh->base);
    Expect(gLastFreed == fresh && Counts(eighth, 0, 1, 1, 1), "deleting the fresh locale deletes its facet");
    GuestLocaleFacade facade{};
    _ZNSt8__sce_v26locale5emptyEv_nid_postfix(&facade);
    Expect(facade.implementation != nullptr && facade.implementation->base.vtable == locimpVtable, "empty() uses the owning vtable");
    Expect(facade.implementation->base.vtable->release(&facade.implementation->base) == &facade.implementation->base, "the empty locale drops to zero");
    facade.implementation->base.vtable->deleteObject(&facade.implementation->base);
    Expect(gLastFreed == facade.implementation, "deleting the empty locale frees it");

    if (Failures() != 0) std::fprintf(stderr, "%u failures\n", Failures());
    return Failures() == 0 ? 0 : 1;
}
