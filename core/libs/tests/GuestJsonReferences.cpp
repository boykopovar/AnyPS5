#include "prx/libSceJson2/include/Api.hpp"
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

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using Mismatch = const Value& (APS5_VABI*)(ValueType, const Value*, const Value*, void*);
static_assert(sizeof(Value) == 32 && sizeof(Parameter) == 40 && offsetof(Parameter, mode) == 24);
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static void CheckParameters() {
    int first = 1, second = 2;
    const Parameter parameters[]{
        {nullptr, nullptr, 0, 0, {1, 2, 3}},
        {&first, &second, 8192, 2, {4, 5, 6}},
        {&second, &first, std::numeric_limits<std::size_t>::max(), -17, {7, 8, 9}},
    };
    for (const auto& parameter : parameters) {
        const auto before = parameter;
        Require(_ZNK3sce4Json14InitParameter212getAllocatorEv(reinterpret_cast<const InitParameter2*>(&parameter)) == parameter.allocator && _ZNK3sce4Json14InitParameter211getUserDataEv(reinterpret_cast<const InitParameter2*>(&parameter)) == parameter.context, "Parameter getters must return exact pointer fields");
        Require(_ZNK3sce4Json14InitParameter217getFileBufferSizeEv(reinterpret_cast<const InitParameter2*>(&parameter)) == parameter.bufferSize && _ZNK3sce4Json14InitParameter225getSpecialFloatFormatTypeEv(reinterpret_cast<const InitParameter2*>(&parameter)) == parameter.mode, "Parameter getters must return field bits without normalizing or validating settings");
        Require(std::memcmp(&before, &parameter, sizeof(parameter)) == 0, "Parameter getters must not mutate the parameter block");
    }
}
static void CheckReferences() {
    int storage = 1, replacement = 2;
    String string{reinterpret_cast<Text*>(&storage)};
    for (std::int32_t type : {0, 1, 2, 3, 4, 5, 6, 7, 8, -1, INT32_MIN, INT32_MAX}) {
        Value value{}; value.type = static_cast<ValueType>(type);
        value.payload = type == 5 ? reinterpret_cast<std::uintptr_t>(&string) : UINT64_C(0x1122334455667788);
        const auto before = value;
        auto* boolean = _ZN3sce4Json5Value12referBooleanEv(&value);
        auto* integer = _ZN3sce4Json5Value12referIntegerEv(&value);
        auto* uinteger = _ZN3sce4Json5Value13referUIntegerEv(&value);
        auto* real = _ZN3sce4Json5Value9referRealEv(&value);
        auto* text = _ZN3sce4Json5Value11referStringEv(&value);
        Require(reinterpret_cast<void*>(boolean) == (type == 1 ? &value.payload : nullptr), "Boolean references must identify this value's payload or return null");
        Require(reinterpret_cast<void*>(integer) == ((type == 2 || type == 3) ? &value.payload : nullptr) && reinterpret_cast<void*>(uinteger) == reinterpret_cast<void*>(integer), "Both integer reference functions must accept signed and unsigned values and share their actual storage");
        Require(reinterpret_cast<void*>(real) == (type == 4 ? &value.payload : nullptr) && text == (type == 5 ? &string : nullptr), "Real and string references must return compatible owned storage without fallback");
        Require(std::memcmp(&before, &value, sizeof(value)) == 0, "Obtaining references must preserve the value and its metadata");
        if (boolean) {
            *boolean = true;
            Require(value.payload == ((before.payload & ~UINT64_C(255)) | 1), "Boolean references must permit in-place writes without changing inactive payload bytes");
        }
        if (integer) {
            *integer = -123;
            Require(value.payload == std::bit_cast<std::uint64_t>(std::int64_t{-123}), "Signed reference writes must preserve their exact payload bits");
            *uinteger = UINT64_MAX;
            Require(value.payload == UINT64_MAX, "Unsigned reference writes must update the same integer payload");
        }
        if (real) {
            *real = -1.25;
            Require(value.payload == std::bit_cast<std::uint64_t>(-1.25), "Real references must permit writes to the actual payload");
        }
        if (text) {
            text->text = reinterpret_cast<Text*>(&replacement);
            Require(string.text == reinterpret_cast<Text*>(&replacement), "String references must expose the actual mutable string handle");
        }
        Require(value.type == before.type && value.parent == before.parent && value.auxiliary == before.auxiliary, "Reference writes must preserve type and ownership metadata");
    }
    Value missing{}; missing.type = static_cast<ValueType>(5);
    Require(_ZN3sce4Json5Value11referStringEv(&missing) == nullptr, "A string value with missing storage must return null without allocating a replacement");
}
struct ReferenceTrace {
    std::map<void*, std::size_t> live;
    bool rejectAllocations = false;
};
static void* APS5_VABI AllocateReference(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ReferenceTrace*>(context);
    Require(!trace.rejectAllocations, "Reference and parameter access must not allocate");
    void* memory = std::malloc(bytes); Require(memory != nullptr, "Reference allocator requires host memory");
    trace.live.emplace(memory, bytes); return memory;
}
static void APS5_VABI FreeReference(void*, void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<ReferenceTrace*>(context)->live.erase(memory) == 1, "Reference metadata must return through the owning allocator exactly once");
    std::free(memory);
}
static void APS5_VABI NotifyReference(void*, int, std::size_t, void*) { Require(false, "Reference tests must not encounter allocation failure"); }
static void* const ReferenceVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateReference), reinterpret_cast<void*>(FreeReference), reinterpret_cast<void*>(NotifyReference)};
struct ReferenceAllocator { void* const* vtable = ReferenceVtable; };
struct Recovery { Value replacement{}; unsigned calls = 0; };
static const Value& APS5_VABI RecoverReference(ValueType type, const Value*, const Value*, void* context) {
    auto& recovery = *static_cast<Recovery*>(context); ++recovery.calls;
    Require(type == 1, "The preservation control must request a boolean replacement");
    return recovery.replacement;
}
int main() {
    CheckParameters(); CheckReferences();
    ReferenceTrace trace; ReferenceAllocator allocator;
    Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize reference callback tests");
    Value value{}; Recovery recovery;
    _ZN3sce4Json5ValueC1Ev(&value); _ZN3sce4Json5ValueC1Eb(&recovery.replacement, true);
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&value, RecoverReference, &recovery) == 0, "Register a mismatch handler for the callback-bypass test");
    trace.rejectAllocations = true;
    CheckParameters(); CheckReferences();
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        Require(!_ZN3sce4Json5Value12referBooleanEv(&value) && !_ZN3sce4Json5Value12referIntegerEv(&value) && !_ZN3sce4Json5Value13referUIntegerEv(&value) && !_ZN3sce4Json5Value9referRealEv(&value) && !_ZN3sce4Json5Value11referStringEv(&value), "Reference accessors must return null for mismatched values even when recovery is registered");
        Require(recovery.calls == attempt, "Reference accessors must never invoke recovery callbacks");
        Require(*_ZNK3sce4Json5Value10getBooleanEv(&value) && recovery.calls == attempt + 1, "Ordinary getters must preserve their working callback recovery behavior");
    }
    trace.rejectAllocations = false;
    _ZN3sce4Json5ValueD1Ev(&recovery.replacement); _ZN3sce4Json5ValueD1Ev(&value);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Reference teardown must reclaim all initialized resources and callback metadata");
    CheckParameters(); CheckReferences();
    return 0;
}
