#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <limits>
#include <vector>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(Value*, std::int32_t);
void APS5_VABI _ZN3sce4Json5Value3setENS0_9ValueTypeE(Value*, std::int32_t);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value*, double);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct ValueAllocationTrace {
    std::map<void*, std::size_t> live;
    std::vector<std::size_t> requests;
    int budget = -1;
    unsigned notifications = 0;
};
static void* APS5_VABI ValueAllocationAllocate(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ValueAllocationTrace*>(context); trace.requests.push_back(bytes);
    if (trace.budget == 0) return nullptr;
    if (trace.budget > 0) --trace.budget;
    auto* memory = std::malloc(bytes); Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.live.emplace(memory, bytes); return memory;
}
static void APS5_VABI ValueAllocationFree(void*, void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<ValueAllocationTrace*>(context)->live.erase(memory) == 1, "Value resources must return through their owning allocator");
    std::free(memory);
}
static void APS5_VABI ValueAllocationNotify(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ValueAllocationTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == bytes, "Value failure notification must identify its allocation");
    ++trace.notifications;
}
static void* const ValueAllocationVtable[]{nullptr, nullptr, reinterpret_cast<void*>(ValueAllocationAllocate), reinterpret_cast<void*>(ValueAllocationFree), reinterpret_cast<void*>(ValueAllocationNotify)};
struct ValueAllocationAllocator { void* const* vtable = ValueAllocationVtable; };
static bool Owned(const ValueAllocationTrace& trace, const void* pointer, std::size_t bytes) {
    const auto found = trace.live.find(const_cast<void*>(pointer)); return found != trace.live.end() && found->second == bytes;
}
static void Check(const Value& value, int expected, const ValueAllocationTrace& trace) {
    Require(value.type == expected && value.reserved == 0xa5a5a5a5, "Value type admission must preserve inactive metadata");
    if (expected < 5) return;
    const auto* handle = reinterpret_cast<void* const*>(value.payload);
    Require(Owned(trace, handle, 8) && Owned(trace, *handle, expected == 5 ? 24 : 32), "Complex values must own an actual allocator-backed handle and header");
    if (expected != 5) {
        Value* owner = nullptr; std::memcpy(&owner, static_cast<const char*>(*handle) + 24, sizeof(owner));
        Require(owner == &value, "Collection headers must bind to their enclosing value");
    }
}
int main() {
    ValueAllocationTrace trace; ValueAllocationAllocator allocator; Parameter parameter{&allocator, &trace, 16384, 0, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize value allocation contract");
    const auto baseline = trace.live.size();
    for (int target = 0; target <= 7; ++target) for (int budget = 0; budget <= 4; ++budget) {
        Value value; std::memset(&value, 0xa5, sizeof(value));
        trace.budget = budget;
        _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&value, target);
        trace.budget = -1;
        const auto expected = target < 5 ? target : budget < (target == 7 ? 3 : 2) ? 0 : target;
        Check(value, expected, trace);
        Require(!value.parent && !value.auxiliary, "Typed construction must clear ownership metadata");
        if (target >= 5 && expected == 0) Require(value.payload == 0, "Failed complex construction must clear the payload pointer");
        _ZN3sce4Json5ValueD1Ev(&value);
        Require(trace.live.size() == baseline, "Typed construction must release partial owned resources");
    }
    for (int previous = 0; previous <= 7; ++previous) for (int target = 0; target <= 7; ++target) for (int budget = 0; budget <= 4; ++budget) {
        Value value; std::memset(&value, 0xa5, sizeof(value));
        _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&value, previous);
        Value owner{}; value.parent = &owner;
        const auto payload = value.payload;
        const auto requests = trace.requests.size();
        const auto notifications = trace.notifications;
        trace.budget = budget;
        _ZN3sce4Json5Value3setENS0_9ValueTypeE(&value, target);
        trace.budget = -1;
        const auto expected = previous == target || target < 5 ? target : budget < (target == 7 ? 3 : 2) ? 0 : target;
        Check(value, expected, trace);
        Require(value.parent == &owner && !value.auxiliary, "Type changes must preserve parent and auxiliary ownership");
        if (previous == target) Require(value.payload == payload && trace.requests.size() == requests && trace.notifications == notifications, "Retaining the same type must preserve storage without allocating");
        else if (target >= 5 && expected == 0) Require(value.payload == 0 && trace.notifications > notifications, "Failed type changes must clear the previous payload and notify failure");
        else if (target == 0) Require(value.payload == (previous >= 5 ? 0 : payload), "Clearing a scalar must preserve its inactive payload while complex values clear their handle");
        else if (target == 1) Require(value.payload == ((previous >= 5 ? 0 : payload) & ~UINT64_C(255)), "Boolean type changes must clear only their active byte");
        else if (target < 5) Require(value.payload == 0, "Numeric type changes must initialize their active payload");
        _ZN3sce4Json5ValueD1Ev(&value);
        Require(trace.live.size() == baseline, "Type changes must reclaim the old payload even when replacement fails");
    }
    for (bool constructor : {false, true}) for (int budget = 0; budget <= 5; ++budget) {
        Value value; std::memset(&value, 0xa5, sizeof(value));
        if (!constructor) _ZN3sce4Json5ValueC1Ed(&value, 0.125);
        const auto payload = value.payload;
        const auto notifications = trace.notifications;
        trace.budget = budget;
        if (constructor) _ZN3sce4Json5ValueC1Ed(&value, std::numeric_limits<double>::infinity());
        else _ZN3sce4Json5Value3setEd(&value, std::numeric_limits<double>::infinity());
        trace.budget = -1;
        const auto expected = budget == 0 ? (constructor ? 0 : 4) : budget < 4 ? 0 : 5;
        Check(value, expected, trace);
        Require(trace.notifications - notifications == (budget == 5 ? 0u : budget == 1 ? 2u : 1u), "Float conversion must report temporary and destination allocation failures");
        if (budget == 0) Require(value.payload == payload, "Failure to allocate the conversion temporary must preserve the prior payload");
        else if (expected == 0) Require(value.payload == 0, "Failed destination conversion must leave a null handle");
        else {
            const auto* header = *reinterpret_cast<void* const*>(value.payload);
            const auto* characters = *static_cast<const char* const*>(header);
            Require(std::strcmp(characters, budget == 4 ? "" : "inf") == 0, "Float conversion must retain the admitted string and character failure state");
        }
        _ZN3sce4Json5ValueD1Ev(&value);
        Require(trace.live.size() == baseline, "Float conversion must release temporary and destination allocations");
    }
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Terminate value allocation contract");
    return 0;
}
