#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal locale objects: %s\n", message);
    std::abort();
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
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

void CheckLocinfo() {
    alignas(16) std::array<std::byte, 80> bytes;
    bytes.fill(std::byte{0x5a});
    void* storage = bytes.data() + 8;
    sceLibcInternalLocinfoBaseConstructor_nid_no_patch(storage, "C");
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        Require(bytes[index] == (index >= 8 && index < 72 ? std::byte{} : std::byte{0x5a}), "the C2 constructor clears only the 64-byte object");
    }
    sceLibcInternalLocinfoBaseDestructor_nid_no_patch(storage);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        Require(bytes[index] == (index >= 8 && index < 72 ? std::byte{} : std::byte{0x5a}), "the D2 destructor leaves the storage untouched");
    }

    Require(Throws([&] { sceLibcInternalLocinfoBaseConstructor_nid_no_patch(storage, "unknown-locale"); }), "an unsupported locale name is rejected");
    Require(Throws([] { sceLibcInternalLocinfoBaseConstructor_nid_no_patch(nullptr, "C"); }), "a null object is rejected");
    Require(Throws([] { sceLibcInternalLocinfoBaseDestructor_nid_no_patch(nullptr); }), "destroying a null object is rejected");
}

void CheckLocimpCopy() {
    std::array<GuestLocale::Facet, 2> facets{};
    std::array<GuestLocale::Facet*, 3> slots{&facets[0], nullptr, &facets[1]};
    GuestLocale::Implementation source{};
    source.base.references = 7;
    source.facets = slots.data();
    source.facetCount = slots.size();
    source.category = 0x3f;
    source.transparent = true;
    source.name = "C";

    GuestLocale::Implementation expected{};
    GuestLocale::Implementation actual{};
    _ZNSt6locale7_LocimpC1ERKS0__nid_postfix(&expected, &source);
    sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(&actual, &source);

    Require(actual.base.vtable == expected.base.vtable && actual.base.vtable != nullptr, "the copy gets the same vtable as the C1 copy");
    Require(actual.base.references == 1, "the copy starts with one reference");
    Require(actual.facetCount == source.facetCount && actual.category == source.category, "the copy keeps the facet count and categories");
    Require(actual.transparent == source.transparent && actual.name == source.name, "the copy keeps the name and transparency");
    Require(actual.facets != nullptr && actual.facets != source.facets, "the copy owns a new facet vector");
    Require(std::memcmp(actual.facets, slots.data(), sizeof(slots)) == 0, "the copy shares the source facets");
    Require(source.base.references == 7 && source.facets == slots.data(), "the source is unchanged");

    GuestLocale::Implementation empty{};
    empty.name = "C";
    GuestLocale::Implementation emptyCopy{};
    sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(&emptyCopy, &empty);
    Require(emptyCopy.facetCount == 0 && emptyCopy.facets == nullptr, "a locale without facets copies no vector");

    Require(Throws([&] { sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(nullptr, &source); }), "a null object is rejected");
    Require(Throws([&] { sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(&emptyCopy, nullptr); }), "a null source is rejected");
}


GuestLocale::Facet* deletedFacet = nullptr;
int deleteCalls = 0;

void APS5_VABI CountingDestroy(GuestLocale::Facet*) {}
void APS5_VABI CountingDelete(GuestLocale::Facet* self) {
    deletedFacet = self;
    ++deleteCalls;
}
void APS5_VABI CountingRetain(GuestLocale::Facet* self) { ++self->references; }
GuestLocale::Facet* APS5_VABI CountingRelease(GuestLocale::Facet* self) { return --self->references == 0 ? self : nullptr; }

const GuestLocale::FacetVtable countingVtable{CountingDestroy, CountingDelete, CountingRetain, CountingRelease};

void CheckDeletegloballocale() {
    GuestLocale::Implementation locale{};
    locale.base.vtable = &countingVtable;
    locale.base.references = 2;
    GuestLocale::Implementation* reference = &locale;

    _Deletegloballocale_nid_postfix(&reference);
    Require(locale.base.references == 1 && deleteCalls == 0, "a shared locale loses one reference and is kept");
    Require(reference == &locale, "the reference is left as it was");

    _Deletegloballocale_nid_postfix(&reference);
    Require(locale.base.references == 0 && deleteCalls == 1 && deletedFacet == &locale.base, "the last reference deletes the locale");

    GuestLocale::Implementation* none = nullptr;
    _Deletegloballocale_nid_postfix(&none);
    Require(deleteCalls == 1, "a null locale reference is ignored");

    Require(Throws([] { _Deletegloballocale_nid_postfix(nullptr); }), "a null pointer to the reference is rejected");
}

void CheckGlobalLocale() {
    Require(_Getgloballocale_nid_postfix() == nullptr, "the global locale starts null");

    GuestLocale::Implementation first{};
    first.base.vtable = &countingVtable;
    first.base.references = 3;
    GuestLocale::Implementation second{};
    second.base.vtable = &countingVtable;
    second.base.references = 1;

    _Setgloballocale_nid_postfix(&first);
    Require(_Getgloballocale_nid_postfix() == &first, "setting the global locale stores the pointer");
    Require(first.base.references == 3, "setting the global locale does not alter reference counts");

    _Setgloballocale_nid_postfix(&second);
    Require(_Getgloballocale_nid_postfix() == &second, "replacing the global locale updates the stored pointer");
    Require(first.base.references == 3 && second.base.references == 1, "replacing does not release or retain either locale");

    deletedFacet = nullptr;
    deleteCalls = 0;
    auto* stored = static_cast<GuestLocale::Implementation*>(_Getgloballocale_nid_postfix());
    _Deletegloballocale_nid_postfix(&stored);
    Require(second.base.references == 0 && deleteCalls == 1 && deletedFacet == &second.base, "the stored pointer works with _Deletegloballocale");

    _Setgloballocale_nid_postfix(nullptr);
    Require(_Getgloballocale_nid_postfix() == nullptr, "clearing the global locale stores null");
}

void CheckGuardedGuestPointers() {
    struct GuardedPages {
        unsigned char* base = nullptr;
        std::size_t pageSize = 4096;

        GuardedPages() {
#ifdef _WIN32
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            pageSize = info.dwPageSize ? static_cast<std::size_t>(info.dwPageSize) : 4096;
            base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            Require(base != nullptr, "allocate guarded pages");
            DWORD oldProtect = 0;
            Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &oldProtect) != 0, "protect read-only page");
            Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &oldProtect) != 0, "protect guard page");
#else
            const long size = sysconf(_SC_PAGESIZE);
            pageSize = size > 0 ? static_cast<std::size_t>(size) : 4096;
            void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            Require(mapped != MAP_FAILED, "allocate guarded pages");
            base = static_cast<unsigned char*>(mapped);
            Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0, "protect read-only page");
            Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0, "protect guard page");
#endif
        }

        ~GuardedPages() {
            if (!base) return;
#ifdef _WIN32
            VirtualFree(base, 0, MEM_RELEASE);
#else
            munmap(base, pageSize * 3);
#endif
        }

        unsigned char* ReadOnly() const { return base + pageSize; }
        unsigned char* Guard() const { return base + pageSize * 2; }
    };

    GuardedPages pages;
    GuestLocale::Implementation validImpl{};
    _ZNSt6locale7_LocimpC2Eb_nid_postfix(&validImpl, false);
    GuestLocaleFacade validFacade{&validImpl};
    GuestLocaleFacade outFacade{};

    errno = 53;
    Require(Throws([&] { _ZNSt6locale7_LocimpC2Eb_nid_postfix(reinterpret_cast<GuestLocale::Implementation*>(pages.Guard()), false); }),
        "_LocimpC2Eb rejects guard page");
    Require(Throws([&] { _ZNSt6locale7_LocimpC2Eb_nid_postfix(reinterpret_cast<GuestLocale::Implementation*>(pages.ReadOnly()), false); }),
        "_LocimpC2Eb rejects read-only page");

    Require(Throws([&] { _ZNSt6locale6globalERKS__nid_postfix(reinterpret_cast<GuestLocaleFacade*>(pages.Guard()), &validFacade); }),
        "locale::global rejects guard result");
    Require(Throws([&] { _ZNSt6locale6globalERKS__nid_postfix(reinterpret_cast<GuestLocaleFacade*>(pages.ReadOnly()), &validFacade); }),
        "locale::global rejects read-only result");
    Require(Throws([&] { _ZNSt6locale6globalERKS__nid_postfix(&outFacade, reinterpret_cast<const GuestLocaleFacade*>(pages.Guard())); }),
        "locale::global rejects guard locale");
    GuestLocaleFacade guardImplFacade{reinterpret_cast<GuestLocale::Implementation*>(pages.Guard())};
    Require(Throws([&] { _ZNSt6locale6globalERKS__nid_postfix(&outFacade, &guardImplFacade); }),
        "locale::global rejects guard locale implementation");

    Require(Throws([&] { _Deletegloballocale_nid_postfix(pages.Guard()); }),
        "_Deletegloballocale rejects guard reference");

    auto* guardLocale = reinterpret_cast<GuestLocale::Implementation*>(pages.Guard());
    Require(Throws([&] { _Deletegloballocale_nid_postfix(&guardLocale); }),
        "_Deletegloballocale rejects guard locale object");

    GuestLocale::Implementation badVtableLocale{};
    badVtableLocale.base.vtable = reinterpret_cast<const GuestLocale::FacetVtable*>(pages.Guard());
    GuestLocale::Implementation* badVtableRef = &badVtableLocale;
    Require(Throws([&] { _Deletegloballocale_nid_postfix(&badVtableRef); }),
        "_Deletegloballocale rejects guard vtable");

    GuestLocale::FacetVtable nonExecVtable{CountingDestroy, CountingDelete, CountingRetain,
        reinterpret_cast<GuestLocale::Facet* (APS5_VABI*)(GuestLocale::Facet*)>(pages.ReadOnly())};
    GuestLocale::Implementation nonExecLocale{};
    nonExecLocale.base.vtable = &nonExecVtable;
    GuestLocale::Implementation* nonExecRef = &nonExecLocale;
    Require(Throws([&] { _Deletegloballocale_nid_postfix(&nonExecRef); }),
        "_Deletegloballocale rejects non-executable release callback");

    GuestLocale::FacetVtable nonExecDeleteVtable{CountingDestroy,
        reinterpret_cast<void (APS5_VABI*)(GuestLocale::Facet*)>(pages.ReadOnly()), CountingRetain, CountingRelease};
    GuestLocale::Implementation nonExecDeleteLocale{};
    nonExecDeleteLocale.base.vtable = &nonExecDeleteVtable;
    nonExecDeleteLocale.base.references = 1;
    GuestLocale::Implementation* nonExecDeleteRef = &nonExecDeleteLocale;
    Require(Throws([&] { _Deletegloballocale_nid_postfix(&nonExecDeleteRef); }),
        "_Deletegloballocale rejects non-executable delete callback");

    Require(errno == 53, "guarded pointer checks preserve errno");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckGuardedGuestPointers();
    CheckLocinfo();
    CheckLocimpCopy();
    CheckDeletegloballocale();
    CheckGlobalLocale();

    std::puts("libc internal _Locinfo, _Locimp base-object and global locale checks passed");
    return 0;
}
