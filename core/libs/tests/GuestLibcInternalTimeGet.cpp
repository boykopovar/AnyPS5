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
    std::fprintf(stderr, "libc internal time_get: %s\n", message);
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
    GuestTm* time;
    char format;
    char modifier;
};

Call g_call{};

template<int TBase>
std::int32_t APS5_VABI RecordDateOrder(const GuestLocale::Facet* self) {
    g_call = Call{TBase, g_call.calls + 1, self, {}, {}, nullptr, nullptr, nullptr, 0, 0};
    return TBase == 0 ? 2 : 3;
}

template<int TSlot>
GuestLocale::InputIterator APS5_VABI Record(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    g_call = Call{TSlot, g_call.calls + 1, self, first, last, stream, state, time, 0, 0};
    *state |= 2;
    return Iterator(kReturnedLow + TSlot, kReturnedHigh);
}

template<int TSlot>
GuestLocale::InputIterator APS5_VABI RecordGet(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, char format, char modifier) {
    g_call = Call{TSlot, g_call.calls + 1, self, first, last, stream, state, time, format, modifier};
    *state |= 2;
    return Iterator(kReturnedLow + TSlot, kReturnedHigh);
}

template<int TBase>
GuestLocale::TimeGetVtable RecordingVtable() {
    return GuestLocale::TimeGetVtable{g_testFacet, RecordDateOrder<TBase>, Record<TBase + 1>, Record<TBase + 2>, Record<TBase + 3>, Record<TBase + 4>, Record<TBase + 5>, RecordGet<TBase + 6>};
}

const GuestLocale::TimeGetVtable g_baseVtable = RecordingVtable<0>();
const GuestLocale::TimeGetVtable g_derivedVtable = RecordingVtable<kDerived>();

using DateOrderExport = std::int32_t (APS5_VABI*)(const GuestLocale::Facet*);
using TimeGetExport = GuestLocale::InputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::InputIterator, GuestLocale::InputIterator, GuestLocale::IosBase*, std::int32_t*, GuestTm*);
using TimeGetPatternExport = GuestLocale::InputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::InputIterator, GuestLocale::InputIterator, GuestLocale::IosBase*, std::int32_t*, GuestTm*, char, char);

alignas(8) unsigned char g_time[64];

void CheckDateOrder(const char* nid) {
    const auto dateOrder = Export<DateOrderExport>(nid);
    for (const GuestLocale::TimeGetVtable* vtable : {&g_baseVtable, &g_derivedVtable}) {
        const bool derived = vtable == &g_derivedVtable;
        GuestLocale::Facet facet{&vtable->facet, 1, 0};
        const int calls = g_call.calls;
        const std::int32_t order = dateOrder(&facet);
        Require(g_call.calls == calls + 1 && g_call.slot == (derived ? kDerived : 0), "date_order calls do_date_order of the facet's own vtable once");
        Require(g_call.self == &facet, "date_order passes the facet");
        Require(order == (derived ? 3 : 2), "date_order returns what do_date_order returns");
    }
}

template<typename TFunction, typename... TExtra>
void CheckGet(const char* nid, int slot, char format, char modifier, TExtra... extra) {
    const auto get = Export<TFunction>(nid);
    for (const GuestLocale::TimeGetVtable* vtable : {&g_baseVtable, &g_derivedVtable}) {
        const int expected = slot + (vtable == &g_derivedVtable ? kDerived : 0);
        GuestLocale::Facet facet{&vtable->facet, 1, 0};
        GuestLocale::IosBase stream{};
        std::int32_t state = 4;
        auto* time = reinterpret_cast<GuestTm*>(g_time);
        const GuestLocale::InputIterator first = Iterator(0x1111222233334444ull + expected, 0x0000000000004101ull);
        const GuestLocale::InputIterator last = Iterator(0x5555666677778888ull, 0x0000000000000001ull + expected);
        const int calls = g_call.calls;
        const GuestLocale::InputIterator result = get(&facet, first, last, &stream, &state, time, extra...);
        Require(g_call.calls == calls + 1, "the member calls its do_ member exactly once");
        Require(g_call.slot == expected, "the member calls its own do_ slot in the facet's own vtable");
        Require(g_call.self == &facet && g_call.stream == &stream && g_call.state == &state && g_call.time == time, "the member passes the facet, stream, state and tm");
        Require(Same(g_call.first, first) && Same(g_call.last, last), "the member passes both iterators unchanged");
        Require(g_call.format == format && g_call.modifier == modifier, "get passes the format and modifier");
        Require(Same(result, Iterator(kReturnedLow + expected, kReturnedHigh)), "the member returns the iterator its do_ member returns");
        Require(state == 6, "the member leaves the state to its do_ member");
    }
}

void CheckTimeGet(const char* dateOrder, const char* getTime, const char* getDate, const char* getWeekday, const char* getMonthname, const char* getYear, const char* get) {
    CheckDateOrder(dateOrder);
    CheckGet<TimeGetExport>(getTime, 1, 0, 0);
    CheckGet<TimeGetExport>(getDate, 2, 0, 0);
    CheckGet<TimeGetExport>(getWeekday, 3, 0, 0);
    CheckGet<TimeGetExport>(getMonthname, 4, 0, 0);
    CheckGet<TimeGetExport>(getYear, 5, 0, 0);
    CheckGet<TimeGetPatternExport>(get, 6, 'Y', 'E', 'Y', 'E');
    CheckGet<TimeGetPatternExport>(get, 6, 'x', '\0', 'x', '\0');
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

    CheckTimeGet("ShlQcYrzRF8", "rrqNi95bhMs", "OY5mqEBxP+8", "KwJ5V3D0v3A", "vvA7HtdtWnY", "5L5Aft+9nZU", "+RuThw5axA4");
    CheckTimeGet("nc6OsiDx630", "9TfGnN6xq-U", "kwp-0uidHpw", "A0PftWMfrhk", "xy0MR+OOZI8", "Krt-A7EnHHs", "64pqofAwJEg");

    std::puts("libc internal time_get checks passed");
    return 0;
}
