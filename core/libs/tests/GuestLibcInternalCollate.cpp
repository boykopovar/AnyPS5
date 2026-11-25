#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "tests/CommandLineArgument.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
}

namespace {

int derivedCompares = 0;
int derivedHashes = 0;
long liveAllocations = 0;
long totalAllocations = 0;
long totalFrees = 0;
void* lastFreed = nullptr;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal collate: %s\n", message);
    std::abort();
}

template<typename TAction>
void Reject(TAction action, const char* message) {
    bool rejected = false;
    try { action(); } catch (const std::exception&) { rejected = true; }
    Require(rejected, message);
}

void* APS5_VABI TestAllocate(std::size_t size) {
    ++liveAllocations;
    ++totalAllocations;
    return std::malloc(size);
}
void APS5_VABI TestFree(void* pointer) {
    if (pointer != nullptr) --liveAllocations;
    ++totalFrees;
    lastFreed = pointer;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    ++liveAllocations;
    ++totalAllocations;
    return std::calloc(count, size);
}
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) {
    if (pointer == nullptr) {
        ++liveAllocations;
        ++totalAllocations;
    }
    return std::realloc(pointer, size);
}
void* APS5_VABI TestAlign(std::size_t, std::size_t size) {
    ++liveAllocations;
    ++totalAllocations;
    return std::malloc(size);
}
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) {
    if (pointer == nullptr) {
        ++liveAllocations;
        ++totalAllocations;
    }
    return std::realloc(pointer, size);
}
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    if (*pointer) {
        ++liveAllocations;
        ++totalAllocations;
        return 0;
    }
    return 12;
}

int APS5_VABI DerivedCompare(const GuestLocale::CollateFacet*, const char*, const char*, const char*, const char*) {
    ++derivedCompares;
    return 7;
}

std::int64_t APS5_VABI DerivedHash(const GuestLocale::CollateFacet*, const char*, const char*) {
    ++derivedHashes;
    return 42;
}

int DoCompare(const GuestLocale::CollateFacet* facet, const std::string& left, const std::string& right) {
    return sceLibcInternalCollateDoCompare_nid_no_patch(facet, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
}

int Compare(const GuestLocale::CollateFacet* facet, const std::string& left, const std::string& right) {
    return sceLibcInternalCollateCompare_nid_no_patch(facet, left.data(), left.data() + left.size(), right.data(), right.data() + right.size());
}

std::int64_t DoHash(const GuestLocale::CollateFacet* facet, const std::string& text) {
    return sceLibcInternalCollateDoHash_nid_no_patch(facet, text.data(), text.data() + text.size());
}

std::int64_t Hash(const GuestLocale::CollateFacet* facet, const std::string& text) {
    return sceLibcInternalCollateHash_nid_no_patch(facet, text.data(), text.data() + text.size());
}

void CheckClassicFacet(const GuestLocale::CollateFacet* facet) {
    Require(DoCompare(facet, "abc", "abd") == -1, "do_compare orders a smaller byte first");
    Require(DoCompare(facet, "abd", "abc") == 1, "do_compare orders a larger byte last");
    Require(DoCompare(facet, "abc", "abc") == 0, "do_compare finds equal ranges equal");
    Require(DoCompare(facet, "ab", "abc") == -1, "do_compare orders a prefix first");
    Require(DoCompare(facet, "abc", "ab") == 1, "do_compare orders the longer range last");
    Require(DoCompare(facet, "", "") == 0, "do_compare finds empty ranges equal");
    Require(DoCompare(facet, "\x80", "a") == 1, "do_compare compares unsigned bytes");
    Require(DoCompare(facet, std::string("a\0b", 3), std::string("a\0c", 3)) == -1, "do_compare reads past embedded nulls");
    Require(sceLibcInternalCollateDoCompare_nid_no_patch(facet, nullptr, nullptr, nullptr, nullptr) == 0, "do_compare accepts null empty ranges");
    const char text[] = "abc";
    Reject([&] { sceLibcInternalCollateDoCompare_nid_no_patch(facet, text + 2, text, text, text + 3); }, "do_compare rejects a reversed range");

    Require(Compare(facet, "abc", "abd") == -1, "compare forwards to do_compare");
    Require(Compare(facet, "abd", "abc") == 1, "compare returns do_compare's positive order");
    Require(Compare(facet, "abc", "abc") == 0, "compare returns do_compare's equality");

    Require(DoHash(facet, "abc") == -1792535898324117685LL, "do_hash matches the libc collate hash");
    Require(DoHash(facet, "") == static_cast<std::int64_t>(0xcbf29ce484222325ULL), "do_hash of an empty range is the offset basis");
    Require(Hash(facet, "abc") == DoHash(facet, "abc"), "hash forwards to do_hash");
    Require(sceLibcInternalCollateDoHash_nid_no_patch(facet, nullptr, nullptr) == DoHash(facet, ""), "do_hash accepts a null empty range");
    Reject([&] { sceLibcInternalCollateDoHash_nid_no_patch(facet, text + 1, text); }, "do_hash rejects a reversed range");

    GuestLocale::String shortResult{};
    Require(sceLibcInternalCollateDoTransform_nid_no_patch(&shortResult, facet, "xyz", "xyz" + 3) == &shortResult, "do_transform returns its result");
    Require(shortResult.size == 3 && shortResult.capacity == 15 && std::memcmp(shortResult.buffer, "xyz", 4) == 0, "do_transform copies a short range inline");

    const std::string longText(40, 'q');
    GuestLocale::String longResult{};
    Require(sceLibcInternalCollateDoTransform_nid_no_patch(&longResult, facet, longText.data(), longText.data() + longText.size()) == &longResult, "do_transform returns a long result");
    Require(longResult.size == 40 && longResult.capacity == (40 | 15), "do_transform sizes a long result");
    Require(std::memcmp(longResult.pointer, longText.c_str(), 41) == 0, "do_transform copies a long range");
    _ZdlPv_nid_postfix(longResult.pointer);
    Reject([&] { sceLibcInternalCollateDoTransform_nid_no_patch(nullptr, facet, "a", "a" + 1); }, "do_transform rejects a null result");
}

void CheckDerivedFacet() {
    const auto& base = *reinterpret_cast<const GuestLocale::CollateVtable*>(&_ZTVSt7collateIcE_nid_postfix[2]);
    GuestLocale::CollateVtable derived = base;
    derived.compare = &DerivedCompare;
    derived.hash = &DerivedHash;
    GuestLocale::CollateFacet facet{};
    facet.base.vtable = &derived.facet;

    Require(Compare(&facet, "abc", "abd") == 7 && derivedCompares == 1, "compare calls the facet's virtual do_compare");
    Require(Hash(&facet, "abc") == 42 && derivedHashes == 1, "hash calls the facet's virtual do_hash");
    Require(DoCompare(&facet, "abc", "abd") == -1 && derivedCompares == 1, "do_compare is the collate<char> member, not the override");
    Require(DoHash(&facet, "abc") == -1792535898324117685LL && derivedHashes == 1, "do_hash is the collate<char> member, not the override");

    Reject([] { sceLibcInternalCollateCompare_nid_no_patch(nullptr, "a", "a" + 1, "b", "b" + 1); }, "compare rejects a null facet");
    Reject([] { sceLibcInternalCollateHash_nid_no_patch(nullptr, "a", "a" + 1); }, "hash rejects a null facet");
    GuestLocale::CollateFacet empty{};
    Reject([&] { sceLibcInternalCollateCompare_nid_no_patch(&empty, "a", "a" + 1, "b", "b" + 1); }, "compare rejects a facet without a vtable");
    Reject([&] { sceLibcInternalCollateHash_nid_no_patch(&empty, "a", "a" + 1); }, "hash rejects a facet without a vtable");
}

struct alignas(GuestLocale::CollateFacet) FacetStorage {
    unsigned char bytes[sizeof(GuestLocale::CollateFacet) + 16];
};

FacetStorage FilledStorage() {
    FacetStorage storage;
    std::memset(storage.bytes, 0xa5, sizeof(storage.bytes));
    return storage;
}

GuestLocale::CollateFacet* FacetPtr(FacetStorage& storage) {
    return reinterpret_cast<GuestLocale::CollateFacet*>(storage.bytes);
}

bool UntouchedStorage(const FacetStorage& storage) {
    for (unsigned char byte : storage.bytes) {
        if (byte != 0xa5) return false;
    }
    return true;
}

void CheckConstructedLayout(const FacetStorage& storage, std::uint64_t expectedRefs) {
    const auto* facet = reinterpret_cast<const GuestLocale::CollateFacet*>(storage.bytes);
    const void* expectedVtable = &_ZTVSt7collateIcE_nid_postfix[2];
    Require(facet->base.vtable == expectedVtable, "constructor installs _ZTVSt7collateIcE");
    Require(facet->base.references == static_cast<std::uint32_t>(expectedRefs), "constructor writes low 32 bits of references");
    Require(facet->base.reserved == static_cast<std::uint32_t>(expectedRefs >> 32), "constructor writes high 32 bits of references");
    Require(facet->collation == nullptr && facet->wideCollation == nullptr, "constructor zeroes collation pointers");
    for (std::size_t index = sizeof(GuestLocale::CollateFacet); index < sizeof(storage.bytes); ++index) {
        Require(storage.bytes[index] == 0xa5, "constructor does not write past CollateFacet");
    }
}

void CheckConstructedOperations(const GuestLocale::CollateFacet* facet) {
    const auto* vtable = reinterpret_cast<const GuestLocale::CollateVtable*>(facet->base.vtable);
    Require(vtable != nullptr, "constructed facet carries non-null vtable");

    Require(Compare(facet, "abc", "abd") == -1, "constructed compare orders abc < abd");
    Require(Compare(facet, "abd", "abc") == 1, "constructed compare orders abd > abc");
    Require(Compare(facet, "", "") == 0, "constructed compare treats empty ranges as equal");
    Require(Compare(facet, "", "a") == -1, "constructed compare orders empty before non-empty");
    const std::string nulLeft("a\0b", 3);
    const std::string nulRight("a\0c", 3);
    Require(Compare(facet, nulLeft, nulRight) == -1, "constructed compare handles embedded nulls");
    Require(vtable->compare(facet, nulLeft.data(), nulLeft.data() + nulLeft.size(), nulRight.data(), nulRight.data() + nulRight.size()) == -1, "vtable compare handles embedded nulls");

    Require(Hash(facet, "") == static_cast<std::int64_t>(0xcbf29ce484222325ULL), "constructed hash handles empty range");
    Require(Hash(facet, "abc") == -1792535898324117685LL, "constructed hash handles abc");
    Require(Hash(facet, nulLeft) != Hash(facet, nulRight), "constructed hash distinguishes embedded nulls");
    Require(vtable->hash(facet, nulLeft.data(), nulLeft.data() + nulLeft.size()) == Hash(facet, nulLeft), "vtable hash matches collate::hash");

    GuestLocale::String inlineResult{};
    Require(_ZNKSt7collateIcE9transformEPKcS2__nid_postfix(&inlineResult, facet, "hello", "hello" + 5) == &inlineResult, "transform returns result");
    Require(inlineResult.size == 5 && inlineResult.capacity == 15 && std::memcmp(inlineResult.buffer, "hello", 6) == 0, "constructed transform produces inline result");

    GuestLocale::String emptyResult{};
    Require(vtable->transform(&emptyResult, facet, nullptr, nullptr) == &emptyResult, "vtable transform handles null empty range");
    Require(emptyResult.size == 0 && emptyResult.capacity == 15 && emptyResult.buffer[0] == 0, "vtable transform writes empty string");

    const std::string longText(32, 'k');
    GuestLocale::String heapResult{};
    const long beforeTransform = liveAllocations;
    Require(_ZNKSt7collateIcE9transformEPKcS2__nid_postfix(&heapResult, facet, longText.data(), longText.data() + longText.size()) == &heapResult, "constructed heap transform returns result");
    Require(liveAllocations == beforeTransform + 1 && heapResult.size == 32 && std::memcmp(heapResult.pointer, longText.data(), 33) == 0, "constructed heap transform allocates and copies");
    _ZdlPv_nid_postfix(heapResult.pointer);
    Require(liveAllocations == beforeTransform, "heap transform result is freed");

    const long beforeNulTransform = liveAllocations;
    Reject([&] { _ZNKSt7collateIcE9transformEPKcS2__nid_postfix(&heapResult, facet, nulLeft.data(), nulLeft.data() + nulLeft.size()); }, "transform rejects embedded nulls on constructed facet");
    Require(liveAllocations == beforeNulTransform, "failed transform leaks no storage");
}

void CheckConstructorsAndLifetime() {
    GuestLocale::LocinfoStorage locinfo{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&locinfo, "C");
    GuestLocale::LocinfoStorage locinfoSnapshot = locinfo;

    const long beforeAll = liveAllocations;

    FacetStorage s1 = FilledStorage();
    _ZNSt7collateIcEC1Em_nid_postfix(FacetPtr(s1), 0);
    CheckConstructedOperations(FacetPtr(s1));
    CheckConstructedLayout(s1, 0);

    FacetStorage s2 = FilledStorage();
    _ZNSt7collateIcEC1EPKcm_nid_postfix(FacetPtr(s2), "C", 1);
    CheckConstructedOperations(FacetPtr(s2));
    CheckConstructedLayout(s2, 1);

    FacetStorage s3 = FilledStorage();
    constexpr std::uint64_t wideRefs = 0x100000007ULL;
    _ZNSt7collateIcEC1ERKSt8_Locinfom_nid_postfix(FacetPtr(s3), &locinfo, wideRefs);
    CheckConstructedOperations(FacetPtr(s3));
    CheckConstructedLayout(s3, wideRefs);
    Require(std::memcmp(&locinfo, &locinfoSnapshot, sizeof(locinfo)) == 0, "_Locinfo constructor does not mutate borrowed _Locinfo");

    FacetStorage s4 = FilledStorage();
    _ZNSt7collateIcEC2EPKcm_nid_postfix(FacetPtr(s4), "C", 2);
    CheckConstructedOperations(FacetPtr(s4));
    CheckConstructedLayout(s4, 2);

    FacetStorage s5 = FilledStorage();
    _ZNSt7collateIcEC2Em_nid_postfix(FacetPtr(s5), 3);
    CheckConstructedOperations(FacetPtr(s5));
    CheckConstructedLayout(s5, 3);

    FacetStorage s6 = FilledStorage();
    _ZNSt7collateIcEC2ERKSt8_Locinfom_nid_postfix(FacetPtr(s6), &locinfo, wideRefs);
    CheckConstructedOperations(FacetPtr(s6));
    CheckConstructedLayout(s6, wideRefs);
    Require(std::memcmp(&locinfo, &locinfoSnapshot, sizeof(locinfo)) == 0, "the base _Locinfo constructor does not mutate borrowed _Locinfo");

    auto* deleting = static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet)));
    _ZNSt7collateIcEC2Em_nid_postfix(deleting, 0);
    const long freesBeforeDeleting = totalFrees;
    _ZNSt7collateIcED0Ev_nid_postfix(deleting);
    Require(totalFrees == freesBeforeDeleting + 1 && liveAllocations == beforeAll, "the deleting destructor frees the facet");
    Reject([] { _ZNSt7collateIcED0Ev_nid_postfix(nullptr); }, "the deleting destructor rejects a null facet");

    const long freesBeforeDestroy = totalFrees;
    FacetPtr(s3)->base.vtable->destroy(&FacetPtr(s3)->base);
    Require(totalFrees == freesBeforeDestroy && liveAllocations == beforeAll, "destroy on stack facet frees nothing");
    Require(std::memcmp(&locinfo, &locinfoSnapshot, sizeof(locinfo)) == 0, "destroy does not touch borrowed _Locinfo");

    auto* heapZero = static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet)));
    _ZNSt7collateIcEC1Em_nid_postfix(heapZero, 0);
    heapZero->base.vtable->retain(&heapZero->base);
    Require(heapZero->base.references == 1, "retain increments refs=0 to 1");
    Require(heapZero->base.vtable->release(&heapZero->base) == &heapZero->base && heapZero->base.references == 0, "release returns facet at zero");
    const long freesBeforeHeapZero = totalFrees;
    heapZero->base.vtable->deleteObject(&heapZero->base);
    Require(totalFrees == freesBeforeHeapZero + 1 && lastFreed == heapZero && liveAllocations == beforeAll, "deleteObject frees only owned heap facet");

    auto* heapTwo = static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet)));
    _ZNSt7collateIcEC1ERKSt8_Locinfom_nid_postfix(heapTwo, &locinfo, 2);
    Require(heapTwo->base.vtable->release(&heapTwo->base) == nullptr && heapTwo->base.references == 1, "first release decrements refs=2 to 1 without deleting");
    Require(liveAllocations == beforeAll + 1, "facet stays alive while refcount > 0");
    Require(heapTwo->base.vtable->release(&heapTwo->base) == &heapTwo->base && heapTwo->base.references == 0, "second release returns owned facet");
    heapTwo->base.vtable->deleteObject(&heapTwo->base);
    Require(liveAllocations == beforeAll, "owned heap facet freed cleanly");
    Require(std::memcmp(&locinfo, &locinfoSnapshot, sizeof(locinfo)) == 0, "borrowed _Locinfo survives facet destruction");

    _ZNSt8_LocinfoD1Ev_nid_postfix(&locinfo);
}

void CheckErrorBranches() {
    const long before = liveAllocations;
    GuestLocale::LocinfoStorage locinfo{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&locinfo, "C");

    Reject([] { _ZNSt7collateIcEC1Em_nid_postfix(nullptr, 0); }, "C1(size_t) rejects null this");
    Reject([] { _ZNSt7collateIcEC1EPKcm_nid_postfix(nullptr, "C", 0); }, "C1(const char*, size_t) rejects null this");
    Reject([&] { _ZNSt7collateIcEC1ERKSt8_Locinfom_nid_postfix(nullptr, &locinfo, 0); }, "C1(const _Locinfo&, size_t) rejects null this");
    Reject([] { _ZNSt7collateIcEC2EPKcm_nid_postfix(nullptr, "C", 0); }, "C2(const char*, size_t) rejects null this");
    Reject([] { _ZNSt7collateIcEC2Em_nid_postfix(nullptr, 0); }, "C2(size_t) rejects null this");
    Reject([&] { _ZNSt7collateIcEC2ERKSt8_Locinfom_nid_postfix(nullptr, &locinfo, 0); }, "C2(const _Locinfo&, size_t) rejects null this");

    FacetStorage sentinel = FilledStorage();
    Reject([&] { _ZNSt7collateIcEC1ERKSt8_Locinfom_nid_postfix(FacetPtr(sentinel), nullptr, 1); }, "C1(const _Locinfo&, size_t) rejects null _Locinfo");
    Require(UntouchedStorage(sentinel), "null _Locinfo leaves facet storage untouched");

    const char* const invalidNames[] = {nullptr, "", "fr_FR", "POSIX", "en_US.UTF-8"};
    for (const char* badName : invalidNames) {
        FacetStorage s1 = FilledStorage();
        Reject([&] { _ZNSt7collateIcEC1EPKcm_nid_postfix(FacetPtr(s1), badName, 1); }, "C1(const char*, size_t) rejects unsupported locale name");
        Require(UntouchedStorage(s1), "C1 error leaves facet storage untouched");

        FacetStorage s2 = FilledStorage();
        Reject([&] { _ZNSt7collateIcEC2EPKcm_nid_postfix(FacetPtr(s2), badName, 1); }, "C2(const char*, size_t) rejects unsupported locale name");
        Require(UntouchedStorage(s2), "C2 error leaves facet storage untouched");
    }

    _ZNSt8_LocinfoD1Ev_nid_postfix(&locinfo);
    Require(liveAllocations == before, "error branches leak zero allocations");
}

void* LoadShared(const std::filesystem::path& path) {
#ifdef _WIN32
    return LoadLibraryW(path.c_str());
#else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
}

void* FindExport(void* library, const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library), name));
#else
    return dlsym(library, name);
#endif
}

void CloseShared(void* library) {
    if (library == nullptr) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
}

struct SharedLibraryGuard {
    void* handle = nullptr;
    ~SharedLibraryGuard() { CloseShared(handle); }
};

void CheckLibraryDispatch(const std::filesystem::path& libraryPath, bool patchedNids) {
    const long before = liveAllocations;
    SharedLibraryGuard library{LoadShared(libraryPath)};
    Require(library.handle != nullptr, "libSceLibcInternal loads via dlopen/LoadLibrary");

    using CtorSize = void (APS5_VABI*)(GuestLocale::CollateFacet*, std::size_t);
    using CtorName = void (APS5_VABI*)(GuestLocale::CollateFacet*, const char*, std::size_t);
    using CtorLocinfo = void (APS5_VABI*)(GuestLocale::CollateFacet*, const GuestLocale::LocinfoStorage*, std::size_t);
    using Destructor = void (APS5_VABI*)(void*);
    using CompareFn = int (APS5_VABI*)(const GuestLocale::CollateFacet*, const char*, const char*, const char*, const char*);
    using HashFn = std::int64_t (APS5_VABI*)(const GuestLocale::CollateFacet*, const char*, const char*);

    auto resolve = [&](const char* nid, const char* mangled) -> void* {
        if (patchedNids) return FindExport(library.handle, nid);
        if (void* mangledPtr = FindExport(library.handle, mangled)) return mangledPtr;
        return FindExport(library.handle, (std::string(nid) + "_nid_no_patch_cut").c_str());
    };
    auto resolveNidCut = [&](const char* nid) -> void* {
        return FindExport(library.handle, patchedNids ? nid : (std::string(nid) + "_nid_no_patch_cut").c_str());
    };

    auto ctorSize = reinterpret_cast<CtorSize>(resolve("Oo1r8jKGZQ4", "_ZNSt7collateIcEC1Em_nid_postfix"));
    auto ctorName1 = reinterpret_cast<CtorName>(resolve("splBMMcF3yk", "_ZNSt7collateIcEC1EPKcm_nid_postfix"));
    auto ctorLocinfo = reinterpret_cast<CtorLocinfo>(resolve("raLgIUi3xmk", "_ZNSt7collateIcEC1ERKSt8_Locinfom_nid_postfix"));
    auto ctorName2 = reinterpret_cast<CtorName>(resolve("VClCrMDyydE", "_ZNSt7collateIcEC2EPKcm_nid_postfix"));
    auto dtor1 = reinterpret_cast<Destructor>(resolveNidCut("8pXCeme0FC4"));
    auto dtor2 = reinterpret_cast<Destructor>(resolveNidCut("dP5zwQ2Yc8g"));
    auto compareFn = reinterpret_cast<CompareFn>(resolveNidCut("Q-8lQCGMj4g"));
    auto hashFn = reinterpret_cast<HashFn>(resolveNidCut("6vYXzFD-mrk"));

    Require(ctorSize && ctorName1 && ctorLocinfo && ctorName2 && dtor1 && dtor2 && compareFn && hashFn, "all collate constructors, destructors and member NIDs resolve");

    GuestLocale::LocinfoStorage locinfo{};
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&locinfo, "C");

    const std::array<GuestLocale::CollateFacet*, 4> facets{
        static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet))),
        static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet))),
        static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet))),
        static_cast<GuestLocale::CollateFacet*>(_Znwm_nid_postfix(sizeof(GuestLocale::CollateFacet))),
    };
    ctorSize(facets[0], 0);
    ctorName1(facets[1], "C", 1);
    ctorLocinfo(facets[2], &locinfo, 1);
    ctorName2(facets[3], "C", 1);

    const char left[] = "alpha";
    const char right[] = "beta";
    const std::int64_t expectedHash = hashFn(facets[0], left, left + 5);
    for (std::size_t index = 0; index < facets.size(); ++index) {
        GuestLocale::CollateFacet* facet = facets[index];
        Require(compareFn(facet, left, left + 5, right, right + 4) == -1, "NID compare works on constructed facet");
        Require(hashFn(facet, left, left + 5) == expectedHash, "NID hash matches across constructors");
        if (index == 0) facet->base.vtable->retain(&facet->base);
        dtor1(facet);
        dtor2(facet);
        Require(facet->base.vtable->release(&facet->base) == &facet->base, "release returns facet after destructors");
        facet->base.vtable->deleteObject(&facet->base);
    }

    _ZNSt8_LocinfoD1Ev_nid_postfix(&locinfo);
    Require(liveAllocations == before, "library dispatch frees all allocations");
}

void CheckLStrcoll() {
    static_assert(std::is_same_v<decltype(&_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix),
                                 int (APS5_VABI*)(const char*, const char*, const char*, const char*, const void*)>);
    const char empty[] = "";
    const char alpha[] = "alpha";
    const char beta[] = "beta";
    const char embeddedLeft[] = {'a', '\0', 'b'};
    const char embeddedRight[] = {'a', '\0', 'c'};
    const unsigned char highByte[] = {0x80};
    const char asciiByte[] = {'a'};
    Require(_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(empty, empty, empty, empty, nullptr) == 0, "_LStrcoll<char> empty ranges are equal");
    Require(_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(alpha, alpha + 5, alpha, alpha + 5, nullptr) == 0, "_LStrcoll<char> equal ranges return zero");
    Require(_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(alpha, alpha + 5, beta, beta + 4, nullptr) == -1, "_LStrcoll<char> orders alpha before beta");
    Require(_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(beta, beta + 4, alpha, alpha + 5, nullptr) == 1, "_LStrcoll<char> orders beta after alpha");
    Require(_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(alpha, alpha + 2, alpha, alpha + 3, &alpha) == -1, "_LStrcoll<char> orders shorter prefix before longer string");
    Require(_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(alpha, alpha + 3, alpha, alpha + 2, &alpha) == 1, "_LStrcoll<char> orders longer string after shorter prefix");
    Require(_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(embeddedLeft, embeddedLeft + 3, embeddedRight, embeddedRight + 3, nullptr) == -1, "_LStrcoll<char> compares past embedded nul bytes");
    Require(_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(reinterpret_cast<const char*>(highByte), reinterpret_cast<const char*>(highByte) + 1, asciiByte, asciiByte + 1, nullptr) == 1, "_LStrcoll<char> compares bytes unsigned");
    Reject([&] { (void)_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(nullptr, alpha, alpha, alpha + 5, nullptr); }, "_LStrcoll<char> rejects null first1");
    Reject([&] { (void)_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(alpha + 2, alpha, alpha, alpha + 5, nullptr); }, "_LStrcoll<char> rejects inverted first range");
    Reject([&] { (void)_ZSt9_LStrcollIcEiPKT_S2_S2_S2_PKSt8_Collvec_nid_postfix(alpha, alpha + 5, beta + 2, beta, nullptr); }, "_LStrcoll<char> rejects inverted second range");
}

void CheckLStrxfrm() {
    static_assert(std::is_same_v<decltype(&libSceLibcInternalInventory_252_nid_no_patch),
                                 std::size_t (APS5_VABI*)(char*, char*, const char*, const char*, const void*)>);
    static_assert(std::is_same_v<decltype(&libSceLibcInternalInventory_253_nid_no_patch),
                                 std::size_t (APS5_VABI*)(char16_t*, char16_t*, const char16_t*, const char16_t*, const void*)>);
    const char source[] = {'b', '\0', 'a', '\xff'};
    std::array<char, 6> output{'x', 'x', 'x', 'x', 'x', 'x'};
    Require(libSceLibcInternalInventory_252_nid_no_patch(output.data(), output.data() + output.size(), source, source + 4, nullptr) == 4, "_LStrxfrm<char> returns the source length");
    Require(std::memcmp(output.data(), source, 4) == 0 && output[4] == 'x' && output[5] == 'x', "_LStrxfrm<char> copies the source unchanged, embedded nul and high bytes included, and writes nothing more");
    output.fill('x');
    Require(libSceLibcInternalInventory_252_nid_no_patch(output.data(), output.data() + 4, source, source + 4, &output) == 4 && std::memcmp(output.data(), source, 4) == 0,
        "_LStrxfrm<char> fills an exact-size buffer");
    Require(output[4] == 'x', "_LStrxfrm<char> stays within an exact-size buffer");
    output.fill('x');
    Require(libSceLibcInternalInventory_252_nid_no_patch(output.data(), output.data() + 3, source, source + 4, nullptr) == 4, "_LStrxfrm<char> reports the length a short buffer needs");
    Require(output == std::array<char, 6>{'x', 'x', 'x', 'x', 'x', 'x'}, "_LStrxfrm<char> leaves a short buffer untouched");
    Require(libSceLibcInternalInventory_252_nid_no_patch(nullptr, nullptr, source, source + 4, nullptr) == 4, "_LStrxfrm<char> measures with an empty buffer");
    Require(libSceLibcInternalInventory_252_nid_no_patch(output.data(), output.data() + output.size(), source, source, nullptr) == 0, "_LStrxfrm<char> transforms an empty source to nothing");
    Require(libSceLibcInternalInventory_252_nid_no_patch(nullptr, nullptr, nullptr, nullptr, nullptr) == 0, "_LStrxfrm<char> accepts null empty ranges");
    Require(output == std::array<char, 6>{'x', 'x', 'x', 'x', 'x', 'x'}, "_LStrxfrm<char> writes nothing for an empty source");
    Reject([&] { (void)libSceLibcInternalInventory_252_nid_no_patch(nullptr, output.data(), source, source + 4, nullptr); }, "_LStrxfrm<char> rejects a null buffer start");
    Reject([&] { (void)libSceLibcInternalInventory_252_nid_no_patch(output.data() + 2, output.data(), source, source + 4, nullptr); }, "_LStrxfrm<char> rejects an inverted buffer");
    Reject([&] { (void)libSceLibcInternalInventory_252_nid_no_patch(output.data(), output.data() + 6, nullptr, source + 4, nullptr); }, "_LStrxfrm<char> rejects a null source start");
    Reject([&] { (void)libSceLibcInternalInventory_252_nid_no_patch(output.data(), output.data() + 6, source + 2, source, nullptr); }, "_LStrxfrm<char> rejects an inverted source");
    auto* const unmapped = reinterpret_cast<char*>(std::uintptr_t{0x10});
    Require(libSceLibcInternalInventory_252_nid_no_patch(output.data(), output.data() + 3, unmapped, unmapped + 4, nullptr) == 4, "_LStrxfrm<char> does not read a source that does not fit");
    Reject([&] { (void)libSceLibcInternalInventory_252_nid_no_patch(output.data(), output.data() + 6, unmapped, unmapped + 4, nullptr); }, "_LStrxfrm<char> rejects an unreadable source");
    Reject([&] { (void)libSceLibcInternalInventory_252_nid_no_patch(unmapped, unmapped + 6, source, source + 4, nullptr); }, "_LStrxfrm<char> rejects an unwritable buffer");

    const char16_t wideSource[] = {u'b', u'\0', u'a', u'\xffff'};
    std::array<char16_t, 6> wideOutput{u'x', u'x', u'x', u'x', u'x', u'x'};
    Require(libSceLibcInternalInventory_253_nid_no_patch(wideOutput.data(), wideOutput.data() + wideOutput.size(), wideSource, wideSource + 4, nullptr) == 4, "_LStrxfrm<wchar_t> returns the source length in characters");
    Require(std::memcmp(wideOutput.data(), wideSource, sizeof(wideSource)) == 0 && wideOutput[4] == u'x' && wideOutput[5] == u'x', "_LStrxfrm<wchar_t> copies the source unchanged and writes nothing more");
    wideOutput.fill(u'x');
    Require(libSceLibcInternalInventory_253_nid_no_patch(wideOutput.data(), wideOutput.data() + 4, wideSource, wideSource + 4, nullptr) == 4
            && std::memcmp(wideOutput.data(), wideSource, sizeof(wideSource)) == 0,
        "_LStrxfrm<wchar_t> fills an exact-size buffer");
    Require(wideOutput[4] == u'x', "_LStrxfrm<wchar_t> stays within an exact-size buffer");
    wideOutput.fill(u'x');
    Require(libSceLibcInternalInventory_253_nid_no_patch(wideOutput.data(), wideOutput.data() + 3, wideSource, wideSource + 4, nullptr) == 4, "_LStrxfrm<wchar_t> reports the length a short buffer needs");
    Require(wideOutput == std::array<char16_t, 6>{u'x', u'x', u'x', u'x', u'x', u'x'}, "_LStrxfrm<wchar_t> leaves a short buffer untouched");
    Require(libSceLibcInternalInventory_253_nid_no_patch(nullptr, nullptr, wideSource, wideSource + 4, nullptr) == 4, "_LStrxfrm<wchar_t> measures with an empty buffer");
    Require(libSceLibcInternalInventory_253_nid_no_patch(nullptr, nullptr, nullptr, nullptr, nullptr) == 0, "_LStrxfrm<wchar_t> accepts null empty ranges");
    Reject([&] { (void)libSceLibcInternalInventory_253_nid_no_patch(wideOutput.data() + 2, wideOutput.data(), wideSource, wideSource + 4, nullptr); }, "_LStrxfrm<wchar_t> rejects an inverted buffer");
    Reject([&] { (void)libSceLibcInternalInventory_253_nid_no_patch(wideOutput.data(), wideOutput.data() + 6, wideSource + 2, wideSource, nullptr); }, "_LStrxfrm<wchar_t> rejects an inverted source");
    auto* const wideUnmapped = reinterpret_cast<char16_t*>(std::uintptr_t{0x10});
    Require(libSceLibcInternalInventory_253_nid_no_patch(wideOutput.data(), wideOutput.data() + 3, wideUnmapped, wideUnmapped + 4, nullptr) == 4, "_LStrxfrm<wchar_t> does not read a source that does not fit");
    Reject([&] { (void)libSceLibcInternalInventory_253_nid_no_patch(wideOutput.data(), wideOutput.data() + 6, wideUnmapped, wideUnmapped + 4, nullptr); }, "_LStrxfrm<wchar_t> rejects an unreadable source");
    Reject([&] { (void)libSceLibcInternalInventory_253_nid_no_patch(wideUnmapped, wideUnmapped + 6, wideSource, wideSource + 4, nullptr); }, "_LStrxfrm<wchar_t> rejects an unwritable buffer");
}

}

int main(int argc, char** argv) {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    GuestLocale::Facet* facet = nullptr;
    const GuestLocale::Implementation* const* classic = &_ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(_ZNSt7collateIcE7_GetcatEPPKNSt6locale5facetEPKS1__nid_postfix(&facet, classic) == 1 && facet != nullptr, "the classic collate facet is built");
    CheckClassicFacet(reinterpret_cast<const GuestLocale::CollateFacet*>(facet));
    reinterpret_cast<const GuestLocale::CollateVtable*>(facet->vtable)->facet.deleteObject(facet);
    CheckDerivedFacet();
    CheckConstructorsAndLifetime();
    CheckErrorBranches();
    CheckLStrcoll();
    CheckLStrxfrm();
    if (argc >= 2) CheckLibraryDispatch(ArgumentPath(argv, 1), argc >= 3 && std::strcmp(argv[2], "patched") == 0);

    std::puts("libc internal collate<char> member and constructor checks passed");
    return 0;
}
