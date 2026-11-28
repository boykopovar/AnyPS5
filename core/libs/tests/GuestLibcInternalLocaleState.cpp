#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <atomic>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

extern "C" {
extern GuestLocale::Implementation* _ZSt21_sceLibcClassicLocale_nid_postfix;
extern std::uint64_t _ZNSt8numpunctIcE2idE_nid_postfix;
}

namespace {

std::vector<void*> g_liveAllocations;
std::vector<void*> g_freedAllocations;
bool g_doubleFreeDetected = false;
std::mutex g_allocationMutex;

void RecordAlloc(void* pointer) {
    if (pointer == nullptr) return;
    std::lock_guard<std::mutex> lock(g_allocationMutex);
    g_liveAllocations.push_back(pointer);
}

void RecordFree(void* pointer) {
    if (pointer == nullptr) return;
    std::lock_guard<std::mutex> lock(g_allocationMutex);
    auto it = std::find(g_liveAllocations.begin(), g_liveAllocations.end(), pointer);
    if (it != g_liveAllocations.end()) {
        g_liveAllocations.erase(it);
        g_freedAllocations.push_back(pointer);
        return;
    }
    if (std::find(g_freedAllocations.begin(), g_freedAllocations.end(), pointer) != g_freedAllocations.end()) {
        g_doubleFreeDetected = true;
    }
}

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size);
    RecordAlloc(pointer);
    return pointer;
}

void APS5_VABI TestFree(void* pointer) {
    RecordFree(pointer);
    std::free(pointer);
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count, size);
    RecordAlloc(pointer);
    return pointer;
}

void* APS5_VABI TestReallocate(void* pointer, std::size_t size) {
    if (pointer != nullptr) RecordFree(pointer);
    void* resized = std::realloc(pointer, size);
    RecordAlloc(resized);
    return resized;
}

void* APS5_VABI TestAlign(std::size_t, std::size_t size) {
    void* pointer = std::malloc(size);
    RecordAlloc(pointer);
    return pointer;
}

void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) {
    if (pointer != nullptr) RecordFree(pointer);
    void* resized = std::realloc(pointer, size);
    RecordAlloc(resized);
    return resized;
}

int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    RecordAlloc(*pointer);
    return *pointer != nullptr ? 0 : 12;
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "LibcInternal locale state: %s\n", message);
    std::abort();
}

template<typename Action>
bool Throws(Action action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void ReleaseLocaleFacade(GuestLocaleFacade& facade) {
    if (facade.implementation == nullptr) return;
    auto* base = &facade.implementation->base;
    if (auto* released = base->vtable->release(base)) {
        released->vtable->deleteObject(released);
    }
    facade.implementation = nullptr;
}

class GlobalLocaleGuard {
public:
    GlobalLocaleGuard() : original_(_ZNSt6locale16_GetgloballocaleEv_nid_postfix()) {
        if (original_ != nullptr) original_->base.vtable->retain(&original_->base);
    }

    ~GlobalLocaleGuard() {
        if (original_ == nullptr) return;
        GuestLocaleFacade restoreTarget{original_};
        GuestLocaleFacade discarded{};
        try {
            _ZNSt6locale6globalERKS__nid_postfix(&discarded, &restoreTarget);
            ReleaseLocaleFacade(discarded);
        } catch (...) {
        }
        ReleaseLocaleFacade(restoreTarget);
    }

    GlobalLocaleGuard(const GlobalLocaleGuard&) = delete;
    GlobalLocaleGuard& operator=(const GlobalLocaleGuard&) = delete;

private:
    GuestLocale::Implementation* original_;
};

struct GuardedLocimp {
    std::uint64_t before = 0x1122334455667788ULL;
    GuestLocale::Implementation impl{};
    std::uint64_t after = 0x8877665544332211ULL;
};

struct GuardedLocinfo {
    std::uint64_t before = 0x123456789abcdef0ULL;
    GuestLocale::LocinfoStorage storage{};
    std::uint64_t after = 0x0fedcba987654321ULL;
};

void APS5_VABI NoOpDestroyFacet(GuestLocale::Facet*) {}
void APS5_VABI NoOpRetainFacet(GuestLocale::Facet*) {}
GuestLocale::Facet* APS5_VABI NoOpReleaseFacet(GuestLocale::Facet*) { return nullptr; }

const GuestLocale::NumpunctVtable g_testNumpunctVtable{
    {NoOpDestroyFacet, NoOpDestroyFacet, NoOpRetainFacet, NoOpReleaseFacet},
    &_ZNKSt8numpunctIcE16do_decimal_pointEv_nid_postfix,
    &_ZNKSt8numpunctIcE16do_thousands_sepEv_nid_postfix,
    &_ZNKSt8numpunctIcE11do_groupingEv_nid_postfix,
    &_ZNKSt8numpunctIcE12do_falsenameEv_nid_postfix,
    &_ZNKSt8numpunctIcE11do_truenameEv_nid_postfix,
};

struct TestStreambuf {
    GuestLocale::Streambuf buffer;
    char* next = nullptr;
    char* end = nullptr;
    std::string written;
};

int APS5_VABI TestOverflow(GuestLocale::Streambuf* self, int character) {
    auto* stream = reinterpret_cast<TestStreambuf*>(self);
    stream->written.push_back(static_cast<char>(character));
    return character;
}

const GuestLocale::StreambufVtable g_testStreambufVtable{{}, TestOverflow};

std::string FormatDoubleWithTransparentStream(const GuestLocale::Facet* numPut, GuestLocale::Implementation* transparentLocale, double value) {
    TestStreambuf buf{};
    buf.buffer = {&g_testStreambufVtable, {}, &buf.next, {}, &buf.end};
    GuestLocale::IosBase ios{};
    ios.precision = 6;
    ios.locale = &transparentLocale;
    const auto* vtable = reinterpret_cast<const GuestLocale::NumPutVtable*>(numPut->vtable);
    GuestLocale::OutputIterator out{false, &buf.buffer};
    out = vtable->putDouble(numPut, out, &ios, ' ', value);
    Require(!out.failed, "num_put double output should succeed");
    return buf.written;
}

GuestLocale::Implementation* AllocateLocimp(bool transparent) {
    auto* impl = static_cast<GuestLocale::Implementation*>(_Znwm_nid_postfix(sizeof(GuestLocale::Implementation)));
    _ZNSt6locale7_LocimpC2Eb_nid_postfix(impl, transparent);
    return impl;
}

std::size_t CountFreedPointer(const void* pointer) {
    return static_cast<std::size_t>(std::count(g_freedAllocations.begin(), g_freedAllocations.end(), pointer));
}

void CheckLocimpBaseConstructor() {
    GlobalLocaleGuard guard;
    for (const bool expectedTransparent : {false, true}) {
        GuardedLocimp guarded{};
        std::memset(&guarded.impl, 0xa5, sizeof(guarded.impl));
        _ZNSt6locale7_LocimpC2Eb_nid_postfix(&guarded.impl, expectedTransparent);
        Require(guarded.before == 0x1122334455667788ULL && guarded.after == 0x8877665544332211ULL, "_LocimpC2Eb must not overwrite adjacent memory");
        Require(guarded.impl.base.vtable == LibcLocimpVtable_nid_no_patch(), "_LocimpC2Eb sets the _Locimp vtable");
        Require(guarded.impl.base.references == 1, "_LocimpC2Eb initializes references to 1");
        Require(guarded.impl.base.reserved == 0, "_LocimpC2Eb initializes reserved to 0");
        Require(guarded.impl.facets == nullptr && guarded.impl.facetCount == 0, "_LocimpC2Eb starts with empty facets");
        Require(guarded.impl.category == 0, "_LocimpC2Eb initializes category to none");
        Require(guarded.impl.transparent == expectedTransparent, "_LocimpC2Eb stores transparent flag");
        Require(guarded.impl.name != nullptr && std::strcmp(guarded.impl.name, "*") == 0, "_LocimpC2Eb initializes name to *");

        guarded.impl.base.vtable->retain(&guarded.impl.base);
        Require(guarded.impl.base.references == 2, "retain increments _Locimp references");
        Require(guarded.impl.base.vtable->release(&guarded.impl.base) == nullptr, "release from 2 to 1 keeps _Locimp alive");
        Require(guarded.impl.base.references == 1, "release decremented references to 1");
        Require(guarded.impl.base.vtable->release(&guarded.impl.base) == &guarded.impl.base, "final release returns _Locimp base");
        Require(guarded.impl.base.references == 0, "final release leaves references at 0");
        guarded.impl.base.vtable->destroy(&guarded.impl.base);
    }

    g_liveAllocations.clear();
    g_freedAllocations.clear();
    g_doubleFreeDetected = false;
    const GuestLocale::FacetVtable dummyVtable{NoOpDestroyFacet, NoOpDestroyFacet, NoOpRetainFacet, NoOpReleaseFacet};
    GuestLocale::Facet dummyFacet{&dummyVtable, 1, 0};
    auto* heapImp = AllocateLocimp(true);
    _ZNSt6locale7_Locimp7_AddfacEPNS_5facetEm_nid_postfix(heapImp, &dummyFacet, 1);
    void* facetArray = heapImp->facets;
    Require(facetArray != nullptr && heapImp->facetCount >= 2, "_Addfac allocates facet array");
    Require(heapImp->base.vtable->release(&heapImp->base) == &heapImp->base, "final release on heap _Locimp returns base");
    heapImp->base.vtable->deleteObject(&heapImp->base);
    Require(!g_doubleFreeDetected, "destroying heap _Locimp must not double-free");
    Require(CountFreedPointer(facetArray) == 1, "deleting _Locimp frees its facet array once");
    Require(CountFreedPointer(heapImp) == 1, "deleting _Locimp frees the implementation once");

    Require(Throws([] { _ZNSt6locale7_LocimpC2Eb_nid_postfix(nullptr, false); }), "_LocimpC2Eb rejects null self");
}

void CheckLocinfoWithCategoryConstructor() {
    GlobalLocaleGuard guard;
    const std::array<int, 10> validCategories{0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x01 | 0x08, 0x02 | 0x10, 0x3f};
    for (const int category : validCategories) {
        GuardedLocinfo guarded{};
        std::memset(guarded.storage.bytes, 0x5a, sizeof(guarded.storage.bytes));
        const char* beforeC = setlocale_nid_postfix(0, nullptr);
        Require(beforeC != nullptr && std::strcmp(beforeC, "C") == 0, "C locale starts as C");
        _ZNSt8_LocinfoC1EiPKc_nid_postfix(&guarded.storage, category, "C");
        Require(guarded.before == 0x123456789abcdef0ULL && guarded.after == 0x0fedcba987654321ULL, "_LocinfoC1EiPKc must not overwrite adjacent memory");
        for (const unsigned char byte : guarded.storage.bytes) {
            Require(byte == 0, "_LocinfoC1EiPKc initializes storage cleanly");
        }
        const char* duringC = setlocale_nid_postfix(0, nullptr);
        Require(duringC != nullptr && std::strcmp(duringC, "C") == 0, "C locale remains C during _Locinfo lifetime");
        _ZNSt8_LocinfoD1Ev_nid_postfix(&guarded.storage);
        const char* afterC = setlocale_nid_postfix(0, nullptr);
        Require(afterC != nullptr && std::strcmp(afterC, "C") == 0, "C locale remains restored after _Locinfo destruction");
    }

    GuardedLocinfo invalidStorage{};
    Require(Throws([&] { _ZNSt8_LocinfoC1EiPKc_nid_postfix(nullptr, 0x3f, "C"); }), "_LocinfoC1EiPKc rejects null self");
    Require(Throws([&] { _ZNSt8_LocinfoC1EiPKc_nid_postfix(&invalidStorage.storage, 0x3f, nullptr); }), "_LocinfoC1EiPKc rejects null name");
    for (const int badCategory : {-1, -64, 0x40, 0x100, 0x7fffffff}) {
        Require(Throws([&] { _ZNSt8_LocinfoC1EiPKc_nid_postfix(&invalidStorage.storage, badCategory, "C"); }), "_LocinfoC1EiPKc rejects invalid category");
    }
    for (const char* badName : {"", "*", "CX", "en_US.UTF-8", "unknown-locale"}) {
        Require(Throws([&] { _ZNSt8_LocinfoC1EiPKc_nid_postfix(&invalidStorage.storage, 0x3f, badName); }), "_LocinfoC1EiPKc rejects unsupported locale name");
    }
}

void CheckGlobalLocaleLifecycle() {
    GlobalLocaleGuard guard;
    GuestLocale::Implementation* classic = _ZSt21_sceLibcClassicLocale_nid_postfix;
    Require(_ZNSt6locale16_GetgloballocaleEv_nid_postfix() == classic, "global locale starts as classic");
    Require(_ZNSt6locale5_InitEv_nid_postfix() == classic, "locale::_Init returns classic initially");

    const GuestLocale::Implementation* classicConst = classic;
    GuestLocale::Facet* numPut = nullptr;
    Require(_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&numPut, &classicConst) == 4 && numPut != nullptr, "num_put _Getcat succeeds");
    numPut->vtable->retain(numPut);

    GuardedLocimp transparentStreamHolder{};
    _ZNSt6locale7_LocimpC2Eb_nid_postfix(&transparentStreamHolder.impl, true);
    Require(FormatDoubleWithTransparentStream(numPut, &transparentStreamHolder.impl, 1.5) == "1.5", "classic num_put formats 1.5 with dot");
    Require(_ZNSt8numpunctIcE2idE_nid_postfix != 0, "numpunct<char>::id is allocated");

    g_liveAllocations.clear();
    g_freedAllocations.clear();
    g_doubleFreeDetected = false;

    GuestLocale::NumpunctFacet commaPunct{{&g_testNumpunctVtable.facet, 1, 0}, "", ',', ',', "false", "true"};
    GuestLocale::NumpunctFacet semicolonPunct{{&g_testNumpunctVtable.facet, 1, 0}, "", ';', ',', "false", "true"};

    GuestLocale::Implementation* impA = AllocateLocimp(false);
    _ZNSt6locale7_Locimp7_AddfacEPNS_5facetEm_nid_postfix(impA, &commaPunct.base, _ZNSt8numpunctIcE2idE_nid_postfix);
    void* facetsA = impA->facets;

    GuestLocale::Implementation* impB = AllocateLocimp(true);
    _ZNSt6locale7_Locimp7_AddfacEPNS_5facetEm_nid_postfix(impB, &semicolonPunct.base, _ZNSt8numpunctIcE2idE_nid_postfix);
    void* facetsB = impB->facets;

    GuestLocaleFacade locA{impA};
    GuestLocaleFacade prev1{};
    GuestLocaleFacade* sret1 = _ZNSt6locale6globalERKS__nid_postfix(&prev1, &locA);
    Require(sret1 == &prev1, "global(A) returns sret pointer");
    Require(prev1.implementation == classic, "global(A) returns previous classic locale");
    Require(impA->base.references == 2, "global(A) retains A while caller holds A");
    Require(_ZNSt6locale16_GetgloballocaleEv_nid_postfix() == impA, "_Getgloballocale reflects A");
    Require(_ZNSt6locale5_InitEv_nid_postfix() == impA, "_Init reflects A");
    Require(FormatDoubleWithTransparentStream(numPut, &transparentStreamHolder.impl, 1.5) == "1,5", "real num_put consumer observes global A decimal point");
    Require(std::strcmp(setlocale_nid_postfix(0, nullptr), "C") == 0, "unnamed locale keeps C locale valid");

    ReleaseLocaleFacade(prev1);
    ReleaseLocaleFacade(locA);
    Require(impA->base.references == 1, "global keeps A alive after caller drops its reference");
    Require(CountFreedPointer(impA) == 0, "A must not be destroyed while global");
    Require(FormatDoubleWithTransparentStream(numPut, &transparentStreamHolder.impl, 1.5) == "1,5", "real num_put consumer still observes A after caller releases locA");

    GuestLocaleFacade locB{impB};
    GuestLocaleFacade prev2{};
    GuestLocaleFacade* sret2 = _ZNSt6locale6globalERKS__nid_postfix(&prev2, &locB);
    Require(sret2 == &prev2, "global(B) returns sret pointer");
    Require(prev2.implementation == impA, "global(B) returns previous locale A");
    Require(impA->base.references == 1, "previous locale A stays alive in returned sret");
    Require(impB->base.references == 2, "global(B) retains B");
    Require(CountFreedPointer(impA) == 0, "A is still alive while held by prev2");
    Require(prev2.implementation->facets[_ZNSt8numpunctIcE2idE_nid_postfix] == &commaPunct.base, "returned sret A remains usable");
    Require(_ZNSt6locale16_GetgloballocaleEv_nid_postfix() == impB, "_Getgloballocale reflects B");
    Require(_ZNSt6locale5_InitEv_nid_postfix() == impB, "_Init reflects B");
    Require(FormatDoubleWithTransparentStream(numPut, &transparentStreamHolder.impl, 1.5) == "1;5", "real num_put consumer observes global B decimal point");

    GuestLocaleFacade prev3{};
    GuestLocaleFacade* sret3 = _ZNSt6locale6globalERKS__nid_postfix(&prev3, &locB);
    Require(sret3 == &prev3 && prev3.implementation == impB, "repeated global(B) returns B");
    Require(impB->base.references == 3, "repeated global(B) retains returned reference");
    ReleaseLocaleFacade(prev3);
    ReleaseLocaleFacade(locB);
    Require(impB->base.references == 1 && CountFreedPointer(impB) == 0, "B remains alive as global after caller drops references");

    GuestLocaleFacade classicFacade{classic};
    GuestLocaleFacade prev4{};
    GuestLocaleFacade* sret4 = _ZNSt6locale6globalERKS__nid_postfix(&prev4, &classicFacade);
    Require(sret4 == &prev4 && prev4.implementation == impB, "restoring classic returns previous B");
    Require(_ZNSt6locale16_GetgloballocaleEv_nid_postfix() == classic, "_Getgloballocale restored to classic");
    Require(_ZNSt6locale5_InitEv_nid_postfix() == classic, "_Init restored to classic");
    Require(FormatDoubleWithTransparentStream(numPut, &transparentStreamHolder.impl, 1.5) == "1.5", "real num_put consumer observes classic restored");
    Require(CountFreedPointer(impA) == 0 && CountFreedPointer(impB) == 0, "A and B remain unfreed while held in prev2 and prev4");

    GuestLocaleFacade prev5{};
    _ZNSt6locale6globalERKS__nid_postfix(&prev5, &classicFacade);
    Require(prev5.implementation == classic, "global(classic) when already classic returns classic");
    ReleaseLocaleFacade(prev5);

    ReleaseLocaleFacade(prev2);
    Require(CountFreedPointer(impA) == 1 && CountFreedPointer(facetsA) == 1, "releasing last ref to A destroys and frees A once");
    ReleaseLocaleFacade(prev4);
    Require(CountFreedPointer(impB) == 1 && CountFreedPointer(facetsB) == 1, "releasing last ref to B destroys and frees B once");
    Require(!g_doubleFreeDetected, "lifecycle must not trigger any double-free");

    if (auto* releasedPut = numPut->vtable->release(numPut)) {
        releasedPut->vtable->deleteObject(releasedPut);
    }
}

void CheckGlobalLocaleConcurrentSwapConsumer() {
    GlobalLocaleGuard guard;
    GuestLocale::Implementation* classic = _ZSt21_sceLibcClassicLocale_nid_postfix;
    GuestLocale::Facet* numPut = nullptr;
    const GuestLocale::Implementation* classicConst = classic;
    Require(_ZNSt7num_putIcSt19ostreambuf_iteratorIcSt11char_traitsIcEEE7_GetcatEPPKNSt6locale5facetEPKS5__nid_postfix(&numPut, &classicConst) == 4 && numPut != nullptr, "num_put _Getcat succeeds for concurrent consumer");
    numPut->vtable->retain(numPut);
    GuardedLocimp transparentHolder{};
    _ZNSt6locale7_LocimpC2Eb_nid_postfix(&transparentHolder.impl, true);

    static GuestLocale::NumpunctFacet commaPunct{{&g_testNumpunctVtable.facet, 1, 0}, "", ',', ',', "false", "true"};
    static GuestLocale::NumpunctFacet semicolonPunct{{&g_testNumpunctVtable.facet, 1, 0}, "", ';', ',', "false", "true"};
    std::atomic<bool> start{false};
    auto swapAndConsume = [&](GuestLocale::NumpunctFacet* punct) {
        while (!start.load(std::memory_order_acquire)) {}
        for (int iteration = 0; iteration < 300; ++iteration) {
            GuestLocale::Implementation* imp = AllocateLocimp(false);
            _ZNSt6locale7_Locimp7_AddfacEPNS_5facetEm_nid_postfix(imp, &punct->base, _ZNSt8numpunctIcE2idE_nid_postfix);
            GuestLocaleFacade candidate{imp};
            GuestLocaleFacade previous{};
            _ZNSt6locale6globalERKS__nid_postfix(&previous, &candidate);
            ReleaseLocaleFacade(previous);
            ReleaseLocaleFacade(candidate);
            const std::string formatted = FormatDoubleWithTransparentStream(numPut, &transparentHolder.impl, 1.5);
            Require(formatted == "1,5" || formatted == "1;5" || formatted == "1.5", "concurrent num_put observes one complete global locale");
        }
    };
    std::thread commaThread(swapAndConsume, &commaPunct);
    std::thread semicolonThread(swapAndConsume, &semicolonPunct);
    start.store(true, std::memory_order_release);
    commaThread.join();
    semicolonThread.join();

    GuestLocaleFacade classicTarget{classic};
    GuestLocaleFacade previous{};
    _ZNSt6locale6globalERKS__nid_postfix(&previous, &classicTarget);
    ReleaseLocaleFacade(previous);
    if (auto* releasedPut = numPut->vtable->release(numPut)) releasedPut->vtable->deleteObject(releasedPut);
}

void CheckGlobalFacetHeldAcrossSwap() {
    GlobalLocaleGuard guard;
    g_liveAllocations.clear();
    g_freedAllocations.clear();
    g_doubleFreeDetected = false;

    GuestLocale::NumpunctFacet commaPunct{{&g_testNumpunctVtable.facet, 1, 0}, "", ',', ',', "false", "true"};
    GuestLocale::NumpunctFacet semicolonPunct{{&g_testNumpunctVtable.facet, 1, 0}, "", ';', ',', "false", "true"};

    GuestLocale::Implementation* impA = AllocateLocimp(false);
    _ZNSt6locale7_Locimp7_AddfacEPNS_5facetEm_nid_postfix(impA, &commaPunct.base, _ZNSt8numpunctIcE2idE_nid_postfix);
    void* facetsA = impA->facets;

    GuestLocale::Implementation* impB = AllocateLocimp(false);
    _ZNSt6locale7_Locimp7_AddfacEPNS_5facetEm_nid_postfix(impB, &semicolonPunct.base, _ZNSt8numpunctIcE2idE_nid_postfix);

    GuestLocaleFacade locA{impA};
    GuestLocaleFacade prev1{};
    _ZNSt6locale6globalERKS__nid_postfix(&prev1, &locA);
    ReleaseLocaleFacade(prev1);
    ReleaseLocaleFacade(locA);
    Require(_ZNSt6locale16_GetgloballocaleEv_nid_postfix() == impA, "held facet test installs A as global");

    GuestLocale::GlobalLocaleHold hold;
    const GuestLocale::Facet* held = hold.Find(_ZNSt8numpunctIcE2idE_nid_postfix);
    Require(held == &commaPunct.base, "hold finds the global facet");

    GuestLocaleFacade locB{impB};
    GuestLocaleFacade prev2{};
    _ZNSt6locale6globalERKS__nid_postfix(&prev2, &locB);
    Require(prev2.implementation == impA, "swap to B returns A");
    ReleaseLocaleFacade(locB);
    ReleaseLocaleFacade(prev2);
    Require(_ZNSt6locale16_GetgloballocaleEv_nid_postfix() == impB, "held facet test installs B as global");
    Require(CountFreedPointer(impA) == 0, "held global locale A must not be freed after the swap");
    Require(CountFreedPointer(facetsA) == 0, "held global locale A facets must not be freed after the swap");
    Require(reinterpret_cast<const GuestLocale::NumpunctFacet*>(held)->decimalPoint == ',', "held facet still reads its expected value");

    hold = GuestLocale::GlobalLocaleHold();
    Require(CountFreedPointer(impA) == 1, "releasing the hold frees A exactly once");
    Require(CountFreedPointer(facetsA) == 1, "releasing the hold frees A facets exactly once");
    Require(!g_doubleFreeDetected, "releasing the hold must not trigger a double-free");
}

void CheckGlobalLocaleNegativeInputs() {
    GlobalLocaleGuard guard;
    GuestLocale::Implementation* impA = AllocateLocimp(false);
    GuestLocaleFacade locA{impA};
    GuestLocaleFacade prev{};
    _ZNSt6locale6globalERKS__nid_postfix(&prev, &locA);
    ReleaseLocaleFacade(prev);
    ReleaseLocaleFacade(locA);

    GuestLocaleFacade out{};
    GuestLocaleFacade nullImpl{nullptr};
    GuestLocale::Implementation noVtable{{nullptr, 1, 0}, nullptr, 0, 0, false, "*"};
    GuestLocaleFacade noVtableFacade{&noVtable};
    GuestLocale::Implementation nullName{{LibcLocimpVtable_nid_no_patch(), 1, 0}, nullptr, 0, 0, false, nullptr};
    GuestLocaleFacade nullNameFacade{&nullName};
    GuestLocale::Implementation badName{{LibcLocimpVtable_nid_no_patch(), 1, 0}, nullptr, 0, 0x3f, false, "en_US.UTF-8"};
    GuestLocaleFacade badNameFacade{&badName};
    GuestLocaleFacade classicTarget{_ZSt21_sceLibcClassicLocale_nid_postfix};

    Require(Throws([&] { _ZNSt6locale6globalERKS__nid_postfix(nullptr, &classicTarget); }), "global rejects null result");
    Require(Throws([&] { _ZNSt6locale6globalERKS__nid_postfix(&out, nullptr); }), "global rejects null locale wrapper");
    Require(Throws([&] { _ZNSt6locale6globalERKS__nid_postfix(&out, &nullImpl); }), "global rejects null implementation");
    Require(Throws([&] { _ZNSt6locale6globalERKS__nid_postfix(&out, &noVtableFacade); }), "global rejects null vtable");
    Require(Throws([&] { _ZNSt6locale6globalERKS__nid_postfix(&out, &nullNameFacade); }), "global rejects null locale name");
    Require(Throws([&] { _ZNSt6locale6globalERKS__nid_postfix(&out, &badNameFacade); }), "global rejects unsupported locale name");

    Require(_ZNSt6locale16_GetgloballocaleEv_nid_postfix() == impA, "failed global calls must preserve active global locale");
    Require(_ZNSt6locale5_InitEv_nid_postfix() == impA, "failed global calls must preserve _Init global locale");
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    const std::uint64_t beforeNotImplemented = NotImplementedCalls_nid_no_patch();
    CheckLocimpBaseConstructor();
    CheckLocinfoWithCategoryConstructor();
    CheckGlobalLocaleLifecycle();
    CheckGlobalLocaleConcurrentSwapConsumer();
    CheckGlobalFacetHeldAcrossSwap();
    CheckGlobalLocaleNegativeInputs();
    Require(NotImplementedCalls_nid_no_patch() == beforeNotImplemented, "tests must not hit NotImplemented stubs");
    return 0;
}
