#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* storage; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using NullHandler = const Value& (APS5_VABI*)(std::int32_t, const Value*, void*);
using MismatchHandler = const Value& (APS5_VABI*)(std::int32_t, const Value*, const Value*, void*);
using FloatHandler = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
using ElementHandler = void (APS5_VABI*)(int, const String*, const std::size_t*, const Value*, const Value*, void*);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value*, const char*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueD2Ev(Value*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(Value*, NullHandler, void*);
int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value*, MismatchHandler, void*);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value*, FloatHandler, void*);
int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(Value*, ElementHandler, void*);
}
#if defined(JSON2_DESTRUCTION_LEGACY_SDK)
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    static const std::uint32_t words[]{0, 0, 0, 0, 0x05000000};
    return words;
}
static constexpr bool LegacySdk = true;
static constexpr unsigned ModuleResources = 11;
static constexpr unsigned AuxiliaryBytes = 24;
static constexpr unsigned ShapeCount = 3;
#else
static constexpr bool LegacySdk = false;
static constexpr unsigned ModuleResources = 12;
static constexpr unsigned AuxiliaryBytes = 72;
static constexpr unsigned ShapeCount = 4;
#endif
static constexpr unsigned ModuleFreeCalls = 12;
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct DestructionTrace {
    std::map<void*, std::size_t> live;
    std::map<void*, std::size_t> module;
    std::map<void*, std::size_t> source;
    std::size_t generation = 0;
    std::size_t sourceBase = 0;
    void* receiver = nullptr;
    std::uint8_t* owner = nullptr;
    unsigned target = 0;
    unsigned calls = 0;
    unsigned terminations = 0;
    unsigned refusals = 0;
    unsigned nullCalls = 0;
    bool rejectTermination = false;
    bool observing = false;
    std::vector<std::size_t> releases;
    std::vector<std::size_t> moduleReleases;
};
static void* APS5_VABI AllocateDestruction(void* receiver, std::size_t bytes, void* context) {
    auto& trace = *static_cast<DestructionTrace*>(context);
    Require(receiver == trace.receiver, "Composite allocations must retain their actual allocator receiver and context");
    void* memory = std::malloc(bytes); Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes);
    Require(trace.live.emplace(memory, ++trace.generation).second, "Composite resources must own distinct live allocation generations");
    return memory;
}
static void ReleaseStorage(DestructionTrace& trace, void* memory) {
    Require(trace.live.erase(memory) == 1, "Every composite resource must be reclaimed exactly once by its owning domain");
    std::free(memory);
}
static void APS5_VABI FreeDestruction(void* receiver, void* memory, void* context) {
    auto& trace = *static_cast<DestructionTrace*>(context);
    Require(receiver == trace.receiver, "Composite cleanup must preserve allocator receiver and context even through module termination");
    if (trace.observing) {
        ++trace.calls;
        if (!memory) { Require(LegacySdk, "Only the measured legacy module cleanup may forward a null free"); ++trace.nullCalls; return; }
        const auto identity = trace.live.at(memory);
        if (trace.module.contains(memory)) {
            Require(trace.module.at(memory) == identity, "Nested module termination must retain its original allocation generations");
            trace.moduleReleases.push_back(identity);
        } else {
            Require(trace.source.contains(memory) && trace.source.at(memory) == identity, "Composite release callbacks must identify their exact original payload generation");
            trace.releases.push_back(identity - trace.sourceBase);
            if (trace.target && trace.releases.size() == trace.target) {
                trace.target = 0;
                if (trace.rejectTermination) {
                    const auto before = trace.live;
                    bool refused = false;
                    try { _ZN3sce4Json11Initializer9terminateEv(trace.owner); }
                    catch (const std::runtime_error& error) { Require(std::strstr(error.what(), "terminate"), "Unsafe legacy cleanup termination must use the explicit unsupported boundary"); refused = true; }
                    Require(refused && *trace.owner == 1 && trace.live == before, "Legacy auxiliary cleanup must reject callback termination before changing module ownership");
                    ++trace.refusals;
                } else {
                    auto expected = trace.live;
                    for (const auto& [pointer, generation] : trace.module) Require(expected.erase(pointer) == 1, "Callback termination must begin with every original module resource still live");
                    Require(_ZN3sce4Json11Initializer9terminateEv(trace.owner) == 0 && *trace.owner == 0 && trace.live == expected,
                            "Composite free callbacks must retain reference-supported module termination while preserving all remaining source allocations including the current release");
                    ++trace.terminations;
                }
            }
        }
    }
    if (memory) ReleaseStorage(trace, memory);
}
static void APS5_VABI NotifyDestruction(void*, int, std::size_t, void*) { Require(false, "Healthy composite destruction must not fail allocation"); }
static void* const DestructionVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateDestruction), reinterpret_cast<void*>(FreeDestruction), reinterpret_cast<void*>(NotifyDestruction)};
struct DestructionAllocator { void* const* vtable = DestructionVtable; };
static const std::vector<std::size_t> ReleaseOrders[]{
    {5, 4, 3},
    {31, 30, 29, 28, 27, 26, 25, 24, 18, 17, 16, 15, 14, 13, 12, 3, 2, 1, 5, 4},
    {9, 8, 7, 6, 26, 25, 24, 23, 22, 21, 20, 13, 12, 11, 10, 3, 2, 1, 5, 4},
    {5, 4, 3, 8, 9, 10, 7, 6},
};
static void DestroyAlias(Value& value, unsigned alias) {
    if (alias) _ZN3sce4Json5ValueD2Ev(&value);
    else _ZN3sce4Json5ValueD1Ev(&value);
}
static void Check(unsigned shape, unsigned target, unsigned alias) {
    DestructionTrace trace; DestructionAllocator allocator; trace.receiver = &allocator;
    Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0; trace.owner = &initializer; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0 && initializer == 1 && trace.live.size() == ModuleResources, "Initialize the actual composite-destruction resource epoch");
    trace.module = trace.live; trace.sourceBase = trace.generation;
    Value value; std::memset(&value, 0xa5, sizeof(value));
    if (shape == 0 || shape == 3) {
        _ZN3sce4Json5ValueC1EPKc(&value, "text");
        if (shape == 3) Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&value, nullptr, nullptr) == 0 &&
            _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&value, nullptr, nullptr) == 0 &&
            _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&value, nullptr, nullptr) == 0 &&
            _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&value, nullptr, nullptr) == 0, "Admit all modern auxiliary callback records through their real allocation paths");
    } else {
        _ZN3sce4Json5ValueC1Ev(&value);
        const char* document = shape == 1 ? R"({"s":"text","a":[1]})" : R"(["text",{"s":"more"}])";
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, document, std::strlen(document)) == 0, "Construct actual nested object/array ownership before callback destruction");
    }
    for (const auto& [pointer, generation] : trace.live) if (!trace.module.contains(pointer)) trace.source.emplace(pointer, generation);
    const auto& order = ReleaseOrders[shape];
    Require(trace.source.size() == order.size(), "Each composite payload must own its complete measured source resource epoch");
    void* auxiliary = value.auxiliary;
    std::array<std::byte, 72> auxiliaryBytes{};
    if (auxiliary) std::memcpy(auxiliaryBytes.data(), auxiliary, AuxiliaryBytes);
    trace.rejectTermination = LegacySdk && shape != 0 && target > 18;
    const bool terminated = target && !trace.rejectTermination;
    trace.target = target; trace.observing = true;
    DestroyAlias(value, alias);
    const unsigned released = terminated ? target : static_cast<unsigned>(order.size());
    Require(trace.releases == std::vector<std::size_t>(order.begin(), order.begin() + released), "Composite destruction must preserve the exact native release-generation prefix before callback termination");
    auto remaining = trace.source;
    for (unsigned index = 0; index < released; ++index) {
        bool found = false;
        for (auto iterator = remaining.begin(); iterator != remaining.end(); ++iterator) if (iterator->second == trace.sourceBase + order[index]) { remaining.erase(iterator); found = true; break; }
        Require(found, "Every observed release must remove exactly its expected original payload generation");
    }
    const bool retainedAuxiliary = terminated && ((shape == 1 || shape == 2) ? target <= 18 : shape == 3 && target <= 3);
    Require(value.type == 0 && !value.parent && !value.payload && value.reserved == UINT32_C(0xa5a5a5a5) && value.auxiliary == (retainedAuxiliary ? auxiliary : nullptr),
            "Destruction must skip auxiliary cleanup after payload callbacks terminate the module, retaining its original pointer rather than falsely clearing ownership");
    if (retainedAuxiliary) Require(std::memcmp(auxiliary, auxiliaryBytes.data(), AuxiliaryBytes) == 0, "Skipped auxiliary cleanup must preserve the complete original auxiliary bytes and callback/fallback identities");
    Require(trace.terminations == unsigned(terminated) && trace.refusals == unsigned(trace.rejectTermination) && !trace.target && trace.calls == released + (terminated ? ModuleFreeCalls : 0u) && trace.moduleReleases.size() == (terminated ? ModuleResources : 0u) && trace.nullCalls == (terminated ? ModuleFreeCalls - ModuleResources : 0u) && initializer == unsigned(!terminated) && trace.live == (terminated ? remaining : trace.module),
            "Callback termination must reclaim only the module epoch and released source prefix while preserving the reference's orphaned allocations");
    const auto beforeRepeat = trace.live; const auto calls = trace.calls;
    DestroyAlias(value, 1 - alias);
    Require(trace.live == beforeRepeat && trace.calls == calls && value.auxiliary == (retainedAuxiliary ? auxiliary : nullptr), "A repeated destructor must preserve inactive-module auxiliary state without attempting another release");
    trace.observing = false;
    if (!terminated) Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Ordinary composite destruction must remain fully reclaimable");
    else {
        Require(trace.live == remaining, "Fixture reclamation may begin only after the exact orphaned source epoch has been observed");
        for (const auto& [pointer, generation] : remaining) { Require(trace.live.at(pointer) == generation, "Caller-owned fixture cleanup must retain each orphan allocation generation"); ReleaseStorage(trace, pointer); }
    }
    const auto generation = trace.generation;
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0 && initializer == 1 && trace.live.size() == ModuleResources, "Initialization must admit a fresh module after every measured composite-destruction outcome");
    for (const auto& [pointer, identity] : trace.live) Require(pointer && identity > generation, "Composite lifecycle retry must retain no stale module or source resource generations");
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && initializer == 0 && trace.live.empty(), "Release the fresh composite lifecycle generation");
}
int main() {
    for (unsigned shape = 0; shape < ShapeCount; ++shape) for (unsigned target = 0; target <= ReleaseOrders[shape].size(); ++target) for (unsigned alias = 0; alias < 2; ++alias) Check(shape, target, alias);
    return 0;
}
