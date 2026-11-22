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
    std::fprintf(stderr, "libSceJson2 Array guest pointer test failed at line %d\n", line);
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
    reinterpret_cast<void*>(TrackAllocate),
    reinterpret_cast<void*>(TrackFree),
    reinterpret_cast<void*>(TrackNotify),
};

struct Allocator {
    void* const* vtable = Vtable;
};

struct GuardPages {
    std::byte* base = nullptr;
    std::size_t pageSize = 0;

    GuardPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<std::byte*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        CHECK(base != nullptr);
        DWORD oldProtect = 0;
        CHECK(VirtualProtect(base + pageSize, pageSize, PAGE_NOACCESS, &oldProtect) != 0);
        CHECK(VirtualProtect(base + pageSize * 2, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(mapped != MAP_FAILED);
        base = static_cast<std::byte*>(mapped);
        CHECK(mprotect(base + pageSize, pageSize, PROT_NONE) == 0);
        CHECK(mprotect(base + pageSize * 2, pageSize, PROT_READ) == 0);
#endif
    }

    ~GuardPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    std::byte* NoAccess() const { return base + pageSize; }
    std::byte* Straddle() const { return base + pageSize - 4; }
    std::byte* ReadOnly() const { return base + pageSize * 2; }
};

}

int main() {
    Trace trace;
    Allocator allocator;
    Parameter parameter{&allocator, &trace, 16384, 2, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    CHECK(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0);

    GuardPages pages;
    auto* badArray = reinterpret_cast<Array*>(pages.NoAccess());
    auto* straddleArray = reinterpret_cast<Array*>(pages.Straddle());
    auto* roArray = reinterpret_cast<Array*>(pages.ReadOnly());
    auto* badIter = reinterpret_cast<ArrayIterator*>(pages.NoAccess());
    auto* straddleIter = reinterpret_cast<ArrayIterator*>(pages.Straddle());
    auto* roIter = reinterpret_cast<ArrayIterator*>(pages.ReadOnly());
    auto* badValue = reinterpret_cast<Value*>(pages.NoAccess());
    auto* straddleValue = reinterpret_cast<Value*>(pages.Straddle());

    const auto baseline = trace.live.size();

    CHECK_THROWS(_ZN3sce4Json5ArrayC1Ev(nullptr));
    CHECK_THROWS(_ZN3sce4Json5ArrayC1Ev(badArray));
    CHECK_THROWS(_ZN3sce4Json5ArrayC1Ev(straddleArray));
    CHECK_THROWS(_ZN3sce4Json5ArrayC1Ev(roArray));
    CHECK(trace.live.size() == baseline);

    Array valid{};
    _ZN3sce4Json5ArrayC1Ev(&valid);
    Value val1{};
    _ZN3sce4Json5ValueC1El(&val1, 42);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&valid, &val1);
    Value val2{};
    _ZN3sce4Json5ValueC1El(&val2, 99);
    _ZN3sce4Json5Array10push_frontERKNS0_5ValueE(&valid, &val2);
    CHECK(_ZNK3sce4Json5Array4sizeEv(&valid) == 2);
    CHECK(!_ZNK3sce4Json5Array5emptyEv(&valid));

    const auto afterValid = trace.live.size();

    CHECK_THROWS(_ZN3sce4Json5ArrayC1ERKS1_(badArray, &valid));
    CHECK_THROWS(_ZN3sce4Json5ArrayC1ERKS1_(roArray, &valid));
    Array unusedTarget{};
    CHECK_THROWS(_ZN3sce4Json5ArrayC1ERKS1_(&unusedTarget, badArray));
    CHECK_THROWS(_ZN3sce4Json5ArrayC1ERKS1_(&unusedTarget, straddleArray));
    CHECK_THROWS(_ZN3sce4Json5ArrayD1Ev(badArray));
    CHECK_THROWS(_ZN3sce4Json5ArrayD1Ev(roArray));
    CHECK_THROWS(_ZN3sce4Json5ArrayaSERKS1_(badArray, &valid));
    CHECK_THROWS(_ZN3sce4Json5ArrayaSERKS1_(roArray, &valid));
    CHECK_THROWS(_ZN3sce4Json5ArrayaSERKS1_(&valid, badArray));
    CHECK_THROWS(_ZN3sce4Json5Array10push_frontERKNS0_5ValueE(badArray, &val1));
    CHECK_THROWS(_ZN3sce4Json5Array10push_frontERKNS0_5ValueE(&valid, badValue));
    CHECK_THROWS(_ZN3sce4Json5Array10push_frontERKNS0_5ValueE(&valid, straddleValue));
    CHECK_THROWS(_ZN3sce4Json5Array9push_backERKNS0_5ValueE(badArray, &val1));
    CHECK_THROWS(_ZN3sce4Json5Array9push_backERKNS0_5ValueE(&valid, badValue));
    CHECK_THROWS(_ZN3sce4Json5Array9push_backERKNS0_5ValueE(&valid, straddleValue));
    CHECK_THROWS(_ZNK3sce4Json5Array5emptyEv(badArray));
    CHECK_THROWS(_ZNK3sce4Json5Array5emptyEv(straddleArray));
    CHECK_THROWS(_ZNK3sce4Json5Array4sizeEv(badArray));
    CHECK_THROWS(_ZN3sce4Json5Array5clearEv(badArray));
    CHECK_THROWS(_ZNK3sce4Json5Array5frontEv(badArray));
    CHECK_THROWS(_ZNK3sce4Json5Array4backEv(badArray));
    CHECK_THROWS(_ZN3sce4Json5Array8pop_backEv(badArray));
    CHECK_THROWS(_ZN3sce4Json5Array9pop_frontEv(badArray));
    CHECK(trace.live.size() == afterValid);

    ArrayIterator beginIter{};
    _ZNK3sce4Json5Array5beginEv(&beginIter, &valid);
    ArrayIterator endIter{};
    _ZNK3sce4Json5Array3endEv(&endIter, &valid);
    CHECK(_ZNK3sce4Json5Array8iteratorneERKS2_(&beginIter, &endIter));

    const auto afterIters = trace.live.size();

    CHECK_THROWS(_ZNK3sce4Json5Array5beginEv(badIter, &valid));
    CHECK_THROWS(_ZNK3sce4Json5Array5beginEv(roIter, &valid));
    ArrayIterator probeResult{};
    CHECK_THROWS(_ZNK3sce4Json5Array5beginEv(&probeResult, badArray));
    CHECK_THROWS(_ZNK3sce4Json5Array3endEv(badIter, &valid));
    CHECK_THROWS(_ZNK3sce4Json5Array3endEv(roIter, &valid));
    CHECK_THROWS(_ZNK3sce4Json5Array3endEv(&probeResult, badArray));
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(badIter, &valid, &beginIter, &val1));
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(straddleIter, &valid, &beginIter, &val1));
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(roIter, &valid, &beginIter, &val1));
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&probeResult, badArray, &beginIter, &val1));
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&probeResult, &valid, badIter, &val1));
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&probeResult, &valid, &beginIter, badValue));
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&probeResult, &valid, &beginIter, straddleValue));
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(badIter, &valid, &beginIter));
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(straddleIter, &valid, &beginIter));
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(roIter, &valid, &beginIter));
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&probeResult, badArray, &beginIter));
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&probeResult, &valid, badIter));
    CHECK(trace.live.size() == afterIters);

    auto* flipIter = reinterpret_cast<ArrayIterator*>(pages.base);
    trace.protectPage = pages.base;
    trace.protectSize = pages.pageSize;
    Array absent{};

    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(flipIter, &absent, &beginIter, &val1));
    SetPageWritable(pages.base, pages.pageSize, true);
    CHECK(trace.live.size() == afterIters);

    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(flipIter, &absent, &beginIter));
    SetPageWritable(pages.base, pages.pageSize, true);
    CHECK(trace.live.size() == afterIters);

    trace.protectReadOnly = true;
    CHECK_THROWS(_ZNK3sce4Json5Array5beginEv(flipIter, &valid));
    SetPageWritable(pages.base, pages.pageSize, true);
    CHECK(trace.live.size() == afterIters);

    trace.protectReadOnly = true;
    CHECK_THROWS(_ZNK3sce4Json5Array3endEv(flipIter, &valid));
    SetPageWritable(pages.base, pages.pageSize, true);
    CHECK(trace.live.size() == afterIters);

    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1Ev(flipIter));
    SetPageWritable(pages.base, pages.pageSize, true);
    CHECK(trace.live.size() == afterIters);

    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1ERKS2_(flipIter, &beginIter));
    SetPageWritable(pages.base, pages.pageSize, true);
    CHECK(trace.live.size() == afterIters);

    Array publishFail{};
    _ZN3sce4Json5ArrayC1Ev(&publishFail);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&publishFail, &val1);
    ArrayIterator pfBegin{};
    _ZNK3sce4Json5Array5beginEv(&pfBegin, &publishFail);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(flipIter, &publishFail, &pfBegin, &val2));
    SetPageWritable(pages.base, pages.pageSize, true);
    trace.protectReadOnly = true;
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(flipIter, &publishFail, &pfBegin));
    SetPageWritable(pages.base, pages.pageSize, true);
    _ZN3sce4Json5Array8iteratorD1Ev(&pfBegin);
    _ZN3sce4Json5ArrayD1Ev(&publishFail);
    CHECK(trace.live.size() == afterIters);
    trace.protectPage = nullptr;

    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1Ev(badIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1Ev(roIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1ERKS2_(badIter, &beginIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1ERKS2_(roIter, &beginIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1ERKS2_(&probeResult, badIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1ERKS2_(&probeResult, straddleIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorD1Ev(badIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorD1Ev(roIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratoraSERKS2_(badIter, &beginIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratoraSERKS2_(&beginIter, badIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorppEv(badIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorppEi(badIter, 0));
    CHECK_THROWS(_ZNK3sce4Json5Array8iteratordeEv(badIter));
    CHECK_THROWS(_ZNK3sce4Json5Array8iteratorptEv(badIter));
    CHECK_THROWS(_ZNK3sce4Json5Array8iteratorneERKS2_(badIter, &endIter));
    CHECK_THROWS(_ZNK3sce4Json5Array8iteratorneERKS2_(&beginIter, badIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iterator7advanceEm(badIter, 1));

    ArrayIterator corruptStateIter{reinterpret_cast<ArrayIteratorState*>(pages.NoAccess())};
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1ERKS2_(&probeResult, &corruptStateIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratoraSERKS2_(&beginIter, &corruptStateIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorppEv(&corruptStateIter));
    CHECK_THROWS(_ZNK3sce4Json5Array8iteratordeEv(&corruptStateIter));
    CHECK_THROWS(_ZNK3sce4Json5Array8iteratorneERKS2_(&corruptStateIter, &endIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iterator7advanceEm(&corruptStateIter, 1));
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&probeResult, &valid, &corruptStateIter, &val1));
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&probeResult, &valid, &corruptStateIter));
    CHECK(trace.live.size() == afterIters);

    auto* const savedValidItems = valid.items;
    auto* const savedBeginState = beginIter.state;
    auto* const savedBeginElement = _ZNK3sce4Json5Array8iteratordeEv(&beginIter);
    const Value savedVal1 = val1;

    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(reinterpret_cast<ArrayIterator*>(&valid), &valid, &beginIter, &val1));
    CHECK(valid.items == savedValidItems && _ZNK3sce4Json5Array4sizeEv(&valid) == 2);
    CHECK(beginIter.state == savedBeginState && _ZNK3sce4Json5Array8iteratordeEv(&beginIter) == savedBeginElement);
    CHECK(trace.live.size() == afterIters);

    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(reinterpret_cast<ArrayIterator*>(&valid), &valid, &beginIter));
    CHECK(valid.items == savedValidItems && _ZNK3sce4Json5Array4sizeEv(&valid) == 2);
    CHECK(beginIter.state == savedBeginState && _ZNK3sce4Json5Array8iteratordeEv(&beginIter) == savedBeginElement);
    CHECK(trace.live.size() == afterIters);

    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&beginIter, &valid, &beginIter, &val1));
    CHECK(valid.items == savedValidItems && _ZNK3sce4Json5Array4sizeEv(&valid) == 2);
    CHECK(beginIter.state == savedBeginState && _ZNK3sce4Json5Array8iteratordeEv(&beginIter) == savedBeginElement);
    CHECK(trace.live.size() == afterIters);

    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&beginIter, &valid, &beginIter));
    CHECK(valid.items == savedValidItems && _ZNK3sce4Json5Array4sizeEv(&valid) == 2);
    CHECK(beginIter.state == savedBeginState && _ZNK3sce4Json5Array8iteratordeEv(&beginIter) == savedBeginElement);
    CHECK(trace.live.size() == afterIters);

    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(reinterpret_cast<ArrayIterator*>(beginIter.state), &valid, &beginIter, &val1));
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(reinterpret_cast<ArrayIterator*>(beginIter.state), &valid, &beginIter));
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(reinterpret_cast<ArrayIterator*>(&val1), &valid, &beginIter, &val1));
    CHECK(std::memcmp(&val1, &savedVal1, sizeof(Value)) == 0);
    CHECK(valid.items == savedValidItems && _ZNK3sce4Json5Array4sizeEv(&valid) == 2);
    CHECK(beginIter.state == savedBeginState && _ZNK3sce4Json5Array8iteratordeEv(&beginIter) == savedBeginElement);
    CHECK(trace.live.size() == afterIters);

    alignas(8) std::byte overlapStorage[sizeof(Array) + sizeof(ArrayIterator)]{};
    auto* overlapArray = reinterpret_cast<Array*>(overlapStorage + 4);
    overlapArray->items = valid.items;
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(reinterpret_cast<ArrayIterator*>(overlapStorage), overlapArray, &beginIter, &val1));
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(reinterpret_cast<ArrayIterator*>(overlapStorage), overlapArray, &beginIter));
    CHECK_THROWS(_ZNK3sce4Json5Array5beginEv(reinterpret_cast<ArrayIterator*>(overlapStorage), overlapArray));
    CHECK_THROWS(_ZNK3sce4Json5Array3endEv(reinterpret_cast<ArrayIterator*>(overlapStorage), overlapArray));
    CHECK(overlapArray->items == savedValidItems && _ZNK3sce4Json5Array4sizeEv(&valid) == 2);
    CHECK(trace.live.size() == afterIters);

    CHECK_THROWS(_ZNK3sce4Json5Array5beginEv(reinterpret_cast<ArrayIterator*>(&valid), &valid));
    CHECK_THROWS(_ZNK3sce4Json5Array3endEv(reinterpret_cast<ArrayIterator*>(&valid), &valid));
    CHECK(valid.items == savedValidItems && _ZNK3sce4Json5Array4sizeEv(&valid) == 2);
    CHECK_THROWS(_ZN3sce4Json5ArrayC1ERKS1_(&valid, &valid));
    CHECK(valid.items == savedValidItems && _ZNK3sce4Json5Array4sizeEv(&valid) == 2);
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1ERKS2_(&beginIter, &beginIter));
    CHECK_THROWS(_ZN3sce4Json5Array8iteratorC1ERKS2_(reinterpret_cast<ArrayIterator*>(beginIter.state), &beginIter));
    CHECK(beginIter.state == savedBeginState && _ZNK3sce4Json5Array8iteratordeEv(&beginIter) == savedBeginElement);
    CHECK(trace.live.size() == afterIters);

    auto* const sentinelPtr = reinterpret_cast<ArrayIteratorState*>(std::uintptr_t{0x12345678u});
    ArrayIterator preservedResult{sentinelPtr};
    CHECK_THROWS(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&preservedResult, &valid, &corruptStateIter, &val1));
    CHECK(preservedResult.state == sentinelPtr);
    CHECK_THROWS(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&preservedResult, &valid, &corruptStateIter));
    CHECK(preservedResult.state == sentinelPtr);
    CHECK(trace.live.size() == afterIters);

    Array copy{};
    _ZN3sce4Json5ArrayC1ERKS1_(&copy, &valid);
    CHECK(_ZNK3sce4Json5Array4sizeEv(&copy) == 2);
    _ZN3sce4Json5Array8iteratorD1Ev(&endIter);
    _ZN3sce4Json5Array8iteratorD1Ev(&beginIter);
    _ZN3sce4Json5ArrayD1Ev(&copy);
    _ZN3sce4Json5ArrayD1Ev(&valid);
    _ZN3sce4Json5ValueD1Ev(&val2);
    _ZN3sce4Json5ValueD1Ev(&val1);
    CHECK(trace.live.size() == baseline);

    CHECK(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0);
    _ZN3sce4Json11InitializerD1Ev(&initializer);
    CHECK(trace.live.empty());
    return 0;
}
