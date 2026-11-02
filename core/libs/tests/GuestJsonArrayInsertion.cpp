#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <algorithm>
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
    std::map<void*, std::size_t> generations;
    std::size_t generation = 0;
};
static void* APS5_VABI AllocateArray(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ArrayTrace*>(context); trace.requests.push_back(bytes);
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.live.emplace(memory, bytes);
    Require(trace.generations.emplace(memory, ++trace.generation).second, "Each insertion allocation must own a fresh live generation");
    return memory;
}
static void APS5_VABI FreeArray(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<ArrayTrace*>(context);
    if (trace.headers.erase(memory)) {
        const auto* words = reinterpret_cast<const std::uint64_t*>(memory);
        Require(words[0] == 0xffffffff && words[1] == UINT64_MAX, "Array teardown must expose the native header poison before allocator release");
    }
    Require(trace.live.erase(memory) == 1 && trace.generations.erase(memory) == 1, "Array must release through its owning allocator exactly once"); std::free(memory);
}
static void APS5_VABI NotifyArray(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ArrayTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == bytes, "Array notification must identify the failed allocation"); ++trace.notifications;
}
static void* const ArrayVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateArray), reinterpret_cast<void*>(FreeArray), reinterpret_cast<void*>(NotifyArray)};
struct ArrayAllocator { void* const* vtable = ArrayVtable; };
static void Fill(Array& array) {
    _ZN3sce4Json5ArrayC1Ev(&array);
    for (int number : {10, 20, 30}) {
        Value value{}; _ZN3sce4Json5ValueC1El(&value, number);
        _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &value);
        _ZN3sce4Json5ValueD1Ev(&value);
    }
}
static void CheckSequence(const Array& array, bool front, bool once, int budget) {
    const bool inserted = front ? budget >= 4 : (once ? budget != 4 : budget >= 5);
    const bool lostFirst = front && !inserted && (!once || budget == 3);
    const auto* head = *reinterpret_cast<void* const* const*>(array.items);
    auto* node = reinterpret_cast<void* const*>(head[1]);
    unsigned seen = 0;
    for (int slot = 0; slot < 4; ++slot) {
        const bool newSlot = slot == (front ? 0 : 1);
        if (newSlot && !inserted) continue;
        const int original = front ? slot : (slot == 0 ? 1 : slot);
        if (!newSlot && lostFirst && original == 1) continue;
        Require(node != head, "Insertion must preserve the expected list length");
        const auto* value = reinterpret_cast<const Value*>(node + 2);
        if (newSlot) {
            const bool emptyNull = !front && once && (budget == 1 || budget == 2);
            Require(value->type == (emptyNull ? 0 : 5), "Insertion must retain the native partial-copy type after one-shot failure");
            if (!emptyNull) {
                const char* expected = !front && once && budget == 3 ? "" : "abc";
                Require(value->payload && std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(value->payload)), expected) == 0, "Insertion must retain the native partial string payload");
            }
        } else Require(value->type == 2 && value->payload == static_cast<unsigned>(10 * original), "Insertion failure must preserve the reference's surviving original values");
        node = reinterpret_cast<void* const*>(node[1]); ++seen;
    }
    Require(node == head && _ZNK3sce4Json5Array4sizeEv(&array) == seen, "Insertion links and element count must agree");
}
struct TransferSnapshot {
    void* pointer;
    std::size_t generation;
    std::vector<unsigned char> bytes;
};
static void CheckNestedTransferFailures(ArrayTrace& trace) {
    const auto baseline = trace.generations;
    for (bool object : {false, true}) for (bool front : {false, true}) for (bool once : {false, true}) for (int failure = 0; failure <= 5; ++failure) {
        Array inner{}; Object innerObject{};
        if (object) {
            _ZN3sce4Json6ObjectC1Ev(&innerObject);
            int number = 10;
            for (const char* text : {"a", "b", "c"}) {
                String key{}; _ZN3sce4Json6StringC1EPKc(&key, text);
                _ZN3sce4Json5ValueC1El(_ZN3sce4Json6ObjectixERKNS0_6StringE(&innerObject, &key), number);
                number += 10; _ZN3sce4Json6StringD1Ev(&key);
            }
        } else Fill(inner);
        const auto beforeSource = trace.generations;
        Value source{};
        if (object) _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&source, &innerObject);
        else _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&source, &inner);
        std::vector<TransferSnapshot> sourceAllocations;
        for (const auto& [pointer, generation] : trace.generations) {
            if (beforeSource.contains(pointer)) continue;
            const auto* bytes = reinterpret_cast<const unsigned char*>(pointer);
            sourceAllocations.push_back({pointer, generation, {bytes, bytes + trace.live.at(pointer)}});
        }
        Array destination{}; Fill(destination);
        ArrayIterator position{}, result{}; _ZNK3sce4Json5Array5beginEv(&position, &destination);
        _ZN3sce4Json5Array8iterator7advanceEm(&position, 1);
        const auto* survivor = _ZNK3sce4Json5Array8iteratordeEv(&position);
        const auto sourceValue = source;
        std::vector<std::size_t> copyRequests = object
            ? std::vector<std::size_t>{8, 32, 104, 32, 104, 24, 2, 24, 2, 2, 2, 24, 2, 104, 2, 24, 2, 24, 2, 24, 2, 2, 2, 24, 2, 104, 2, 24, 2, 24, 2, 24, 2, 2, 2, 24, 2, 104, 2, 24, 2, 48}
            : std::vector<std::size_t>{8, 32, 48, 32, 48, 48, 48, 48, 48};
        const auto requests = trace.requests.size(); const auto notices = trace.notifications;
        trace.budget = static_cast<int>(copyRequests.size()) + (front ? 0 : 1) + failure; trace.once = once;
        if (front) _ZN3sce4Json5Array10push_frontERKNS0_5ValueE(&destination, &source);
        else Require(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&result, &destination, &position, &source) == &result, "Nested insertion must return its actual caller-owned iterator");
        trace.budget = -1; trace.once = false;
        std::vector<std::size_t> expected;
        if (!front) expected.push_back(8);
        expected.insert(expected.end(), copyRequests.begin(), copyRequests.end());
        expected.insert(expected.end(), std::max(2, std::min(failure + 1, 5)), 8);
        Require(std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected, "Nested transfer must retain the measured copy, node and interrupted iterator allocation sequence");
        const unsigned failures = failure == 5 ? 0 : failure == 0 && !once ? 2 : 1;
        Require(trace.notifications == notices + failures, "Nested transfer must notify every rejected iterator allocation without retrying the interrupted traversal");
        const bool rolledBack = front && failure < 5;
        Require(_ZNK3sce4Json5Array4sizeEv(&destination) == (rolledBack ? 3u : 4u), "Prepending must remove its new entry after transfer failure while insertion retains it");
        Require(_ZNK3sce4Json5Array8iteratordeEv(&position) == survivor && survivor->type == 2 && survivor->payload == 20, "Interrupted nested transfer must preserve surviving iterator and element identity");
        auto* head = *reinterpret_cast<void* const* const*>(destination.items);
        auto* node = reinterpret_cast<void* const*>(head[1]);
        const Value* inserted = nullptr;
        for (int original = 0; original < 3; ++original) {
            if (!rolledBack && original == (front ? 0 : 1)) {
                inserted = reinterpret_cast<const Value*>(node + 2); node = reinterpret_cast<void* const*>(node[1]);
            }
            Require(node != head && reinterpret_cast<const Value*>(node + 2)->type == 2 && reinterpret_cast<const Value*>(node + 2)->payload == static_cast<unsigned>((original + 1) * 10), "Transfer failure must preserve the full order of all existing destination entries");
            node = reinterpret_cast<void* const*>(node[1]);
        }
        Require(node == head, "Interrupted nested transfer must preserve list closure");
        if (!rolledBack) {
            Require(inserted && !inserted->parent && !inserted->auxiliary && inserted->type == (object ? 7 : 6) && inserted->payload && inserted->payload != source.payload, "Transferred nested collections must own an independent root payload");
            if (!front) Require(_ZNK3sce4Json5Array8iteratordeEv(&result) == inserted, "Returning iterator must retain the new node after an interrupted child transfer");
            const auto rebound = std::max(0, std::min(failure - 1, 3));
            const auto checkChild = [&](const Value* value, int index) {
                Require(value->type == 2 && value->payload == static_cast<unsigned>((index + 1) * 10) && (value->parent == inserted) == (index < rebound), "Transfer must rebind only the child prefix admitted before iterator failure");
            };
            if (object) {
                const auto* nested = reinterpret_cast<const Object*>(inserted->payload);
                ObjectIterator current{}, end{}; _ZNK3sce4Json6Object5beginEv(&current, nested); _ZNK3sce4Json6Object3endEv(&end, nested);
                for (int index = 0; index < 3; ++index) {
                    Require(_ZNK3sce4Json6Object8iteratorneERKS2_(&current, &end), "Interrupted object transfer must retain every copied tree node");
                    const auto* pair = _ZNK3sce4Json6Object8iteratordeEv(&current);
                    const char key[]{static_cast<char>('a' + index), 0};
                    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&pair->key), key) == 0, "Object transfer must preserve ordered public keys");
                    checkChild(&pair->value, index); _ZN3sce4Json6Object8iteratorppEv(&current);
                }
                Require(!_ZNK3sce4Json6Object8iteratorneERKS2_(&current, &end) && _ZNK3sce4Json6Object4sizeEv(nested) == 3, "Interrupted object transfer must preserve tree endpoints and count");
                _ZN3sce4Json6Object8iteratorD1Ev(&current); _ZN3sce4Json6Object8iteratorD1Ev(&end);
            } else {
                const auto* nested = reinterpret_cast<const Array*>(inserted->payload);
                auto* nestedHead = *reinterpret_cast<void* const* const*>(nested->items);
                auto* child = reinterpret_cast<void* const*>(nestedHead[1]);
                for (int index = 0; index < 3; ++index) {
                    Require(child != nestedHead, "Interrupted child rebinding must preserve every copied node");
                    checkChild(reinterpret_cast<const Value*>(child + 2), index); child = reinterpret_cast<void* const*>(child[1]);
                }
                Require(child == nestedHead && _ZNK3sce4Json5Array4sizeEv(nested) == 3, "Nested payload count and circular links must survive interrupted transfer");
            }
        }
        Require(std::memcmp(&source, &sourceValue, sizeof(Value)) == 0, "Interrupted transfer must preserve the source Value bytes");
        for (const auto& snapshot : sourceAllocations) Require(trace.generations.at(snapshot.pointer) == snapshot.generation && std::memcmp(snapshot.pointer, snapshot.bytes.data(), snapshot.bytes.size()) == 0, "Interrupted transfer must preserve all source payload allocation generations and bytes");
        _ZN3sce4Json5Array8iteratorD1Ev(&result); _ZN3sce4Json5Array8iteratorD1Ev(&position);
        _ZN3sce4Json5ArrayD1Ev(&destination); _ZN3sce4Json5ValueD1Ev(&source); _ZN3sce4Json5ArrayD1Ev(&inner); _ZN3sce4Json6ObjectD1Ev(&innerObject);
        Require(trace.generations == baseline, "Both retained and rolled-back nested transfers must reclaim every newly admitted allocation generation");
    }
}
int main() {
    ArrayTrace trace; ArrayAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, reinterpret_cast<const InitParameter2*>(&parameter)) == 0, "Initialize insertion allocation contracts");
    const auto baseline = trace.live.size();
    for (bool front : {false, true}) for (bool once : {false, true}) for (int budget = 0; budget <= 5; ++budget) {
        Array array{}; Fill(array);
        Value source{}; _ZN3sce4Json5ValueC1EPKc(&source, "abc");
        ArrayIterator position{}, result{}; _ZNK3sce4Json5Array5beginEv(&position, &array); _ZN3sce4Json5Array8iterator7advanceEm(&position, 1);
        const auto* survivor = _ZNK3sce4Json5Array8iteratordeEv(&position);
        const auto requests = trace.requests.size(); const auto notices = trace.notifications;
        trace.budget = budget; trace.once = once;
        if (front) _ZN3sce4Json5Array10push_frontERKNS0_5ValueE(&array, &source);
        else Require(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&result, &array, &position, &source) == &result, "Insert must return the caller's hidden iterator object");
        trace.budget = -1; trace.once = false;
        CheckSequence(array, front, once, budget);
        Require(_ZNK3sce4Json5Array8iteratordeEv(&position) == survivor && survivor->payload == 20, "Insertion must preserve surviving node and iterator identity");
        std::vector<std::size_t> expected;
        if (!front) expected.push_back(8);
        expected.push_back(8);
        const int copied = budget - (front ? 0 : 1);
        if (copied >= 1 || (!front && once && budget == 0)) expected.push_back(24);
        if (copied >= 2 || (!front && once && budget == 0)) expected.push_back(4);
        expected.push_back(48);
        Require(std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected, "Insertion must preserve allocation order across result, copied storage and list-node admission");
        const unsigned failed = once ? (budget < (front ? 4 : 5) ? 1 : 0) : (front ? (budget < 3 ? 2 : budget == 3 ? 1 : 0) : (budget == 0 ? 3 : budget < 4 ? 2 : budget == 4 ? 1 : 0));
        Require(trace.notifications == notices + failed, "Every rejected insertion allocation must notify exactly once");
        if (!front) {
            Require((result.state != nullptr) == (budget != 0), "Result allocation failure must not suppress later insertion work");
            if (result.state) {
                const bool nodeFailed = once ? budget == 4 : budget < 5;
                auto* head = *reinterpret_cast<void***>(array.items);
                auto* returned = *reinterpret_cast<void**>(result.state);
                Require(nodeFailed ? returned == head : returned != head && reinterpret_cast<void**>(returned)[1] == *reinterpret_cast<void**>(position.state), "Insert must return the admitted node or the sentinel when node allocation fails");
            }
        }
        Require(source.type == 5 && std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(source.payload)), "abc") == 0, "Insertion must preserve its independently owned source");
        _ZN3sce4Json5Array8iteratorD1Ev(&result); _ZN3sce4Json5Array8iteratorD1Ev(&position);
        _ZN3sce4Json5ValueD1Ev(&source); _ZN3sce4Json5ArrayD1Ev(&array);
        Require(trace.live.size() == baseline, "Every insertion failure must remain destructible without leaked ownership");
    }
    for (bool front : {false, true}) {
        Array inner{}; Fill(inner);
        Value source{}; _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&source, &inner);
        Array destination{}; _ZN3sce4Json5ArrayC1Ev(&destination);
        ArrayIterator position{}, result{}; _ZNK3sce4Json5Array3endEv(&position, &destination);
        const auto requests = trace.requests.size();
        if (front) _ZN3sce4Json5Array10push_frontERKNS0_5ValueE(&destination, &source);
        else _ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&result, &destination, &position, &source);
        const auto* inserted = _ZNK3sce4Json5Array5frontEv(&destination);
        Require(inserted->type == 6 && inserted->payload != source.payload && inserted->parent == nullptr, "Nested insertion must own an independent array payload with the destination's parent");
        auto* nested = reinterpret_cast<const Array*>(inserted->payload);
        auto* head = *reinterpret_cast<void* const* const*>(nested->items);
        for (auto* node = reinterpret_cast<void* const*>(head[1]); node != head; node = reinterpret_cast<void* const*>(node[1])) Require(reinterpret_cast<const Value*>(node + 2)->parent == inserted, "Payload transfer must rebind every child to the actual inserted value");
        Require(trace.requests.size() >= requests + 5 && std::vector<std::size_t>(trace.requests.end() - 5, trace.requests.end()) == std::vector<std::size_t>({8, 8, 8, 8, 8}), "Nested payload transfer must allocate its begin and repeated end iterator states through the supplied allocator");
        _ZN3sce4Json5Array8iteratorD1Ev(&result); _ZN3sce4Json5Array8iteratorD1Ev(&position);
        _ZN3sce4Json5ArrayD1Ev(&destination); _ZN3sce4Json5ValueD1Ev(&source); _ZN3sce4Json5ArrayD1Ev(&inner);
        Require(trace.live.size() == baseline, "Nested insertion and iterator traversal must release every admitted allocation");
    }
    CheckNestedTransferFailures(trace);
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty() && trace.generations.empty(), "Terminate insertion allocation contracts");
    return 0;
}
