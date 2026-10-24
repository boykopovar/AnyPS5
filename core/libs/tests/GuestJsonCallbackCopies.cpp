#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <string>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* storage; };
struct Array { void* storage; };
struct Pair { String key; std::uint64_t reserved; Value value; };
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
void APS5_VABI _ZN3sce4Json5ValueC1El(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(Value*, const Value*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value3setERKS1_(Value*, const Value*);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value*, String*);
const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv(const Value*);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value*, const char*);
void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array*);
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array*);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array*, const Value*);
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array*);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
void APS5_VABI _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(Pair*, const String*, const Value*);
void APS5_VABI _ZN3sce4Json6Object4PairD1Ev(Pair*);
int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(Value*, NullHandler, void*);
int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value*, MismatchHandler, void*);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value*, FloatHandler, void*);
int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(Value*, ElementHandler, void*);
}
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct CopyAllocations { std::map<void*, std::size_t> live; std::size_t generation = 0; };
static void* APS5_VABI AllocateCopy(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<CopyAllocations*>(context);
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes);
    Require(trace.live.emplace(memory, ++trace.generation).second, "Callback copy allocations must have independent live generations");
    return memory;
}
static void APS5_VABI FreeCopy(void*, void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<CopyAllocations*>(context)->live.erase(memory) == 1, "Callback copy teardown must release each allocation through its owner exactly once");
    std::free(memory);
}
static void APS5_VABI NotifyCopy(void*, int, std::size_t, void*) { Require(false, "Healthy callback copies must not fail allocation"); }
static void* const CopyVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateCopy), reinterpret_cast<void*>(FreeCopy), reinterpret_cast<void*>(NotifyCopy)};
struct CopyAllocator { void* const* vtable = CopyVtable; };
struct CopyContext {
    Value replacement{};
    const Value* receiver = nullptr;
    unsigned nulls = 0, mismatches = 0, floats = 0, elements = 0;
};
static const Value& APS5_VABI NullCopy(std::int32_t type, const Value* parent, void* context) {
    Require(type == 2 && !parent, "Copied null recovery must preserve requested type and parent ABI");
    auto& state = *static_cast<CopyContext*>(context); ++state.nulls;
    return state.replacement;
}
static const Value& APS5_VABI MismatchCopy(std::int32_t type, const Value* self, const Value* parent, void* context) {
    auto& state = *static_cast<CopyContext*>(context);
    Require(type == 2 && self == state.receiver && !parent, "Copied mismatch recovery must preserve receiver, type and parent ABI");
    ++state.mismatches;
    return state.replacement;
}
static Value* APS5_VABI FloatCopy(Value* result, std::int32_t function, double number, const Value* parent, void* context, bool* error) {
    Require(function == 1 && number == std::numeric_limits<double>::infinity() && !parent && !error, "Copied float recovery must preserve its assignment ABI");
    auto& state = *static_cast<CopyContext*>(context); ++state.floats;
    _ZN3sce4Json5ValueC1El(result, static_cast<std::int64_t>(state.replacement.payload));
    return result;
}
static void APS5_VABI ElementCopy(int error, const String* key, const std::size_t* index, const Value* parent, const Value* self, void* context) {
    auto& state = *static_cast<CopyContext*>(context);
    Require(static_cast<unsigned>(error) == 0x80848115 && key && !index && !parent && self == state.receiver && std::strcmp(_ZNK3sce4Json6String5c_strEv(key), "missing") == 0, "Copied element recovery must retain its exact key, receiver and parent ABI");
    ++state.elements;
}
static void Register(Value& value, CopyContext& context) {
    context.receiver = &value;
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&value, NullCopy, &context) == 0, "Register source null recovery");
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&value, MismatchCopy, &context) == 0, "Register source mismatch recovery");
    Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&value, FloatCopy, &context) == 0, "Register source float recovery");
    Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&value, ElementCopy, &context) == 0, "Register source element recovery");
}
struct MetadataSnapshot {
    std::array<std::byte, 72> bytes;
    std::array<void*, 5> allocations;
    std::array<std::size_t, 5> generations;
};
static MetadataSnapshot Capture(const Value& value, const CopyAllocations& trace) {
    MetadataSnapshot snapshot{};
    const auto* words = static_cast<void* const*>(value.auxiliary);
    snapshot.allocations = {value.auxiliary, words[2], words[3], words[4], words[5]};
    std::memcpy(snapshot.bytes.data(), value.auxiliary, snapshot.bytes.size());
    for (unsigned i = 0; i < snapshot.allocations.size(); ++i) snapshot.generations[i] = trace.live.at(snapshot.allocations[i]);
    return snapshot;
}
static void Verify(const Value& value, const MetadataSnapshot& snapshot, const CopyAllocations& trace) {
    Require(value.auxiliary == snapshot.allocations[0] && std::memcmp(value.auxiliary, snapshot.bytes.data(), snapshot.bytes.size()) == 0, "Copying must preserve complete source/destination auxiliary bytes and handler/context identities");
    for (unsigned i = 0; i < snapshot.allocations.size(); ++i) Require(trace.live.at(snapshot.allocations[i]) == snapshot.generations[i], "Copying must preserve auxiliary, fallback and all three callback-record allocation generations");
    Require(static_cast<const Value*>(snapshot.allocations[1])->parent == &value, "Copied callback metadata must retain its original fallback owner");
}
static std::string Serialize(Value& value) {
    String output{}; _ZN3sce4Json6StringC1EPKc(&output, "");
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0, "Callback copy source must remain serializable");
    std::string text = _ZNK3sce4Json6String5c_strEv(&output);
    _ZN3sce4Json6StringD1Ev(&output);
    return text;
}
int main() {
    CopyAllocations allocations; CopyAllocator allocator; Parameter parameter{&allocator, &allocations, 8192, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize callback copy contracts");
    const auto baseline = allocations.live;
    for (const char* document : {"null", "1.25", "{\"n\":null,\"s\":\"text\"}"}) for (unsigned mode = 0; mode < 5; ++mode) {
        Value source{}, copy{}; Pair pair{}; String key{}; Array array{};
        _ZN3sce4Json5ValueC1Ev(&source);
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&source, document, std::strlen(document)) == 0, "Construct callback-bearing source");
        CopyContext sourceContext, destinationContext;
        _ZN3sce4Json5ValueC1El(&sourceContext.replacement, 77); _ZN3sce4Json5ValueC1El(&destinationContext.replacement, 99);
        Register(source, sourceContext);
        const auto sourceMetadata = Capture(source, allocations);
        Value* target = &copy;
        MetadataSnapshot destinationMetadata{};
        if (mode == 0) _ZN3sce4Json5ValueC1ERKS1_(&copy, &source);
        else if (mode == 1 || mode == 2) {
            _ZN3sce4Json5ValueC1Ev(&copy);
            if (mode == 2) { Register(copy, destinationContext); destinationMetadata = Capture(copy, allocations); }
            _ZN3sce4Json5Value3setERKS1_(&copy, &source);
        } else if (mode == 3) {
            _ZN3sce4Json6StringC1EPKc(&key, "key");
            _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &source); target = &pair.value;
        } else {
            _ZN3sce4Json5ArrayC1Ev(&array);
            _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &source); target = const_cast<Value*>(_ZNK3sce4Json5Array5frontEv(&array));
        }
        Require(!target->parent && target->type == source.type && Serialize(*target) == document, "Copies must retain source contents with independent root ownership");
        if (mode == 2) Verify(*target, destinationMetadata, allocations);
        else Require(!target->auxiliary, "Modern constructors and unregistered destinations must not inherit source-local callback metadata");
        const auto* sourceInteger = _ZNK3sce4Json5Value10getIntegerEv(&source);
        const auto* targetInteger = _ZNK3sce4Json5Value10getIntegerEv(target);
        Require(sourceInteger == reinterpret_cast<const std::int64_t*>(&sourceContext.replacement.payload) && *sourceInteger == 77, "Source recovery must retain its exact borrowed replacement identity");
        Require(mode == 2 ? targetInteger == reinterpret_cast<const std::int64_t*>(&destinationContext.replacement.payload) && *targetInteger == 99 : *targetInteger == 0, "Copies must use destination handlers or typed defaults rather than source callbacks");
        if (source.type == 7) { _ZNK3sce4Json5ValueixEPKc(&source, "missing"); _ZNK3sce4Json5ValueixEPKc(target, "missing"); }
        _ZN3sce4Json5Value3setEd(target, std::numeric_limits<double>::infinity());
        Require(target->type == (mode == 2 ? 2 : 4) && target->payload == (mode == 2 ? UINT64_C(99) : std::bit_cast<std::uint64_t>(std::numeric_limits<double>::infinity())), "Copies must retain destination float recovery without dispatching source handlers");
        Require(sourceContext.nulls == unsigned(source.type == 0) && sourceContext.mismatches == unsigned(source.type != 0) && !sourceContext.floats && sourceContext.elements == unsigned(source.type == 7), "Copy consumers must not invoke source callbacks except explicit source operations");
        Require(destinationContext.nulls == unsigned(mode == 2 && source.type == 0) && destinationContext.mismatches == unsigned(mode == 2 && source.type != 0) && destinationContext.floats == unsigned(mode == 2) && destinationContext.elements == unsigned(mode == 2 && source.type == 7), "Assignment must preserve each destination callback and context independently");
        Verify(source, sourceMetadata, allocations);
        Require(Serialize(source) == document, "Copied value mutation must preserve its independent source contents");
        if (mode == 2) Verify(*target, destinationMetadata, allocations);
        if (mode == 3) { _ZN3sce4Json6Object4PairD1Ev(&pair); _ZN3sce4Json6StringD1Ev(&key); }
        else if (mode == 4) _ZN3sce4Json5ArrayD1Ev(&array);
        else _ZN3sce4Json5ValueD1Ev(&copy);
        Verify(source, sourceMetadata, allocations);
        _ZN3sce4Json5Value3setEd(&source, std::numeric_limits<double>::infinity());
        Require(sourceContext.floats == 1 && source.type == 2 && source.payload == 77, "Source callbacks must remain usable after copied storage and destination callbacks are destroyed");
        Verify(source, sourceMetadata, allocations);
        _ZN3sce4Json5ValueD1Ev(&source); _ZN3sce4Json5ValueD1Ev(&sourceContext.replacement); _ZN3sce4Json5ValueD1Ev(&destinationContext.replacement);
        Require(allocations.live == baseline, "Callback copy cleanup must reclaim every temporary generation without replacing module resources");
    }
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && allocations.live.empty(), "Release callback copy resources");
    return 0;
}
