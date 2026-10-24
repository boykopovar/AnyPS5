#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct Value { Value* parent; void* auxiliary; std::uintptr_t payload; std::uint32_t reserved; std::int32_t type; };
struct Array { void* items; };
struct String { void* text; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1El(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value*, const char*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(Value*, const Array*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(Value*, const Value*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value3setENS0_9ValueTypeE(Value*, std::int32_t);
void APS5_VABI _ZN3sce4Json5Value3setERKS1_(Value*, const Value*);
void APS5_VABI _ZN3sce4Json5Value4swapERS1_(Value*, Value*);
const Array* APS5_VABI _ZNK3sce4Json5Value8getArrayEv(const Value*);
const String* APS5_VABI _ZNK3sce4Json5Value9getStringEv(const Value*);
void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array*);
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array*);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array*, const Value*);
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array*);
const Value* APS5_VABI _ZNK3sce4Json5Array4backEv(const Array*);
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
static const Value& Front(const Value& value) {
    return *_ZNK3sce4Json5Array5frontEv(_ZNK3sce4Json5Value8getArrayEv(&value));
}
static void Tree(const Value& root) {
    Require(root.type == 6 && !root.parent, "A copied root must own an independent root value");
    const auto& child = Front(root);
    Require(child.parent == &root && child.type == 6, "An array element must refer to its owning value");
    const auto& leaf = Front(child);
    Require(leaf.parent == &child && leaf.type == 2 && leaf.payload == 42,
            "Nested children must refer to their immediate owner and preserve inline payloads");
}
int main() {
    JsonTest::Allocator allocator;
    Parameter parameter{&allocator, nullptr, 0, 2, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize ownership tests");
    Value text{};
    _ZN3sce4Json5ValueC1EPKc(&text, "keep");
    const auto payload = text.payload;
    const auto* string = _ZNK3sce4Json5Value9getStringEv(&text);
    Require(reinterpret_cast<std::uintptr_t>(string) == payload, "A string payload must hold its String object address");
    _ZN3sce4Json5Value3setENS0_9ValueTypeE(&text, 5);
    Require(text.payload == payload && std::strcmp(_ZNK3sce4Json6String5c_strEv(string), "keep") == 0,
            "Setting an existing type must preserve its contents and identity");
    Value leaf{};
    _ZN3sce4Json5ValueC1El(&leaf, 42);
    Array inner{};
    _ZN3sce4Json5ArrayC1Ev(&inner);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&inner, &leaf);
    Require(!_ZNK3sce4Json5Array5frontEv(&inner)->parent, "A standalone array must not invent a value owner");
    Value child{};
    _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&child, &inner);
    Array outer{};
    _ZN3sce4Json5ArrayC1Ev(&outer);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&outer, &child);
    Value root{};
    _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&root, &outer);
    Tree(root);
    Require(reinterpret_cast<std::uintptr_t>(_ZNK3sce4Json5Value8getArrayEv(&root)) == root.payload,
            "An array payload must hold its Array object address");
    Value copy{};
    _ZN3sce4Json5ValueC1ERKS1_(&copy, &root);
    Tree(copy);
    Require(copy.payload != root.payload && Front(copy).payload != Front(root).payload,
            "Copies must not share owned containers");
    const auto rootPayload = root.payload;
    _ZN3sce4Json5Value3setENS0_9ValueTypeE(&root, 6);
    Require(root.payload == rootPayload && _ZNK3sce4Json5Array4sizeEv(_ZNK3sce4Json5Value8getArrayEv(&root)) == 1,
            "Setting the existing array type must retain elements and storage");
    auto* ownedArray = const_cast<Array*>(_ZNK3sce4Json5Value8getArrayEv(&copy));
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(ownedArray, &child);
    const auto* appended = _ZNK3sce4Json5Array4backEv(ownedArray);
    Require(appended->parent == &copy && Front(*appended).parent == appended,
            "Appending to an owned array must bind both new and nested children");
    _ZN3sce4Json5Value3setERKS1_(&copy, &Front(copy));
    Require(!copy.parent && Front(copy).parent == &copy && Front(copy).payload == 42,
            "Assignment from a descendant must finish copying before releasing the old tree");
    _ZN3sce4Json5Value4swapERS1_(&root, &text);
    Require(root.type == 5 && text.type == 6, "Swap must exchange payloads");
    Tree(text);
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(_ZNK3sce4Json5Value9getStringEv(&root)), "keep") == 0,
            "Swap must preserve string contents");
    _ZN3sce4Json5ValueD1Ev(&root);
    _ZN3sce4Json5ValueD1Ev(&copy);
    _ZN3sce4Json5ValueD1Ev(&child);
    _ZN3sce4Json5ValueD1Ev(&leaf);
    _ZN3sce4Json5ValueD1Ev(&text);
    _ZN3sce4Json5ArrayD1Ev(&inner);
    _ZN3sce4Json5ArrayD1Ev(&outer);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0, "Terminate ownership tests");
    return 0;
}
