#include "prx/libkernel/Pthread/include/Mutex.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using RttiAllocate = void* (APS5_VABI*)(std::size_t, void*);
using RttiFree = void (APS5_VABI*)(void*, void*);
using RttiNotify = void (APS5_VABI*)(int, std::size_t, void*);
struct RttiCallbacks { RttiAllocate allocate; RttiFree free; RttiNotify notify; };
extern "C" {
void APS5_VABI _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(Parameter*, const RttiCallbacks*, void*, std::size_t);
void APS5_VABI _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(Parameter*, const RttiCallbacks*, void*);
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
}
#if defined(JSON2_PUBLICATION_LEGACY_SDK)
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    static const std::uint32_t words[]{0, 0, 0, 0, 0x05000000};
    return words;
}
static constexpr std::size_t ModuleResources = 11;
static constexpr unsigned KernelResources = 1;
static constexpr std::size_t ModuleFreeCalls = ModuleResources + 1;
#else
static constexpr std::size_t ModuleResources = 12;
static constexpr unsigned KernelResources = 2;
static constexpr std::size_t ModuleFreeCalls = ModuleResources;
#endif
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
    PublicationTrace* nestedTrace = nullptr;
    void* expectedAllocator = nullptr;
    std::size_t rttiAllocations[2]{};
    std::size_t rttiReleases[2]{};
    bool deferredConfiguration = false;
    Parameter* pendingParameter = nullptr;
    const RttiCallbacks* pendingCallbacks = nullptr;
    void* pendingContext = nullptr;
    void* adapter = nullptr;
};
struct ContextRecord { void* allocator; void* userData; std::size_t bufferSize; };
static void CheckContext(const PublicationTrace& trace, std::size_t generation, const Parameter& parameter) {
    unsigned found = 0;
    for (const auto& [pointer, identity] : trace.live) {
        if (identity.generation != generation) continue;
        Require(identity.size == sizeof(ContextRecord), "Publication must allocate its actual context record in the owning domain");
        ContextRecord context{}; std::memcpy(&context, pointer, sizeof(context));
        Require(context.allocator == parameter.allocator && context.userData == parameter.context && context.bufferSize == parameter.bufferSize,
                "Publication must snapshot the correct allocator receiver, user context and file-buffer setting for each domain");
        ++found;
    }
    Require(found == 1, "The published allocation context must retain its exact allocation generation");
}
static void ConfigureRtti(bool legacy, Parameter& parameter, const RttiCallbacks& callbacks, void* context);
static void* AllocateStorage(PublicationTrace& trace, std::size_t size) {
    if (trace.armed) {
        trace.armed = false;
        if (trace.deferredConfiguration) {
            ConfigureRtti(trace.nestedLegacy, *trace.pendingParameter, *trace.pendingCallbacks, trace.pendingContext);
            Require(trace.pendingParameter->allocator == trace.adapter, "Reentrant RTTI configuration must update the shared adapter before context publication");
        }
        Require(Initialize(trace.nestedLegacy, trace.nestedOwner, trace.nestedParameter) == 0 && *trace.nestedOwner == 1 && trace.nestedTrace->live.size() == ModuleResources,
                "Nested initialization in the unpublished-context callback must publish its actual complete resource generation");
        CheckContext(*trace.nestedTrace, trace.nestedTrace->generation - (ModuleResources - 1), *trace.nestedParameter);
        trace.nested = trace.nestedTrace->live; trace.nestedGeneration = trace.nestedTrace->generation;
        if (trace.action == Publication::CompleteNested)
            Require(_ZN3sce4Json11Initializer9terminateEv(trace.nestedOwner) == 0 && trace.nestedTrace->live.empty() && *trace.nestedOwner == 0, "The initial allocation callback may complete a nested module lifecycle before returning its allocation");
        if (trace.action == Publication::Handoff) return nullptr;
    }
    void* memory = std::malloc(size);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, size);
    Require(trace.live.emplace(memory, AllocationIdentity{size, ++trace.generation}).second, "Publication resources must have independently owned live generations");
    return memory;
}
static void FreeStorage(PublicationTrace& trace, void* memory) {
    if (!memory) return;
    trace.releases.push_back(trace.live.at(memory).generation);
    Require(trace.live.erase(memory) == 1, "Publication cleanup must release each allocation through its owning allocator exactly once");
    std::free(memory);
}
static void* APS5_VABI AllocatePublication(void* self, std::size_t size, void* context) {
    auto& trace = *static_cast<PublicationTrace*>(context);
    Require(self == trace.expectedAllocator, "Publication allocation must preserve its exact allocator receiver and user context");
    return AllocateStorage(trace, size);
}
static void APS5_VABI FreePublication(void* self, void* memory, void* context) {
    auto& trace = *static_cast<PublicationTrace*>(context);
    Require(self == trace.expectedAllocator, "Publication release must preserve its exact allocator receiver and user context");
    FreeStorage(trace, memory);
}
static void* APS5_VABI AllocateRttiFirst(std::size_t size, void* context) {
    auto& trace = *static_cast<PublicationTrace*>(context); ++trace.rttiAllocations[0];
    return AllocateStorage(trace, size);
}
static void* APS5_VABI AllocateRttiSecond(std::size_t size, void* context) {
    auto& trace = *static_cast<PublicationTrace*>(context); ++trace.rttiAllocations[1];
    return AllocateStorage(trace, size);
}
static void APS5_VABI FreeRttiFirst(void* memory, void* context) {
    auto& trace = *static_cast<PublicationTrace*>(context); ++trace.rttiReleases[0];
    FreeStorage(trace, memory);
}
static void APS5_VABI FreeRttiSecond(void* memory, void* context) {
    auto& trace = *static_cast<PublicationTrace*>(context); ++trace.rttiReleases[1];
    FreeStorage(trace, memory);
}
static void APS5_VABI NotifyRtti(int, std::size_t, void*) { Require(false, "Failure of the unpublished RTTI context allocation must omit notification"); }
static void ConfigureRtti(bool legacy, Parameter& parameter, const RttiCallbacks& callbacks, void* context) {
    if (legacy) _ZN3sce4Json17InitParameterRttiC1EPNS0_14AllocParamRttiEPvm(&parameter, &callbacks, context, parameter.bufferSize);
    else _ZN3sce4Json18InitParameterRtti216setAllocatorRttiEPNS0_14AllocParamRttiEPv(&parameter, &callbacks, context);
}
static void APS5_VABI NotifyPublication(void*, int, std::size_t, void*) { Require(false, "Failure of the unpublished context allocation must omit allocator notification"); }
static void* const PublicationVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocatePublication), reinterpret_cast<void*>(FreePublication), reinterpret_cast<void*>(NotifyPublication)};
struct PublicationAllocator { void* const* vtable = PublicationVtable; };
static void ReclaimOrphanedFixtureEpoch(PublicationTrace& trace, const std::map<void*, AllocationIdentity>& nested, std::size_t generation) {
    Require(trace.live == nested && trace.live.size() == ModuleResources, "The fixture may reclaim only the exact orphaned epoch already observed after library teardown");
    unsigned mutexes = 0;
    for (const auto& [pointer, identity] : nested) {
        if (identity.generation + KernelResources > generation) {
            Require(identity.size == sizeof(PthreadMutex) && scePthreadMutexDestroy(static_cast<PthreadMutex*>(pointer)) == 0,
                    "Fixture cleanup must destroy the actual orphaned kernel mutex resources before reclaiming their handle storage");
            ++mutexes;
        }
    }
    Require(mutexes == KernelResources, "The orphaned epoch must retain its SDK-specific count of actual mutex handles");
    for (const auto& [pointer, identity] : nested) {
        Require(trace.live.at(pointer) == identity, "Fixture reclamation must retain the observed orphan allocation generation");
        FreeStorage(trace, pointer);
    }
    Require(trace.live.empty(), "Caller-owned allocator fixture cleanup must reclaim the measured orphaned epoch");
}
static void Check(Publication action, bool sharedOwner, bool outerLegacy, bool nestedLegacy, unsigned domain, bool rtti, bool deferredConfiguration) {
    PublicationTrace trace, separateTrace;
    PublicationAllocator allocator, separateAllocator;
    const bool separate = domain != 0;
    trace.expectedAllocator = &allocator;
    separateTrace.expectedAllocator = domain == 2 ? &separateAllocator : &allocator;
    Parameter parameter{&allocator, &trace, 8192, 2, {}};
    Parameter separateParameter{separateTrace.expectedAllocator, &separateTrace, 16384, 2, {}};
    const RttiCallbacks first{AllocateRttiFirst, FreeRttiFirst, NotifyRtti}, second{AllocateRttiSecond, FreeRttiSecond, NotifyRtti};
    if (rtti) {
        ConfigureRtti(outerLegacy, parameter, first, &trace);
        if (!deferredConfiguration) ConfigureRtti(nestedLegacy, separateParameter, second, &separateTrace);
        Require(parameter.allocator && (deferredConfiguration || parameter.allocator == separateParameter.allocator) && parameter.context == &trace && separateParameter.context == &separateTrace,
                "Both RTTI parameter families must share the adapter while retaining independent user contexts");
        trace.expectedAllocator = parameter.allocator; separateTrace.expectedAllocator = parameter.allocator;
        trace.deferredConfiguration = deferredConfiguration;
        trace.pendingParameter = &separateParameter; trace.pendingCallbacks = &second; trace.pendingContext = &separateTrace; trace.adapter = parameter.allocator;
    }
    std::uint8_t outer = 0, nested = 0;
    _ZN3sce4Json11InitializerC1Ev(&outer); _ZN3sce4Json11InitializerC1Ev(&nested);
    trace.action = action; trace.armed = true; trace.nestedLegacy = nestedLegacy;
    trace.nestedOwner = sharedOwner ? &outer : &nested;
    trace.nestedParameter = separate ? &separateParameter : &parameter;
    trace.nestedTrace = separate ? &separateTrace : &trace;
    const int status = Initialize(outerLegacy, &outer, &parameter);
    Require(!trace.armed && trace.nested.size() == ModuleResources && trace.nestedGeneration == ModuleResources, "Every publication case must exercise actual first-allocation callback reentry");
    if (action == Publication::Handoff) {
        Require(static_cast<unsigned>(status) == 0x80848102 && *trace.nestedOwner == 1 && outer == unsigned(sharedOwner) && trace.nestedTrace->live == trace.nested && trace.releases.empty() && trace.nestedTrace->releases.empty(),
                "A failed outer context allocation must retain the nested initializer's ownership, active byte and untouched resource generation");
        Require(trace.live.size() == (separate ? 0u : ModuleResources), "Outer context failure must allocate no resources in a separate outer allocator domain");
        Require(_ZN3sce4Json11Initializer9terminateEv(trace.nestedOwner) == 0 && *trace.nestedOwner == 0 && trace.live.empty() && separateTrace.live.empty(),
                "The nested owner must terminate through its original allocator after outer initialization reports OOM");
    } else {
        Require(status == 0 && outer == 1 && trace.live.size() == (action == Publication::Overwrite && !separate ? 2 * ModuleResources : ModuleResources),
                "Successful outer initialization must preserve the reference resource count in each allocator domain");
        CheckContext(trace, separate ? 1u : trace.nestedGeneration + 1, parameter);
        if (action == Publication::CompleteNested) {
            Require(nested == 0 && trace.nestedTrace->releases.size() == ModuleResources, "Completed nested teardown must reclaim exactly its original resource generation");
            Require(!separate || (separateTrace.live.empty() && trace.releases.empty()), "A completed nested lifecycle must release only its owning allocator domain");
            for (const auto& [pointer, identity] : trace.live) Require(pointer && identity.generation > (separate ? 0u : trace.nestedGeneration), "Outer initialization after a nested lifecycle must own a fresh generation in its own domain");
        } else if (separate) {
            Require(separateTrace.live == trace.nested && separateTrace.releases.empty(), "Outer publication must not reclaim or replace the nested allocator's orphaned generation");
        }
        Require(_ZN3sce4Json11Initializer9terminateEv(&outer) == 0 && outer == 0, "Terminate the final published outer module generation");
        if (action == Publication::Overwrite) {
            Require(trace.nestedTrace->live == trace.nested && nested == unsigned(!sharedOwner) && trace.releases.size() == ModuleResources && (!separate || (trace.live.empty() && separateTrace.releases.empty())),
                    "Double successful publication must retain the reference's orphaned first epoch in its original allocator domain");
            const auto released = trace.releases, nestedReleased = trace.nestedTrace->releases;
            Require(static_cast<unsigned>(_ZN3sce4Json11Initializer9terminateEv(trace.nestedOwner)) == 0x80848110 && trace.nestedTrace->live == trace.nested && trace.releases == released && trace.nestedTrace->releases == nestedReleased,
                    "An overwritten nested owner must report missing module state without releasing resources through either allocator domain");
            ReclaimOrphanedFixtureEpoch(*trace.nestedTrace, trace.nested, trace.nestedGeneration);
        } else Require(trace.live.empty() && separateTrace.live.empty(), "A completed nested lifecycle must not orphan module resources in either allocator domain");
    }
    std::uint8_t retry = 0; _ZN3sce4Json11InitializerC1Ev(&retry);
    const auto generation = trace.generation, separateGeneration = separateTrace.generation;
    const auto separateReleases = separateTrace.releases;
    Require(Initialize(outerLegacy, &retry, &parameter) == 0 && retry == 1 && trace.live.size() == ModuleResources, "A fresh initializer must work after every measured publication outcome");
    for (const auto& [pointer, identity] : trace.live) Require(pointer && identity.generation > generation, "Publication retry must retain no old fixture allocation generations");
    Require(_ZN3sce4Json11Initializer9terminateEv(&retry) == 0 && retry == 0 && trace.live.empty() && separateTrace.live.empty() && separateTrace.generation == separateGeneration && separateTrace.releases == separateReleases,
            "A fresh outer generation must not call into a stale nested allocator domain");
    if (rtti) {
        Require(trace.rttiAllocations[0] == unsigned(deferredConfiguration) && !trace.rttiReleases[0] && !separateTrace.rttiAllocations[0] && !separateTrace.rttiReleases[0] && trace.rttiAllocations[1] && trace.rttiReleases[1],
                "RTTI module allocation and release must use the most recently configured callback table even through an older parameter object");
        Require(!separate || (separateTrace.rttiAllocations[1] == ModuleResources && separateTrace.rttiReleases[1] == (action == Publication::Overwrite ? 0u : ModuleFreeCalls)), "RTTI callbacks must preserve the nested user context and leave an orphaned epoch unreleased by the library");
    }
}
int main() {
    for (auto action : {Publication::Handoff, Publication::CompleteNested, Publication::Overwrite})
        for (bool sharedOwner : {false, true}) for (bool outerLegacy : {false, true}) for (bool nestedLegacy : {false, true})
            for (bool rtti : {false, true}) for (unsigned domain = 0; domain < (rtti ? 2u : 3u); ++domain)
                for (unsigned deferred = 0; deferred < (rtti ? 2u : 1u); ++deferred)
                    Check(action, sharedOwner, outerLegacy, nestedLegacy, domain, rtti, deferred != 0);
    return 0;
}
