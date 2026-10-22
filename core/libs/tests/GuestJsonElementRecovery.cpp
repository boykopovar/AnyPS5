#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

struct Value { Value* parent; void* auxiliary; std::uintptr_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* text; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using Handler = void (APS5_VABI*)(int, const String*, const std::size_t*, const Value*, const Value*, void*);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value5clearEv(Value*);
int APS5_VABI _ZN3sce4Json5Value3setEl(Value*, std::int64_t);
int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(Value*, Handler, void*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value*, const char*);
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKNS0_6StringE(const Value*, const String*);
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueEm(const Value*, std::size_t);
Value* APS5_VABI _ZN3sce4Json5Value10referValueEm(Value*, std::size_t);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct Context {
    int calls = 0;
    unsigned error = 0;
    std::string key;
    const String* keyPointer = nullptr;
    std::size_t index = 0;
    bool indexed = false;
    const Value* parent = nullptr;
    const Value* value = nullptr;
    bool reenter = false;
    const Value* fallback = nullptr;
};
static void APS5_VABI Recover(int error, const String* key, const std::size_t* index,
                             const Value* parent, const Value* value, void* context) {
    auto& trace = *static_cast<Context*>(context);
    ++trace.calls;
    trace.error = static_cast<unsigned>(error);
    trace.key = key ? _ZNK3sce4Json6String5c_strEv(key) : "";
    trace.keyPointer = key;
    trace.indexed = index != nullptr;
    trace.index = index ? *index : 0;
    trace.parent = parent;
    trace.value = value;
    if (trace.reenter) {
        trace.fallback = _ZNK3sce4Json5ValueixEPKc(value, "retry");
        _ZN3sce4Json5Value3setEl(const_cast<Value*>(trace.fallback), 77);
    }
}
static void Event(const Context& trace, int calls, unsigned error, const Value* value, const Value* parent) {
    Require(trace.calls == calls && trace.error == error && trace.value == value && trace.parent == parent,
            "Recovery must receive the exact error, source value, parent and context");
}
int main() {
    Value root{};
    _ZN3sce4Json5ValueC1Ev(&root);
    Context trace;
    Require(static_cast<unsigned>(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, Recover, &trace)) == 0x80848110,
            "Registration before initialization must report not initialized");
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, 2, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize element recovery");
    const char text[] = "{\"list\":[1],\"number\":2}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, text, sizeof(text) - 1) == 0, "Parse recovery tree");
    Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, Recover, &trace) == 0,
            "Register root recovery");
    const auto* list = _ZNK3sce4Json5ValueixEPKc(&root, "list");
    const auto* number = _ZNK3sce4Json5ValueixEPKc(&root, "number");
    Require(trace.calls == 0 && list->parent == &root && number->parent == &root, "Successful lookups must bypass failure recovery");
    const auto* fallback = _ZNK3sce4Json5ValueixEPKc(&root, "missing");
    Event(trace, 1, 0x80848115, &root, nullptr);
    Require(trace.key == "missing" && !trace.indexed && fallback->type == 0 && fallback->parent == &root,
            "A missing object key must return its root-owned fallback value");
    String missing{};
    _ZN3sce4Json6StringC1EPKc(&missing, "missing");
    Require(_ZNK3sce4Json5Value8getValueERKNS0_6StringE(&root, &missing) == fallback,
            "String and C-string lookup must share the root fallback");
    Event(trace, 2, 0x80848115, &root, nullptr);
    Require(trace.keyPointer == &missing, "The String overload must pass the supplied key object");
    _ZNK3sce4Json5Value8getValueERKNS0_6StringE(list, &missing);
    Event(trace, 3, 0x80848117, list, &root);
    Require(trace.key == "missing" && !trace.indexed, "Wrong-type key lookup must pass the key and no index");
    Require(_ZNK3sce4Json5Value8getValueEm(list, 1) == fallback, "Nested refusal must use the same root fallback");
    Event(trace, 4, 0x80848116, list, &root);
    Require(trace.indexed && trace.index == 1 && !trace.keyPointer, "Bounds failure must pass the complete index and no key");
    _ZNK3sce4Json5Value8getValueEm(number, static_cast<std::size_t>(-1));
    Event(trace, 5, 0x80848118, number, &root);
    Require(trace.indexed && trace.index == static_cast<std::size_t>(-1), "Wrong-type numeric lookup must preserve all index bits");
    Require(_ZNK3sce4Json5Value8getValueEm(&root, 0) == list && _ZN3sce4Json5Value10referValueEm(&root, 1) == number && trace.calls == 5,
            "Numeric object lookup must use ordered entries without reporting failure");
    Require(_ZN3sce4Json5Value10referValueEm(&root, 2) == nullptr && trace.calls == 5,
            "The nullable mutable lookup must not dispatch failure recovery");
    trace.reenter = true;
    Require(_ZNK3sce4Json5ValueixEPKc(list, "missing") == fallback, "Reentrant refusal must keep fallback identity");
    Event(trace, 6, 0x80848117, list, &root);
    Require(trace.fallback == fallback && fallback->type == 2 && fallback->payload == 77,
            "The callback must be able to update the root fallback without recursively dispatching itself");
    trace.reenter = false;
    Require(_ZNK3sce4Json5Value8getValueEm(fallback, 9) == fallback && trace.calls == 6,
            "A refusal on the fallback itself must suppress recovery");
    _ZN3sce4Json5Value5clearEv(const_cast<Value*>(fallback));
    Require(_ZNK3sce4Json5ValueixEPKc(&root, "missing") == fallback && trace.calls == 7 && fallback->type == 0,
            "Recovery must resume after reentry and retain fallback ownership");
    Require(static_cast<unsigned>(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(const_cast<Value*>(list), Recover, &trace)) == 0x80848104,
            "An owned child must reject handler registration");
    Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, nullptr, nullptr) == 0,
            "Unregister element recovery");
    Require(_ZNK3sce4Json5ValueixEPKc(&root, "missing") == fallback && trace.calls == 7,
            "Unregistration must preserve fallback identity and stop dispatch");
    _ZN3sce4Json6StringD1Ev(&missing);
    _ZN3sce4Json5ValueD1Ev(&root);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate element recovery");
    return 0;
}
