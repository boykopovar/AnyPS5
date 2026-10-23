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

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct Array { void* storage; };
struct Iterator { void* state; };
struct Object { void* storage; };
struct String { void* storage; };
struct Pair { String key; std::uint64_t reserved; Value value; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
extern "C" {
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
static void CheckLinks(const Array& array, std::size_t count, std::initializer_list<std::uint64_t> values = {}) {
    Require(values.size() == 0 || values.size() == count, "Expected array values must cover every live node");
    const auto* header = static_cast<const std::uintptr_t*>(array.storage);
    Require(header && header[1] == count, "Array header must expose its native element count");
    const auto* head = reinterpret_cast<const std::uintptr_t*>(header[0]);
    const auto* previous = head;
    std::size_t seen = 0;
    for (auto* node = reinterpret_cast<const std::uintptr_t*>(head[1]); node != head; node = reinterpret_cast<const std::uintptr_t*>(node[1])) {
        Require(node[0] == reinterpret_cast<std::uintptr_t>(previous), "Every array link must retain its predecessor");
        const auto* value = reinterpret_cast<const Value*>(node + 2);
        Require(++seen <= count, "Array links must close at the sentinel");
        const auto expected = values.size() ? *(values.begin() + seen - 1) : 10 * seen;
        Require(!value->parent && !value->auxiliary && value->type == 2 && value->payload == expected, "Array nodes must expose their inline values and ownership");
        previous = node;
    }
    Require(seen == count && head[0] == reinterpret_cast<std::uintptr_t>(previous), "Array sentinel must close both list directions");
}
int main() {
    ArrayTrace trace; ArrayAllocator allocator;
    Parameter parameter{&allocator, &trace, 16384, 2, {}};
    std::uint8_t initializer = 0;
    _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize array mutation contract");
    const auto baseline = trace.live.size();
    Array absent{};
    const auto beforeMissing = trace.requests.size();
    _ZN3sce4Json5Array8pop_backEv(&absent);
    Require(trace.requests.size() == beforeMissing, "Popping missing storage must do nothing without allocating");
    Iterator missingResult{};
    Require(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&missingResult, &absent, nullptr) == &missingResult && missingResult.state && *static_cast<void**>(missingResult.state) == nullptr, "Erase with missing array storage must return an allocated unbound iterator without reading the input iterator");
    _ZN3sce4Json5Array8iteratorD1Ev(&missingResult);
    for (bool rejectResult : {false, true}) {
        for (unsigned index : {0u, 1u, 2u}) {
            Array array{}; _ZN3sce4Json5ArrayC1Ev(&array);
            Value value{};
            for (int number : {10, 20, 30}) {
                _ZN3sce4Json5ValueC1El(&value, number);
                _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &value);
                _ZN3sce4Json5ValueD1Ev(&value);
            }
            Iterator selected{}, survivor{}, end{};
            _ZNK3sce4Json5Array5beginEv(&selected, &array);
            _ZN3sce4Json5Array8iterator7advanceEm(&selected, index);
            _ZNK3sce4Json5Array5beginEv(&survivor, &array);
            _ZN3sce4Json5Array8iterator7advanceEm(&survivor, index == 0 ? 1 : 0);
            _ZNK3sce4Json5Array3endEv(&end, &array);
            const auto* survivorAddress = _ZNK3sce4Json5Array8iteratordeEv(&survivor);
            const auto* removed = _ZNK3sce4Json5Array8iteratordeEv(&selected);
            void* selectedNode = *static_cast<void**>(selected.state);
            void* nextNode = static_cast<void**>(selectedNode)[1];
            const auto requests = trace.requests.size();
            const auto notices = trace.notifications;
            trace.budget = rejectResult ? 0 : -1;
            Iterator result{};
            Require(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&result, &array, &selected) == &result, "Erase must return the caller's hidden result object");
            trace.budget = -1;
            Require(trace.requests.size() == requests + 1 && trace.requests.back() == 8 && trace.notifications == notices + (rejectResult ? 1 : 0), "Erase must request exactly one iterator state and notify only when that allocation fails");
            Require(!trace.live.count(selectedNode) && _ZNK3sce4Json5Array4sizeEv(&array) == 2, "Erase must remove the requested node even when its result-state allocation fails");
            Require(result.state ? *static_cast<void**>(result.state) == nextNode : rejectResult, "The returned iterator must identify the successor or remain absent after allocation failure");
            Require(_ZNK3sce4Json5Array8iteratordeEv(&survivor) == survivorAddress && survivorAddress != removed, "Erasure must preserve every surviving inline value and iterator identity");
            if (index == 0) CheckLinks(array, 2, {20, 30});
            else if (index == 1) CheckLinks(array, 2, {10, 30});
            else CheckLinks(array, 2, {10, 20});
            _ZN3sce4Json5Array8iteratorD1Ev(&result);
            _ZN3sce4Json5Array8iteratorD1Ev(&selected);
            _ZN3sce4Json5Array8iteratorD1Ev(&end);
            const auto popRequests = trace.requests.size();
            _ZN3sce4Json5Array8pop_backEv(&array);
            Require(trace.requests.size() == popRequests && _ZNK3sce4Json5Array4sizeEv(&array) == 1, "Pop back must destroy the final node without allocating");
            _ZN3sce4Json5Array8pop_backEv(&array);
            _ZN3sce4Json5Array8pop_backEv(&array);
            CheckLinks(array, 0);
            _ZN3sce4Json5Array8iteratorD1Ev(&survivor);
            _ZN3sce4Json5ArrayD1Ev(&array);
            Require(trace.live.size() == baseline, "Mutation and iterator teardown must restore allocator ownership without leaks");
        }
    }
    for (bool erase : {false, true}) {
        Array array{}; _ZN3sce4Json5ArrayC1Ev(&array);
        Value source{}; _ZN3sce4Json5ValueC1EPKc(&source, "owned payload");
        _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &source);
        const auto sourceAllocations = trace.live.size();
        if (erase) {
            Iterator selected{}, result{}; _ZNK3sce4Json5Array5beginEv(&selected, &array);
            trace.budget = 0;
            _ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&result, &array, &selected);
            trace.budget = -1;
            Require(!result.state, "Rejected erase result allocation must stay absent while destroying its owned payload");
            _ZN3sce4Json5Array8iteratorD1Ev(&selected);
        } else _ZN3sce4Json5Array8pop_backEv(&array);
        Require(trace.live.size() + 4 == sourceAllocations && source.type == 5 && source.payload, "Removal must release the string node, handle, header and characters while preserving the independent source");
        _ZN3sce4Json5ValueD1Ev(&source);
        _ZN3sce4Json5ArrayD1Ev(&array);
        Require(trace.live.size() == baseline, "Owned-payload removal must leave no allocator resources behind");
    }
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Terminate the mutation allocation context without outstanding resources");
    return 0;
}
