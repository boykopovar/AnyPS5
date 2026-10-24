#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

struct Value { Value* parent; void* auxiliary; std::uintptr_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* text; };
struct Array { void* items; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using Handler = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1Ed(Value*, double);
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value*, const char*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(Value*, const Array*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value*, Handler, void*);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value*, String*);
void APS5_VABI _ZNK3sce4Json5Value8toStringERNS0_6StringE(const Value*, String*);
const String* APS5_VABI _ZNK3sce4Json5Value9getStringEv(const Value*);
const Array* APS5_VABI _ZNK3sce4Json5Value8getArrayEv(const Value*);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array*);
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array*);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array*, const Value*);
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct Context {
    int calls = 0;
    int function = -1;
    const Value* parent = nullptr;
    bool sawErrorPointer = false;
    bool reject = false;
    bool returnReal = false;
    Value* reenter = nullptr;
};
static std::string Serialize(Value& value, int expectedError = 0, const char* prefix = "") {
    String output{};
    _ZN3sce4Json6StringC1EPKc(&output, prefix);
    const int result = _ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output);
    Require(result == expectedError, "Serialization must propagate the callback's error flag");
    std::string text = _ZNK3sce4Json6String5c_strEv(&output);
    _ZN3sce4Json6StringD1Ev(&output);
    return text;
}
static Value* APS5_VABI Recover(Value* result, std::int32_t function, double number,
                               const Value* parent, void* context, bool* error) {
    auto& trace = *static_cast<Context*>(context);
    ++trace.calls;
    trace.function = function;
    trace.parent = parent;
    trace.sawErrorPointer = error != nullptr;
    Require(number == std::numeric_limits<double>::infinity(), "The handler must receive the original non-finite value");
    if (trace.reenter) {
        Require(Serialize(*trace.reenter) == "inf", "Reentrant serialization must use the configured fallback");
        _ZN3sce4Json5Value3setEd(trace.reenter, number);
    }
    if (trace.returnReal) _ZN3sce4Json5ValueC1Ed(result, number);
    else _ZN3sce4Json5ValueC1EPKc(result, function == 0 ? "format" : "assign");
    if (error) *error = trace.reject;
    return result;
}
static void CheckMode(int mode) {
    Value value{};
    _ZN3sce4Json5ValueC1Ed(&value, std::numeric_limits<double>::infinity());
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, mode, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize special-float recovery");
    Context trace;
    Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&value, Recover, &trace) == 0,
            "Register a special-float handler");
    Require(Serialize(value, 0, "prefix:") == "prefix:\"format\"" && value.type == 4,
            "Serialization must append a separately owned replacement without mutating the original real");
    Require(trace.calls == 1 && trace.function == 0 && !trace.parent && trace.sawErrorPointer,
            "Serialization must supply function 0, the parent and an error pointer");
    String direct{};
    _ZN3sce4Json6StringC1EPKc(&direct, "prefix:");
    _ZNK3sce4Json5Value8toStringERNS0_6StringE(&value, &direct);
    const char* directExpected = mode == 0 ? "prefix:\"inf\"" : mode == 1 ? "prefix:null" : "prefix:inf";
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&direct), directExpected) == 0 && trace.calls == 1,
            "toString must append the configured scalar format without dispatching recovery");
    _ZN3sce4Json6StringD1Ev(&direct);
    trace.reject = true;
    Require(Serialize(value, static_cast<int>(0x80848114), "keep") == "keep" && value.type == 4,
            "A rejected replacement must not overwrite existing output or the source");
    trace.reject = false;
    _ZN3sce4Json5Value3setEd(&value, 0.125);
    Require(trace.calls == 2 && value.type == 4, "Finite assignment must bypass recovery");
    _ZN3sce4Json5Value3setEd(&value, std::numeric_limits<double>::infinity());
    Require(trace.calls == 3 && trace.function == 1 && !trace.parent && !trace.sawErrorPointer,
            "Assignment must supply function 1 and no error pointer for the void setter");
    Require(value.type == 5 && std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&value)), "assign") == 0,
            "Assignment must retain the replacement after the callback temporary is destroyed");
    Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&value, nullptr, nullptr) == 0,
            "Unregister recovery");
    _ZN3sce4Json5Value3setEd(&value, std::numeric_limits<double>::infinity());
    Require(value.type == (mode == 0 ? 5 : mode == 1 ? 0 : 4) && trace.calls == 3,
            "Unregistering must restore the configured fallback conversion");
    _ZN3sce4Json5ValueD1Ev(&value);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate recovery tests");
}
static void CheckParentsAndReentry() {
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, 2, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize parent recovery");
    Array array{};
    _ZN3sce4Json5ArrayC1Ev(&array);
    Value leaf{}, root{}, other{};
    _ZN3sce4Json5ValueC1Ed(&leaf, std::numeric_limits<double>::infinity());
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &leaf);
    _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&root, &array);
    _ZN3sce4Json5ValueC1Ed(&other, std::numeric_limits<double>::infinity());
    Context trace;
    Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&root, Recover, &trace) == 0, "Register ancestor recovery");
    auto* child = const_cast<Value*>(_ZNK3sce4Json5Array5frontEv(_ZNK3sce4Json5Value8getArrayEv(&root)));
    Require(static_cast<unsigned>(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(child, Recover, &trace)) == 0x80848104,
            "An owned child must reject local handler registration");
    Require(Serialize(root) == "[\"format\"]" && trace.calls == 1 && trace.parent == &root,
            "Nested serialization must use the root handler and pass the immediate parent");
    trace.reject = true;
    Require(Serialize(root, static_cast<int>(0x80848114), "keep") == "keep[" && trace.calls == 2,
            "A nested rejection must retain output already emitted before the refused element");
    trace.reject = false;
    _ZN3sce4Json5Value3setEd(child, std::numeric_limits<double>::infinity());
    Require(trace.calls == 3 && trace.function == 1 && trace.parent == &root && child->type == 5,
            "Nested assignment must use ancestor recovery");
    trace.reenter = &other;
    trace.returnReal = true;
    Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&other, Recover, &trace) == 0, "Register reentry target");
    const int before = trace.calls;
    Require(Serialize(other) == "inf" && trace.calls == before + 1,
            "A returned non-finite real and reentrant operations must not recursively dispatch recovery");
    trace.reenter = nullptr;
    trace.returnReal = false;
    Require(Serialize(other) == "\"format\"" && trace.calls == before + 2,
            "The callback guard must be released after recovery");
    _ZN3sce4Json5ValueD1Ev(&root);
    _ZN3sce4Json5ValueD1Ev(&leaf);
    _ZN3sce4Json5ValueD1Ev(&other);
    _ZN3sce4Json5ArrayD1Ev(&array);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate parent recovery");
}
int main() {
    for (int mode = 0; mode < 3; ++mode) CheckMode(mode);
    CheckParentsAndReentry();
    return 0;
}
