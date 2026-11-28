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
    std::fprintf(stderr, "libSceJson2 Value guest pointer test failed at line %d\n", line);
    std::abort();
}

#define CHECK(cond) Require((cond), __LINE__)

template<typename TException, typename TCall>
void ExpectException(TCall&& call, int line) {
    bool threw = false;
    try {
        call();
    } catch (const TException&) {
        threw = true;
    }
    Require(threw, line);
}

#define CHECK_THROWS(expr) ExpectException<std::exception>([&] { expr; }, __LINE__)
#define CHECK_INVALID_ARG(expr) ExpectException<std::invalid_argument>([&] { expr; }, __LINE__)

struct Trace {
    std::map<void*, std::size_t> live;
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

int APS5_VABI DummySerializeCallback(String&, void*) { return 0; }
const Value& APS5_VABI DummyNullAccess(ValueType, const Value*, void* ctx) { return *static_cast<const Value*>(ctx); }
const Value& APS5_VABI DummyTypeMismatch(ValueType, const Value*, const Value*, void* ctx) { return *static_cast<const Value*>(ctx); }
Value* APS5_VABI DummySpecialFloat(Value* out, std::int32_t, double, const Value*, void*, bool*) { return out; }
void APS5_VABI DummyElementFailure(int, const String*, const std::size_t*, const Value*, const Value*, void*) {}

}

int main() {
    ProtectedPages pages;
    Trace trace;
    Allocator allocator;
    alignas(8) std::byte initializer[64]{};
    _ZN3sce4Json11InitializerC1Ev(initializer);
    Parameter parameter{&allocator, &trace, 0x100, 0, {}};
    CHECK(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(initializer, &parameter) == 0);

    auto* badValue = static_cast<Value*>(pages.unmapped);
    auto* roValue = static_cast<Value*>(pages.readOnly);
    auto* badString = static_cast<String*>(pages.unmapped);
    auto* badArray = static_cast<Array*>(pages.unmapped);
    auto* badObject = static_cast<Object*>(pages.unmapped);
    const auto* badText = static_cast<const char*>(pages.unmapped);

    String str{};
    _ZN3sce4Json6StringC1EPKc(&str, "hello");
    Array arr{};
    _ZN3sce4Json5ArrayC1Ev(&arr);
    Object obj{};
    _ZN3sce4Json6ObjectC1Ev(&obj);
    Value validInt{};
    _ZN3sce4Json5ValueC1El(&validInt, 42);
    Value validObj{};
    _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&validObj, &obj);
    Value* entry = _ZN3sce4Json6ObjectixERKNS0_6StringE(_ZN3sce4Json5Value11referObjectEv(&validObj), &str);
    _ZN3sce4Json5Value3setEl(entry, 99);
    String outStr{};
    _ZN3sce4Json6StringC1Ev(&outStr);

    String corruptTextStr{static_cast<Text*>(pages.unmapped)};
    String roTextStr{static_cast<Text*>(pages.readOnly)};

    auto* roInt = static_cast<Value*>(pages.scratch);
    SetPageWritable(pages.scratch, pages.pageSize, true);
    _ZN3sce4Json5ValueC1Em(roInt, 123456);
    SetPageWritable(pages.scratch, pages.pageSize, false);
    CHECK(*_ZNK3sce4Json5Value10getIntegerEv(roInt) == 123456);
    SetPageWritable(pages.scratch, pages.pageSize, true);
    _ZN3sce4Json5Value3setEl(roInt, 654321);
    SetPageWritable(pages.scratch, pages.pageSize, false);
    CHECK(*_ZNK3sce4Json5Value11getUIntegerEv(roInt) == 654321);
    SetPageWritable(pages.scratch, pages.pageSize, true);
    _ZN3sce4Json5ValueD1Ev(roInt);

    const auto baselineLive = trace.live.size();

    CHECK_THROWS(_ZN3sce4Json5ValueC1Ev(nullptr));
    CHECK_THROWS(_ZN3sce4Json5ValueC1Ev(badValue));
    CHECK_THROWS(_ZN3sce4Json5ValueC1Ev(roValue));
    CHECK_THROWS(_ZN3sce4Json5ValueC1Eb(roValue, true));
    CHECK_THROWS(_ZN3sce4Json5ValueC1El(roValue, 1));
    CHECK_THROWS(_ZN3sce4Json5ValueC1Em(roValue, 2));
    CHECK_THROWS(_ZN3sce4Json5ValueC1Ed(roValue, 3.5));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ENS0_9ValueTypeE(roValue, TypeArray));
    CHECK_THROWS(_ZN3sce4Json5ValueC1EPKc(roValue, "abc"));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKNS0_6StringE(roValue, &str));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKNS0_6StringE(&validInt, nullptr));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKNS0_6StringE(&validInt, badString));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKNS0_6StringE(&validInt, &corruptTextStr));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKNS0_5ArrayE(roValue, &arr));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&validInt, nullptr));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&validInt, badArray));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKNS0_6ObjectE(roValue, &obj));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&validInt, nullptr));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&validInt, badObject));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKS1_(roValue, &validInt));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKS1_(&validInt, nullptr));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKS1_(&validInt, badValue));
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKS1_(&validInt, &validInt));

    CHECK_THROWS(_ZN3sce4Json5ValueD1Ev(nullptr));
    CHECK_THROWS(_ZN3sce4Json5ValueD1Ev(badValue));
    CHECK_THROWS(_ZN3sce4Json5ValueD1Ev(roValue));

    CHECK_THROWS(_ZN3sce4Json5ValueaSERKS1_(nullptr, &validInt));
    CHECK_THROWS(_ZN3sce4Json5ValueaSERKS1_(badValue, &validInt));
    CHECK_THROWS(_ZN3sce4Json5ValueaSERKS1_(roValue, &validInt));
    CHECK_THROWS(_ZN3sce4Json5ValueaSERKS1_(&validInt, nullptr));
    CHECK_THROWS(_ZN3sce4Json5ValueaSERKS1_(&validInt, badValue));

    CHECK_THROWS(_ZN3sce4Json5Value4swapERS1_(nullptr, &validInt));
    CHECK_THROWS(_ZN3sce4Json5Value4swapERS1_(&validInt, nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value4swapERS1_(roValue, &validInt));
    CHECK_THROWS(_ZN3sce4Json5Value4swapERS1_(&validInt, roValue));
    CHECK_THROWS(_ZN3sce4Json5Value4swapERS1_(badValue, &validInt));

    CHECK_THROWS(_ZN3sce4Json5Value3setENS0_9ValueTypeE(roValue, TypeNull));
    CHECK_THROWS(_ZN3sce4Json5Value3setEb(roValue, true));
    CHECK_THROWS(_ZN3sce4Json5Value3setEl(roValue, 1));
    CHECK_THROWS(_ZN3sce4Json5Value3setEm(roValue, 2));
    CHECK_THROWS(_ZN3sce4Json5Value3setEd(roValue, 1.5));
    CHECK_THROWS(_ZN3sce4Json5Value3setEPKc(roValue, "x"));
    CHECK_INVALID_ARG(_ZN3sce4Json5Value3setEPKc(&validInt, nullptr));
    CHECK_INVALID_ARG(_ZN3sce4Json5Value3setEPKc(&validInt, badText));
    CHECK_THROWS(_ZN3sce4Json5Value3setERKNS0_6StringE(roValue, &str));
    CHECK_THROWS(_ZN3sce4Json5Value3setERKNS0_6StringE(&validInt, badString));
    CHECK_THROWS(_ZN3sce4Json5Value3setERKNS0_6StringE(&validInt, &corruptTextStr));
    CHECK_THROWS(_ZN3sce4Json5Value3setERKNS0_5ArrayE(roValue, &arr));
    CHECK_THROWS(_ZN3sce4Json5Value3setERKNS0_5ArrayE(&validInt, badArray));
    CHECK_THROWS(_ZN3sce4Json5Value3setERKNS0_6ObjectE(roValue, &obj));
    CHECK_THROWS(_ZN3sce4Json5Value3setERKNS0_6ObjectE(&validInt, badObject));
    CHECK_THROWS(_ZN3sce4Json5Value3setERKS1_(roValue, &validInt));
    CHECK_THROWS(_ZN3sce4Json5Value3setERKS1_(&validInt, badValue));

    CHECK_THROWS(_ZNK3sce4Json5Value7getTypeEv(nullptr));
    CHECK_THROWS(_ZNK3sce4Json5Value7getTypeEv(badValue));
    CHECK_THROWS(_ZNK3sce4Json5Value10getBooleanEv(nullptr));
    CHECK_THROWS(_ZNK3sce4Json5Value10getBooleanEv(badValue));
    CHECK_THROWS(_ZNK3sce4Json5Value10getIntegerEv(badValue));
    CHECK_THROWS(_ZNK3sce4Json5Value11getUIntegerEv(badValue));
    CHECK_THROWS(_ZNK3sce4Json5Value7getRealEv(badValue));
    CHECK_THROWS(_ZNK3sce4Json5Value9getStringEv(badValue));
    CHECK_THROWS(_ZNK3sce4Json5Value8getArrayEv(badValue));
    CHECK_THROWS(_ZNK3sce4Json5Value9getObjectEv(badValue));
    CHECK_THROWS(_ZNK3sce4Json5Value5countEv(nullptr));
    CHECK_THROWS(_ZNK3sce4Json5Value5countEv(badValue));

    CHECK_THROWS(_ZNK3sce4Json5ValueixERKNS0_6StringE(badValue, &str));
    CHECK_THROWS(_ZNK3sce4Json5ValueixERKNS0_6StringE(&validObj, nullptr));
    CHECK_THROWS(_ZNK3sce4Json5ValueixERKNS0_6StringE(&validObj, badString));
    CHECK_THROWS(_ZNK3sce4Json5ValueixERKNS0_6StringE(&validObj, &corruptTextStr));
    CHECK_THROWS(_ZNK3sce4Json5ValueixEPKc(badValue, "hello"));
    CHECK_THROWS(_ZNK3sce4Json5ValueixEPKc(&validObj, badText));
    CHECK_THROWS(_ZNK3sce4Json5ValueixEm(badValue, 0));
    CHECK_THROWS(_ZNK3sce4Json5Value8getValueEm(badValue, 0));
    CHECK_THROWS(_ZNK3sce4Json5Value8getValueERKNS0_6StringE(badValue, &str));

    CHECK_THROWS(_ZN3sce4Json5Value10referValueEm(nullptr, 0));
    CHECK_THROWS(_ZN3sce4Json5Value10referValueEm(badValue, 0));
    CHECK_THROWS(_ZN3sce4Json5Value10referValueERKNS0_6StringE(nullptr, &str));
    CHECK_THROWS(_ZN3sce4Json5Value10referValueERKNS0_6StringE(badValue, &str));
    CHECK(_ZN3sce4Json5Value10referValueERKNS0_6StringE(&validObj, nullptr) == nullptr);
    CHECK_THROWS(_ZN3sce4Json5Value10referValueERKNS0_6StringE(&validObj, badString));
    CHECK_THROWS(_ZN3sce4Json5Value10referValueERKNS0_6StringE(&validObj, &corruptTextStr));
    CHECK_THROWS(_ZN3sce4Json5Value10referArrayEv(nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value10referArrayEv(roValue));
    CHECK_THROWS(_ZN3sce4Json5Value11referObjectEv(nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value11referObjectEv(roValue));
    CHECK_THROWS(_ZN3sce4Json5Value11referStringEv(nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value11referStringEv(badValue));
    CHECK_THROWS(_ZN3sce4Json5Value12referBooleanEv(nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value12referBooleanEv(badValue));
    CHECK_THROWS(_ZN3sce4Json5Value12referIntegerEv(nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value12referIntegerEv(badValue));
    CHECK_THROWS(_ZN3sce4Json5Value13referUIntegerEv(nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value13referUIntegerEv(badValue));
    CHECK_THROWS(_ZN3sce4Json5Value9referRealEv(nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value9referRealEv(badValue));

    CHECK_THROWS(_ZN3sce4Json5Value9serializeEPFiRNS0_6StringEPvES4_(nullptr, DummySerializeCallback, nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeEPFiRNS0_6StringEPvES4_(badValue, DummySerializeCallback, nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(badValue, &outStr, DummySerializeCallback, nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(&validInt, badString, DummySerializeCallback, nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(&validInt, &corruptTextStr, DummySerializeCallback, nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(&validInt, &roTextStr, DummySerializeCallback, nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringE(badValue, &outStr));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringE(&validInt, badString));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringE(&validInt, &roTextStr));

    CHECK_THROWS(_ZNK3sce4Json5Value8toStringERNS0_6StringE(nullptr, &outStr));
    CHECK_THROWS(_ZNK3sce4Json5Value8toStringERNS0_6StringE(badValue, &outStr));
    CHECK_THROWS(_ZNK3sce4Json5Value8toStringERNS0_6StringE(&validInt, nullptr));
    CHECK_THROWS(_ZNK3sce4Json5Value8toStringERNS0_6StringE(&validInt, badString));
    CHECK_THROWS(_ZNK3sce4Json5Value8toStringERNS0_6StringE(&validInt, &corruptTextStr));
    CHECK_THROWS(_ZNK3sce4Json5Value8toStringERNS0_6StringE(&validInt, &roTextStr));
    auto* roString = static_cast<String*>(pages.readOnly);
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringE(&validInt, roString));
    CHECK_THROWS(_ZNK3sce4Json5Value8toStringERNS0_6StringE(&validInt, roString));
    Text roCharsText{static_cast<char*>(pages.readOnly), 0, 64, 0};
    String roCharsStr{&roCharsText};
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringE(&validInt, &roCharsStr));
    CHECK_THROWS(_ZNK3sce4Json5Value8toStringERNS0_6StringE(&validInt, &roCharsStr));

    _ZN3sce4Json5Value5clearEv(nullptr);
    CHECK_THROWS(_ZN3sce4Json5Value5clearEv(badValue));
    CHECK_THROWS(_ZN3sce4Json5Value5clearEv(roValue));

    CHECK_THROWS(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(roValue, DummyNullAccess, &validInt));
    CHECK_THROWS(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(roValue, DummyElementFailure, nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(roValue, DummySpecialFloat, nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(roValue, DummyTypeMismatch, &validInt));
    CHECK_THROWS(_ZNK3sce4Json5ValuecvbEv(nullptr));
    CHECK_THROWS(_ZNK3sce4Json5ValuecvbEv(badValue));

    alignas(8) std::byte overlapBuf[64]{};
    std::memcpy(overlapBuf, &validInt, sizeof(Value));
    auto* overlapA = reinterpret_cast<Value*>(overlapBuf);
    auto* overlapB = reinterpret_cast<Value*>(overlapBuf + 8);
    CHECK_THROWS(_ZN3sce4Json5ValueC1ERKS1_(overlapB, overlapA));
    CHECK_THROWS(_ZN3sce4Json5ValueaSERKS1_(overlapB, overlapA));
    CHECK_THROWS(_ZN3sce4Json5Value4swapERS1_(overlapB, overlapA));
    CHECK_THROWS(_ZN3sce4Json5Value3setERKS1_(overlapB, overlapA));
    CHECK_THROWS(_ZNK3sce4Json5Value8toStringERNS0_6StringE(overlapA, reinterpret_cast<String*>(overlapBuf + 8)));
    CHECK(std::memcmp(overlapBuf, &validInt, sizeof(Value)) == 0);

    CHECK(trace.live.size() == baselineLive);

    CHECK(_ZN3sce4Json5ValueaSERKS1_(&validInt, &validInt) == &validInt);
    CHECK(*_ZNK3sce4Json5Value10getIntegerEv(&validInt) == 42);
    CHECK(_ZNK3sce4Json5ValuecvbEv(&validInt));
    CHECK(_ZNK3sce4Json5Value5countEv(&validObj) == 1);
    CHECK(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5ValueixEPKc(&validObj, "hello")) == 99);
    CHECK(_ZN3sce4Json5Value9serializeERNS0_6StringE(&validInt, &outStr) == 0);
    CHECK(std::strcmp(_ZNK3sce4Json6String5c_strEv(&outStr), "42") == 0);

    _ZN3sce4Json6StringD1Ev(&outStr);
    _ZN3sce4Json5ValueD1Ev(&validObj);
    _ZN3sce4Json5ValueD1Ev(&validInt);
    _ZN3sce4Json6ObjectD1Ev(&obj);
    _ZN3sce4Json5ArrayD1Ev(&arr);
    _ZN3sce4Json6StringD1Ev(&str);
    CHECK(_ZN3sce4Json11Initializer9terminateEv(initializer) == 0);
    _ZN3sce4Json11InitializerD1Ev(initializer);
    CHECK(trace.live.empty());

    Value uninitValue{};
    _ZN3sce4Json5ValueC1Ev(&uninitValue);
    String validEmptyOut{};
    CHECK(static_cast<unsigned>(_ZN3sce4Json5Value9serializeERNS0_6StringE(&uninitValue, &validEmptyOut)) == 0x80848110);
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringE(&uninitValue, badString));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringE(&uninitValue, roString));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(&uninitValue, badString, DummySerializeCallback, nullptr));
    CHECK_THROWS(_ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(&uninitValue, roString, DummySerializeCallback, nullptr));
    CHECK(trace.live.empty());
    return 0;
}
