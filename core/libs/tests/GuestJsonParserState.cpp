#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* storage; };
struct Array { void* storage; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; };
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1El(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value*, const char*);
std::size_t APS5_VABI _ZNK3sce4Json5Value5countEv(const Value*);
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array*);
const Value* APS5_VABI _ZNK3sce4Json5Array4backEv(const Array*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static void* APS5_VABI AllocateTracked(void*, std::size_t size, void* context) {
    void* memory = std::malloc(size);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    static_cast<std::map<void*, std::size_t>*>(context)->emplace(memory, size);
    return memory;
}
static void APS5_VABI FreeTracked(void*, void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<std::map<void*, std::size_t>*>(context)->erase(memory) == 1, "Parser allocations must be freed exactly once by their owning allocator");
    std::free(memory);
}
static int Parse(Value& value, const char* text) {
    return _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, text, std::strlen(text));
}
int main() {
    JsonTest::Allocator allocator;
    void* const vtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateTracked), reinterpret_cast<void*>(FreeTracked), reinterpret_cast<void*>(JsonTest::Notify)};
    allocator.vtable = vtable;
    std::map<void*, std::size_t> live;
    Parameter parameter{&allocator, &live, 512};
    std::uint64_t initializer[2]{};
    _ZN3sce4Json11InitializerC1Ev(initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(initializer, &parameter) == 0, "Initialize parser state tests");
    const auto baseline = live.size();
    for (const char* text : {"", "?", "tru"}) {
        Value value{}; _ZN3sce4Json5ValueC1El(&value, 77);
        Require(static_cast<unsigned>(Parse(value, text)) == 0x80848101 && value.type == 2 && value.payload == 77, "Unrecognized or incomplete scalar input must preserve the previous payload");
        _ZN3sce4Json5ValueD1Ev(&value);
    }
    for (const char* text : {"[1,", "[1,?]"}) {
        Value value{}; _ZN3sce4Json5ValueC1El(&value, 77);
        Require(static_cast<unsigned>(Parse(value, text)) == 0x80848101 && value.type == 6 && _ZNK3sce4Json5Value5countEv(&value) == 2, "Failed array parsing must retain admitted entries instead of restoring the destination");
        auto* array = reinterpret_cast<const Array*>(value.payload);
        const auto* first = _ZNK3sce4Json5Array5frontEv(array);
        const auto* last = _ZNK3sce4Json5Array4backEv(array);
        Require(first->type == 3 && first->payload == 1 && first->parent == &value && last->type == 0, "Partial array entries must retain their payload and destination parent");
        Require(Parse(value, "[]") == 0 && _ZNK3sce4Json5Value5countEv(&value) == 2, "Parsing an empty array into an existing array must retain its entries");
        _ZN3sce4Json5ValueD1Ev(&value);
    }
    for (const char* text : {"{\"a\":1,", "{\"a\":?}"}) {
        Value value{}; _ZN3sce4Json5ValueC1El(&value, 77);
        Require(static_cast<unsigned>(Parse(value, text)) == 0x80848101 && value.type == 7 && _ZNK3sce4Json5Value5countEv(&value) == 1, "Failed object parsing must retain the admitted member");
        Require(Parse(value, "{}") == 0 && _ZNK3sce4Json5Value5countEv(&value) == 1, "Parsing an empty object into an existing object must retain its members");
        _ZN3sce4Json5ValueD1Ev(&value);
    }
    Value nested{}; _ZN3sce4Json5ValueC1El(&nested, 77);
    Require(static_cast<unsigned>(Parse(nested, "[[1,")) == 0x80848101 && nested.type == 6, "Nested failures must retain the outer collection");
    const auto* inner = _ZNK3sce4Json5Array5frontEv(reinterpret_cast<const Array*>(nested.payload));
    Require(inner->type == 6 && inner->parent == &nested && _ZNK3sce4Json5Value5countEv(inner) == 2, "Nested partial collections must retain their immediate owner");
    const auto* leaf = _ZNK3sce4Json5Array5frontEv(reinterpret_cast<const Array*>(inner->payload));
    Require(leaf->parent == inner && leaf->payload == 1, "Nested leaf parents must point into the retained destination tree");
    Require(Parse(nested, "42") == 0 && nested.type == 3 && nested.payload == 42, "Numeric parsing must replace an existing collection");
    _ZN3sce4Json5ValueD1Ev(&nested);
    Value object{}; _ZN3sce4Json5ValueC1El(&object, 77);
    Require(Parse(object, "{\"a\":9}") == 0 && static_cast<unsigned>(Parse(object, "{\"a\":?}")) == 0x80848101, "Prepare duplicate member failure");
    const auto* retained = _ZNK3sce4Json5ValueixEPKc(&object, "a");
    Require(retained->type == 3 && retained->payload == 9 && retained->parent == &object, "An unrecognized duplicate member value must preserve the prior member");
    _ZN3sce4Json5ValueD1Ev(&object);
    Value value{}; _ZN3sce4Json5ValueC1El(&value, 77);
    Require(static_cast<unsigned>(Parse(value, "\"unterminated")) == 0x80848101 && value.type == 5, "Unterminated strings must change the destination type");
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(value.payload)), "unterminated") == 0, "Unterminated strings must retain their decoded prefix");
    Require(Parse(value, "\"!\"") == 0 && std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(value.payload)), "unterminated!") == 0, "Parsing another string must append to existing string storage");
    Require(Parse(value, "true") == 0 && Parse(value, "false") == 0 && value.type == 1 && !(value.payload & 255), "Parsing false must replace an existing true boolean");
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(live.size() == baseline, "Partial parsing and numeric replacement must reclaim all value allocations");
    Require(_ZN3sce4Json11Initializer9terminateEv(initializer) == 0, "Terminate parser state tests");
    Require(live.empty(), "Termination must reclaim the remaining parser context resources");
    return 0;
}
