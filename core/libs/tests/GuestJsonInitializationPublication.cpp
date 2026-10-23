#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
}
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static int Initialize(bool legacy, void* self, const Parameter* parameter) {
    return legacy ? _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(self, parameter) :
                    _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(self, parameter);
}
enum class Publication { Handoff, CompleteNested, Overwrite };
struct AllocationIdentity {
    std::size_t size;
    std::size_t generation;
    bool operator==(const AllocationIdentity&) const = default;
};
struct PublicationTrace {
    std::map<void*, AllocationIdentity> live;
    std::map<void*, AllocationIdentity> nested;
    std::vector<std::size_t> releases;
    std::size_t generation = 0;
    std::size_t nestedGeneration = 0;
    Publication action = Publication::Handoff;
    bool armed = false;
    bool nestedLegacy = false;
    std::uint8_t* nestedOwner = nullptr;
    const Parameter* nestedParameter = nullptr;
};
static void* APS5_VABI AllocatePublication(void*, std::size_t size, void* context) {
    auto& trace = *static_cast<PublicationTrace*>(context);
    if (trace.armed) {
        trace.armed = false;
        Require(Initialize(trace.nestedLegacy, trace.nestedOwner, trace.nestedParameter) == 0 && *trace.nestedOwner == 1 && trace.live.size() == 12,
                "Nested initialization in the unpublished-context callback must publish its actual complete resource generation");
        trace.nested = trace.live; trace.nestedGeneration = trace.generation;
        if (trace.action == Publication::CompleteNested)
            Require(_ZN3sce4Json11Initializer9terminateEv(trace.nestedOwner) == 0 && trace.live.empty() && *trace.nestedOwner == 0, "The initial allocation callback may complete a nested module lifecycle before returning its allocation");
        if (trace.action == Publication::Handoff) return nullptr;
    }
    void* memory = std::malloc(size);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, size);
    Require(trace.live.emplace(memory, AllocationIdentity{size, ++trace.generation}).second, "Publication resources must have independently owned live generations");
    return memory;
}
static void APS5_VABI FreePublication(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<PublicationTrace*>(context);
    trace.releases.push_back(trace.live.at(memory).generation);
    Require(trace.live.erase(memory) == 1, "Publication cleanup must release each allocation through its owning allocator exactly once");
    std::free(memory);
}
static void APS5_VABI NotifyPublication(void*, int, std::size_t, void*) { Require(false, "Failure of the unpublished context allocation must omit allocator notification"); }
static void* const PublicationVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocatePublication), reinterpret_cast<void*>(FreePublication), reinterpret_cast<void*>(NotifyPublication)};
struct PublicationAllocator { void* const* vtable = PublicationVtable; };
static void ReclaimOrphanedFixtureEpoch(PublicationTrace& trace) {
    Require(trace.live == trace.nested && trace.live.size() == 12, "The fixture may reclaim only the exact orphaned epoch already observed after library teardown");
    unsigned mutexes = 0;
    for (const auto& [pointer, identity] : trace.nested) {
        if (identity.generation + 1 >= trace.nestedGeneration) {
            Require(identity.size == sizeof(PthreadMutex) && scePthreadMutexDestroy(static_cast<PthreadMutex*>(pointer)) == 0,
                    "Fixture cleanup must destroy the two real orphaned kernel mutex resources before reclaiming their handle storage");
            ++mutexes;
        }
    }
    Require(mutexes == 2, "The SDK 6 orphaned epoch must retain its two actual mutex handles");
    for (const auto& [pointer, identity] : trace.nested) {
        Require(trace.live.at(pointer) == identity, "Fixture reclamation must retain the observed orphan allocation generation");
        FreePublication(nullptr, pointer, &trace);
    }
    Require(trace.live.empty(), "Caller-owned allocator fixture cleanup must reclaim the measured orphaned epoch");
}
static void Check(Publication action, bool sharedOwner, bool outerLegacy, bool nestedLegacy) {
    PublicationTrace trace; PublicationAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t outer = 0, nested = 0;
    _ZN3sce4Json11InitializerC1Ev(&outer); _ZN3sce4Json11InitializerC1Ev(&nested);
    trace.action = action; trace.armed = true; trace.nestedLegacy = nestedLegacy;
    trace.nestedOwner = sharedOwner ? &outer : &nested; trace.nestedParameter = &parameter;
    const int status = Initialize(outerLegacy, &outer, &parameter);
    Require(!trace.armed && trace.nested.size() == 12 && trace.nestedGeneration == 12, "Every publication case must exercise actual first-allocation callback reentry");
    if (action == Publication::Handoff) {
        Require(static_cast<unsigned>(status) == 0x80848102 && *trace.nestedOwner == 1 && outer == unsigned(sharedOwner) && trace.live == trace.nested && trace.releases.empty(),
                "A failed outer context allocation must retain the nested initializer's ownership, active byte and untouched resource generation");
        Require(_ZN3sce4Json11Initializer9terminateEv(trace.nestedOwner) == 0 && *trace.nestedOwner == 0 && trace.live.empty(), "The nested owner must remain able to terminate after outer initialization reports OOM");
    } else {
        Require(status == 0 && outer == 1 && trace.live.size() == (action == Publication::Overwrite ? 24u : 12u),
                "Successful outer initialization must preserve the reference resource count after nested publication");
        if (action == Publication::CompleteNested) {
            Require(nested == 0 && trace.releases.size() == 12, "Completed nested teardown must reclaim exactly its original resource generation");
            for (const auto& [pointer, identity] : trace.live) Require(pointer && identity.generation > trace.nestedGeneration, "Outer initialization after a nested lifecycle must own an entirely fresh generation");
        }
        Require(_ZN3sce4Json11Initializer9terminateEv(&outer) == 0 && outer == 0, "Terminate the final published outer module generation");
        if (action == Publication::Overwrite) {
            Require(trace.live == trace.nested && nested == unsigned(!sharedOwner) && trace.releases.size() == 12,
                    "Double successful publication must preserve the reference's orphaned first epoch instead of claiming the library reclaimed it");
            const auto released = trace.releases;
            Require(static_cast<unsigned>(_ZN3sce4Json11Initializer9terminateEv(trace.nestedOwner)) == 0x80848110 && trace.live == trace.nested && trace.releases == released,
                    "An overwritten nested owner must report missing module state without releasing its orphaned epoch");
            ReclaimOrphanedFixtureEpoch(trace);
        } else Require(trace.live.empty(), "A completed nested lifecycle must not orphan module resources");
    }
    std::uint8_t retry = 0; _ZN3sce4Json11InitializerC1Ev(&retry);
    const auto generation = trace.generation;
    Require(Initialize(outerLegacy, &retry, &parameter) == 0 && retry == 1 && trace.live.size() == 12, "A fresh initializer must work after every measured publication outcome");
    for (const auto& [pointer, identity] : trace.live) Require(pointer && identity.generation > generation, "Publication retry must retain no old fixture allocation generations");
    Require(_ZN3sce4Json11Initializer9terminateEv(&retry) == 0 && retry == 0 && trace.live.empty(), "Release the fresh publication generation");
}
int main() {
    for (auto action : {Publication::Handoff, Publication::CompleteNested, Publication::Overwrite})
        for (bool sharedOwner : {false, true}) for (bool outerLegacy : {false, true}) for (bool nestedLegacy : {false, true})
            Check(action, sharedOwner, outerLegacy, nestedLegacy);
    return 0;
}
