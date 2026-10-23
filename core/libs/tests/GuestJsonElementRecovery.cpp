#include "prx/libc/include/general/VabiMacros.hpp"
#include "JsonTestAllocator.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <limits>

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
void APS5_VABI _ZN3sce4Json5Value3setEPKc(Value*, const char*);
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
struct PartialElementTrace {
    std::map<void*, std::size_t> live;
    std::map<void*, unsigned> generations;
    std::vector<std::size_t> requests;
    unsigned nextGeneration = 0;
    int budget = -1;
    unsigned notifications = 0;
    unsigned calls = 0;
    const Value* expectedValue = nullptr;
    const Value* expectedParent = nullptr;
    const Value* expectedFallback = nullptr;
    bool reenter = false;
    bool assignString = false;
    unsigned expectedError = 0x80848118;
};
static void* APS5_VABI AllocatePartialElement(void*, std::size_t size, void* data) {
    auto& trace = *static_cast<PartialElementTrace*>(data); trace.requests.push_back(size);
    if (!trace.budget) return nullptr;
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(size);
    Require(memory && trace.live.emplace(memory, size).second && trace.generations.emplace(memory, ++trace.nextGeneration).second, "Partial metadata requires independent allocation generations");
    std::memset(memory, 0xa5, size); return memory;
}
static void APS5_VABI FreePartialElement(void*, void* memory, void* data) {
    if (!memory) return;
    auto& trace = *static_cast<PartialElementTrace*>(data);
    Require(trace.live.erase(memory) == 1 && trace.generations.erase(memory) == 1, "Partial recovery metadata must release through its owning allocator exactly once");
    std::free(memory);
}
static void APS5_VABI NotifyPartialElement(void*, int error, std::size_t size, void* data) {
    auto& trace = *static_cast<PartialElementTrace*>(data);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == size, "Failure notices must identify the rejected metadata allocation");
    ++trace.notifications;
}
static void APS5_VABI RecoverPartialElement(int error, const String* key, const std::size_t* index, const Value* parent, const Value* value, void* data) {
    auto& trace = *static_cast<PartialElementTrace*>(data); ++trace.calls;
    Require(static_cast<unsigned>(error) == trace.expectedError && !key && index && *index == std::numeric_limits<std::size_t>::max() && value == trace.expectedValue && parent == trace.expectedParent,
            "Partial recovery must preserve error, all index bits, source, immediate parent and context");
    if (trace.reenter || trace.assignString) {
        const auto calls = trace.calls;
        Require(_ZNK3sce4Json5Value8getValueEm(value, *index) == trace.expectedFallback && trace.calls == calls, "Reentry must share the fallback while suppressing recursive callbacks");
        if (trace.assignString) _ZN3sce4Json5Value3setEPKc(const_cast<Value*>(trace.expectedFallback), "recovery");
        else _ZN3sce4Json5Value3setEl(const_cast<Value*>(trace.expectedFallback), 77);
    }
}
static void* const PartialElementTable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocatePartialElement), reinterpret_cast<void*>(FreePartialElement), reinterpret_cast<void*>(NotifyPartialElement)};
struct PartialElementAllocator { void* const* table = PartialElementTable; };
static void CheckPartialElementFallbacks() {
    for (unsigned budget = 0; budget <= 3; ++budget) for (bool reenter : {false, true}) {
        PartialElementTrace trace; PartialElementAllocator allocator; Parameter parameter{&allocator, &trace, 16384, 2, {}};
        std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, &parameter) == 0, "Initialize partial element recovery");
        Value root{}; _ZN3sce4Json5ValueC1Ev(&root);
        const auto* moduleFallback = _ZNK3sce4Json5Value8getValueEm(&root, std::numeric_limits<std::size_t>::max());
        Require(moduleFallback && trace.live.at(const_cast<Value*>(moduleFallback)) == 32, "Initial recovery must expose the initialized module fallback");
        const auto requests = trace.requests.size(); trace.budget = static_cast<int>(budget);
        const int status = _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, RecoverPartialElement, &trace);
        std::vector<std::size_t> expected{72}; if (budget) expected.push_back(32); if (budget >= 2) expected.push_back(16);
        Require(status == (budget < 3 ? static_cast<int>(0x80848102) : 0) && trace.notifications == unsigned(budget < 3) &&
                std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected,
                "Partial handler admission must preserve exact auxiliary, fallback and record request order");
        void* originalAuxiliary = root.auxiliary; const auto retryRequests = trace.requests.size(); trace.budget = -1;
        Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, RecoverPartialElement, &trace) == 0, "Independent registration retry must succeed");
        const auto retryExpected = budget == 0 ? std::vector<std::size_t>{72, 32, 16} : budget < 3 ? std::vector<std::size_t>{16} : std::vector<std::size_t>{};
        Require(std::vector<std::size_t>(trace.requests.begin() + retryRequests, trace.requests.end()) == retryExpected && (!originalAuxiliary || root.auxiliary == originalAuxiliary),
                "Registration retry must reuse admitted metadata without recreating a missing fallback");
        auto* auxiliary = static_cast<void**>(root.auxiliary); auto* fallback = static_cast<Value*>(auxiliary[2]);
        Require(bool(fallback) == (budget != 1) && auxiliary[5], "Failed fallback admission must remain missing after successful handler retry");
        const auto auxiliaryGeneration = trace.generations.at(auxiliary), recordGeneration = trace.generations.at(auxiliary[5]);
        const auto fallbackGeneration = fallback ? trace.generations.at(fallback) : 0;
        trace.expectedFallback = fallback ? fallback : moduleFallback; trace.reenter = reenter;
        const auto dispatch = [&](const Value* source, const Value* parent, unsigned calls) {
            trace.expectedValue = source; trace.expectedParent = parent;
            const auto before = trace.requests.size(); const auto notices = trace.notifications;
            trace.budget = 0;
            Require(_ZNK3sce4Json5Value8getValueEm(source, std::numeric_limits<std::size_t>::max()) == trace.expectedFallback && trace.calls == calls &&
                    trace.requests.size() == before && trace.notifications == notices,
                    "Partial fallback dispatch and callback reentry must preserve identity without allocation or notifications");
            trace.budget = -1;
            Require(!reenter || (trace.expectedFallback->type == 2 && trace.expectedFallback->payload == 77), "Callback mutation must remain visible through the returned fallback");
        };
        dispatch(&root, nullptr, 1);
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, "[1]", 3) == 0, "Parsing must retain partial root recovery metadata");
        const auto* child = _ZNK3sce4Json5Value8getValueEm(&root, 0);
        Require(child && child->parent == &root, "The parsed child must retain its actual parent"); dispatch(child, &root, 2);
        Require(root.auxiliary == auxiliary && auxiliary[2] == fallback && trace.generations.at(auxiliary) == auxiliaryGeneration && trace.generations.at(auxiliary[5]) == recordGeneration &&
                (!fallback || (trace.generations.at(fallback) == fallbackGeneration && fallback->parent == &root)) &&
                static_cast<void**>(auxiliary[5])[0] == reinterpret_cast<void*>(&RecoverPartialElement) && static_cast<void**>(auxiliary[5])[1] == &trace,
                "Parsing and dispatch must preserve auxiliary, fallback and handler generations, callback ABI and ownership");
        _ZN3sce4Json5ValueD1Ev(&root);
        Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.live.empty() && trace.generations.empty(), "Every partial admission and retry must reclaim real allocation ownership");
    }
}

static void CheckElementStringFailures() {
    for (bool missingFallback : {false, true}) for (bool nested : {false, true}) for (unsigned budget = 0; budget <= 5; ++budget) {
        PartialElementTrace trace; PartialElementAllocator allocator; Parameter parameter{&allocator, &trace, 16384, 2, {}};
        std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
        Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&owner, &parameter) == 0, "Initialize fallback String failure resources");
        Value root{}; _ZN3sce4Json5ValueC1Ev(&root);
        const auto* moduleFallback = _ZNK3sce4Json5Value8getValueEm(&root, std::numeric_limits<std::size_t>::max());
        if (missingFallback) {
            trace.budget = 1;
            Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, RecoverPartialElement, &trace) == static_cast<int>(0x80848102), "Prepare a root without its auxiliary fallback");
            trace.budget = -1;
        }
        Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&root, RecoverPartialElement, &trace) == 0 &&
                _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&root, "[1]", 3) == 0, "Prepare nested recovery with admitted callback metadata");
        auto* auxiliary = static_cast<void**>(root.auxiliary);
        auto* fallback = static_cast<Value*>(auxiliary[2]);
        Require(bool(fallback) != missingFallback, "Parsing must preserve the intended fallback admission state");
        trace.expectedFallback = fallback ? fallback : moduleFallback;
        trace.expectedValue = nested ? _ZNK3sce4Json5Value8getValueEm(&root, 0) : &root;
        trace.expectedParent = nested ? &root : nullptr; trace.expectedError = nested ? 0x80848118 : 0x80848116; trace.assignString = true;
        const auto generations = trace.generations;
        std::map<void*, std::vector<unsigned char>> snapshots;
        for (const auto& [pointer, size] : trace.live) {
            if (pointer == trace.expectedFallback) continue;
            const auto* bytes = static_cast<const unsigned char*>(pointer); snapshots.emplace(pointer, std::vector<unsigned char>{bytes, bytes + size});
        }
        const auto requests = trace.requests.size(); const auto notices = trace.notifications;
        trace.budget = static_cast<int>(budget);
        Require(_ZNK3sce4Json5Value8getValueEm(trace.expectedValue, std::numeric_limits<std::size_t>::max()) == trace.expectedFallback && trace.calls == 1,
                "Callback String allocation failure must preserve the exact returned fallback and suppress reentry");
        std::vector<std::size_t> expected{24}; if (budget) { expected.push_back(9); expected.push_back(8); } if (budget >= 3) expected.push_back(24); if (budget >= 4) expected.push_back(9);
        Require(std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected && trace.notifications == notices + (budget == 1 ? 2u : unsigned(budget < 5)),
                "Callback String assignment must preserve temporary and destination handle/header/character request order and actual failure notices");
        Require(trace.expectedFallback->type == (budget < 4 ? 0 : 5) && trace.expectedFallback->parent == (missingFallback ? nullptr : &root),
                "Failed String assignment must retain the admitted partial payload and fallback owner");
        if (budget >= 4) Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(trace.expectedFallback->payload)), budget == 4 ? "" : "recovery") == 0,
                "Character failure must retain an empty String while successful admission exposes complete text");
        trace.budget = -1; const auto retryRequests = trace.requests.size();
        _ZN3sce4Json5Value3setEPKc(const_cast<Value*>(trace.expectedFallback), "recovery");
        const auto retryExpected = budget < 4 ? std::vector<std::size_t>{24, 9, 8, 24, 9} : std::vector<std::size_t>{24, 9, 9};
        Require(std::vector<std::size_t>(trace.requests.begin() + retryRequests, trace.requests.end()) == retryExpected && trace.expectedFallback->type == 5 &&
                std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(trace.expectedFallback->payload)), "recovery") == 0,
                "Independent retry must reuse admitted String storage and complete only missing payload allocations");
        const auto retryGenerations = trace.generations; const auto repeatedRequests = trace.requests.size(); const auto repeatedNotices = trace.notifications;
        trace.budget = 0;
        Require(_ZNK3sce4Json5Value8getValueEm(trace.expectedValue, std::numeric_limits<std::size_t>::max()) == trace.expectedFallback && trace.calls == 2 &&
                trace.generations == retryGenerations && trace.requests.size() == repeatedRequests + 1 && trace.requests.back() == 24 && trace.notifications == repeatedNotices + 1,
                "Recovery must resume after allocation failure and preserve the completed payload when temporary admission fails");
        trace.budget = -1;
        Require(root.auxiliary == auxiliary && auxiliary[2] == fallback && auxiliary[5], "Payload failure and retry must not replace callback metadata or repair a missing fallback");
        for (const auto& [pointer, generation] : generations) Require(trace.generations.at(pointer) == generation, "Payload recovery must preserve all preexisting allocation identities");
        for (const auto& [pointer, bytes] : snapshots) Require(std::memcmp(pointer, bytes.data(), bytes.size()) == 0, "Payload recovery must preserve source, module and callback-record bytes");
        _ZN3sce4Json5ValueD1Ev(&root);
        Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.live.empty() && trace.generations.empty(), "Root and module fallbacks must release their real String payload ownership on teardown");
    }
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
    CheckPartialElementFallbacks();
    CheckElementStringFailures();
    return 0;
}
