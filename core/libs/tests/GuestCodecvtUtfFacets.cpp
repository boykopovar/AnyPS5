#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/CodecvtUtf.hpp"
#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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
using Length = int (APS5_VABI*)(const Facet*, void*, const char*, const char*, std::size_t);

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

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "UTF codecvt facet check failed at line %u\n", location.line());
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

    facet->mode = ConsumeHeader;
    const char lenBytes[] = "\xef\xbb\xbf" "A\xf0\x9f\x98\x80Z";
    std::uint64_t lenState[2] = {};
    const int expectedSupp = sizeof(TElement) == 2 ? 7 : 8;
    Require(Slot<Length>(vtable, 12)(facet, lenState, lenBytes, lenBytes + 9, 2) == expectedSupp);
    if (sizeof(TElement) == 2) Require((lenState[0] & 0xffff) == 0xde00);
    else Require((lenState[0] & 0xff) == 1);

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
    RequireThrows<std::invalid_argument>([] { CodecvtUtf16Destroy_nid_no_patch(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZNSt7codecvtIDsc9_MbstatetED0Ev_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZNSt7codecvtIDic9_MbstatetED1Ev_nid_postfix(nullptr); });
    RequireThrows<std::invalid_argument>([] { _ZNSt7codecvtIDic9_MbstatetED0Ev_nid_postfix(nullptr); });
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

    const Facet utf16Facet{_ZTVSt7codecvtIDsc9_MbstatetE_nid_postfix + 2, 1, 0, 0x10ffffu, ConsumeHeader, 0};
    const Facet utf32Facet{_ZTVSt7codecvtIDic9_MbstatetE_nid_postfix + 2, 1, 0, 0xffffffffu, ConsumeHeader, 0};
    const char input[] = "\xef\xbb\xbf\xf0\x9f\x98\x80" "A";
    std::uint64_t state16[2] = {};
    Require(reinterpret_cast<Length>(&_ZNKSt7codecvtIDsc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix)(&utf16Facet, state16, input, input + 8, 1) == 6);
    Require((state16[0] & 0xffff) == 0xde00);
    Require(reinterpret_cast<Length>(&_ZNKSt7codecvtIDsc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix)(&utf16Facet, state16, input + 6, input + 8, 2) == 2);
    Require((state16[0] & 0xffff) == 1);

    std::uint64_t state32[2] = {};
    Require(reinterpret_cast<Length>(&sceLibcInternalPending3_019_nid_no_patch)(&utf32Facet, state32, input, input + 8, 1) == 7);
    Require((state32[0] & 0xff) == 1);
    Require(reinterpret_cast<Length>(&sceLibcInternalPending3_019_nid_no_patch)(&utf32Facet, state32, input + 7, input + 8, 1) == 1);
    RequireThrows<std::invalid_argument>([&] { reinterpret_cast<Length>(&_ZNKSt7codecvtIDsc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix)(nullptr, state16, input, input + 8, 1); });
    RequireThrows<std::invalid_argument>([&] { reinterpret_cast<Length>(&_ZNKSt7codecvtIDsc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix)(&utf16Facet, nullptr, input, input + 8, 1); });
    RequireThrows<std::invalid_argument>([&] { reinterpret_cast<Length>(&sceLibcInternalPending3_019_nid_no_patch)(nullptr, state32, input, input + 8, 1); });
    RequireThrows<std::invalid_argument>([&] { reinterpret_cast<Length>(&sceLibcInternalPending3_019_nid_no_patch)(&utf32Facet, nullptr, input, input + 8, 1); });

    Facet* utf16 = NewFacet(_ZTVSt7codecvtIDsc9_MbstatetE_nid_postfix, 0x10ffff, 0);
    Facet* utf32 = NewFacet(_ZTVSt7codecvtIDic9_MbstatetE_nid_postfix, 0xffffffff, 0);
    releasedCount = 0;
    libSceLibcInternalInventory_092_nid_no_patch(utf16);
    libSceLibcInternalInventory_090_nid_no_patch(utf32);
    Require(releasedCount == 2 && released[0] == utf16 && released[1] == utf32);
    releasedCount = 0;
    RequireThrows<std::invalid_argument>([] { libSceLibcInternalInventory_092_nid_no_patch(nullptr); });
    RequireThrows<std::invalid_argument>([] { libSceLibcInternalInventory_090_nid_no_patch(nullptr); });
    Require(releasedCount == 0);
}

void* LoadShared(const char* path) {
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
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

void CheckLibraryNids(const char* libcPath, const char* internalPath, bool patched) {
    SharedLibraryGuard libcLib{LoadShared(libcPath)};
    SharedLibraryGuard internalLib{LoadShared(internalPath)};
    Require(libcLib.handle != nullptr && internalLib.handle != nullptr);

    auto resolve = [&](void* library, const char* nid, const char* mangled) -> Length {
        void* symbol = nullptr;
        if (patched) {
            symbol = FindExport(library, nid);
        } else {
            symbol = FindExport(library, mangled);
            if (symbol == nullptr) symbol = FindExport(library, (std::string(nid) + "_nid_no_patch_cut").c_str());
        }
        Require(symbol != nullptr);
        return reinterpret_cast<Length>(symbol);
    };

    const Length libcLen16 = resolve(libcLib.handle, "lynApfiP6Lw", "CodecvtUtf16Length_nid_no_patch");
    const Length libcLen32 = resolve(libcLib.handle, "m6rjfL4aMcA", "_ZNKSt7codecvtIDic9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix");
    const Length internalLen16 = resolve(internalLib.handle, "lynApfiP6Lw", "_ZNKSt7codecvtIDsc9_MbstatetE9do_lengthERS0_PKcS4_m_nid_postfix");
    const Length internalLen32 = resolve(internalLib.handle, "m6rjfL4aMcA", "sceLibcInternalPending3_019_nid_no_patch");

    const Facet f16{_ZTVSt7codecvtIDsc9_MbstatetE_nid_postfix + 2, 1, 0, 0x10ffffu, ConsumeHeader, 0};
    const Facet f32{_ZTVSt7codecvtIDic9_MbstatetE_nid_postfix + 2, 1, 0, 0xffffffffu, ConsumeHeader, 0};
    const char text[] = "\xef\xbb\xbf" "A\xf0\x9f\x98\x80";
    for (Length fn : {libcLen16, internalLen16}) {
        std::uint64_t s[2] = {};
        Require(fn(&f16, s, text, text + 8, 2) == 7 && (s[0] & 0xffff) == 0xde00);
        Require(fn(&f16, s, text + 7, text + 8, 1) == 1 && (s[0] & 0xffff) == 1);
    }
    for (Length fn : {libcLen32, internalLen32}) {
        std::uint64_t s[2] = {};
        Require(fn(&f32, s, text, text + 8, 2) == 8 && (s[0] & 0xff) == 1);
    }
}

}

int main(int argc, char** argv) {
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
    if (argc >= 3) CheckLibraryNids(argv[1], argv[2], argc >= 4 && std::strcmp(argv[3], "patched") == 0);
    std::puts("UTF codecvt facet checks passed");
}
