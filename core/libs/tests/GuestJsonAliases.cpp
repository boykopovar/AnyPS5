#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* storage; };
struct Array { void* storage; };
struct Object { void* storage; };
struct Iterator { void* state; };
struct Pair { String key; std::uint64_t reserved; Value value; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; };
using AliasAllocateCallback = void* (APS5_VABI*)(std::size_t, void*);
using AliasFreeCallback = void (APS5_VABI*)(void*, void*);
using AliasNotifyCallback = void (APS5_VABI*)(int, std::size_t, void*);
struct Callbacks { AliasAllocateCallback allocate; AliasFreeCallback free; AliasNotifyCallback notify; };
static_assert(sizeof(Value) == 32 && sizeof(Parameter) == 24 && sizeof(Pair) == 48);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC2Ev(void*);
void APS5_VABI _ZN3sce4Json11InitializerD2Ev(void*);
void APS5_VABI _ZN3sce4Json17InitParameterRttiC2EPNS0_14AllocParamRttiEPvm(Parameter*, void*, void*, std::size_t);
void APS5_VABI _ZN3sce4Json5Array8iteratorC2ERKS2_(Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json5Array8iteratorC2Ev(Iterator*);
void APS5_VABI _ZN3sce4Json5Array8iteratorD2Ev(Iterator*);
void APS5_VABI _ZN3sce4Json5ArrayC2ERKS1_(Array*, const Array*);
void APS5_VABI _ZN3sce4Json5ArrayC2Ev(Array*);
void APS5_VABI _ZN3sce4Json5ArrayD2Ev(Array*);
void APS5_VABI _ZN3sce4Json5ValueC2ENS0_9ValueTypeE(Value*, std::int32_t);
void APS5_VABI _ZN3sce4Json5ValueC2EPKc(Value*, const char*);
void APS5_VABI _ZN3sce4Json5ValueC2ERKNS0_5ArrayE(Value*, const Array*);
void APS5_VABI _ZN3sce4Json5ValueC2ERKNS0_6ObjectE(Value*, const Object*);
void APS5_VABI _ZN3sce4Json5ValueC2ERKNS0_6StringE(Value*, const String*);
void APS5_VABI _ZN3sce4Json5ValueC2ERKS1_(Value*, const Value*);
void APS5_VABI _ZN3sce4Json5ValueC2Eb(Value*, bool);
void APS5_VABI _ZN3sce4Json5ValueC2Ed(Value*, double);
void APS5_VABI _ZN3sce4Json5ValueC2El(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5ValueC2Em(Value*, std::uint64_t);
void APS5_VABI _ZN3sce4Json5ValueC2Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueD2Ev(Value*);
void APS5_VABI _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(Pair*, const String*, const Value*);
void APS5_VABI _ZN3sce4Json6Object4PairC2Ev(Pair*);
void APS5_VABI _ZN3sce4Json6Object4PairD2Ev(Pair*);
void APS5_VABI _ZN3sce4Json6Object8iteratorC2ERKS2_(Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json6Object8iteratorC2Ev(Iterator*);
void APS5_VABI _ZN3sce4Json6Object8iteratorD2Ev(Iterator*);
void APS5_VABI _ZN3sce4Json6ObjectC2ERKS1_(Object*, const Object*);
void APS5_VABI _ZN3sce4Json6ObjectC2Ev(Object*);
void APS5_VABI _ZN3sce4Json6ObjectD2Ev(Object*);
void APS5_VABI _ZN3sce4Json6StringC2EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringC2ERKS1_(String*, const String*);
void APS5_VABI _ZN3sce4Json6StringC2Ev(String*);
void APS5_VABI _ZN3sce4Json6StringD2Ev(String*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void*, const Parameter*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array*, const Value*);
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array*);
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array*);
Iterator* APS5_VABI _ZNK3sce4Json5Array5beginEv(Iterator*, const Array*);
Value* APS5_VABI _ZNK3sce4Json5Array8iteratordeEv(const Iterator*);
Iterator* APS5_VABI _ZNK3sce4Json6Object5beginEv(Iterator*, const Object*);
const Pair* APS5_VABI _ZNK3sce4Json6Object8iteratordeEv(const Iterator*);
std::size_t APS5_VABI _ZNK3sce4Json6Object4sizeEv(const Object*);
Value* APS5_VABI _ZN3sce4Json6ObjectixERKNS0_6StringE(Object*, const String*);
void APS5_VABI _ZN3sce4Json5Value3setEl(Value*, std::int64_t);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value*, const char*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct AliasTrace {
    std::map<void*, std::size_t> live;
    int budget = -1;
    bool once = false;
    std::size_t requests = 0;
    unsigned notifications = 0;
};
static void* APS5_VABI AllocateAlias(std::size_t size, void* context) {
    auto& trace = *static_cast<AliasTrace*>(context);
    ++trace.requests;
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(size);
    Require(memory != nullptr, "Alias tracking allocator requires host memory");
    std::memset(memory, 0xa5, size); trace.live.emplace(memory, size); return memory;
}
static void APS5_VABI FreeAlias(void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<AliasTrace*>(context)->live.erase(memory) == 1, "Alias destruction must free each allocation through its owning context exactly once");
    std::free(memory);
}
static void APS5_VABI NotifyAlias(int error, std::size_t, void* context) {
    Require(static_cast<unsigned>(error) == 0x80848102, "Alias allocation failures must retain the Json2 error");
    ++static_cast<AliasTrace*>(context)->notifications;
}
static const String* Text(const Value& value) { return reinterpret_cast<const String*>(value.payload); }
static void CheckText(const Value& value, const char* expected) {
    Require(value.type == 5 && value.payload && std::strcmp(_ZNK3sce4Json6String5c_strEv(Text(value)), expected) == 0, "Value constructor variants must preserve string contents");
}
int main() {
    AliasTrace trace;
    Callbacks callbacks{AllocateAlias, FreeAlias, NotifyAlias};
    alignas(8) std::array<std::byte, 40> settings; settings.fill(std::byte{0xa5});
    auto* parameter = reinterpret_cast<Parameter*>(settings.data());
    _ZN3sce4Json17InitParameterRttiC2EPNS0_14AllocParamRttiEPvm(parameter, &callbacks, &trace, 8192);
    Require(parameter->allocator && parameter->context == &trace && parameter->bufferSize == 8192, "RTTI constructor variant must retain its callbacks, context and buffer setting");
    for (std::size_t i = sizeof(Parameter); i < settings.size(); ++i) Require(settings[i] == std::byte{0xa5}, "RTTI construction must preserve bytes beyond its parameter block");
    std::array<unsigned char, 16> initializer; initializer.fill(0xa5);
    _ZN3sce4Json11InitializerC2Ev(initializer.data());
    Require(initializer[0] == 0, "Initializer constructor variant must clear its active byte");
    for (std::size_t i = 1; i < initializer.size(); ++i) Require(initializer[i] == 0xa5, "Initializer construction must preserve every adjacent byte");
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(initializer.data(), parameter) == 0, "Initialize through the constructor variant and RTTI allocator");
    const auto baseline = trace.live.size();
    for (std::int32_t type = 0; type < 8; ++type) {
        Value value; std::memset(&value, 0xa5, sizeof(value));
        _ZN3sce4Json5ValueC2ENS0_9ValueTypeE(&value, type);
        Require(value.type == type && !value.parent && !value.auxiliary && value.reserved == 0xa5a5a5a5, "Typed value constructor variant must retain its type and inactive metadata");
        _ZN3sce4Json5ValueD2Ev(&value);
        Require(trace.live.size() == baseline, "Typed constructor/destructor variants must reclaim their handles and storage");
    }
    Value value{};
    _ZN3sce4Json5ValueC2Ev(&value); Require(value.type == 0, "Default value constructor variant must construct null"); _ZN3sce4Json5ValueD2Ev(&value);
    _ZN3sce4Json5ValueC2Eb(&value, true); Require(value.type == 1 && (value.payload & 255) == 1, "Boolean constructor variant must preserve its payload"); _ZN3sce4Json5ValueD2Ev(&value);
    _ZN3sce4Json5ValueC2El(&value, -77); Require(value.type == 2 && value.payload == std::bit_cast<std::uint64_t>(std::int64_t{-77}), "Signed constructor variant must preserve all payload bits"); _ZN3sce4Json5ValueD2Ev(&value);
    _ZN3sce4Json5ValueC2Em(&value, UINT64_MAX); Require(value.type == 3 && value.payload == UINT64_MAX, "Unsigned constructor variant must preserve all payload bits"); _ZN3sce4Json5ValueD2Ev(&value);
    _ZN3sce4Json5ValueC2Ed(&value, 1.25); Require(value.type == 4 && value.payload == std::bit_cast<std::uint64_t>(1.25), "Real constructor variant must preserve its payload"); _ZN3sce4Json5ValueD2Ev(&value);
    String empty{}, text{}, copiedText{};
    _ZN3sce4Json6StringC2Ev(&empty); Require(empty.storage && !*_ZNK3sce4Json6String5c_strEv(&empty), "Default string constructor variant must own empty storage");
    _ZN3sce4Json6StringC2EPKc(&text, "abc"); _ZN3sce4Json6StringC2ERKS1_(&copiedText, &text);
    Require(text.storage != copiedText.storage && std::strcmp(_ZNK3sce4Json6String5c_strEv(&copiedText), "abc") == 0, "String copy constructor variant must own independent storage");
    _ZN3sce4Json5ValueC2EPKc(&value, "abc"); CheckText(value, "abc");
    Value copiedValue{}; _ZN3sce4Json5ValueC2ERKS1_(&copiedValue, &value); CheckText(copiedValue, "abc");
    Require(copiedValue.payload != value.payload, "Value copy constructor variant must own an independent handle");
    _ZN3sce4Json5ValueD2Ev(&copiedValue); _ZN3sce4Json5ValueD2Ev(&value);
    _ZN3sce4Json5ValueC2ERKNS0_6StringE(&value, &text); CheckText(value, "abc");
    Require(Text(value)->storage != text.storage, "String-to-value constructor variant must copy storage"); _ZN3sce4Json5ValueD2Ev(&value);
    Array array{}, copiedArray{}; _ZN3sce4Json5ArrayC2Ev(&array);
    _ZN3sce4Json5ValueC2El(&value, 42); _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &value);
    _ZN3sce4Json5ArrayC2ERKS1_(&copiedArray, &array);
    Require(array.storage != copiedArray.storage && _ZNK3sce4Json5Array4sizeEv(&copiedArray) == 1 && _ZNK3sce4Json5Array5frontEv(&copiedArray)->payload == 42, "Array copy constructor variant must copy its entries into independent storage");
    Value wrapped{}; _ZN3sce4Json5ValueC2ERKNS0_5ArrayE(&wrapped, &array);
    const auto* child = _ZNK3sce4Json5Array5frontEv(reinterpret_cast<const Array*>(wrapped.payload));
    Require(wrapped.type == 6 && child->parent == &wrapped && child->payload == 42, "Array-to-value constructor variant must bind copied children to their owner"); _ZN3sce4Json5ValueD2Ev(&wrapped);
    Object object{}, copiedObject{}; _ZN3sce4Json6ObjectC2Ev(&object);
    _ZN3sce4Json5Value3setEl(_ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &text), 42);
    _ZN3sce4Json6ObjectC2ERKS1_(&copiedObject, &object);
    Require(object.storage != copiedObject.storage && _ZNK3sce4Json6Object4sizeEv(&copiedObject) == 1, "Object copy constructor variant must own independent tree storage");
    _ZN3sce4Json5ValueC2ERKNS0_6ObjectE(&wrapped, &object);
    child = _ZNK3sce4Json5ValueixEPKc(&wrapped, "abc");
    Require(wrapped.type == 7 && child->parent == &wrapped && child->payload == 42, "Object-to-value constructor variant must bind copied members to their owner"); _ZN3sce4Json5ValueD2Ev(&wrapped);
    Pair pair{}; _ZN3sce4Json6Object4PairC2Ev(&pair);
    Require(pair.key.storage && pair.value.type == 0, "Default pair constructor variant must construct its key and null value"); _ZN3sce4Json6Object4PairD2Ev(&pair);
    _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(&pair, &text, &value);
    Require(pair.key.storage != text.storage && pair.value.type == 2 && pair.value.payload == 42, "Pair constructor variant must copy its key and value"); _ZN3sce4Json6Object4PairD2Ev(&pair);
    Iterator iterator{}, copy{};
    _ZN3sce4Json5Array8iteratorC2Ev(&iterator); _ZN3sce4Json5Array8iteratorC2ERKS2_(&copy, &iterator);
    Require(iterator.state && copy.state && iterator.state != copy.state && !*static_cast<void**>(copy.state), "Default array iterator variants must own independent unbound states");
    _ZN3sce4Json5Array8iteratorD2Ev(&copy); _ZN3sce4Json5Array8iteratorD2Ev(&iterator);
    _ZNK3sce4Json5Array5beginEv(&iterator, &array); _ZN3sce4Json5Array8iteratorC2ERKS2_(&copy, &iterator);
    Require(copy.state != iterator.state && _ZNK3sce4Json5Array8iteratordeEv(&copy) == _ZNK3sce4Json5Array5frontEv(&array), "Array iterator copy variant must preserve the actual element identity");
    _ZN3sce4Json5Array8iteratorD2Ev(&copy); _ZN3sce4Json5Array8iteratorD2Ev(&iterator);
    _ZN3sce4Json6Object8iteratorC2Ev(&iterator); _ZN3sce4Json6Object8iteratorC2ERKS2_(&copy, &iterator);
    Require(iterator.state && copy.state && iterator.state != copy.state && !*static_cast<void**>(copy.state), "Default object iterator variants must own independent unbound states");
    _ZN3sce4Json6Object8iteratorD2Ev(&copy); _ZN3sce4Json6Object8iteratorD2Ev(&iterator);
    _ZNK3sce4Json6Object5beginEv(&iterator, &object); _ZN3sce4Json6Object8iteratorC2ERKS2_(&copy, &iterator);
    Require(copy.state != iterator.state && _ZNK3sce4Json6Object8iteratordeEv(&copy) == _ZNK3sce4Json6Object8iteratordeEv(&iterator), "Object iterator copy variant must preserve the actual member identity");
    _ZN3sce4Json6Object8iteratorD2Ev(&copy); _ZN3sce4Json6Object8iteratorD2Ev(&iterator);
    _ZN3sce4Json6ObjectD2Ev(&copiedObject); _ZN3sce4Json6ObjectD2Ev(&object);
    _ZN3sce4Json5ArrayD2Ev(&copiedArray); _ZN3sce4Json5ArrayD2Ev(&array); _ZN3sce4Json5ValueD2Ev(&value);
    _ZN3sce4Json6StringD2Ev(&copiedText); _ZN3sce4Json6StringD2Ev(&text); _ZN3sce4Json6StringD2Ev(&empty);
    Require(trace.live.size() == baseline, "All constructor/destructor variants must reclaim their owned resources");
    String absent{};
    const auto requests = trace.requests;
    _ZN3sce4Json5ValueC2ERKNS0_6StringE(&value, &absent);
    Require(value.type == 0 && trace.requests == requests, "Missing string storage must leave a null value without allocating"); _ZN3sce4Json5ValueD2Ev(&value);
    _ZN3sce4Json5ValueC2EPKc(&value, nullptr); CheckText(value, ""); _ZN3sce4Json5ValueD2Ev(&value);
    struct Failure { bool once; int budget; int type; unsigned notifications; std::size_t requests; const char* text; };
    const Failure failures[]{
        {false, 0, 0, 1, 1, ""}, {false, 1, 0, 2, 3, ""},
        {false, 2, 0, 1, 3, ""}, {false, 3, 0, 1, 4, ""},
        {false, 4, 5, 1, 5, ""}, {false, 5, 5, 0, 5, "abc"},
        {true, 0, 0, 1, 1, ""}, {true, 1, 5, 1, 4, ""},
        {true, 2, 0, 1, 3, ""}, {true, 3, 0, 1, 4, ""},
        {true, 4, 5, 1, 5, ""}, {true, 5, 5, 0, 5, "abc"},
    };
    for (const auto& failure : failures) {
        const auto notifications = trace.notifications;
        const auto before = trace.requests;
        trace.budget = failure.budget; trace.once = failure.once;
        _ZN3sce4Json5ValueC2EPKc(&value, "abc"); trace.budget = -1; trace.once = false;
        Require(value.type == failure.type && trace.notifications - notifications == failure.notifications && trace.requests - before == failure.requests, "Text constructor variant must preserve temporary string allocation and failure admission");
        if (value.type == 5) CheckText(value, failure.text);
        _ZN3sce4Json5ValueD2Ev(&value);
        Require(trace.live.size() == baseline, "Failed constructor variants must reclaim their admitted prefix");
    }
    _ZN3sce4Json11InitializerD2Ev(initializer.data());
    Require(initializer[0] == 0 && trace.live.empty(), "Initializer destructor variant must terminate its resources");
    for (std::size_t i = 1; i < initializer.size(); ++i) Require(initializer[i] == 0xa5, "Initializer destruction must preserve every adjacent byte");
    return 0;
}
