#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

struct Value { Value* parent; void* auxiliary; std::uintptr_t payload; unsigned reserved; int type; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; int mode; unsigned reserved[3]; };
using NullHandler = const Value& (APS5_VABI*)(int, const Value*, void*);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value3setERKS1_(Value*, const Value*);
Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1_(Value*, const Value*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(Value*, NullHandler, void*);
}
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    static const std::uint32_t words[]{0, 0, 0, 0, 0x05000000};
    return words;
}
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct LegacySelfTrace {
    std::map<void*, std::size_t> live;
    std::map<void*, unsigned> generations;
    std::vector<std::size_t> requests;
    std::vector<void*> releases;
    unsigned nextGeneration = 0;
    unsigned notices = 0;
    unsigned nullFrees = 0;
    int budget = -1;
};
static void* APS5_VABI AllocateSelf(void*, std::size_t size, void* data) {
    auto& trace = *static_cast<LegacySelfTrace*>(data); trace.requests.push_back(size);
    if (!trace.budget) return nullptr;
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(size);
    Require(memory && trace.live.emplace(memory, size).second && trace.generations.emplace(memory, ++trace.nextGeneration).second, "Self-setting allocations require distinct live generations");
    std::memset(memory, 0xa5, size); return memory;
}
static void APS5_VABI ReleaseSelf(void*, void* memory, void* data) {
    auto& trace = *static_cast<LegacySelfTrace*>(data);
    if (!memory) { ++trace.nullFrees; return; }
    Require(trace.live.erase(memory) == 1 && trace.generations.erase(memory) == 1, "Legacy self-setting must release each owned allocation exactly once");
    trace.releases.push_back(memory); std::free(memory);
}
static void APS5_VABI NotifySelf(void*, int error, std::size_t size, void* data) {
    auto& trace = *static_cast<LegacySelfTrace*>(data);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == size, "Legacy self-setting failure must identify the rejected sentinel");
    ++trace.notices;
}
static void* const SelfTable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateSelf), reinterpret_cast<void*>(ReleaseSelf), reinterpret_cast<void*>(NotifySelf)};
struct LegacySelfAllocator { void* const* table = SelfTable; };
int main() {
    for (bool object : {false, true}) for (bool setter : {false, true}) for (unsigned budget = 0; budget <= 2; ++budget) {
        LegacySelfTrace trace; LegacySelfAllocator allocator; Parameter parameter{&allocator, &trace, 16384, 2, {}};
        std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, &parameter) == 0 && trace.live.size() == 11, "Initialize the eleven-resource legacy module");
        Value value{}; _ZN3sce4Json5ValueC1Ev(&value);
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, object ? "{}" : "[]", 2) == 0 &&
                _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&value, nullptr, &trace) == 0, "Prepare legacy collection ownership and auxiliary callback context");
        const Value before = value; const auto generations = trace.generations;
        void* header = *reinterpret_cast<void**>(value.payload); void* sentinel = *static_cast<void**>(header);
        Require(trace.live.at(header) == 24 && trace.live.at(sentinel) == (object ? 104u : 48u), "Legacy collections must retain their 24-byte header and actual sentinel");
        std::map<void*, std::vector<unsigned char>> snapshots;
        for (const auto& [pointer, size] : trace.live) {
            if (pointer == header || pointer == sentinel) continue;
            const auto* bytes = static_cast<const unsigned char*>(pointer); snapshots.emplace(pointer, std::vector<unsigned char>{bytes, bytes + size});
        }
        const auto requests = trace.requests.size(); const auto releases = trace.releases.size(); const auto notices = trace.notices; const auto nullFrees = trace.nullFrees;
        trace.budget = static_cast<int>(budget);
        if (setter) _ZN3sce4Json5Value3setERKS1_(&value, &value);
        else Require(_ZN3sce4Json5ValueaSERKS1_(&value, &value) == &value, "Legacy self-assignment must return its receiver");
        trace.budget = -1;
        auto* words = static_cast<std::uintptr_t*>(header);
        Require(std::memcmp(&value, &before, sizeof(value)) == 0 && *reinterpret_cast<void**>(value.payload) == header && words[1] == 0 &&
                trace.requests.size() == requests + 1 && trace.requests.back() == (object ? 104u : 48u) && trace.notices == notices + unsigned(!budget) &&
                trace.releases.size() == releases + 1 && trace.releases.back() == sentinel && trace.nullFrees == nullFrees,
                "Legacy self-use must keep its header and metadata while releasing the original sentinel even when replacement admission fails");
        Require((words[0] != 0) == bool(budget), "Failed legacy replacement must retain a missing sentinel rather than restore the old one");
        if (budget) Require(reinterpret_cast<void*>(words[0]) != sentinel && trace.live.at(reinterpret_cast<void*>(words[0])) == (object ? 104u : 48u), "Successful legacy replacement must publish a fresh sentinel");
        for (const auto& [pointer, generation] : generations) if (pointer != sentinel) Require(trace.generations.at(pointer) == generation, "Self-use must preserve all non-sentinel allocation generations");
        for (const auto& [pointer, bytes] : snapshots) Require(std::memcmp(pointer, bytes.data(), bytes.size()) == 0, "Self-use must preserve callback, fallback, handle and module bytes");
        const auto retryRequests = trace.requests.size(); const auto retryReleases = trace.releases.size(); const auto retryNullFrees = trace.nullFrees; const auto retryNotices = trace.notices;
        const auto previousSentinel = words[0];
        if (setter) _ZN3sce4Json5Value3setERKS1_(&value, &value);
        else Require(_ZN3sce4Json5ValueaSERKS1_(&value, &value) == &value, "Legacy self-use retry must return its original receiver");
        Require(words[0] && words[0] != previousSentinel && words[1] == 0 && std::memcmp(&value, &before, sizeof(value)) == 0 &&
                trace.requests.size() == retryRequests + 1 && trace.requests.back() == (object ? 104u : 48u) && trace.releases.size() == retryReleases + unsigned(previousSentinel != 0) &&
                trace.nullFrees == retryNullFrees && trace.notices == retryNotices,
                "Same-Value retry must repair a missing sentinel without fabricating a null free or replacing its header and metadata");
        _ZN3sce4Json5ValueD1Ev(&value);
        Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.live.empty() && trace.generations.empty(), "Partial legacy collections and module resources must remain destructible");
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, &parameter) == 0 && _ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && trace.live.empty(), "A completed legacy self-use case must permit a fresh module generation");
    }
    return 0;
}
