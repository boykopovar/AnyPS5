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
struct LookupTrace { unsigned calls = 0; const String* key = nullptr; const Value* source = nullptr; };
static void APS5_VABI LookupFailure(int error, const String* key, const std::size_t* index, const Value* parent, const Value* value, void* context) {
    auto& trace = *static_cast<LookupTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848115 && !index && !parent, "Rejected object lookup must preserve its error, key form and parent");
    ++trace.calls; trace.key = key; trace.source = value;
}
int main() {
    ArrayTrace trace; ArrayAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    Value root{}; _ZN3sce4Json5ValueC1Ev(&root);
    String missingHeader{};
    Require(_ZNK3sce4Json5ValueixERKNS0_6StringE(&root, &missingHeader) == nullptr, "Before initialization a headerless key must return the absent module fallback without allocation");
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize keyed lookup contract");
    const char json[] = "{\"a\":7,\"\":9}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, json, sizeof(json) - 1) == 0, "Parse lookup values including an empty key");
    String key{}; _ZN3sce4Json6StringC1EPKc(&key, "a");
    String absent{}; _ZN3sce4Json6StringC1EPKc(&absent, "absent");
    const auto* moduleFallback = _ZNK3sce4Json5ValueixERKNS0_6StringE(&root, &missingHeader);
    Require(moduleFallback && moduleFallback->type == 0, "Headerless keys must select the module fallback");
    LookupTrace lookup;
    Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, LookupFailure, &lookup) == 0, "Register object lookup recovery");
    const auto* rootFallback = _ZNK3sce4Json5ValueixERKNS0_6StringE(&root, &absent);
    Require(rootFallback && rootFallback != moduleFallback && rootFallback->parent == &root && lookup.calls == 1, "Missing keys must retain root-local fallback identity");
    const auto* actual = _ZNK3sce4Json5ValueixERKNS0_6StringE(&root, &key);
    Require(actual && actual->type == 3 && actual->payload == 7 && actual->parent == &root, "Successful const lookup must return the actual owned element");
    const auto baseline = trace.live.size();
    for (bool getter : {false, true}) for (bool once : {false, true}) for (int budget : {0, 1, 2}) {
        const auto requests = trace.requests.size(); const auto notices = trace.notifications; const auto calls = lookup.calls;
        trace.budget = budget; trace.once = once;
        const auto* result = getter ? _ZNK3sce4Json5Value8getValueERKNS0_6StringE(&root, &key) : _ZNK3sce4Json5ValueixERKNS0_6StringE(&root, &key);
        trace.budget = -1; trace.once = false;
        Require(result == (budget < 2 ? rootFallback : actual) && trace.live.size() == baseline, "Both keyed exports must preserve fallback identity and release temporary iterators");
        Require(trace.requests.size() == requests + 2 && trace.requests[requests] == 8 && trace.requests[requests + 1] == 8, "Every object lookup must allocate find and end iterator states in order");
        const auto failed = budget == 2 ? 0u : once ? 1u : budget == 0 ? 2u : 1u;
        Require(trace.notifications == notices + failed && lookup.calls == calls + (budget < 2 ? 1 : 0), "Iterator allocation failure must notify and dispatch missing-element recovery even for an existing key");
        if (budget < 2) Require(lookup.key == &key && lookup.source == &root, "Failure recovery must borrow the exact supplied key and source objects");
    }
    const auto requests = trace.requests.size(); const auto calls = lookup.calls;
    Require(_ZNK3sce4Json5ValueixERKNS0_6StringE(&root, &missingHeader) == moduleFallback && _ZNK3sce4Json5Value8getValueERKNS0_6StringE(&root, &missingHeader) == moduleFallback && lookup.calls == calls && trace.requests.size() == requests, "A missing string header must bypass iterator allocation, recovery and the root fallback");
    trace.budget = 0;
    Require(_ZNK3sce4Json5ValueixEPKc(&root, "a") == moduleFallback && lookup.calls == calls && trace.requests.size() == requests + 1 && trace.requests.back() == 24, "C-string key-header failure must bypass lookup and recovery");
    trace.budget = 1; trace.once = true;
    const auto* emptyKey = _ZNK3sce4Json5ValueixEPKc(&root, "a");
    trace.budget = -1; trace.once = false;
    Require(emptyKey && emptyKey->type == 3 && emptyKey->payload == 9 && lookup.calls == calls && trace.live.size() == baseline, "One-shot character-allocation failure must admit the constructed empty key and look up that key faithfully");
    _ZN3sce4Json6StringD1Ev(&key); _ZN3sce4Json6StringD1Ev(&absent); _ZN3sce4Json5ValueD1Ev(&root);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Destroy all lookup resources and callbacks");
    return 0;
}
