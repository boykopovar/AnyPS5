#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <initializer_list>
#include <set>
#include <vector>
#include <string>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct Array { void* storage; };
struct Iterator { void* state; };
struct Object { void* storage; };
struct String { void* storage; };
struct Pair { String key; std::uint64_t reserved; Value value; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using SerializationCallback = int (APS5_VABI*)(String&, void*);
extern "C" {
int APS5_VABI _ZN3sce4Json5Value9serializeEPFiRNS0_6StringEPvES4_(Value*, SerializationCallback, void*);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(Value*, String*, SerializationCallback, void*);
String* APS5_VABI _ZN3sce4Json6String6appendEPKc(String*, const char*);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value*, const char*);
const Value* APS5_VABI _ZNK3sce4Json5ValueixERKNS0_6StringE(const Value*, const String*);
const Value* APS5_VABI _ZNK3sce4Json5Value8getValueERKNS0_6StringE(const Value*, const String*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
void APS5_VABI _ZN3sce4Json5ValueC1EPKc(Value*, const char*);
void APS5_VABI _ZN3sce4Json5Array8pop_backEv(Array*);
Iterator* APS5_VABI _ZN3sce4Json5Array5eraseERKNS1_8iteratorE(Iterator*, Array*, const Iterator*);
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1El(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv(const Value*);
void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array*);
void APS5_VABI _ZN3sce4Json5ArrayC1ERKS1_(Array*, const Array*);
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array*);
Array* APS5_VABI _ZN3sce4Json5ArrayaSERKS1_(Array*, const Array*);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array*, const Value*);
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array*);
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array*);
const Value* APS5_VABI _ZNK3sce4Json5Array4backEv(const Array*);
void APS5_VABI _ZN3sce4Json5Array5clearEv(Array*);
void APS5_VABI _ZN3sce4Json5Array9pop_frontEv(Array*);
Iterator* APS5_VABI _ZNK3sce4Json5Array5beginEv(Iterator*, const Array*);
Iterator* APS5_VABI _ZNK3sce4Json5Array3endEv(Iterator*, const Array*);
void APS5_VABI _ZN3sce4Json5Array8iteratorC1Ev(Iterator*);
void APS5_VABI _ZN3sce4Json5Array8iteratorC1ERKS2_(Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json5Array8iteratorD1Ev(Iterator*);
Iterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEv(Iterator*);
Iterator* APS5_VABI _ZN3sce4Json5Array8iteratorppEi(Iterator*, int);
void APS5_VABI _ZN3sce4Json5Array8iterator7advanceEm(Iterator*, std::size_t);
const Value* APS5_VABI _ZNK3sce4Json5Array8iteratordeEv(const Iterator*);
bool APS5_VABI _ZNK3sce4Json5Array8iteratorneERKS2_(const Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json6ObjectC1Ev(Object*);
void APS5_VABI _ZN3sce4Json6ObjectD1Ev(Object*);
Value* APS5_VABI _ZN3sce4Json6ObjectixERKNS0_6StringE(Object*, const String*);
Iterator* APS5_VABI _ZNK3sce4Json6Object5beginEv(Iterator*, const Object*);
Iterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEi(Iterator*, int);
const Pair* APS5_VABI _ZNK3sce4Json6Object8iteratordeEv(const Iterator*);
void APS5_VABI _ZN3sce4Json6Object8iteratorD1Ev(Iterator*);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct ArrayTrace {
    std::map<void*, std::size_t> live;
    std::vector<std::size_t> requests;
    int budget = -1;
    bool once = false;
    unsigned notifications = 0;
    std::set<void*> headers;
};
static void* APS5_VABI AllocateArray(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ArrayTrace*>(context); trace.requests.push_back(bytes);
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.live.emplace(memory, bytes); return memory;
}
static void APS5_VABI FreeArray(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<ArrayTrace*>(context);
    if (trace.headers.erase(memory)) {
        const auto* words = static_cast<const std::uint64_t*>(memory);
        Require(words[0] == 0xffffffff && words[1] == UINT64_MAX, "Array teardown must expose the native header poison before allocator release");
    }
    Require(trace.live.erase(memory) == 1, "Array must release through its owning allocator"); std::free(memory);
}
static void APS5_VABI NotifyArray(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ArrayTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == bytes, "Array notification must identify the failed allocation"); ++trace.notifications;
}
static void* const ArrayVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateArray), reinterpret_cast<void*>(FreeArray), reinterpret_cast<void*>(NotifyArray)};
struct ArrayAllocator { void* const* vtable = ArrayVtable; };
struct SerializationTrace {
    std::vector<std::string> chunks;
    String* identity = nullptr;
    int stopAt = -1;
    int result = 23;
    bool mutate = false;
};
static int APS5_VABI Observe(String& chunk, void* context) {
    auto& trace = *static_cast<SerializationTrace*>(context);
    if (!trace.identity) trace.identity = &chunk;
    Require(trace.identity == &chunk, "Serialization must reuse the same chunk handle or supplied output object throughout the call");
    trace.chunks.emplace_back(_ZNK3sce4Json6String5c_strEv(&chunk));
    if (trace.mutate) _ZN3sce4Json6String6appendEPKc(&chunk, "!");
    return static_cast<int>(trace.chunks.size()) == trace.stopAt ? -77 : trace.result;
}
int main() {
    ArrayTrace allocation; ArrayAllocator allocator;
    Parameter parameter{&allocator, &allocation, 8192, 2, {}};
    Value root{}; _ZN3sce4Json5ValueC1Ev(&root);
    SerializationTrace before;
    Require(static_cast<unsigned>(_ZN3sce4Json5Value9serializeEPFiRNS0_6StringEPvES4_(&root, Observe, &before)) == 0x80848110 && before.chunks.empty(), "Callback serialization before initialization must refuse without invoking the callback");
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize serialization callbacks");
    const char json[] = R"({"a":[1,true,"x\n/\u007f"],"b":{}})";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, json, sizeof(json) - 1) == 0, "Parse nested callback serialization input");
    const std::vector<std::string> chunks{"{", "\"a\":[", "1", ",true", ",\"x\\n\\/\\u007f\"", "]", ",\"b\":{", "}", "}"};
    const auto baseline = allocation.live.size();
    for (bool collect : {false, true}) for (bool mutate : {false, true}) for (int stop : {-1, 1, 3, 9}) {
        String output{}; _ZN3sce4Json6StringC1EPKc(&output, "prefix");
        SerializationTrace trace; trace.stopAt = stop; trace.mutate = mutate;
        if (collect) trace.identity = &output;
        const int status = collect ? _ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(&root, &output, Observe, &trace) : _ZN3sce4Json5Value9serializeEPFiRNS0_6StringEPvES4_(&root, Observe, &trace);
        Require(status == (stop < 0 ? trace.result : -77), "Serialization must propagate the last callback result and stop on negative results");
        const auto count = stop < 0 ? chunks.size() : static_cast<std::size_t>(stop);
        Require(trace.chunks.size() == count, "Callback errors must stop traversal without emitting later or closing chunks");
        std::string cumulative = "prefix";
        for (std::size_t i = 0; i < count; ++i) {
            cumulative += chunks[i];
            Require(trace.chunks[i] == (collect ? cumulative : chunks[i]), "Callback chunks must preserve structural boundaries, slash/DEL escaping and cumulative output semantics");
            if (collect && mutate) cumulative += '!';
        }
        Require(std::string(_ZNK3sce4Json6String5c_strEv(&output)) == (collect ? cumulative : "prefix"), "Callback mutations must persist only in the supplied cumulative output, while streaming chunks are cleared after delivery");
        _ZN3sce4Json6StringD1Ev(&output);
        Require(allocation.live.size() == baseline, "Successful and interrupted traversals must release all temporary buffers and iterator states");
    }
    Value scalar{}; _ZN3sce4Json5ValueC1El(&scalar, 1234);
    for (bool collect : {false, true}) for (bool once : {false, true}) for (int budget : {0, 1, 2, 3}) {
        String output{}; _ZN3sce4Json6StringC1EPKc(&output, "p");
        const auto requests = allocation.requests.size(); const auto notices = allocation.notifications;
        allocation.budget = budget; allocation.once = once;
        SerializationTrace trace;
        const int status = collect ? _ZN3sce4Json5Value9serializeERNS0_6StringEPFiS3_PvES4_(&scalar, &output, Observe, &trace) : _ZN3sce4Json5Value9serializeEPFiRNS0_6StringEPvES4_(&scalar, Observe, &trace);
        allocation.budget = -1; allocation.once = false;
        Require(status == 23 && trace.chunks.size() == 1, "Allocation rejection must retain the reference callback-driven return contract");
        const bool complete = budget >= (collect ? 3 : 2);
        Require(trace.chunks[0] == (collect ? complete ? "p1234" : "p" : complete ? "1234" : ""), "Allocation failures must expose the native admitted partial chunk or output");
        std::vector<std::size_t> expected{24};
        if (budget >= 1) expected.push_back(21);
        if (collect && budget >= 2) expected.push_back(22);
        Require(std::vector<std::size_t>(allocation.requests.begin() + requests, allocation.requests.end()) == expected, "Scalar callback serialization must allocate its header, chunk and optional cumulative-output growth in order");
        Require(allocation.notifications == notices + (complete ? 0 : 1), "Each scalar serialization rejection must notify exactly once");
        _ZN3sce4Json6StringD1Ev(&output);
        Require(allocation.live.size() == baseline, "Rejected serialization allocations must not leak admitted storage");
    }
    _ZN3sce4Json5ValueD1Ev(&scalar); _ZN3sce4Json5ValueD1Ev(&root);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && allocation.live.empty(), "Terminate callback serialization allocation context");
    return 0;
}
