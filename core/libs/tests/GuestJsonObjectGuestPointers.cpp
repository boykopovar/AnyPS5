#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using namespace SceJson2;

namespace {

struct Parameter {
    void* allocator;
    void* context;
    std::size_t bufferSize;
    std::int32_t mode;
    std::uint32_t reserved[3];
};

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "libSceJson2 Object guest pointer test failed at line %d\n", line);
    std::abort();
}

#define CHECK(cond) Require((cond), __LINE__)

template<typename TCall>
void ExpectThrow(TCall&& call, int line) {
    bool threw = false;
    try {
        call();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, line);
}

#define CHECK_THROWS(expr) ExpectThrow([&] { expr; }, __LINE__)

struct Trace {
    std::map<void*, std::size_t> live;
    void* protectPage = nullptr;
    std::size_t protectSize = 0;
    bool protectReadOnly = false;
};

void SetPageWritable(void* page, std::size_t size, bool writable) {
#ifdef _WIN32
    DWORD oldProtect = 0;
    CHECK(VirtualProtect(page, size, writable ? PAGE_READWRITE : PAGE_READONLY, &oldProtect) != 0);
#else
    CHECK(mprotect(page, size, writable ? (PROT_READ | PROT_WRITE) : PROT_READ) == 0);
#endif
}

void* APS5_VABI TrackAllocate(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<Trace*>(context);
    void* memory = std::malloc(bytes);
    CHECK(memory != nullptr);
    std::memset(memory, 0, bytes);
    trace.live.emplace(memory, bytes);
    if (trace.protectReadOnly && trace.protectPage) {
        trace.protectReadOnly = false;
        SetPageWritable(trace.protectPage, trace.protectSize, false);
    }
    return memory;
}

void APS5_VABI TrackFree(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<Trace*>(context);
    CHECK(trace.live.erase(memory) == 1);
    std::free(memory);
}

void APS5_VABI TrackNotify(void*, int, std::size_t, void*) {}

void* const Vtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<void*>(&TrackAllocate),
    reinterpret_cast<void*>(&TrackFree),
    reinterpret_cast<void*>(&TrackNotify),
};

struct Allocator {
    void* const* vtable = Vtable;
};

struct ProtectedPages {
    std::size_t pageSize = 0;
    void* unmapped = nullptr;
    void* readOnly = nullptr;
    void* scratch = nullptr;

    ProtectedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        unmapped = VirtualAlloc(nullptr, pageSize, MEM_RESERVE, PAGE_NOACCESS);
        CHECK(unmapped != nullptr);
        readOnly = VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECK(readOnly != nullptr);
        std::memset(readOnly, 0, pageSize);
        SetPageWritable(readOnly, pageSize, false);
        scratch = VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECK(scratch != nullptr);
        std::memset(scratch, 0, pageSize);
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        unmapped = mmap(nullptr, pageSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(unmapped != MAP_FAILED);
        readOnly = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(readOnly != MAP_FAILED);
        std::memset(readOnly, 0, pageSize);
        SetPageWritable(readOnly, pageSize, false);
        scratch = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(scratch != MAP_FAILED);
        std::memset(scratch, 0, pageSize);
#endif
    }

    ~ProtectedPages() {
#ifdef _WIN32
        if (unmapped) VirtualFree(unmapped, 0, MEM_RELEASE);
        if (readOnly) VirtualFree(readOnly, 0, MEM_RELEASE);
        if (scratch) VirtualFree(scratch, 0, MEM_RELEASE);
#else
        if (unmapped) munmap(unmapped, pageSize);
        if (readOnly) munmap(readOnly, pageSize);
        if (scratch) munmap(scratch, pageSize);
#endif
    }
};

}

int main() {
    ProtectedPages pages;
    Trace trace;
    Allocator allocator;
    alignas(8) std::byte initializer[64]{};
    _ZN3sce4Json11InitializerC1Ev(initializer);
    Parameter parameter{&allocator, &trace, 0x100, 0, {}};
    CHECK(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(initializer, &parameter) == 0);

    auto* badObject = static_cast<Object*>(pages.unmapped);
    auto* roObject = static_cast<Object*>(pages.readOnly);
    auto* badIter = static_cast<ObjectIterator*>(pages.unmapped);
    auto* roIter = static_cast<ObjectIterator*>(pages.readOnly);
    auto* badPair = static_cast<Pair*>(pages.unmapped);
    auto* roPair = static_cast<Pair*>(pages.readOnly);
    auto* badString = static_cast<String*>(pages.unmapped);
    auto* badValue = static_cast<Value*>(pages.unmapped);

    Object valid{};
    _ZN3sce4Json6ObjectC1Ev(&valid);
    String keyA{};
    _ZN3sce4Json6StringC1EPKc(&keyA, "alpha");
    String keyB{};
    _ZN3sce4Json6StringC1EPKc(&keyB, "beta");
    Value val1{};
    _ZN3sce4Json5ValueC1El(&val1, 11);
    Value val2{};
    _ZN3sce4Json5ValueC1El(&val2, 22);

    Value* slotA = _ZN3sce4Json6ObjectixERKNS0_6StringE(&valid, &keyA);
    CHECK(slotA != nullptr);
    _ZN3sce4Json5Value3setEl(slotA, 11);

    Pair pairB{};
    _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pairB, &keyB, &val2);
    ObjectIterator insertedB{};
    CHECK(_ZN3sce4Json6Object6insertERKNS1_4PairE(&insertedB, &valid, &pairB) == &insertedB);
    CHECK(insertedB.state != nullptr);
    CHECK(_ZNK3sce4Json6Object4sizeEv(&valid) == 2);
    CHECK(!_ZNK3sce4Json6Object5emptyEv(&valid));

    ObjectIterator beginIter{};
    ObjectIterator endIter{};
    ObjectIterator foundIter{};
    CHECK(_ZNK3sce4Json6Object5beginEv(&beginIter, &valid) == &beginIter);
    CHECK(_ZNK3sce4Json6Object3endEv(&endIter, &valid) == &endIter);
    CHECK(_ZNK3sce4Json6Object4findERKNS0_6StringE(&foundIter, &valid, &keyB) == &foundIter);
    CHECK(_ZNK3sce4Json6Object8iteratoreqERKS2_(&foundIter, &insertedB));
    CHECK(_ZNK3sce4Json6Object8iteratorneERKS2_(&beginIter, &endIter));
    CHECK(_ZNK3sce4Json6Object8iteratordeEv(&beginIter) != nullptr);
    CHECK(_ZNK3sce4Json6Object8iteratorptEv(&beginIter) == _ZNK3sce4Json6Object8iteratordeEv(&beginIter));

    const auto baselineLive = trace.live.size();

    CHECK_THROWS(_ZN3sce4Json6ObjectC1Ev(nullptr));
    CHECK_THROWS(_ZN3sce4Json6ObjectC1Ev(badObject));
    CHECK_THROWS(_ZN3sce4Json6ObjectC1Ev(roObject));
    CHECK_THROWS(_ZN3sce4Json6ObjectC2Ev(roObject));

    CHECK_THROWS(_ZN3sce4Json6ObjectC1ERKS1_(nullptr, &valid));
    CHECK_THROWS(_ZN3sce4Json6ObjectC1ERKS1_(badObject, &valid));
    CHECK_THROWS(_ZN3sce4Json6ObjectC1ERKS1_(roObject, &valid));
    CHECK_THROWS(_ZN3sce4Json6ObjectC1ERKS1_(&valid, nullptr));
    CHECK_THROWS(_ZN3sce4Json6ObjectC1ERKS1_(&valid, badObject));
    CHECK_THROWS(_ZN3sce4Json6ObjectC2ERKS1_(roObject, &valid));

    CHECK_THROWS(_ZN3sce4Json6ObjectD1Ev(nullptr));
    CHECK_THROWS(_ZN3sce4Json6ObjectD1Ev(badObject));
    _ZN3sce4Json6ObjectD1Ev(roObject);
    _ZN3sce4Json6ObjectD2Ev(roObject);
    CHECK(roObject->items == nullptr);

    auto* roLiveObject = static_cast<Object*>(pages.scratch);
    SetPageWritable(pages.scratch, pages.pageSize, true);
    _ZN3sce4Json6ObjectC1Ev(roLiveObject);
    CHECK(roLiveObject->items != nullptr);
    const auto liveWithRoObject = trace.live.size();
    SetPageWritable(pages.scratch, pages.pageSize, false);
    CHECK_THROWS(_ZN3sce4Json6ObjectD1Ev(roLiveObject));
    CHECK_THROWS(_ZN3sce4Json6ObjectD2Ev(roLiveObject));
    CHECK(roLiveObject->items != nullptr && trace.live.size() == liveWithRoObject);
    SetPageWritable(pages.scratch, pages.pageSize, true);
    _ZN3sce4Json6ObjectD1Ev(roLiveObject);
    CHECK(roLiveObject->items == nullptr && trace.live.size() == baselineLive);

    CHECK_THROWS(_ZN3sce4Json6ObjectaSERKS1_(nullptr, &valid));
    CHECK_THROWS(_ZN3sce4Json6ObjectaSERKS1_(badObject, &valid));
    CHECK_THROWS(_ZN3sce4Json6ObjectaSERKS1_(roObject, &valid));
    CHECK_THROWS(_ZN3sce4Json6ObjectaSERKS1_(&valid, nullptr));
    CHECK_THROWS(_ZN3sce4Json6ObjectaSERKS1_(&valid, badObject));

    CHECK_THROWS(_ZN3sce4Json6Object5clearEv(nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object5clearEv(badObject));

    CHECK_THROWS(_ZN3sce4Json6ObjectixERKNS0_6StringE(nullptr, &keyA));
    CHECK_THROWS(_ZN3sce4Json6ObjectixERKNS0_6StringE(badObject, &keyA));
    CHECK_THROWS(_ZN3sce4Json6ObjectixERKNS0_6StringE(&valid, nullptr));
    CHECK_THROWS(_ZN3sce4Json6ObjectixERKNS0_6StringE(&valid, badString));

    String corruptTextString{};
    corruptTextString.text = static_cast<Text*>(pages.unmapped);
    CHECK_THROWS(_ZN3sce4Json6ObjectixERKNS0_6StringE(&valid, &corruptTextString));
    CHECK_THROWS(_ZN3sce4Json6Object5eraseERKNS0_6StringE(&valid, &corruptTextString));
    ObjectIterator probeIter{};
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(&probeIter, &valid, &corruptTextString));

    Text badCharsText{};
    badCharsText.characters = static_cast<char*>(pages.unmapped);
    badCharsText.length = 4;
    badCharsText.capacity = 8;
    String corruptCharsString{&badCharsText};
    CHECK_THROWS(_ZN3sce4Json6ObjectixERKNS0_6StringE(&valid, &corruptCharsString));
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(&probeIter, &valid, &corruptCharsString));
    CHECK_THROWS(_ZN3sce4Json6Object5eraseERKNS0_6StringE(&valid, &corruptCharsString));

    CHECK_THROWS(_ZNK3sce4Json6Object5emptyEv(nullptr));
    CHECK_THROWS(_ZNK3sce4Json6Object5emptyEv(badObject));
    CHECK_THROWS(_ZNK3sce4Json6Object4sizeEv(nullptr));
    CHECK_THROWS(_ZNK3sce4Json6Object4sizeEv(badObject));

    CHECK_THROWS(_ZN3sce4Json6Object5eraseERKNS0_6StringE(nullptr, &keyA));
    CHECK_THROWS(_ZN3sce4Json6Object5eraseERKNS0_6StringE(badObject, &keyA));
    CHECK_THROWS(_ZN3sce4Json6Object5eraseERKNS0_6StringE(&valid, nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object5eraseERKNS0_6StringE(&valid, badString));

    CHECK_THROWS(_ZNK3sce4Json6Object5beginEv(nullptr, &valid));
    CHECK_THROWS(_ZNK3sce4Json6Object5beginEv(badIter, &valid));
    CHECK_THROWS(_ZNK3sce4Json6Object5beginEv(roIter, &valid));
    CHECK_THROWS(_ZNK3sce4Json6Object5beginEv(&probeIter, nullptr));
    CHECK_THROWS(_ZNK3sce4Json6Object5beginEv(&probeIter, badObject));

    CHECK_THROWS(_ZNK3sce4Json6Object3endEv(nullptr, &valid));
    CHECK_THROWS(_ZNK3sce4Json6Object3endEv(badIter, &valid));
    CHECK_THROWS(_ZNK3sce4Json6Object3endEv(roIter, &valid));
    CHECK_THROWS(_ZNK3sce4Json6Object3endEv(&probeIter, nullptr));
    CHECK_THROWS(_ZNK3sce4Json6Object3endEv(&probeIter, badObject));

    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(nullptr, &valid, &keyA));
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(badIter, &valid, &keyA));
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(roIter, &valid, &keyA));
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(&probeIter, nullptr, &keyA));
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(&probeIter, badObject, &keyA));
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(&probeIter, &valid, nullptr));
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(&probeIter, &valid, badString));

    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(nullptr, &valid, &pairB));
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(badIter, &valid, &pairB));
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(roIter, &valid, &pairB));
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(&probeIter, nullptr, &pairB));
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(&probeIter, badObject, &pairB));
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(&probeIter, &valid, nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(&probeIter, &valid, badPair));
    Pair corruptKeyPair{};
    corruptKeyPair.key = corruptTextString;
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(&probeIter, &valid, &corruptKeyPair));

    CHECK_THROWS(_ZN3sce4Json6Object4PairC1Ev(nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1Ev(badPair));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1Ev(roPair));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC2Ev(roPair));

    Pair probePair{};
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(nullptr, &keyA, &val1));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(badPair, &keyA, &val1));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(roPair, &keyA, &val1));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&probePair, nullptr, &val1));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&probePair, badString, &val1));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&probePair, &corruptTextString, &val1));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&probePair, &keyA, nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&probePair, &keyA, badValue));

    CHECK_THROWS(_ZN3sce4Json6Object4PairD1Ev(nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object4PairD1Ev(badPair));
    CHECK_THROWS(_ZN3sce4Json6Object4PairD1Ev(roPair));
    CHECK_THROWS(_ZN3sce4Json6Object4PairD2Ev(roPair));

    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1Ev(nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1Ev(badIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1Ev(roIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC2Ev(roIter));

    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1ERKS2_(nullptr, &beginIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1ERKS2_(badIter, &beginIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1ERKS2_(roIter, &beginIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1ERKS2_(&probeIter, nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1ERKS2_(&probeIter, badIter));

    ObjectIterator badStateIter{static_cast<ObjectIteratorState*>(pages.unmapped)};
    ObjectIterator roStateIter{static_cast<ObjectIteratorState*>(pages.readOnly)};
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1ERKS2_(&probeIter, &badStateIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorD1Ev(nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorD1Ev(badIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorD1Ev(roIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorD2Ev(roIter));

    auto* roLiveIter = static_cast<ObjectIterator*>(pages.scratch);
    SetPageWritable(pages.scratch, pages.pageSize, true);
    CHECK(_ZNK3sce4Json6Object5beginEv(roLiveIter, &valid) == roLiveIter);
    const auto savedRoIterState = roLiveIter->state;
    CHECK(savedRoIterState != nullptr);
    const auto liveWithRoIterator = trace.live;
    SetPageWritable(pages.scratch, pages.pageSize, false);
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorD1Ev(roLiveIter));
    CHECK(roLiveIter->state == savedRoIterState && trace.live == liveWithRoIterator);
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorD2Ev(roLiveIter));
    CHECK(roLiveIter->state == savedRoIterState && trace.live == liveWithRoIterator);
    CHECK(_ZNK3sce4Json6Object8iteratordeEv(roLiveIter) == _ZNK3sce4Json6Object8iteratordeEv(&beginIter));
    SetPageWritable(pages.scratch, pages.pageSize, true);
    _ZN3sce4Json6Object8iteratorD1Ev(roLiveIter);
    CHECK(roLiveIter->state == nullptr && trace.live.size() == baselineLive);

    CHECK_THROWS(_ZN3sce4Json6Object8iteratorppEv(nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorppEv(badIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorppEv(&badStateIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorppEv(&roStateIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorppEi(&roStateIter, 0));

    CHECK_THROWS(_ZN3sce4Json6Object8iterator7advanceEm(nullptr, 1));
    CHECK_THROWS(_ZN3sce4Json6Object8iterator7advanceEm(badIter, 1));
    CHECK_THROWS(_ZN3sce4Json6Object8iterator7advanceEm(&badStateIter, 0));
    CHECK_THROWS(_ZN3sce4Json6Object8iterator7advanceEm(&badStateIter, 1));
    CHECK_THROWS(_ZN3sce4Json6Object8iterator7advanceEm(&roStateIter, 1));
    _ZN3sce4Json6Object8iterator7advanceEm(&roStateIter, 0);

    CHECK_THROWS(_ZNK3sce4Json6Object8iteratordeEv(nullptr));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratordeEv(badIter));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratordeEv(&badStateIter));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratorptEv(&badStateIter));

    CHECK_THROWS(_ZNK3sce4Json6Object8iteratoreqERKS2_(nullptr, &beginIter));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratoreqERKS2_(&beginIter, nullptr));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratoreqERKS2_(badIter, &beginIter));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratoreqERKS2_(&beginIter, badIter));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratoreqERKS2_(&badStateIter, &beginIter));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratoreqERKS2_(&beginIter, &badStateIter));

    CHECK_THROWS(_ZNK3sce4Json6Object8iteratorneERKS2_(nullptr, &beginIter));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratorneERKS2_(&beginIter, nullptr));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratorneERKS2_(badIter, &beginIter));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratorneERKS2_(&beginIter, badIter));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratorneERKS2_(&badStateIter, &beginIter));
    CHECK_THROWS(_ZNK3sce4Json6Object8iteratorneERKS2_(&beginIter, &badStateIter));

    CHECK_THROWS(_ZN3sce4Json6Object8iteratoraSERKS2_(nullptr, &beginIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratoraSERKS2_(&beginIter, nullptr));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratoraSERKS2_(badIter, &beginIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratoraSERKS2_(&beginIter, badIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratoraSERKS2_(&beginIter, &badStateIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratoraSERKS2_(&roStateIter, &beginIter));
    ObjectIterator emptyIter{};
    CHECK(_ZN3sce4Json6Object8iteratoraSERKS2_(roIter, &emptyIter) == roIter);
    auto* roNonNullIter = static_cast<ObjectIterator*>(pages.scratch);
    SetPageWritable(pages.scratch, pages.pageSize, true);
    roNonNullIter->state = beginIter.state;
    SetPageWritable(pages.scratch, pages.pageSize, false);
    CHECK_THROWS(_ZN3sce4Json6Object8iteratoraSERKS2_(roNonNullIter, &emptyIter));
    SetPageWritable(pages.scratch, pages.pageSize, true);
    roNonNullIter->state = nullptr;
    CHECK(trace.live.size() == baselineLive);

    auto* savedItems = valid.items;
    auto* savedBeginState = beginIter.state;
    auto* savedKeyText = keyA.text;
    auto* savedPairKeyText = pairB.key.text;

    CHECK_THROWS(_ZN3sce4Json6ObjectC1ERKS1_(&valid, &valid));
    CHECK(valid.items == savedItems);
    CHECK_THROWS(_ZNK3sce4Json6Object5beginEv(reinterpret_cast<ObjectIterator*>(&valid), &valid));
    CHECK(valid.items == savedItems);
    CHECK_THROWS(_ZNK3sce4Json6Object3endEv(reinterpret_cast<ObjectIterator*>(&valid), &valid));
    CHECK(valid.items == savedItems);
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(reinterpret_cast<ObjectIterator*>(&valid), &valid, &keyA));
    CHECK(valid.items == savedItems);
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(reinterpret_cast<ObjectIterator*>(&keyA), &valid, &keyA));
    CHECK(keyA.text == savedKeyText);
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(reinterpret_cast<ObjectIterator*>(keyA.text), &valid, &keyA));
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(reinterpret_cast<ObjectIterator*>(&valid), &valid, &pairB));
    CHECK(valid.items == savedItems);
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(reinterpret_cast<ObjectIterator*>(&pairB), &valid, &pairB));
    CHECK(pairB.key.text == savedPairKeyText);
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(reinterpret_cast<ObjectIterator*>(pairB.key.text), &valid, &pairB));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1ERKS2_(&beginIter, &beginIter));
    CHECK(beginIter.state == savedBeginState);
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1ERKS2_(reinterpret_cast<ObjectIterator*>(beginIter.state), &beginIter));
    CHECK_THROWS(_ZN3sce4Json6Object8iteratoraSERKS2_(reinterpret_cast<ObjectIterator*>(beginIter.state), &beginIter));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pairB, &pairB.key, &val1));
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pairB, &keyA, &pairB.value));
    CHECK(pairB.key.text == savedPairKeyText);

    alignas(8) std::byte overlapBuf[32]{};
    std::memcpy(overlapBuf, &valid, sizeof(Object));
    auto* partialObjA = reinterpret_cast<Object*>(overlapBuf);
    auto* partialObjB = reinterpret_cast<Object*>(overlapBuf + 4);
    CHECK_THROWS(_ZN3sce4Json6ObjectC1ERKS1_(partialObjB, partialObjA));
    CHECK_THROWS(_ZN3sce4Json6ObjectaSERKS1_(partialObjB, partialObjA));
    CHECK_THROWS(_ZNK3sce4Json6Object5beginEv(reinterpret_cast<ObjectIterator*>(overlapBuf + 4), partialObjA));
    CHECK_THROWS(_ZNK3sce4Json6Object3endEv(reinterpret_cast<ObjectIterator*>(overlapBuf + 4), partialObjA));
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(reinterpret_cast<ObjectIterator*>(overlapBuf + 4), partialObjA, &pairB));
    CHECK(std::memcmp(overlapBuf, &valid, sizeof(Object)) == 0);

    auto* sentinelState = reinterpret_cast<ObjectIteratorState*>(std::uintptr_t{0x1234});
    ObjectIterator preserved{sentinelState};
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(&preserved, badObject, &pairB));
    CHECK(preserved.state == sentinelState);
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(&preserved, &valid, badPair));
    CHECK(preserved.state == sentinelState);
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(&preserved, &valid, badString));
    CHECK(preserved.state == sentinelState);
    CHECK(trace.live.size() == baselineLive);

    auto* flipIter = static_cast<ObjectIterator*>(pages.scratch);
    auto* flipObj = static_cast<Object*>(pages.scratch);
    auto* flipPair = static_cast<Pair*>(pages.scratch);
    trace.protectPage = pages.scratch;
    trace.protectSize = pages.pageSize;

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZNK3sce4Json6Object5beginEv(flipIter, &valid));
    CHECK(trace.live.size() == baselineLive);

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZNK3sce4Json6Object3endEv(flipIter, &valid));
    CHECK(trace.live.size() == baselineLive);

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZNK3sce4Json6Object4findERKNS0_6StringE(flipIter, &valid, &keyA));
    CHECK(trace.live.size() == baselineLive);

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1Ev(flipIter));
    CHECK(trace.live.size() == baselineLive);

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json6Object8iteratorC1ERKS2_(flipIter, &beginIter));
    CHECK(trace.live.size() == baselineLive);

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json6Object6insertERKNS1_4PairE(flipIter, &valid, &pairB));
    CHECK(trace.live.size() == baselineLive);

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json6ObjectC1Ev(flipObj));
    CHECK(trace.live.size() == baselineLive);

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json6ObjectC1ERKS1_(flipObj, &valid));
    CHECK(trace.live.size() == baselineLive);

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1Ev(flipPair));
    CHECK(trace.live.size() == baselineLive);

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(flipPair, &keyA, &val1));
    CHECK(trace.live.size() == baselineLive);

    SetPageWritable(pages.scratch, pages.pageSize, true);
    trace.protectReadOnly = false;
    _ZN3sce4Json6ObjectC1Ev(flipObj);
    const auto withFlipObjLive = trace.live.size();
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json6ObjectaSERKS1_(flipObj, &valid));
    CHECK(trace.live.size() == withFlipObjLive);
    SetPageWritable(pages.scratch, pages.pageSize, true);
    _ZN3sce4Json6ObjectD1Ev(flipObj);
    CHECK(trace.live.size() == baselineLive);

    Object copy{};
    _ZN3sce4Json6ObjectC1ERKS1_(&copy, &valid);
    CHECK(_ZNK3sce4Json6Object4sizeEv(&copy) == 2);
    CHECK(_ZN3sce4Json6ObjectaSERKS1_(&copy, &copy) == &copy);
    CHECK(_ZNK3sce4Json6Object4sizeEv(&copy) == 2);
    _ZN3sce4Json6Object5eraseERKNS0_6StringE(&copy, &keyA);
    CHECK(_ZNK3sce4Json6Object4sizeEv(&copy) == 1);
    _ZN3sce4Json6Object5clearEv(&copy);
    CHECK(_ZNK3sce4Json6Object5emptyEv(&copy));

    ObjectIterator iterCopy{};
    _ZN3sce4Json6Object8iteratorC1ERKS2_(&iterCopy, &beginIter);
    CHECK(_ZNK3sce4Json6Object8iteratoreqERKS2_(&iterCopy, &beginIter));
    CHECK(_ZN3sce4Json6Object8iteratorppEv(&iterCopy) == &iterCopy);
    CHECK(_ZNK3sce4Json6Object8iteratoreqERKS2_(&iterCopy, &foundIter));
    CHECK(_ZN3sce4Json6Object8iteratoraSERKS2_(&iterCopy, &iterCopy) == &iterCopy);
    CHECK(_ZNK3sce4Json6Object8iteratoreqERKS2_(&iterCopy, &foundIter));
    _ZN3sce4Json6Object8iterator7advanceEm(&iterCopy, 1);
    CHECK(_ZNK3sce4Json6Object8iteratoreqERKS2_(&iterCopy, &endIter));

    _ZN3sce4Json6Object8iteratorD1Ev(&iterCopy);
    _ZN3sce4Json6Object8iteratorD1Ev(&foundIter);
    _ZN3sce4Json6Object8iteratorD1Ev(&endIter);
    _ZN3sce4Json6Object8iteratorD1Ev(&beginIter);
    _ZN3sce4Json6Object8iteratorD1Ev(&insertedB);
    _ZN3sce4Json6Object4PairD1Ev(&pairB);
    _ZN3sce4Json5ValueD1Ev(&val2);
    _ZN3sce4Json5ValueD1Ev(&val1);
    _ZN3sce4Json6StringD1Ev(&keyB);
    _ZN3sce4Json6StringD1Ev(&keyA);
    _ZN3sce4Json6ObjectD1Ev(&copy);
    _ZN3sce4Json6ObjectD1Ev(&valid);

    CHECK(_ZN3sce4Json11Initializer9terminateEv(initializer) == 0);
    _ZN3sce4Json11InitializerD1Ev(initializer);
    CHECK(trace.live.empty());
    return 0;
}
