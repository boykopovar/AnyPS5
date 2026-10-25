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
static void CheckLinks(const Array& array, std::size_t count, std::initializer_list<std::uint64_t> values = {}) {
    Require(values.size() == 0 || values.size() == count, "Expected array values must cover every live node");
    const auto* header = reinterpret_cast<const std::uintptr_t*>(array.items);
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
static void CheckAssignments(ArrayTrace& trace) {
    const auto baseline = trace.live.size();
    for (bool once : {false, true}) for (int budget = 0; budget <= 5; ++budget) {
        Array source{}, destination{};
        _ZN3sce4Json5ArrayC1Ev(&source); _ZN3sce4Json5ArrayC1Ev(&destination);
        Value value{};
        for (int number : {10, 20, 30}) { _ZN3sce4Json5ValueC1El(&value, number); _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&source, &value); _ZN3sce4Json5ValueD1Ev(&value); }
        _ZN3sce4Json5ValueC1El(&value, 99); _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&destination, &value); _ZN3sce4Json5ValueD1Ev(&value);
        Value owner{};
        reinterpret_cast<std::uintptr_t*>(destination.items)[3] = reinterpret_cast<std::uintptr_t>(&owner);
        auto* previous = destination.items;
        trace.headers.insert(previous);
        const auto notifications = trace.notifications;
        trace.budget = budget; trace.once = once;
        Require(_ZN3sce4Json5ArrayaSERKS1_(&destination, &source) == &destination, "Array assignment must return its destination");
        trace.budget = -1; trace.once = false;
        auto* header = reinterpret_cast<std::uintptr_t*>(destination.items);
        Require((destination.items == previous) == (budget < 2) && header[3] == reinterpret_cast<std::uintptr_t>(&owner), "Array assignment must preserve the original on initial failure and carry its owner into a replacement");
        const auto expected = budget < 2 ? 1 : budget == 5 ? 3 : once ? 2 : budget - 2;
        Require(header[1] == static_cast<std::size_t>(expected), "Array assignment must retain the observed partial list");
        Require(trace.notifications - notifications == (budget < 2 ? 1u : budget == 5 ? 0u : once ? 1u : static_cast<unsigned>(5 - budget)), "Array assignment must report failed node admission while continuing through the source");
        auto* head = reinterpret_cast<const std::uintptr_t*>(header[0]);
        auto* node = reinterpret_cast<const std::uintptr_t*>(head[1]);
        for (int index = 0, sourceIndex = 0; index < expected; ++index, ++sourceIndex) {
            if (once && budget >= 2 && budget < 5 && sourceIndex == budget - 2) ++sourceIndex;
            const auto* entry = reinterpret_cast<const Value*>(node + 2);
            Require(entry->type == 2 && entry->payload == static_cast<std::uint64_t>(budget < 2 ? 99 : (sourceIndex + 1) * 10), "Array assignment must preserve retained entry values");
            if (budget >= 2) Require(entry->parent == &owner && !entry->auxiliary, "Replacement array entries must bind to the destination owner");
            node = reinterpret_cast<const std::uintptr_t*>(node[1]);
        }
        Require(node == head, "Replacement array traversal must close at its sentinel");
        CheckLinks(source, 3);
        trace.headers.insert(destination.items);
        _ZN3sce4Json5ArrayD1Ev(&destination); _ZN3sce4Json5ArrayD1Ev(&source);
        Require(trace.live.size() == baseline, "Assignment must release failed replacement resources and its previous list");
    }
    Array array{}; _ZN3sce4Json5ArrayC1Ev(&array);
    Value value{};
    for (int number : {10, 20, 30}) { _ZN3sce4Json5ValueC1El(&value, number); _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &value); _ZN3sce4Json5ValueD1Ev(&value); }
    auto* previous = array.items; trace.headers.insert(previous);
    _ZN3sce4Json5ArrayaSERKS1_(&array, &array);
    Require(array.items != previous, "Modern array self-assignment must replace its header");
    CheckLinks(array, 3);
    trace.headers.insert(array.items); _ZN3sce4Json5ArrayD1Ev(&array);
    Require(trace.live.size() == baseline, "Array self-assignment must release its old list");
}
int main() {
    ArrayTrace trace; ArrayAllocator allocator;
    Parameter parameter{&allocator, &trace, 16384, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize array allocation contract");
    const auto baseline = trace.live.size();
    CheckAssignments(trace);
    for (int budget : {0, 1}) {
        trace.budget = budget; Array partial{}; _ZN3sce4Json5ArrayC1Ev(&partial);
        Require(budget ? partial.items && trace.live.at(partial.items) == 32 && !*reinterpret_cast<void**>(partial.items) : !partial.items,
                "Header and sentinel failures retain distinct partial array ownership");
        Require(_ZNK3sce4Json5Array4sizeEv(&partial) == 0, "Partial arrays remain empty");
        if (partial.items) trace.headers.insert(partial.items);
        trace.budget = -1; _ZN3sce4Json5ArrayD1Ev(&partial);
        Require(!partial.items && trace.live.size() == baseline, "Partial arrays must be destructible without leaks");
    }
    Array array{}; _ZN3sce4Json5ArrayC1Ev(&array);
    trace.headers.insert(array.items);
    auto* header = reinterpret_cast<std::uintptr_t*>(array.items);
    Require(trace.live.at(array.items) == 32 && trace.live.at(reinterpret_cast<void*>(header[0])) == 48 && !header[3], "Actual array header and sentinel must use the configured allocator");
    Require(_ZNK3sce4Json5Array5frontEv(&array) == reinterpret_cast<const Value*>(header[0] + 16) && _ZNK3sce4Json5Array4backEv(&array) == reinterpret_cast<const Value*>(header[0] + 16), "Empty array endpoints expose the sentinel payload address");
    Value value{};
    for (int number : {10, 20, 30}) { _ZN3sce4Json5ValueC1El(&value, number); _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &value); _ZN3sce4Json5ValueD1Ev(&value); }
    CheckLinks(array, 3);
    ArrayIterator begin{}, end{}, copy{};
    _ZNK3sce4Json5Array5beginEv(&begin, &array); _ZNK3sce4Json5Array3endEv(&end, &array);
    Require(trace.live.at(begin.state) == 8 && trace.live.at(end.state) == 8 && *reinterpret_cast<void**>(end.state) == reinterpret_cast<void*>(header[0]), "ArrayIterator state must own one allocator-backed native node pointer");
    Require(_ZN3sce4Json5Array8iteratorppEi(&begin, 0) == &begin && *_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array8iteratordeEv(&begin)) == 20, "Postfix increment must update this iterator without a hidden result object");
    _ZN3sce4Json5Array8iteratorC1ERKS2_(&copy, &begin);
    Require(trace.live.at(copy.state) == 8 && copy.state != begin.state, "ArrayIterator copy must own independent state");
    _ZN3sce4Json5Array8iterator7advanceEm(&begin, 2);
    Require(!_ZNK3sce4Json5Array8iteratorneERKS2_(&begin, &end), "Advance must reach the sentinel");
    _ZN3sce4Json5Array8iteratorppEv(&begin);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array8iteratordeEv(&begin)) == 10, "Array increment must follow the circular link after the sentinel");
    _ZN3sce4Json5Array8iterator7advanceEm(&begin, 6);
    Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array8iteratordeEv(&begin)) == 30, "Advance must preserve circular traversal");
    trace.budget = 0; ArrayIterator failed{}; _ZNK3sce4Json5Array5beginEv(&failed, &array);
    Require(!failed.state, "ArrayIterator allocation failure must leave an empty outer handle");
    const auto fallbackRequests = trace.requests.size(); const auto fallbackNotices = trace.notifications;
    const auto* fallback = _ZNK3sce4Json5Array8iteratordeEv(&failed);
    Require(fallback && trace.live.contains(const_cast<Value*>(fallback)) && trace.live.at(const_cast<Value*>(fallback)) == 32 &&
            fallback->type == 0 && !fallback->parent && !fallback->auxiliary,
            "Missing iterator state must return the allocator-owned initialized module fallback");
    const Value fallbackValue = *fallback;
    ArrayIterator missing{}; Value lookup{}; _ZN3sce4Json5ValueC1El(&lookup, 17);
    Require(_ZNK3sce4Json5Array8iteratordeEv(&missing) == fallback && _ZNK3sce4Json5Array8iteratorptEv(&failed) == fallback &&
            _ZNK3sce4Json5Array8iteratorptEv(&missing) == fallback &&
            trace.requests.size() == fallbackRequests && trace.notifications == fallbackNotices && std::memcmp(fallback, &fallbackValue, sizeof(fallbackValue)) == 0,
            "Independent missing iterators must share the exact fallback without allocations, notices or mutation");
    Require(_ZNK3sce4Json5ValueixEPKc(&lookup, "missing") == fallback && std::memcmp(fallback, &fallbackValue, sizeof(fallbackValue)) == 0,
            "Failed keyed lookup must return the same module fallback without mutating its bytes");
    _ZN3sce4Json5ValueD1Ev(&lookup);
    trace.budget = -1;
    _ZN3sce4Json5Array8iteratorD1Ev(&copy); _ZN3sce4Json5Array8iteratorD1Ev(&begin); _ZN3sce4Json5Array8iteratorD1Ev(&end);
    const auto beforeCopies = trace.live.size();
    for (int budget = 0; budget <= 6; ++budget) {
        trace.budget = budget; Array duplicate{}; _ZN3sce4Json5ArrayC1ERKS1_(&duplicate, &array);
        const auto expected = budget < 4 ? 0 : static_cast<std::size_t>(budget - 3);
        Require(_ZNK3sce4Json5Array4sizeEv(&duplicate) == expected, "Array copy allocation failures retain the observed copied prefix");
        if (expected) CheckLinks(duplicate, expected);
        if (duplicate.items) trace.headers.insert(duplicate.items);
        trace.budget = -1; _ZN3sce4Json5ArrayD1Ev(&duplicate);
        Require(trace.live.size() == beforeCopies, "Partial array copies must release all resources");
    }
    trace.budget = 0; _ZN3sce4Json5ValueC1El(&value, 40); _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &value); trace.budget = -1;
    Require(_ZNK3sce4Json5Array4sizeEv(&array) == 2 && *_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array4backEv(&array)) == 20, "Failed append must retain the reference module's removal of the existing tail");
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &value);
    Require(_ZNK3sce4Json5Array4sizeEv(&array) == 3 && *_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array4backEv(&array)) == 40, "Append remains usable after failure");
    ArrayIterator retained{}; _ZNK3sce4Json5Array5beginEv(&retained, &array); _ZN3sce4Json5Array8iteratorppEv(&retained);
    const auto* survivor = _ZNK3sce4Json5Array8iteratordeEv(&retained);
    const auto requests = trace.requests.size();
    const auto notifications = trace.notifications;
    trace.budget = 0;
    _ZN3sce4Json5Array9pop_frontEv(&array);
    Require(_ZNK3sce4Json5Array4sizeEv(&array) == 2 && _ZNK3sce4Json5Array5frontEv(&array) == survivor, "Removing the front must preserve the next element and its iterator identity");
    CheckLinks(array, 2, {20, 40});
    _ZN3sce4Json5Array9pop_frontEv(&array); _ZN3sce4Json5Array9pop_frontEv(&array);
    Require(trace.requests.size() == requests && trace.notifications == notifications, "Front removal must reclaim allocator-owned nodes without allocating");
    CheckLinks(array, 0);
    trace.budget = -1;
    _ZN3sce4Json5Array8iteratorD1Ev(&retained);
    _ZN3sce4Json5ValueD1Ev(&value); _ZN3sce4Json5Array5clearEv(&array);
    Require(_ZNK3sce4Json5Array4sizeEv(&array) == 0 && trace.live.size() == baseline + 2, "Clear must retain only the array header and sentinel");
    _ZN3sce4Json5ArrayD1Ev(&array);
    Object object{}; _ZN3sce4Json6ObjectC1Ev(&object);
    for (const char* key : {"a", "b"}) { String string{}; _ZN3sce4Json6StringC1EPKc(&string, key); _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &string); _ZN3sce4Json6StringD1Ev(&string); }
    ObjectIterator member{}; _ZNK3sce4Json6Object5beginEv(&member, &object);
    Require(_ZN3sce4Json6Object8iteratorppEi(&member, 0) == &member && std::strcmp(_ZNK3sce4Json6String5c_strEv(&_ZNK3sce4Json6Object8iteratordeEv(&member)->key), "b") == 0,
            "Object postfix increment must also update this iterator without a hidden result object");
    _ZN3sce4Json6Object8iteratorD1Ev(&member); _ZN3sce4Json6ObjectD1Ev(&object);
    Require(trace.live.size() == baseline, "Array and iterator operations must release their allocated resources");
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Terminate array allocation contract");
    return 0;
}
