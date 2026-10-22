#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
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
using NullCallback = const Value& (APS5_VABI*)(std::int32_t, const Value*, void*);
using MismatchCallback = const Value& (APS5_VABI*)(std::int32_t, const Value*, const Value*, void*);
using FloatCallback = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
using ElementCallback = void (APS5_VABI*)(int, const void*, const std::size_t*, const Value*, const Value*, void*);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(Value*, std::int32_t);
void APS5_VABI _ZN3sce4Json5Value3setENS0_9ValueTypeE(Value*, std::int32_t);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value*, double);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value*, const char*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(Value*, const Value*);
Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1_(Value*, const Value*);
int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(Value*, NullCallback, void*);
int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value*, MismatchCallback, void*);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value*, FloatCallback, void*);
int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(Value*, ElementCallback, void*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct ValueAllocationTrace {
    std::map<void*, std::size_t> live;
    std::vector<std::size_t> requests;
    std::vector<void*> frees;
    int budget = -1;
    unsigned notifications = 0;
    unsigned nullFrees = 0;
    const Value* registering = nullptr;
};
static void* APS5_VABI ValueAllocationAllocate(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ValueAllocationTrace*>(context); trace.requests.push_back(bytes);
    if (trace.budget == 0) return nullptr;
    if (trace.budget > 0) --trace.budget;
    auto* memory = std::malloc(bytes); Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.live.emplace(memory, bytes); return memory;
}
static void APS5_VABI ValueAllocationFree(void*, void* memory, void* context) {
    if (!memory) { ++static_cast<ValueAllocationTrace*>(context)->nullFrees; return; }
    Require(static_cast<ValueAllocationTrace*>(context)->live.erase(memory) == 1, "Value resources must return through their owning allocator");
    static_cast<ValueAllocationTrace*>(context)->frees.push_back(memory);
    std::free(memory);
}
static void APS5_VABI ValueAllocationNotify(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ValueAllocationTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == bytes, "Value failure notification must identify its allocation");
    if (trace.registering && bytes != 16) Require(!trace.registering->auxiliary, "Auxiliary admission must remain unpublished during allocation failure notification");
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
static const Value& APS5_VABI NullRecovery(std::int32_t, const Value* value, void*) { return *value; }
static const Value& APS5_VABI MismatchRecovery(std::int32_t, const Value* value, const Value*, void*) { return *value; }
static Value* APS5_VABI FloatRecovery(Value* value, std::int32_t, double, const Value*, void*, bool*) { return value; }
static void APS5_VABI ElementRecovery(int, const void*, const std::size_t*, const Value*, const Value*, void*) {}
static int Register(Value& value, unsigned kind, void* context, bool empty = false) {
    switch (kind) {
        case 0: return _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&value, empty ? nullptr : NullRecovery, context);
        case 1: return _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&value, empty ? nullptr : MismatchRecovery, context);
        case 2: return _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&value, empty ? nullptr : FloatRecovery, context);
        default: return _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&value, empty ? nullptr : ElementRecovery, context);
    }
}
static void CheckMetadata(ValueAllocationTrace& trace) {
    const auto baseline = trace.live.size();
    for (unsigned kind = 0; kind < 4; ++kind) for (int budget = 0; budget <= 3; ++budget) {
        Value value; std::memset(&value, 0xa5, sizeof(value));
        _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&value, 0);
        const auto payload = value.payload;
        const auto notifications = trace.notifications;
        trace.budget = budget;
        trace.registering = &value;
        const auto status = Register(value, kind, &trace);
        trace.registering = nullptr;
        trace.budget = -1;
        const bool admitted = budget >= (kind == 0 ? 2 : 3);
        Require(static_cast<unsigned>(status) == (admitted ? 0u : 0x80848102u) && trace.notifications - notifications == (admitted ? 0u : 1u), "Callback metadata admission must return and notify its actual allocation failure");
        Require(value.payload == payload && value.reserved == 0xa5a5a5a5 && value.type == 0, "Callback registration must preserve the value payload");
        Require((value.auxiliary != nullptr) == (budget >= 1), "Failed fallback allocation must retain its auxiliary record");
        if (value.auxiliary) {
            Require(Owned(trace, value.auxiliary, 72), "Modern auxiliary records must use the supplied allocator");
            auto* slots = static_cast<void**>(value.auxiliary);
            Require((slots[2] != nullptr) == (budget >= 2), "Fallback allocation failure must preserve a null fallback pointer");
            if (slots[2]) {
                auto* fallback = static_cast<Value*>(slots[2]);
                Require(Owned(trace, fallback, 32) && fallback->parent == &value && !fallback->auxiliary && fallback->type == 0 && fallback->payload == UINT64_C(0xa5a5a5a5a5a5a5a5), "Fallback values must retain native ownership and inactive payload bytes");
            }
            for (unsigned index = 6; index < 9; ++index) Require(!slots[index], "Auxiliary reserved fields must initialize to zero");
            if (kind && admitted) Require(Owned(trace, slots[kind + 2], 16), "Callback records must own their actual sixteen-byte allocation");
        }
        auto* auxiliary = value.auxiliary;
        auto* fallback = auxiliary ? static_cast<void**>(auxiliary)[2] : nullptr;
        const auto requests = trace.requests.size();
        Require(Register(value, kind, &value) == 0, "Callback registration must remain usable after allocation failure");
        Require(!auxiliary || (value.auxiliary == auxiliary && static_cast<void**>(auxiliary)[2] == fallback), "Registration retry must reuse partial auxiliary state without recreating a missing fallback");
        if (admitted) Require(trace.requests.size() == requests, "Replacing an existing callback must not allocate");
        auto* slots = static_cast<void**>(value.auxiliary);
        auto* record = kind ? static_cast<void**>(slots[kind + 2]) : slots;
        Require(record[0] && record[1] == &value, "Registration must retain its callback and replacement context");
        const auto retained = trace.requests.size();
        Require(Register(value, kind, &trace, true) == 0 && !record[0] && record[1] == &trace && trace.requests.size() == retained, "A null callback must replace its record without deleting or reallocating metadata");
        _ZN3sce4Json5Value3setENS0_9ValueTypeE(&value, 2);
        Require(value.auxiliary == slots, "Payload type changes must preserve root callback metadata");
        Value parent{}; value.parent = &parent;
        Require(static_cast<unsigned>(Register(value, kind, &trace)) == 0x80848104 && trace.requests.size() == retained, "Owned children must reject callback registration without allocating");
        value.parent = nullptr;
        std::vector<void*> expectedFrees;
        if (kind) expectedFrees.push_back(record);
        if (slots[2]) expectedFrees.push_back(slots[2]);
        expectedFrees.push_back(slots);
        const auto freed = trace.frees.size();
        const auto nullFrees = trace.nullFrees;
        _ZN3sce4Json5ValueD1Ev(&value);
        Require(trace.nullFrees == nullFrees, "Metadata destruction must not free absent callback records");
        Require(trace.frees.size() == freed + expectedFrees.size() && std::equal(expectedFrees.begin(), expectedFrees.end(), trace.frees.begin() + freed), "Metadata teardown must release callback records, fallback and auxiliary in ownership order");
        Require(trace.live.size() == baseline, "Callback metadata destruction must release all admitted and partial resources");
    }
}
static void CheckScalarCopies(ValueAllocationTrace& trace) {
    const auto baseline = trace.live.size();
    constexpr const char* text = "a string requiring independently allocated storage";
    for (int previous = -1; previous <= 7; ++previous) for (int type = 0; type <= 5; ++type) for (int budget = 0; budget <= 3; ++budget) {
        Value source; std::memset(&source, 0xa5, sizeof(source));
        if (type == 5) _ZN3sce4Json5ValueC1EPKc(&source, text);
        else {
            _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&source, type);
            if (type == 1) source.payload = UINT64_C(0xa5a5a5a5a5a5a501);
            if (type == 2) source.payload = 42;
            if (type == 3) source.payload = UINT64_C(0xfedcba9876543210);
            if (type == 4) source.payload = UINT64_C(0x3fe8000000000000);
        }
        Value destination; std::memset(&destination, 0xa5, sizeof(destination));
        void* auxiliary = nullptr;
        std::uint64_t payload = destination.payload;
        void* header = nullptr;
        if (previous >= 0) {
            if (previous == 5) _ZN3sce4Json5ValueC1EPKc(&destination, "previous");
            else _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&destination, previous);
            Require(Register(destination, 0, &trace, true) == 0, "Prepare assignment metadata");
            auxiliary = destination.auxiliary;
            payload = destination.payload;
            if (previous == 5) header = *reinterpret_cast<void* const*>(payload);
        }
        const auto requests = trace.requests.size();
        trace.budget = budget;
        if (previous < 0) _ZN3sce4Json5ValueC1ERKS1_(&destination, &source);
        else Require(_ZN3sce4Json5ValueaSERKS1_(&destination, &source) == &destination, "Value assignment must return its destination");
        trace.budget = -1;
        const auto expected = type == 5 && previous != 5 && budget < 2 ? 0 : type;
        Check(destination, expected, trace);
        Require(!destination.parent && destination.auxiliary == auxiliary, "Copy and assignment must preserve their metadata contract");
        if (type == 0) Require(destination.payload == (previous >= 5 ? 0 : payload), "Null copies must preserve inactive scalar payload bytes");
        else if (type == 1) Require(destination.payload == (((previous >= 5 ? 0 : payload) & ~UINT64_C(255)) | 1), "Boolean copies must preserve inactive destination bytes");
        else if (type < 5) Require(destination.payload == source.payload, "Numeric copies must retain the exact active payload");
        else if (!expected) Require(destination.payload == 0, "Failed string admission must leave a null payload pointer");
        else {
            const auto* stored = *reinterpret_cast<void* const*>(destination.payload);
            const auto* characters = *static_cast<const char* const*>(stored);
            const bool complete = budget >= (previous == 5 ? 1 : 3);
            Require(std::strcmp(characters, complete ? text : "") == 0, "String copy failure must retain its admitted empty header");
            if (previous == 5) Require(stored == header, "String assignment must reuse the existing header");
        }
        if (type < 5) Require(trace.requests.size() == requests, "Scalar copies must not allocate");
        if (type == 5) {
            const auto* stored = *reinterpret_cast<void* const*>(source.payload);
            Require(std::strcmp(*static_cast<const char* const*>(stored), text) == 0, "String copy failures must preserve the source");
        }
        _ZN3sce4Json5ValueD1Ev(&destination); _ZN3sce4Json5ValueD1Ev(&source);
        Require(trace.live.size() == baseline, "Copy failures must reclaim destination and source resources");
    }
}
static void CheckCollectionCopies(ValueAllocationTrace& trace) {
    const auto baseline = trace.live.size();
    for (int previous = -1; previous <= 7; ++previous) for (int type : {6, 7}) for (int budget = 0; budget <= 5; ++budget) {
        Value source; std::memset(&source, 0xa5, sizeof(source));
        _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&source, type);
        const auto sourcePayload = source.payload;
        Value destination; std::memset(&destination, 0xa5, sizeof(destination));
        void* auxiliary = nullptr;
        void* originalHeader = nullptr;
        if (previous >= 0) {
            _ZN3sce4Json5ValueC1ENS0_9ValueTypeE(&destination, previous);
            Require(Register(destination, 0, &trace, true) == 0, "Prepare collection assignment metadata");
            auxiliary = destination.auxiliary;
            if (previous == type) originalHeader = *reinterpret_cast<void* const*>(destination.payload);
        }
        const auto notifications = trace.notifications;
        trace.budget = budget;
        if (previous < 0) _ZN3sce4Json5ValueC1ERKS1_(&destination, &source);
        else _ZN3sce4Json5ValueaSERKS1_(&destination, &source);
        trace.budget = -1;
        const auto threshold = type == 6 ? 2 : 3;
        const auto expected = previous == type || budget >= threshold ? type : 0;
        Check(destination, expected, trace);
        Require(!destination.parent && destination.auxiliary == auxiliary, "Collection copies must preserve destination metadata");
        if (!expected) Require(destination.payload == 0, "Failed collection admission must leave a null handle");
        else {
            auto* header = *reinterpret_cast<void* const*>(destination.payload);
            const auto* words = static_cast<const std::uintptr_t*>(header);
            Require(words[1] == 0 && (words[0] != 0) == !(type == 6 && previous != type && budget == 2), "Collection assignment must preserve the reference partial-header state");
            if (previous == type) Require((header == originalHeader) == (budget < 2), "Same-type assignment must preserve the header only when replacement admission fails");
        }
        const auto expectedNotifications = budget == 5 || (previous == type && budget >= 2) ? 0u : type == 6 && previous != type && budget == 2 ? 2u : 1u;
        Require(trace.notifications - notifications == expectedNotifications, "Collection copies must notify both type-admission and replacement allocation failures");
        Require(source.payload == sourcePayload && source.type == type, "Collection assignment must preserve source ownership");
        _ZN3sce4Json5ValueD1Ev(&destination); _ZN3sce4Json5ValueD1Ev(&source);
        Require(trace.live.size() == baseline, "Collection copies must reclaim partial handles and replaced storage");
    }
}
int main() {
    ValueAllocationTrace trace; ValueAllocationAllocator allocator; Parameter parameter{&allocator, &trace, 16384, 0, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize value allocation contract");
    const auto baseline = trace.live.size();
    CheckMetadata(trace);
    CheckScalarCopies(trace);
    CheckCollectionCopies(trace);
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
