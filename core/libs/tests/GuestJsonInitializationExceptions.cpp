#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
#if defined(JSON2_EXCEPTION_LEGACY_SDK)
static constexpr unsigned ResourceCount = 11;
static constexpr std::size_t HeaderSize = 24;
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    static const std::uint32_t words[]{0, 0, 0, 0, 0x05000000};
    return words;
}
#else
static constexpr unsigned ResourceCount = 12;
static constexpr std::size_t HeaderSize = 32;
#endif
struct Marker { const void* identity; };
struct InitializationExceptionTrace {
    void* allocator = nullptr;
    unsigned target = 0;
    bool notifier = false;
    unsigned notices = 0;
    std::vector<std::size_t> requests;
    std::vector<void*> admitted;
    std::map<void*, std::size_t> live;
    unsigned releases = 0;
};
static void* APS5_VABI AllocateExceptionTracked(void* self, std::size_t size, void* data) {
    auto& trace = *static_cast<InitializationExceptionTrace*>(data);
    Require(self == trace.allocator, "Initialization must preserve allocator receiver and context");
    trace.requests.push_back(size);
    if (trace.requests.size() == trace.target + 1) {
        if (trace.notifier) return nullptr;
        throw Marker{&trace};
    }
    void* memory = std::malloc(size);
    Require(memory && trace.live.emplace(memory, trace.admitted.size()).second, "Every admitted allocation requires independent ownership");
    std::memset(memory, 0xa5, size);
    trace.admitted.push_back(memory);
    return memory;
}
static void APS5_VABI ReleaseExceptionTracked(void* self, void* memory, void* data) {
    auto& trace = *static_cast<InitializationExceptionTrace*>(data);
    Require(self == trace.allocator && (!memory || trace.live.erase(memory) == 1), "Normal termination must release each admitted generation exactly once");
    ++trace.releases;
    std::free(memory);
}
static void APS5_VABI NotifyExceptionTracked(void* self, int error, std::size_t size, void* data) {
    auto& trace = *static_cast<InitializationExceptionTrace*>(data);
    Require(trace.notifier && self == trace.allocator && error == static_cast<int>(0x80848102) && size == trace.requests.back(),
            "Notifier exceptions must retain receiver, context and the rejected allocation request");
    ++trace.notices;
    throw Marker{&trace};
}
static void* const Table[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateExceptionTracked), reinterpret_cast<void*>(ReleaseExceptionTracked), reinterpret_cast<void*>(NotifyExceptionTracked)};
struct ExceptionAllocator { void* const* table = Table; };
int main(int argc, char** argv) {
    Require(argc == 3, "Each exception schedule requires an independent module process");
    const auto target = static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10));
    Require(target <= ResourceCount, "Allocation schedule must include every initialization position and success");
    ExceptionAllocator allocator; InitializationExceptionTrace trace; trace.allocator = &allocator; trace.target = target; trace.notifier = std::strcmp(argv[2], "notify") == 0;
    Require(trace.notifier || std::strcmp(argv[2], "allocate") == 0, "Each schedule requires an allocation or notifier exception");
    Parameter parameter{&allocator, &trace, 16384, 0, {}};
    std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
    bool caught = false;
    try {
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, reinterpret_cast<const InitParameter2*>(&parameter)) == (trace.notifier && !target ? static_cast<int>(0x80848102) : 0), "Only an unpublished null context may return OOM without notifying");
    } catch (const Marker& marker) {
        Require(marker.identity == &trace, "Allocator exceptions must propagate with their original identity");
        caught = true;
    }
    Require(caught == (target < ResourceCount && (!trace.notifier || target)) && owner == unsigned(target == ResourceCount) && trace.releases == 0 && trace.live.size() == target && trace.notices == unsigned(trace.notifier && target && target < ResourceCount),
            "Thrown initialization callbacks must retain every prior allocation without publishing an active owner or rolling back");
    const std::array<std::size_t, 12> sizes{24, 8, 24, 8, HeaderSize, 48, 8, HeaderSize, 104, 32, 8, 8};
    const auto count = target < ResourceCount ? target + 1 : ResourceCount;
    Require(trace.requests == std::vector<std::size_t>(sizes.begin(), sizes.begin() + count), "Exception schedules must cross the exact initialization resource sequence");
    const auto live = trace.live;
    const auto requests = trace.requests;
    const int notInitialized = static_cast<int>(0x80848110), alreadyInitialized = static_cast<int>(0x80848111);
    Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == (target < ResourceCount ? notInitialized : 0), "An inactive owner must not reclaim the published partial context");
    trace.target = 1000;
    if (target && caught) {
        std::uint8_t other = 0; _ZN3sce4Json11InitializerC1Ev(&other);
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, reinterpret_cast<const InitParameter2*>(&parameter)) == alreadyInitialized &&
                _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&other, reinterpret_cast<const InitParameter2*>(&parameter)) == alreadyInitialized &&
                _ZN3sce4Json11Initializer9terminateEv(&other) == notInitialized && !owner && !other && trace.live == live && trace.requests == requests && !trace.releases,
                "A published partial context must reject retries from both owners without changing allocations");
        if (target > 10) Require(scePthreadMutexDestroy(reinterpret_cast<PthreadMutex*>(trace.admitted[10])) == 0, "The fixture must reclaim its orphaned host mutex separately");
        for (const auto& [pointer, generation] : trace.live) std::free(pointer);
        trace.live.clear();
    } else {
        Require(trace.live.empty() && trace.releases == (target == ResourceCount ? 12u : 0u), "Normal termination must reclaim all twelve resources");
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, reinterpret_cast<const InitParameter2*>(&parameter)) == 0 && owner && trace.live.size() == ResourceCount,
                "Unpublished exceptions and completed lifecycles must allow a fresh initialization");
        Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.live.empty(), "Fresh initialization must retain complete teardown");
    }
    return 0;
}
