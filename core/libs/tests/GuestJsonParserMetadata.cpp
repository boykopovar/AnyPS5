#include "prx/libSceJson2/include/Api.hpp"
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

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using LookupCallback = void (APS5_VABI*)(int, const String*, const std::size_t*, const Value*, const Value*, void*);
using NullCallback = const Value& (APS5_VABI*)(ValueType, const Value*, void*);
using MismatchCallback = const Value& (APS5_VABI*)(ValueType, const Value*, const Value*, void*);
using FloatCallback = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
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
    std::vector<std::size_t> frees;
    std::map<void*, std::size_t> ids;
    std::size_t nextId = 0;
};
static void* APS5_VABI AllocateArray(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ArrayTrace*>(context); trace.requests.push_back(bytes);
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.live.emplace(memory, bytes); trace.ids[memory] = ++trace.nextId; return memory;
}
static void APS5_VABI FreeArray(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<ArrayTrace*>(context);
    Require(trace.ids.count(memory) == 1, "Freed metadata must identify a currently live allocation generation");
    trace.frees.push_back(trace.ids.at(memory)); trace.ids.erase(memory);
    if (trace.headers.erase(memory)) {
        const auto* words = reinterpret_cast<const std::uint64_t*>(memory);
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
static const Value& APS5_VABI RejectNull(ValueType, const Value*, void* context) {
    Require(false, "Parser metadata admission must not invoke null recovery");
    return *static_cast<const Value*>(context);
}
static const Value& APS5_VABI RejectMismatch(ValueType, const Value* value, const Value*, void*) {
    Require(false, "Parser metadata admission must not invoke mismatch recovery");
    return *value;
}
static Value* APS5_VABI RejectFloat(Value* result, std::int32_t, double, const Value*, void*, bool*) {
    Require(false, "Finite parser metadata admission must not invoke special-float recovery");
    return result;
}
static void APS5_VABI RejectElement(int, const String*, const std::size_t*, const Value*, const Value*, void*) {
    Require(false, "Parser metadata admission must not invoke element recovery");
}
static Value* MakeRoot(Value& root) {
    _ZN3sce4Json5ValueC1Ev(&root);
    constexpr char json[] = "{\"child\":7}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, json, sizeof(json) - 1) == 0, "Parse an owned child for metadata admission");
    auto* child = const_cast<Value*>(_ZNK3sce4Json5ValueixEPKc(&root, "child"));
    Require(child && child->parent == &root && !child->auxiliary && child->type == 3 && child->payload == 7, "Initial child ownership must match the containing object");
    return child;
}
int main() {
    ArrayTrace trace; ArrayAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize parser metadata transitions");
    const auto baseline = trace.live.size();
    for (bool once : {false, true}) for (int budget : {0, 1, 2}) {
        Value root{}; auto* child = MakeRoot(root);
        const Value before = *child;
        const auto beforeInvalid = trace.requests.size();
        Require(static_cast<unsigned>(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(child, "tru", 3)) == 0x80848101 && std::memcmp(child, &before, sizeof(before)) == 0 && trace.requests.size() == beforeInvalid, "An incomplete literal must preserve the existing child and metadata without admission");
        const auto requests = trace.requests.size(); const auto notices = trace.notifications;
        trace.budget = budget; trace.once = once;
        const int status = _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(child, "true", 4);
        trace.budget = -1; trace.once = false;
        Require(status == (budget < 2 ? static_cast<int>(0x80848102) : 0) && !child->parent && child->type == 1 && (child->payload & 255) == 1, "A recognized public parse must detach the child and retain its admitted scalar even when root metadata allocation fails");
        Require((child->auxiliary != nullptr) == (budget != 0), "Auxiliary allocation failure must leave the detached child without invented metadata");
        if (child->auxiliary) Require((reinterpret_cast<void**>(child->auxiliary)[2] != nullptr) == (budget == 2), "Fallback failure must retain its admitted auxiliary record without synthesizing a fallback");
        const auto expected = budget == 0 ? std::vector<std::size_t>{72} : std::vector<std::size_t>{72, 32};
        Require(std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected && trace.notifications == notices + (budget < 2 ? 1 : 0), "Child-to-root admission must preserve exact allocation order and notification counts");
        void* auxiliary = child->auxiliary;
        const auto retryRequests = trace.requests.size();
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(child, "false", 5) == 0 && !child->parent && child->type == 1 && (child->payload & 255) == 0, "Retry must parse normally after independent admission failure");
        Require(trace.requests.size() == retryRequests + (budget == 0 ? 2 : 0) && (budget == 0 || static_cast<void*>(child->auxiliary) == auxiliary), "Retry must reuse admitted metadata and must not recreate a missing fallback");
        _ZN3sce4Json5ValueD1Ev(&root);
        Require(trace.live.size() == baseline, "The containing object's destruction must release detached child metadata through its original allocator");
    }
    struct Recognized { const char* text; std::int32_t type; };
    for (const auto& input : {Recognized{"null", 0}, Recognized{"true", 1}, Recognized{"false", 1}, Recognized{"123", 3}, Recognized{"3.5", 4}, Recognized{"\"x\"", 5}, Recognized{"[1]", 6}, Recognized{"{\"x\":1}", 7}}) {
        Value root{}; auto* child = MakeRoot(root);
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(child, input.text, std::strlen(input.text)) == 0 && child->type == input.type && !child->parent && child->auxiliary && reinterpret_cast<Value*>(reinterpret_cast<void**>(child->auxiliary)[2])->parent == child, "Every recognized scalar or collection parsed directly into a child must admit it as a root");
        if (input.type == 6) Require(_ZNK3sce4Json5Array5frontEv(reinterpret_cast<const Array*>(child->payload))->parent == child, "A detached parsed array must bind its newly parsed elements to its actual value");
        if (input.type == 7) Require(_ZNK3sce4Json5ValueixEPKc(child, "x")->parent == child, "A detached parsed object must bind its newly parsed entries to its actual value");
        _ZN3sce4Json5ValueD1Ev(&root);
        Require(trace.live.size() == baseline, "Every recognized child-to-root type transition must retain complete cleanup ownership");
    }
    Value root{}; auto* child = MakeRoot(root);
    void* rootAuxiliary = root.auxiliary;
    void* rootFallback = reinterpret_cast<void**>(rootAuxiliary)[2];
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(child, "true", 4) == 0, "Admit a child as an independent root before registering handlers");
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(child, RejectNull, child) == 0 &&
        _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(child, RejectMismatch, child) == 0 &&
        _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(child, RejectFloat, child) == 0 &&
        _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(child, RejectElement, child) == 0, "Register all four callback forms on the detached root");
    auto** auxiliary = reinterpret_cast<void**>(child->auxiliary);
    const std::vector<void*> owned{auxiliary[3], auxiliary[4], auxiliary[5], auxiliary[2], child->auxiliary};
    for (void* allocation : owned) Require(allocation && trace.live.count(allocation), "All callback records, fallback and auxiliary storage must belong to the supplied allocator");
    const auto beforeRootParse = trace.requests.size();
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(child, "false", 5) == 0 && static_cast<void*>(child->auxiliary) == auxiliary && !child->parent && trace.requests.size() == beforeRootParse, "Modern root reparsing must reuse its existing auxiliary record without allocating");
    Require(auxiliary[0] == reinterpret_cast<void*>(&RejectNull) && auxiliary[1] == child && auxiliary[2] == owned[3] && auxiliary[3] == owned[0] && auxiliary[4] == owned[1] && auxiliary[5] == owned[2], "Modern root admission must preserve every existing callback, context, handler record and fallback identity");
    std::vector<std::size_t> ownedIds;
    for (void* allocation : owned) ownedIds.push_back(trace.ids.at(allocation));
    const auto frees = trace.frees.size();
    constexpr char replacement[] = "{\"child\":false}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, replacement, sizeof(replacement) - 1) == 0, "Reparse the containing object using its existing entry");
    Require(child == _ZNK3sce4Json5ValueixEPKc(&root, "child") && child->parent == &root && !child->auxiliary && child->type == 1 && (child->payload & 255) == 0, "Nested admission must preserve node identity, rebind its parent and remove former root metadata");
    std::vector<std::size_t> released;
    for (auto i = frees; i < trace.frees.size(); ++i) for (auto id : ownedIds) if (id == trace.frees[i]) released.push_back(id);
    Require(released == ownedIds && root.auxiliary == rootAuxiliary && reinterpret_cast<void**>(rootAuxiliary)[2] == rootFallback, "Nested admission must release every handler record, fallback and auxiliary in native order while preserving root metadata");
    _ZN3sce4Json5ValueD1Ev(&root);
    Require(trace.live.size() == baseline && _ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty() && trace.ids.empty(), "Parser metadata transitions must leave no outstanding ownership");
    return 0;
}
