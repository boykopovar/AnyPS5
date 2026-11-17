#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/CodecvtUtf.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "Testing.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

constexpr int Ok = 0;
constexpr int Error = 2;
constexpr std::uint32_t GenerateHeader = 2;
constexpr std::uint32_t ConsumeHeader = 4;

struct Facet {
    const std::uintptr_t* vtable;
    std::uint32_t references;
    std::uint32_t reserved;
    std::uint64_t maxcode;
    std::uint32_t mode;
    std::uint32_t padding;
};

static_assert(offsetof(Facet, maxcode) == 0x10);
static_assert(offsetof(Facet, mode) == 0x18);
static_assert(sizeof(_ZTVSt7codecvtIDsc9_MbstatetE_nid_postfix) == 104);
static_assert(sizeof(_ZTVSt7codecvtIDic9_MbstatetE_nid_postfix) == 104);
static_assert(sizeof(_ZNSt7codecvtIDsc9_MbstatetE2idE_nid_postfix) == 8);
static_assert(sizeof(_ZNSt7codecvtIDic9_MbstatetE2idE_nid_postfix) == 8);

using Destructor = void (APS5_VABI*)(Facet*);
using Retain = void (APS5_VABI*)(Facet*);
using Release = Facet* (APS5_VABI*)(Facet*);
using Query = int (APS5_VABI*)(const Facet*);
using Noconv = bool (APS5_VABI*)(const Facet*);
using Unshift = int (APS5_VABI*)(const Facet*, void*, char*, char*, char**);
template<typename TElement>
using In = int (APS5_VABI*)(const Facet*, void*, const char*, const char*, const char**, TElement*, TElement*, TElement**);
template<typename TElement>
using Out = int (APS5_VABI*)(const Facet*, void*, const TElement*, const TElement*, const TElement**, char*, char*, char**);

std::array<void*, 8> released{};
std::size_t releasedCount = 0;

void* APS5_VABI HeapAllocate(std::size_t bytes) { return std::malloc(bytes); }
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

template<typename TFunction>
TFunction Slot(const std::uintptr_t* vtable, std::size_t index) {
    return reinterpret_cast<TFunction>(vtable[index]);
}

Facet* NewFacet(const std::uintptr_t* vtable, std::uint64_t maxcode, std::uint32_t mode) {
    auto* facet = static_cast<Facet*>(_Znwm_nid_postfix(sizeof(Facet)));
    *facet = {vtable + 2, 1, 0, maxcode, mode, 0};
    return facet;
}

template<typename TElement>
void CheckVtable(const std::uintptr_t* vtable, std::uint64_t maxcode, const std::array<int, 8>& maxLengths, const std::array<int, 8>& encodings, int unshiftWithLowSurrogate) {
    Require(vtable[0] == 0 && vtable[1] == 0);
    for (std::size_t index = 2; index < kCodecvtUtfVtableSlots; ++index) Require(vtable[index] != 0);

    Facet* facet = NewFacet(vtable, maxcode, 0);
    Slot<Retain>(vtable, 4)(facet);
    Require(facet->references == 2);
    Require(Slot<Release>(vtable, 5)(facet) == nullptr && facet->references == 1);
    Require(Slot<Release>(vtable, 5)(facet) == facet && facet->references == 0);

    Require(!Slot<Noconv>(vtable, 6)(facet));
    for (std::uint32_t bits = 0; bits < 8; ++bits) {
        facet->mode = bits;
        Require(Slot<Query>(vtable, 7)(facet) == maxLengths[bits]);
        Require(Slot<Query>(vtable, 8)(facet) == encodings[bits]);
    }

    facet->mode = ConsumeHeader;
    const char bytes[] = "\xef\xbb\xbf" "A\xc3\xa9";
    std::uint64_t state[2] = {};
    std::array<TElement, 4> wide{};
    const char* mid1 = nullptr;
    TElement* mid2 = nullptr;
    Require(Slot<In<TElement>>(vtable, 9)(facet, state, bytes, bytes + 6, &mid1, wide.data(), wide.data() + wide.size(), &mid2) == Ok);
    Require(mid1 == bytes + 6 && mid2 == wide.data() + 2 && wide[0] == u'A' && wide[1] == 0xe9);

    facet->mode = GenerateHeader;
    std::uint64_t outState[2] = {};
    std::array<char, 8> narrow{};
    const TElement* outMid1 = nullptr;
    char* outMid2 = nullptr;
    Require(Slot<Out<TElement>>(vtable, 10)(facet, outState, wide.data(), wide.data() + 2, &outMid1, narrow.data(), narrow.data() + narrow.size(), &outMid2) == Ok);
    Require(outMid1 == wide.data() + 2 && outMid2 == narrow.data() + 6 && std::memcmp(narrow.data(), bytes, 6) == 0);

    std::uint64_t shift[2] = {0xdc05, 0};
    char* next = nullptr;
    Require(Slot<Unshift>(vtable, 11)(facet, shift, narrow.data(), narrow.data() + narrow.size(), &next) == unshiftWithLowSurrogate && next == narrow.data());

    releasedCount = 0;
    Slot<Destructor>(vtable, 2)(facet);
    Require(releasedCount == 0);
    Slot<Destructor>(vtable, 3)(facet);
    Require(releasedCount == 1 && released[0] == facet);
}

void CheckVtables() {
    CheckVtable<char16_t>(_ZTVSt7codecvtIDsc9_MbstatetE_nid_postfix, 0x10ffff, {6, 6, 7, 7, 9, 9, 9, 9}, {0, 0, 0, 0, 0, 0, 0, 0}, Error);
    CheckVtable<char32_t>(_ZTVSt7codecvtIDic9_MbstatetE_nid_postfix, 0xffffffff, {6, 6, 9, 9, 9, 9, 9, 9}, {0, 0, -1, -1, -1, -1, -1, -1}, Ok);
}

void CheckIds() {
    Require(_ZNSt7codecvtIDsc9_MbstatetE2idE_nid_postfix == 0);
    Require(_ZNSt7codecvtIDic9_MbstatetE2idE_nid_postfix == 0);
}

void CheckDestructorExports() {
    Facet* utf16 = NewFacet(_ZTVSt7codecvtIDsc9_MbstatetE_nid_postfix, 0x10ffff, 0);
    Facet* utf32 = NewFacet(_ZTVSt7codecvtIDic9_MbstatetE_nid_postfix, 0xffffffff, 0);
    releasedCount = 0;
    CodecvtUtf16Destroy_nid_no_patch(utf16);
    _ZNSt7codecvtIDic9_MbstatetED1Ev_nid_postfix(utf32);
    Require(releasedCount == 0);
    _ZNSt7codecvtIDsc9_MbstatetED0Ev_nid_postfix(utf16);
    _ZNSt7codecvtIDic9_MbstatetED0Ev_nid_postfix(utf32);
    Require(releasedCount == 2 && released[0] == utf16 && released[1] == utf32);

    releasedCount = 0;
    ExpectErr<std::invalid_argument>([] { CodecvtUtf16Destroy_nid_no_patch(nullptr); });
    ExpectErr<std::invalid_argument>([] { _ZNSt7codecvtIDsc9_MbstatetED0Ev_nid_postfix(nullptr); });
    ExpectErr<std::invalid_argument>([] { _ZNSt7codecvtIDic9_MbstatetED1Ev_nid_postfix(nullptr); });
    ExpectErr<std::invalid_argument>([] { _ZNSt7codecvtIDic9_MbstatetED0Ev_nid_postfix(nullptr); });
    Require(releasedCount == 0);
}

void CheckQueryExports() {
    Facet facet{_ZTVSt7codecvtIDsc9_MbstatetE_nid_postfix + 2, 1, 0, 0x10ffff, GenerateHeader | ConsumeHeader, 0};
    Require(!_ZNKSt7codecvtIDsc9_MbstatetE16do_always_noconvEv_nid_postfix(&facet));
    Require(!_ZNKSt7codecvtIDic9_MbstatetE16do_always_noconvEv_nid_postfix(&facet));
    Require(_ZNKSt7codecvtIDsc9_MbstatetE11do_encodingEv_nid_postfix(&facet) == 0);
    Require(CodecvtUtf32Encoding_nid_no_patch(&facet) == -1);
    Require(CodecvtUtf16MaxLength_nid_no_patch(&facet) == 9);
    Require(CodecvtUtf32MaxLength_nid_no_patch(&facet) == 9);
}

template<typename TElement, typename TIn, typename TOut>
void CheckForwarders(const std::uintptr_t* vtable, TIn in, TOut out) {
    const Facet facet{vtable + 2, 1, 0, sizeof(TElement) == 2 ? 0x10ffffu : 0xffffffffu, 0, 0};
    const char bytes[] = "\xe2\x82\xac";
    std::uint64_t state[2] = {};
    std::array<TElement, 2> wide{};
    const char* mid1 = nullptr;
    TElement* mid2 = nullptr;
    Require(in(&facet, state, bytes, bytes + 3, &mid1, wide.data(), wide.data() + wide.size(), &mid2) == Ok);
    Require(mid1 == bytes + 3 && mid2 == wide.data() + 1 && wide[0] == 0x20ac);
    std::uint64_t outState[2] = {};
    std::array<char, 4> narrow{};
    const TElement* outMid1 = nullptr;
    char* outMid2 = nullptr;
    Require(out(&facet, outState, wide.data(), wide.data() + 1, &outMid1, narrow.data(), narrow.data() + narrow.size(), &outMid2) == Ok);
    Require(outMid1 == wide.data() + 1 && outMid2 == narrow.data() + 3 && std::memcmp(narrow.data(), bytes, 3) == 0);
}

void CheckLibcInternalForwarders() {
    CheckForwarders<char16_t>(_ZTVSt7codecvtIDsc9_MbstatetE_nid_postfix, sceLibcInternalPending3_024_nid_no_patch, sceLibcInternalPending3_025_nid_no_patch);
    CheckForwarders<char32_t>(_ZTVSt7codecvtIDic9_MbstatetE_nid_postfix, sceLibcInternalPending3_017_nid_no_patch, sceLibcInternalPending3_018_nid_no_patch);
    Facet* utf16 = NewFacet(_ZTVSt7codecvtIDsc9_MbstatetE_nid_postfix, 0x10ffff, 0);
    Facet* utf32 = NewFacet(_ZTVSt7codecvtIDic9_MbstatetE_nid_postfix, 0xffffffff, 0);
    releasedCount = 0;
    libSceLibcInternalInventory_092_nid_no_patch(utf16);
    libSceLibcInternalInventory_090_nid_no_patch(utf32);
    Require(releasedCount == 2 && released[0] == utf16 && released[1] == utf32);
    releasedCount = 0;
    ExpectErr<std::invalid_argument>([] { libSceLibcInternalInventory_092_nid_no_patch(nullptr); });
    ExpectErr<std::invalid_argument>([] { libSceLibcInternalInventory_090_nid_no_patch(nullptr); });
    Require(releasedCount == 0);
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
    CheckVtables();
    CheckIds();
    CheckDestructorExports();
    CheckQueryExports();
    CheckLibcInternalForwarders();
    std::puts("UTF codecvt facet checks passed");
}
