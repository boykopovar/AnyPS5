#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

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
    std::fprintf(stderr, "libSceJson2 Parser guest pointer test failed at line %d\n", line);
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
    std::size_t allocations = 0;
};

void* APS5_VABI TrackAllocate(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<Trace*>(context);
    void* memory = std::malloc(bytes);
    CHECK(memory != nullptr);
    std::memset(memory, 0, bytes);
    trace.live.emplace(memory, bytes);
    ++trace.allocations;
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

struct GuardedBuffer {
    std::size_t pageSize = 0;
    char* base = nullptr;
    void* unmapped = nullptr;
    void* readOnly = nullptr;

    GuardedBuffer() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE, PAGE_NOACCESS));
        CHECK(base != nullptr);
        CHECK(VirtualAlloc(base, pageSize, MEM_COMMIT, PAGE_READWRITE) == base);
        unmapped = base + pageSize;
        readOnly = VirtualAlloc(nullptr, pageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECK(readOnly != nullptr);
        std::memset(readOnly, 0, pageSize);
        DWORD oldProtect = 0;
        CHECK(VirtualProtect(readOnly, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* region = mmap(nullptr, pageSize * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(region != MAP_FAILED);
        CHECK(mprotect(region, pageSize, PROT_READ | PROT_WRITE) == 0);
        base = static_cast<char*>(region);
        unmapped = base + pageSize;
        readOnly = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(readOnly != MAP_FAILED);
        std::memset(readOnly, 0, pageSize);
        CHECK(mprotect(readOnly, pageSize, PROT_READ) == 0);
#endif
    }

    ~GuardedBuffer() {
#ifdef _WIN32
        if (base) VirtualFree(base, 0, MEM_RELEASE);
        if (readOnly) VirtualFree(readOnly, 0, MEM_RELEASE);
#else
        if (base) munmap(base, pageSize * 2);
        if (readOnly) munmap(readOnly, pageSize);
#endif
    }
};

struct CallbackSource {
    const char* text = nullptr;
    std::size_t length = 0;
    std::size_t offset = 0;
    std::size_t calls = 0;
};

int APS5_VABI ReadByte(char& out, void* context) {
    auto& source = *static_cast<CallbackSource*>(context);
    ++source.calls;
    if (source.offset >= source.length) return static_cast<int>(0x80848101);
    out = source.text[source.offset++];
    return 0;
}

}

int main() {
    GuardedBuffer pages;
    Trace trace;
    Allocator allocator;
    alignas(8) std::byte initializer[64]{};
    _ZN3sce4Json11InitializerC1Ev(initializer);
    Parameter parameter{&allocator, &trace, 0x100, 0, {}};
    CHECK(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(initializer, &parameter) == 0);

    auto* badValue = static_cast<Value*>(pages.unmapped);
    auto* roValue = static_cast<Value*>(pages.readOnly);
    const auto* badText = static_cast<const char*>(pages.unmapped);

    Value value{};
    _ZN3sce4Json5ValueC1El(&value, 77);
    const Value snapshot = value;
    const auto baseLive = trace.live.size();
    const auto baseAllocs = trace.allocations;

    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(nullptr, "true", 4));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(badValue, "true", 4));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(roValue, "true", 4));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(nullptr, "", 0));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(badValue, "", 0));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(roValue, "", 0));

    CHECK(static_cast<unsigned>(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, nullptr, 0)) == 0x80848101);
    CHECK(std::memcmp(&value, &snapshot, sizeof(Value)) == 0);

    CHECK_INVALID_ARG(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, nullptr, 4));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, badText, 4));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, pages.base, std::numeric_limits<std::size_t>::max()));

    char* tailJson = pages.base + pages.pageSize - 4;
    std::memcpy(tailJson, "true", 4);
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, tailJson, 8));
    CHECK(std::memcmp(&value, &snapshot, sizeof(Value)) == 0);

    alignas(8) std::byte overlapBuf[64]{};
    std::memcpy(overlapBuf, &value, sizeof(Value));
    auto* overlapVal = reinterpret_cast<Value*>(overlapBuf);
    const char* overlapText = reinterpret_cast<const char*>(overlapBuf + 16);
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(overlapVal, reinterpret_cast<const char*>(overlapVal), 4));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(overlapVal, overlapText, 4));
    CHECK(std::memcmp(overlapBuf, &value, sizeof(Value)) == 0);

    CallbackSource callback{"true", 4, 0, 0};
    CHECK_INVALID_ARG(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&value, nullptr, &callback));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(nullptr, ReadByte, &callback));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(badValue, ReadByte, &callback));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(roValue, ReadByte, &callback));
    CHECK(callback.calls == 0);

    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(nullptr, "/nonexistent.json"));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(badValue, "/nonexistent.json"));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(roValue, "/nonexistent.json"));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(&value, nullptr));
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(&value, badText));

    char* unterminatedPath = pages.base + pages.pageSize - 5;
    std::memcpy(unterminatedPath, "/abc/", 5);
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(&value, unterminatedPath));

    std::memcpy(overlapBuf, &value, sizeof(Value));
    std::memcpy(overlapBuf + 24, "a.j\0", 4);
    CHECK_THROWS(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc(overlapVal, reinterpret_cast<const char*>(overlapBuf + 24)));

    CHECK(trace.live.size() == baseLive);
    CHECK(trace.allocations == baseAllocs);

    std::string large = "{\"pad\":\"";
    large.append(600, 'x');
    large += "\",\"num\":12345}";
    CHECK(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, large.data(), large.size()) == 0);
    const Value* num = _ZNK3sce4Json5ValueixEPKc(&value, "num");
    CHECK(num != nullptr && *_ZNK3sce4Json5Value10getIntegerEv(num) == 12345);

    callback = {"{\"ok\":true}", 11, 0, 0};
    Value cbValue{};
    _ZN3sce4Json5ValueC1Ev(&cbValue);
    CHECK(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&cbValue, ReadByte, &callback) == 0);
    const Value* ok = _ZNK3sce4Json5ValueixEPKc(&cbValue, "ok");
    CHECK(ok != nullptr && *_ZNK3sce4Json5Value10getBooleanEv(ok));

    _ZN3sce4Json5ValueD1Ev(&cbValue);
    _ZN3sce4Json5ValueD1Ev(&value);
    CHECK(_ZN3sce4Json11Initializer9terminateEv(initializer) == 0);
    _ZN3sce4Json11InitializerD1Ev(initializer);
    CHECK(trace.live.empty());
    return 0;
}
