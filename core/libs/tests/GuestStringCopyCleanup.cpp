#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "GuestStringCopyFault.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix;

static unsigned failures = 0;
static std::array<void*, 128> blocks{};
static std::size_t guestAllocations = 0;
static std::size_t completedMethods = 0;
static std::string inputText;

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void** Slot(void* pointer) {
    for (auto& block : blocks)
        if (block == pointer) return &block;
    return nullptr;
}

static void* Track(void* pointer) {
    if (!pointer) return nullptr;
    auto* slot = Slot(nullptr);
    if (!slot || Slot(pointer)) std::abort();
    *slot = pointer;
    ++guestAllocations;
    return pointer;
}

static void* APS5_VABI Allocate(std::size_t bytes) { return Track(std::malloc(bytes ? bytes : 1)); }
static void APS5_VABI Free(void* pointer) {
    if (pointer) {
        auto* slot = Slot(pointer);
        if (!slot) std::abort();
        *slot = nullptr;
    }
    std::free(pointer);
    if (GuestStringCopyFault::Released(pointer)) throw GuestStringCopyFault::ReleaseMarker{};
}
static void* APS5_VABI Calloc(std::size_t count, std::size_t bytes) { return Track(std::calloc(count ? count : 1, bytes ? bytes : 1)); }
static void* APS5_VABI Reallocate(void* pointer, std::size_t bytes) {
    if (!pointer) return Allocate(bytes);
    if (!bytes) { Free(pointer); return nullptr; }
    auto* slot = Slot(pointer);
    if (!slot) std::abort();
    void* result = std::realloc(pointer, bytes);
    if (result) *slot = result;
    return result;
}
static void* APS5_VABI Align(std::size_t, std::size_t) { std::abort(); }
static void* APS5_VABI Realign(void*, std::size_t, std::size_t) { std::abort(); }
static int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

static GuestLocale::String* MakeGuestStringResult(GuestLocale::String* result) {
    *result = {};
    result->size = inputText.size();
    result->capacity = 15;
    char* data = result->buffer;
    if (result->size > 15) {
        result->capacity = result->size | 15;
        const auto bytes = result->capacity + 1;
        auto* base = static_cast<unsigned char*>(_Znwm_nid_postfix(bytes < 0x1000 ? bytes : bytes + 0x27));
        data = reinterpret_cast<char*>(bytes < 0x1000 ? reinterpret_cast<std::uintptr_t>(base) :
               (reinterpret_cast<std::uintptr_t>(base) + 0x27) & ~std::uintptr_t{0x1f});
        if (bytes >= 0x1000) reinterpret_cast<void**>(data)[-1] = base;
        result->pointer = data;
    }
    std::memcpy(data, inputText.data(), inputText.size());
    data[inputText.size()] = 0;
    ++completedMethods;
    GuestStringCopyFault::Result(*result);
    return result;
}

static GuestLocale::String* APS5_VABI CategoryMessage(GuestLocale::String* result, const LibcException::CategoryObject*, int) { return MakeGuestStringResult(result); }
static GuestLocale::String* APS5_VABI PunctuationName(GuestLocale::String* result, const GuestLocale::NumpunctFacet*) { return MakeGuestStringResult(result); }

static void Observe(const char* helper, const std::array<void*, 128>& before, std::size_t length,
                    bool failCopy, bool releaseThrows, bool badAllocation, bool releaseMarker, bool unexpected) {
    GuestStringCopyFault::Disarm();
    Require(!unexpected, "copy test catches only the selected exception");
    Require(failCopy ? badAllocation && !releaseMarker : releaseMarker == releaseThrows && !badAllocation,
            "host bad_alloc remains primary and an ordinary throwing release is not retried");
    Require(completedMethods == 1 && GuestStringCopyFault::resultReady && GuestStringCopyFault::size == length,
            "guest method finishes constructing its result before host-copy injection");
    Require(guestAllocations == (length > 15 ? 1 : 0), "only the guest result allocates through the guest heap");
    Require(failCopy ? GuestStringCopyFault::failedHostBytes == length + 1 : GuestStringCopyFault::failedHostBytes == 0,
            "fault is consumed at the exact host string-copy allocation");
    Require(GuestStringCopyFault::releases == (length > 15 ? 1 : 0) && blocks == before,
            "converted guest string is released exactly once and guest pointer ledger is restored");
    if (length >= 0x1000) Require(GuestStringCopyFault::capacity + 1 >= 0x1000 &&
        GuestStringCopyFault::raw != GuestStringCopyFault::storage &&
        reinterpret_cast<std::uintptr_t>(GuestStringCopyFault::storage) % 32 == 0,
        "large guest string releases its original allocation base rather than the aligned data pointer");
    if (failCopy) std::printf("%s: result=%zu host-failure=%zu guest-allocations=%zu guest-releases=%zu aligned=%d\n",
        helper, length, GuestStringCopyFault::failedHostBytes.load(), guestAllocations,
        GuestStringCopyFault::releases, GuestStringCopyFault::raw != GuestStringCopyFault::storage);
    if (GuestStringCopyFault::raw && Slot(GuestStringCopyFault::raw)) Free(GuestStringCopyFault::raw);
    Require(blocks == before, "fixture drains any leak observed in the old-source negative control");
}

static void Begin(bool failCopy, bool releaseThrows) {
    guestAllocations = 0;
    completedMethods = 0;
    GuestStringCopyFault::Begin(failCopy, releaseThrows);
}

static void CheckCategoryCopies() {
    const void* vtable[4]{nullptr, nullptr, nullptr, reinterpret_cast<const void*>(CategoryMessage)};
    const LibcException::CategoryObject category{vtable};
    for (const bool sceV2 : {false, true}) {
        for (const std::size_t length : {std::size_t{48}, std::size_t{4200}}) {
            inputText.assign(length, 'c');
            for (const bool failCopy : {false, true}) {
#if !defined(__linux__)
                if (failCopy) continue;
#endif
                for (const bool releaseThrows : {false, true}) {
                    if (!failCopy && !releaseThrows) continue;
                    const auto before = blocks;
                    bool badAllocation = false, releaseMarker = false, unexpected = false;
                    Begin(failCopy, releaseThrows);
                    try {
                        if (sceV2) _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcNS_10error_codeE_nid_postfix("", {0, 7, &category});
                        else _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix("", {0, 7, &category});
                    } catch (const std::bad_alloc&) {
                        badAllocation = true;
                    } catch (const GuestStringCopyFault::ReleaseMarker&) {
                        releaseMarker = true;
                    } catch (...) {
                        unexpected = true;
                    }
                    Observe(sceV2 ? "sce_v2 category" : "category", before, length, failCopy, releaseThrows,
                            badAllocation, releaseMarker, unexpected);
                }
            }
        }
    }
}

static void APS5_VABI FacetNop(GuestLocale::Facet*) {}
static GuestLocale::Facet* APS5_VABI FacetRelease(GuestLocale::Facet*) { return nullptr; }
static char APS5_VABI Separator(const GuestLocale::NumpunctFacet*) { return ','; }
static int APS5_VABI Overflow(GuestLocale::Streambuf*, int) { return -1; }

static void CheckNumpunctCopies() {
    GuestLocale::Facet* facet = nullptr;
    Require(_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&facet, nullptr) == 4,
            "numeric output facet is available");
    const auto* put = reinterpret_cast<const GuestLocale::NumPutVtable*>(facet->vtable);
    const GuestLocale::NumpunctVtable punctuation{{FacetNop, FacetNop, FacetNop, FacetRelease},
        Separator, Separator, PunctuationName, PunctuationName, PunctuationName};
    GuestLocale::Facet punct{&punctuation.facet, 1, 0};
    _ZNSt8numpunctIcE2idE_nid_postfix = 1;
    GuestLocale::Facet* facets[2]{nullptr, &punct};
    GuestLocale::Implementation custom{{nullptr, 1, 0}, facets, 2, 0, false, "C"};
    GuestLocale::Implementation* customPointer = &custom;
    const GuestLocale::StreambufVtable streamVtable{{}, Overflow};
    for (const bool value : {false, true}) {
        for (const std::size_t length : {std::size_t{5}, std::size_t{48}, std::size_t{4200}}) {
            inputText.assign(length, value ? 'y' : 'n');
            for (const bool failCopy : {false, true}) {
#if !defined(__linux__)
                if (failCopy) continue;
#endif
                if (failCopy && length <= 15) continue;
                for (const bool releaseThrows : {false, true}) {
                    if (releaseThrows && length <= 15) continue;
                    const auto before = blocks;
                    char storage[8192]{};
                    char* next = storage;
                    char* end = storage + sizeof(storage);
                    GuestLocale::Streambuf stream{&streamVtable, {}, &next, {}, &end};
                    GuestLocale::IosBase ios{};
                    ios.flags = 0x8000;
                    ios.precision = 6;
                    ios.locale = &customPointer;
                    GuestLocale::OutputIterator out{};
                    bool badAllocation = false, releaseMarker = false, unexpected = false;
                    Begin(failCopy, releaseThrows);
                    try {
                        out = put->putBool(facet, {false, &stream}, &ios, '*', value);
                    } catch (const std::bad_alloc&) {
                        badAllocation = true;
                    } catch (const GuestStringCopyFault::ReleaseMarker&) {
                        releaseMarker = true;
                    } catch (...) {
                        unexpected = true;
                    }
                    Observe("numpunct", before, length, failCopy, releaseThrows, badAllocation, releaseMarker, unexpected);
                    if (!failCopy && !releaseThrows) Require(!out.failed && out.buffer == &stream &&
                        static_cast<std::size_t>(next - storage) == length && std::memcmp(storage, inputText.data(), length) == 0,
                        "inline, heap and aligned long names preserve normal boolalpha output bytes");
                    else Require(next == storage, "copy or release failure publishes no formatted output");
                }
            }
        }
    }
    facet->vtable->deleteObject(facet);
}

int main() {
    const std::array<void*, 10> heap{
        reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign),
        reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    CheckCategoryCopies();
    CheckNumpunctCopies();
    Require(blocks == std::array<void*, 128>{}, "all fixture guest allocations are released");
    if (failures) return 1;
    std::puts("guest string host-copy cleanup: PASS");
    return 0;
}
