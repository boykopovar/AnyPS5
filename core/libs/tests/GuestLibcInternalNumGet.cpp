#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal num_get: %s\n", message);
    std::exit(1);
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

#ifdef _WIN32
HMODULE g_library = nullptr;
#else
void* g_library = nullptr;
#endif

template<typename TFunction>
TFunction Export(const char* nid) {
    const std::string name = std::string(nid) + "_nid_no_patch_cut";
#ifdef _WIN32
    auto* address = reinterpret_cast<void*>(GetProcAddress(g_library, name.c_str()));
#else
    void* address = dlsym(g_library, name.c_str());
#endif
    Require(address != nullptr, "missing export");
    return reinterpret_cast<TFunction>(address);
}

void APS5_VABI TestFacetNop(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI TestFacetRelease(GuestLocale::Facet*) { return nullptr; }
const GuestLocale::FacetVtable g_testFacet{TestFacetNop, TestFacetNop, TestFacetNop, TestFacetRelease};

constexpr int kDerived = 100;
constexpr std::uint64_t kReturnedLow = 0x5a5a000000000000ull;
constexpr std::uint64_t kReturnedHigh = 0x00000000a5a5a502ull;

GuestLocale::InputIterator Iterator(std::uint64_t low, std::uint64_t high) {
    return GuestLocale::InputIterator{{low, high}};
}

bool Same(const GuestLocale::InputIterator& left, const GuestLocale::InputIterator& right) {
    return left.words[0] == right.words[0] && left.words[1] == right.words[1];
}

struct Call {
    int slot;
    int calls;
    const GuestLocale::Facet* self;
    GuestLocale::InputIterator first;
    GuestLocale::InputIterator last;
    GuestLocale::IosBase* stream;
    std::int32_t* state;
    void* value;
};

Call g_call{};

template<int TSlot, typename TValue>
GuestLocale::InputIterator APS5_VABI Record(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, TValue* value) {
    g_call = Call{TSlot, g_call.calls + 1, self, first, last, stream, state, value};
    *state |= 2;
    return Iterator(kReturnedLow + TSlot, kReturnedHigh);
}

template<int TBase>
GuestLocale::NumGetVtable RecordingVtable() {
    return GuestLocale::NumGetVtable{g_testFacet,
        Record<TBase + 0, bool>, Record<TBase + 1, std::uint16_t>, Record<TBase + 2, std::uint32_t>,
        Record<TBase + 3, std::int64_t>, Record<TBase + 4, std::uint64_t>, Record<TBase + 5, std::int64_t>,
        Record<TBase + 6, std::uint64_t>, Record<TBase + 7, float>, Record<TBase + 8, double>,
        Record<TBase + 9, long double>, Record<TBase + 10, void*>};
}

const GuestLocale::NumGetVtable g_baseVtable = RecordingVtable<0>();
const GuestLocale::NumGetVtable g_derivedVtable = RecordingVtable<kDerived>();

template<typename TValue>
using NumGetExport = GuestLocale::InputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::InputIterator, GuestLocale::InputIterator, GuestLocale::IosBase*, std::int32_t*, TValue*);

template<typename TValue>
void CheckGet(const char* nid, int slot) {
    const auto get = Export<NumGetExport<TValue>>(nid);
    for (const GuestLocale::NumGetVtable* vtable : {&g_baseVtable, &g_derivedVtable}) {
        const int expected = slot + (vtable == &g_derivedVtable ? kDerived : 0);
        GuestLocale::Facet facet{&vtable->facet, 1, 0};
        GuestLocale::IosBase stream{};
        std::int32_t state = 4;
        TValue value{};
        const GuestLocale::InputIterator first = Iterator(0x1111222233334444ull + expected, 0x0000000000004101ull);
        const GuestLocale::InputIterator last = Iterator(0x5555666677778888ull, 0x0000000000000001ull + expected);
        const int calls = g_call.calls;
        const GuestLocale::InputIterator result = get(&facet, first, last, &stream, &state, &value);
        Require(g_call.calls == calls + 1, "get calls do_get exactly once");
        Require(g_call.slot == expected, "get calls the do_get slot of its value type in the facet's own vtable");
        Require(g_call.self == &facet && g_call.stream == &stream && g_call.state == &state && g_call.value == &value, "get passes the facet, stream, state and value");
        Require(Same(g_call.first, first) && Same(g_call.last, last), "get passes both iterators unchanged");
        Require(Same(result, Iterator(kReturnedLow + expected, kReturnedHigh)), "get returns the iterator do_get returns");
        Require(state == 6, "get leaves the state to do_get");
    }
}


}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
    Require(argc == 2, "expected the libSceLibcInternal path");
#ifdef _WIN32
    g_library = LoadLibraryA(argv[1]);
#else
    g_library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
#endif
    Require(g_library != nullptr, "load libSceLibcInternal");
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckGet<bool>("OWO5cpNw3NA", 0);
    CheckGet<std::uint16_t>("gozsp4urvq8", 1);
    CheckGet<std::uint32_t>("N1VqUWz2OEI", 2);
    CheckGet<std::int64_t>("I2UzwkwwEPs", 3);
    CheckGet<std::uint64_t>("2bfL3yIBi5k", 4);
    CheckGet<std::int64_t>("4hiQK82QuLc", 5);
    CheckGet<std::uint64_t>("eZfFLyWCkvg", 6);
    CheckGet<float>("6RGkooTERsE", 7);
    CheckGet<double>("mAwXCpkWaYc", 8);
    CheckGet<long double>("wUCRGap1j0U", 9);
    CheckGet<void*>("my9ujasm6-0", 10);
    CheckGet<bool>("vW-nnV62ea4", 0);
    CheckGet<std::uint16_t>("bZ+lKHGvOr8", 1);
    CheckGet<std::uint32_t>("Y3hBU5FYmhM", 2);
    CheckGet<std::int64_t>("-m2YPwVCwJQ", 3);
    CheckGet<std::uint64_t>("94ZLp2+AOq0", 4);
    CheckGet<std::int64_t>("cG5hQhjFGog", 5);
    CheckGet<std::uint64_t>("banNSumaAZ0", 6);
    CheckGet<float>("2mb8FYgER+E", 7);
    CheckGet<double>("+hjXHfvy1Mg", 8);
    CheckGet<long double>("xLZr4GJRMLo", 9);
    CheckGet<void*>("zomvAQ5RFdA", 10);

    std::puts("libc internal num_get checks passed");
    return 0;
}
