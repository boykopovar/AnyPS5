#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal money_get and money_put: %s\n", message);
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
TFunction Export(const char* name) {
#ifdef _WIN32
    auto* address = reinterpret_cast<void*>(GetProcAddress(g_library, name));
#else
    void* address = dlsym(g_library, name);
#endif
    Require(address != nullptr, name);
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

GuestLocale::Streambuf* const kBuffer = reinterpret_cast<GuestLocale::Streambuf*>(static_cast<std::uintptr_t>(0x7000123400ull));
GuestLocale::Streambuf* const kReturnedBuffer = reinterpret_cast<GuestLocale::Streambuf*>(static_cast<std::uintptr_t>(0x7000567800ull));

struct Call {
    int slot;
    int calls;
    const GuestLocale::Facet* self;
    GuestLocale::InputIterator first;
    GuestLocale::InputIterator last;
    GuestLocale::OutputIterator out;
    bool international;
    GuestLocale::IosBase* stream;
    std::int32_t* state;
    std::uint32_t fill;
    long double units;
    const void* value;
};

Call g_call{};

template<int TSlot, typename TValue>
GuestLocale::InputIterator APS5_VABI RecordGet(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, bool international, GuestLocale::IosBase* stream, std::int32_t* state, TValue* value) {
    g_call = Call{TSlot, g_call.calls + 1, self, first, last, {}, international, stream, state, 0, 0, value};
    *state |= 2;
    return Iterator(kReturnedLow + TSlot, kReturnedHigh);
}

template<int TSlot, typename TCharacter>
GuestLocale::OutputIterator APS5_VABI RecordPutLongDouble(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::IosBase* stream, TCharacter fill, long double units) {
    g_call = Call{TSlot, g_call.calls + 1, self, {}, {}, out, international, stream, nullptr, static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<TCharacter>>(fill)), units, nullptr};
    return GuestLocale::OutputIterator{TSlot % 2 == 1, kReturnedBuffer};
}

template<int TSlot, typename TCharacter, typename TString>
GuestLocale::OutputIterator APS5_VABI RecordPutString(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, bool international, GuestLocale::IosBase* stream, TCharacter fill, const TString* digits) {
    g_call = Call{TSlot, g_call.calls + 1, self, {}, {}, out, international, stream, nullptr, static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<TCharacter>>(fill)), 0, digits};
    return GuestLocale::OutputIterator{TSlot % 2 == 1, kReturnedBuffer};
}

template<typename TVtable, typename TString, int TBase>
TVtable RecordingGetVtable() {
    return TVtable{g_testFacet, RecordGet<TBase + 0, long double>, RecordGet<TBase + 1, TString>};
}

template<typename TVtable, typename TCharacter, typename TString, int TBase>
TVtable RecordingPutVtable() {
    return TVtable{g_testFacet, RecordPutLongDouble<TBase + 0, TCharacter>, RecordPutString<TBase + 1, TCharacter, TString>};
}

const GuestLocale::CharMoneyGetVtable g_charGet = RecordingGetVtable<GuestLocale::CharMoneyGetVtable, GuestLocale::String, 0>();
const GuestLocale::CharMoneyGetVtable g_charGetDerived = RecordingGetVtable<GuestLocale::CharMoneyGetVtable, GuestLocale::String, kDerived>();
const GuestLocale::WideMoneyGetVtable g_wideGet = RecordingGetVtable<GuestLocale::WideMoneyGetVtable, void, 0>();
const GuestLocale::WideMoneyGetVtable g_wideGetDerived = RecordingGetVtable<GuestLocale::WideMoneyGetVtable, void, kDerived>();
const GuestLocale::CharMoneyPutVtable g_charPut = RecordingPutVtable<GuestLocale::CharMoneyPutVtable, char, GuestLocale::String, 0>();
const GuestLocale::CharMoneyPutVtable g_charPutDerived = RecordingPutVtable<GuestLocale::CharMoneyPutVtable, char, GuestLocale::String, kDerived>();
const GuestLocale::WideMoneyPutVtable g_widePut = RecordingPutVtable<GuestLocale::WideMoneyPutVtable, std::uint16_t, void, 0>();
const GuestLocale::WideMoneyPutVtable g_widePutDerived = RecordingPutVtable<GuestLocale::WideMoneyPutVtable, std::uint16_t, void, kDerived>();

template<typename TValue>
using MoneyGetExport = GuestLocale::InputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::InputIterator, GuestLocale::InputIterator, bool, GuestLocale::IosBase*, std::int32_t*, TValue*);

template<typename TValue, typename TVtable>
void CheckGet(const char* export_name, int slot, const TVtable& base, const TVtable& derived) {
    const auto get = Export<MoneyGetExport<TValue>>(export_name);
    for (const TVtable* vtable : {&base, &derived}) {
        for (const bool international : {false, true}) {
            const int expected = slot + (vtable == &derived ? kDerived : 0);
            GuestLocale::Facet facet{&vtable->facet, 1, 0};
            GuestLocale::IosBase stream{};
            std::int32_t state = 4;
            alignas(16) unsigned char storage[0x30]{};
            auto* value = static_cast<TValue*>(static_cast<void*>(storage));
            const GuestLocale::InputIterator first = Iterator(0x1111222233334444ull + expected, 0x0000000000004101ull);
            const GuestLocale::InputIterator last = Iterator(0x5555666677778888ull, 0x0000000000000001ull + expected);
            const int calls = g_call.calls;
            const GuestLocale::InputIterator result = get(&facet, first, last, international, &stream, &state, value);
            Require(g_call.calls == calls + 1, "get calls do_get exactly once");
            Require(g_call.slot == expected, "get calls the do_get slot of its value type in the facet's own vtable");
            Require(g_call.self == &facet && g_call.stream == &stream && g_call.state == &state && g_call.value == value, "get passes the facet, stream, state and value");
            Require(g_call.international == international, "get passes intl");
            Require(Same(g_call.first, first) && Same(g_call.last, last), "get passes both iterators unchanged");
            Require(Same(result, Iterator(kReturnedLow + expected, kReturnedHigh)), "get returns the iterator do_get returns");
            Require(state == 6, "get leaves the state to do_get");
        }
    }
}

template<typename TCharacter, typename TVtable>
void CheckPutLongDouble(const char* export_name, const TVtable& base, const TVtable& derived, TCharacter fill) {
    using Function = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, bool, GuestLocale::IosBase*, TCharacter, long double);
    const auto put = Export<Function>(export_name);
    for (const TVtable* vtable : {&base, &derived}) {
        for (const bool international : {false, true}) {
            const int expected = vtable == &derived ? kDerived : 0;
            GuestLocale::Facet facet{&vtable->facet, 1, 0};
            GuestLocale::IosBase stream{};
            const long double units = -(1.0L + 0x1p-60L) * 0x1p40L;
            const int calls = g_call.calls;
            const GuestLocale::OutputIterator result = put(&facet, GuestLocale::OutputIterator{international, kBuffer}, international, &stream, fill, units);
            Require(g_call.calls == calls + 1 && g_call.slot == expected, "put(long double) calls the long double do_put slot of the facet's own vtable once");
            Require(g_call.self == &facet && g_call.stream == &stream && g_call.international == international, "put(long double) passes the facet, intl and stream");
            Require(g_call.out.failed == international && g_call.out.buffer == kBuffer, "put(long double) passes the output iterator");
            Require(g_call.fill == static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<TCharacter>>(fill)), "put(long double) passes the fill");
            Require(g_call.units == units, "put(long double) passes the units");
            Require(result.failed == false && result.buffer == kReturnedBuffer, "put(long double) returns the iterator do_put returns");
        }
    }
}

template<typename TCharacter, typename TString, typename TVtable>
void CheckPutString(const char* export_name, const TVtable& base, const TVtable& derived, TCharacter fill) {
    using Function = GuestLocale::OutputIterator (APS5_VABI*)(const GuestLocale::Facet*, GuestLocale::OutputIterator, bool, GuestLocale::IosBase*, TCharacter, const TString*);
    const auto put = Export<Function>(export_name);
    for (const TVtable* vtable : {&base, &derived}) {
        for (const bool international : {false, true}) {
            const int expected = 1 + (vtable == &derived ? kDerived : 0);
            GuestLocale::Facet facet{&vtable->facet, 1, 0};
            GuestLocale::IosBase stream{};
            alignas(16) unsigned char storage[0x30]{};
            const auto* digits = static_cast<const TString*>(static_cast<const void*>(storage));
            const int calls = g_call.calls;
            const GuestLocale::OutputIterator result = put(&facet, GuestLocale::OutputIterator{international, kBuffer}, international, &stream, fill, digits);
            Require(g_call.calls == calls + 1 && g_call.slot == expected, "put(string) calls the string do_put slot of the facet's own vtable once");
            Require(g_call.self == &facet && g_call.stream == &stream && g_call.international == international, "put(string) passes the facet, intl and stream");
            Require(g_call.out.failed == international && g_call.out.buffer == kBuffer, "put(string) passes the output iterator");
            Require(g_call.fill == static_cast<std::uint32_t>(static_cast<std::make_unsigned_t<TCharacter>>(fill)), "put(string) passes the fill");
            Require(g_call.value == digits, "put(string) passes the digits");
            Require(result.failed == true && result.buffer == kReturnedBuffer, "put(string) returns the iterator do_put returns");
        }
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

    CheckGet<long double>("_ZNKSt9money_getIcSt19istreambuf_iteratorIcSt11char_traitsIcEEE3getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix", 0, g_charGet, g_charGetDerived);
    CheckGet<GuestLocale::String>("2fxdcyt5tGs_nid_no_patch_cut", 1, g_charGet, g_charGetDerived);
    CheckGet<long double>("_ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERe_nid_postfix", 0, g_wideGet, g_wideGetDerived);
    CheckGet<void>("_ZNKSt9money_getIwSt19istreambuf_iteratorIwSt11char_traitsIwEEE3getES3_S3_bRSt8ios_baseRNSt5_IosbIiE8_IostateERSbIwS2_SaIwEE_nid_postfix", 1, g_wideGet, g_wideGetDerived);
    CheckPutLongDouble<char>("_ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_bRSt8ios_basece_nid_postfix", g_charPut, g_charPutDerived, '*');
    CheckPutString<char, GuestLocale::String>("_ZNKSt9money_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE3putES3_bRSt8ios_basecRKSs_nid_postfix", g_charPut, g_charPutDerived, static_cast<char>(0xa4));
    CheckPutLongDouble<std::uint16_t>("_ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_bRSt8ios_basewe_nid_postfix", g_widePut, g_widePutDerived, 0x20ac);
    CheckPutString<std::uint16_t, void>("_ZNKSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEE3putES3_bRSt8ios_basewRKSbIwS2_SaIwEE_nid_postfix", g_widePut, g_widePutDerived, 0xffe5);

    std::puts("libc internal money_get and money_put checks passed");
    return 0;
}
