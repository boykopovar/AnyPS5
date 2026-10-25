#include "prx/libSceJson2/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <new>
#include <vector>
#include <string>

using namespace SceJson2;

struct Parameter { void* allocator; void* context; std::size_t bufferSize; };
using Handler = const Value& (APS5_VABI*)(ValueType, const Value*, const Value*, void*);
using ReadCallback = int (APS5_VABI*)(char&, void*);
using NullHandler = const Value& (APS5_VABI*)(ValueType, const Value*, void*);
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct LegacyTrace {
    std::map<void*, std::size_t> guest;
    std::map<void*, std::size_t> heap;
    int budget = -1;
    int heapBudget = -1;
    bool once = false;
    std::vector<std::size_t> requests;
    std::map<void*, std::size_t> guestGenerations;
    std::map<void*, std::size_t> heapGenerations;
    std::size_t generation = 0;
    unsigned notifications = 0;
    void (*onNotify)(void*) = nullptr;
    void* noticeContext = nullptr;
};
static LegacyTrace* HeapTrace = nullptr;
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    alignas(8) static const std::uint32_t parameters[]{0, 0, 0, 0, 0x05000000, 0};
    return parameters;
}
extern "C" void* APS5_VABI _Znwm_nid_postfix(std::size_t bytes) {
    Require(HeapTrace != nullptr, "Heap allocation requires its owning trace");
    if (HeapTrace->heapBudget == 0) throw std::bad_alloc();
    if (HeapTrace->heapBudget > 0) --HeapTrace->heapBudget;
    void* memory = std::malloc(bytes);
    if (!memory) throw std::bad_alloc();
    std::memset(memory, 0xa5, bytes);
    Require(HeapTrace && bytes == 56, "Legacy map nodes must use the actual libc new allocation boundary");
    Require(HeapTrace->heap.emplace(memory, bytes).second && HeapTrace->heapGenerations.emplace(memory, ++HeapTrace->generation).second, "Legacy libc allocations must own distinct live generations");
    return memory;
}
extern "C" void APS5_VABI _ZdlPv_nid_postfix(void* memory) {
    Require(HeapTrace && HeapTrace->heap.erase(memory) == 1 && HeapTrace->heapGenerations.erase(memory) == 1, "Legacy map nodes must return through libc delete");
    std::free(memory);
}
static void* APS5_VABI AllocateLegacy(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<LegacyTrace*>(context);
    trace.requests.push_back(bytes);
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes); Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes);
    Require(trace.guest.emplace(memory, bytes).second && trace.guestGenerations.emplace(memory, ++trace.generation).second, "Legacy guest allocations must own distinct live generations");
    return memory;
}
static void APS5_VABI FreeLegacy(void*, void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<LegacyTrace*>(context)->guest.erase(memory) == 1 && static_cast<LegacyTrace*>(context)->guestGenerations.erase(memory) == 1, "Legacy metadata must return through its owning allocator");
    std::free(memory);
}
static void APS5_VABI NotifyLegacy(void*, int error, std::size_t bytes, void* context) {
    Require(static_cast<unsigned>(error) == 0x80848102 && bytes == static_cast<LegacyTrace*>(context)->requests.back(), "Legacy allocation failure must identify its actual rejected request");
    auto& trace = *static_cast<LegacyTrace*>(context);
    ++trace.notifications;
    if (const auto action = trace.onNotify) { trace.onNotify = nullptr; action(trace.noticeContext); }
}
static void* const LegacyVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateLegacy), reinterpret_cast<void*>(FreeLegacy), reinterpret_cast<void*>(NotifyLegacy)};
struct LegacyAllocator { void* const* vtable = LegacyVtable; };
struct LegacyNode { LegacyNode* parent; LegacyNode* left; LegacyNode* right; const Value* key; Handler callback; void* context; std::uint8_t color; std::uint8_t nil; std::uint8_t reserved[6]; };
struct LegacyHeader { LegacyNode* head; std::size_t count; std::uint64_t reserved; };
static_assert(sizeof(LegacyNode) == 56 && sizeof(LegacyHeader) == 24);
struct Context { Value replacement{}; unsigned calls = 0; const Value* source = nullptr; const Value* parent = nullptr; };
static const Value& APS5_VABI RecoverLegacy(ValueType type, const Value* source, const Value* parent, void* context) {
    auto& result = *static_cast<Context*>(context);
    Require(type == 1, "Legacy mismatch dispatch must identify the requested type");
    ++result.calls; result.source = source; result.parent = parent; return result.replacement;
}
static void ParseLegacy(Value& value, const char* text = "9") {
    _ZN3sce4Json5ValueC1Ev(&value);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, text, std::strlen(text)) == 0, "Prepare legacy root auxiliary metadata");
}
static void CheckMap(const LegacyTrace& trace, const std::map<const Value*, Context*>& entries) {
    const LegacyHeader* header = nullptr;
    for (const auto& [memory, size] : trace.guest) {
        if (size != 24) continue;
        void* head = nullptr; std::memcpy(&head, memory, sizeof(head));
        if (trace.heap.contains(head)) { Require(!header, "Legacy registration must use one shared map header"); header = reinterpret_cast<const LegacyHeader*>(memory); }
    }
    Require(header && header->count == entries.size() && header->reserved == UINT64_C(0xa5a5a5a5a5a5a5a5), "Legacy map header must preserve count and reserved bytes");
    auto* head = header->head;
    Require(head->nil == 1 && head->color == 0 && head->parent->color == 0, "Legacy sentinel and root must be black");
    auto expected = entries.begin();
    std::function<unsigned(const LegacyNode*, const LegacyNode*)> visit = [&](const LegacyNode* node, const LegacyNode* parent) -> unsigned {
        if (node == head) return 1;
        Require(trace.heap.contains(const_cast<LegacyNode*>(node)) && node->parent == parent && !node->nil && node->color <= 1, "Legacy nodes must retain their libc ownership and parent link");
        Require(!node->color || (!node->left->color && !node->right->color), "Legacy red nodes must have black children");
        const auto left = visit(node->left, node);
        Require(expected != entries.end() && node->key == expected->first && node->callback == (expected->second ? RecoverLegacy : nullptr) && node->context == expected->second, "Legacy map entries must preserve sorted keys and callback identity");
        ++expected;
        const auto right = visit(node->right, node);
        Require(left == right, "Legacy tree paths must preserve black height");
        return left + !node->color;
    };
    visit(head->parent, head);
    Require(expected == entries.end() && trace.heap.size() == entries.size() + 1, "Legacy map must retain only its actual sentinel and entry nodes");
}
struct NullContext { const Value* other; Context* replacement; };
static const Value& APS5_VABI RecoverLegacyNull(ValueType requested, const Value* parent, void* context) {
    auto& value = *static_cast<NullContext*>(context);
    Require(requested == 1 && !parent && *_ZNK3sce4Json5Value10getBooleanEv(value.other), "Legacy null recovery must allow independent mismatch recovery before the reentry guard");
    return value.replacement->replacement;
}
struct LegacySnapshot { void* pointer; std::size_t generation; std::vector<unsigned char> bytes; };
static void CheckPairCopies(LegacyTrace& trace, Context& context) {
    const auto guestBaseline = trace.guestGenerations, heapBaseline = trace.heapGenerations;
    for (bool text : {false, true}) for (bool once : {false, true}) for (unsigned alias = 0; alias < 2; ++alias) for (int budget = 0; budget <= (text ? 5 : 2); ++budget) {
        Value source{}; ParseLegacy(source, text ? "\"abc\"" : "9");
        Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &context) == 0, "Register the actual legacy Pair source map record");
        std::vector<LegacySnapshot> sourceAllocations;
        for (const auto& [pointer, generation] : trace.guestGenerations) {
            if (guestBaseline.contains(pointer)) continue;
            const auto* bytes = reinterpret_cast<const unsigned char*>(pointer);
            sourceAllocations.push_back({pointer, generation, {bytes, bytes + trace.guest.at(pointer)}});
        }
        const LegacyNode* sourceNode = nullptr;
        for (const auto& [pointer, size] : trace.heap) {
            const auto* node = reinterpret_cast<const LegacyNode*>(pointer);
            if (!node->nil && node->key == &source) sourceNode = node;
        }
        Require(sourceNode && sourceNode->callback == RecoverLegacy && sourceNode->context == &context, "Pair copying requires an independently owned source callback record");
        const auto sourceGeneration = trace.heapGenerations.at(const_cast<LegacyNode*>(sourceNode));
        const Value sourceValue = source;
        String key{}; _ZN3sce4Json6StringC1EPKc(&key, "key");
        Pair pair; std::memset(&pair, 0xa5, sizeof(pair));
        const auto requests = trace.requests.size(); const auto notices = trace.notifications;
        trace.budget = budget; trace.once = once;
        if (alias) _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &source);
        else _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &source);
        trace.budget = -1; trace.once = false;
        std::vector<std::size_t> expected{24};
        if (budget > 0) expected.push_back(4);
        if (text) {
            expected.push_back(8);
            if ((once && budget < 2) || budget >= 3) expected.push_back(24);
            if ((once && budget < 2) || budget >= 4) expected.push_back(4);
        }
        Require(std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected, "Legacy Pair failure must preserve the measured key, Value handle, text header and character request order");
        const unsigned failed = text ? (once ? unsigned(budget < 5) : budget < 2 ? 2u : unsigned(budget < 5)) : unsigned(budget < 2);
        Require(trace.notifications == notices + failed, "Legacy Pair copies must notify every admitted allocation failure");
        const int expectedType = text ? ((once && budget < 2) || budget >= 4 ? 5 : 0) : 3;
        Require(pair.reserved == UINT64_C(0xa5a5a5a5a5a5a5a5) && !pair.value.parent && !pair.value.auxiliary && pair.value.type == expectedType, "Legacy Pair copies must preserve inactive Pair bytes and retain independent Value ownership");
        Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&pair.key), budget < 2 ? "" : "key") == 0, "Legacy Pair key failures must preserve the measured empty or completed key");
        if (expectedType == 5) Require(pair.value.payload && std::strcmp(_ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(pair.value.payload)), budget == 4 ? "" : "abc") == 0, "Legacy Pair string failures must preserve the admitted partial text");
        if (!text) Require(pair.value.payload == 9, "Failed Pair key allocation must not suppress scalar copying");
        CheckMap(trace, {{&source, &context}, {&pair.value, &context}});
        const auto calls = context.calls;
        Require(_ZNK3sce4Json5Value10getBooleanEv(&pair.value) == reinterpret_cast<const bool*>(&context.replacement.payload) && context.calls == calls + 1 && context.source == &pair.value && !context.parent,
                "Every partial legacy Pair must retain its independently copied map callback and exact borrowed replacement identity");
        if (alias) _ZN3sce4Json6Object4PairD2Ev(&pair);
        else _ZN3sce4Json6Object4PairD1Ev(&pair);
        CheckMap(trace, {{&source, &context}});
        Require(std::memcmp(&source, &sourceValue, sizeof(source)) == 0 && trace.heapGenerations.at(const_cast<LegacyNode*>(sourceNode)) == sourceGeneration && sourceNode->callback == RecoverLegacy && sourceNode->context == &context,
                "Pair cleanup must preserve source bytes and its original callback-record generation");
        for (const auto& snapshot : sourceAllocations) Require(trace.guestGenerations.at(snapshot.pointer) == snapshot.generation && std::memcmp(snapshot.pointer, snapshot.bytes.data(), snapshot.bytes.size()) == 0,
                "Failed Pair copies must preserve all source guest allocation generations and bytes");
        Require(_ZNK3sce4Json5Value10getBooleanEv(&source) == reinterpret_cast<const bool*>(&context.replacement.payload) && context.source == &source && !context.parent,
                "Source callbacks must remain usable after failed Pair copy teardown");
        _ZN3sce4Json6StringD1Ev(&key); _ZN3sce4Json5ValueD1Ev(&source); CheckMap(trace, {});
        Require(trace.guestGenerations == guestBaseline && trace.heapGenerations == heapBaseline, "Legacy Pair copies must reclaim every new generation in both allocator domains");
    }
}
struct LegacyArrayLink { LegacyArrayLink* previous; LegacyArrayLink* next; Value value; };
struct LegacyArrayHeader { LegacyArrayLink* head; std::size_t count; std::uint64_t reserved; };
struct LegacyTreeLink { LegacyTreeLink* parent; LegacyTreeLink* left; LegacyTreeLink* right; unsigned char key[24]; Pair pair; unsigned char color; unsigned char nil; unsigned char reserved[6]; };
struct LegacyTreeHeader { LegacyTreeLink* head; std::size_t count; std::uint64_t reserved; };
static_assert(sizeof(LegacyArrayHeader) == 24 && sizeof(LegacyTreeHeader) == 24 && sizeof(LegacyTreeLink) == 104);
static void CollectionShape(const Value& value, const Value* parent, std::string& output, std::vector<const Value*>& values);
static void CollectionTreeShape(const LegacyTreeLink* node, const Value* parent, std::string& output, std::vector<const Value*>& values) {
    if (node->nil) return;
    CollectionTreeShape(node->left, parent, output, values);
    output += "k=" + std::string(*reinterpret_cast<char* const*>(node->key)) + "/q=" + _ZNK3sce4Json6String5c_strEv(&node->pair.key) + "/";
    CollectionShape(node->pair.value, parent, output, values);
    CollectionTreeShape(node->right, parent, output, values);
}
static void CollectionShape(const Value& value, const Value* parent, std::string& output, std::vector<const Value*>& values) {
    values.push_back(&value);
    output += std::to_string(value.type) + ":" + std::to_string(value.parent ? value.parent == parent ? 1 : 2 : 0);
    if (value.type > 0 && value.type < 5) output += "=" + std::to_string(value.payload);
    if (value.type == 5) output += "s=" + std::string(value.payload ? _ZNK3sce4Json6String5c_strEv(reinterpret_cast<const String*>(value.payload)) : "<missing>");
    if (value.type == 6) {
        auto* header = value.payload ? *reinterpret_cast<LegacyArrayHeader* const*>(value.payload) : nullptr;
        if (!header) { output += "x;"; return; }
        output += "a" + std::to_string(header->count) + "[";
        if (!header->head) output += "missing";
        else for (auto* node = header->head->next; node != header->head; node = node->next) CollectionShape(node->value, &value, output, values);
        output += "]";
    }
    if (value.type == 7) {
        auto* header = value.payload ? *reinterpret_cast<LegacyTreeHeader* const*>(value.payload) : nullptr;
        if (!header) { output += "x;"; return; }
        output += "o" + std::to_string(header->count) + "{";
        if (!header->head) output += "missing";
        else CollectionTreeShape(header->head->parent, &value, output, values);
        output += "}";
    }
    output += ";";
}
struct LegacyCollectionCase {
    unsigned source;
    bool once;
    int budget;
    unsigned notifications;
    std::size_t requests;
    std::uint64_t requestOrder;
    const char* key;
    const char* shape;
};
static const LegacyCollectionCase CollectionCases[]{
    {0, 0, 0, 2, 2, UINT64_C(0x08842007b530c95d), "", "0:0;"},
    {0, 0, 1, 2, 3, UINT64_C(0xa922fa18e1c37583), "", "0:0;"},
    {0, 0, 2, 1, 3, UINT64_C(0xa922fa18e1c37583), "key", "0:0;"},
    {0, 0, 3, 1, 4, UINT64_C(0x29e493479f20d661), "key", "0:0;"},
    {0, 0, 4, 2, 6, UINT64_C(0x231a8cd446ee0ac9), "key", "6:0a0[missing];"},
    {0, 0, 5, 1, 6, UINT64_C(0x231a8cd446ee0ac9), "key", "6:0a0[missing];"},
    {0, 0, 6, 1, 7, UINT64_C(0x942845b4867ca51b), "key", "6:0a0[];"},
    {0, 0, 7, 1, 8, UINT64_C(0x3d1384c085cc7f49), "key", "6:0a1[0:0;];"},
    {0, 0, 8, 1, 9, UINT64_C(0x94a9e4235a7c56a3), "key", "6:0a1[0:0;];"},
    {0, 0, 9, 1, 10, UINT64_C(0x19057312c1477af1), "key", "6:0a1[0:0;];"},
    {0, 0, 10, 1, 11, UINT64_C(0xcbbd21de6c7551fb), "key", "6:0a1[7:0o0{missing};];"},
    {0, 0, 11, 1, 12, UINT64_C(0xa7b21ff24b599cc9), "key", "6:0a1[7:0o0{};];"},
    {0, 0, 12, 3, 15, UINT64_C(0x44ba72eae582feb9), "key", "6:0a1[7:0o1{k=/q=/0:0;};];"},
    {0, 0, 13, 2, 15, UINT64_C(0x44ba72eae582feb9), "key", "6:0a1[7:0o1{k=s/q=/0:0;};];"},
    {0, 0, 14, 2, 16, UINT64_C(0x4be45a23fda819b9), "key", "6:0a1[7:0o1{k=s/q=/0:0;};];"},
    {0, 0, 15, 1, 16, UINT64_C(0x4be45a23fda819b9), "key", "6:0a1[7:0o1{k=s/q=s/0:0;};];"},
    {0, 0, 16, 1, 17, UINT64_C(0x9d1ecc2804a38c93), "key", "6:0a1[7:0o1{k=s/q=s/0:0;};];"},
    {0, 0, 17, 1, 18, UINT64_C(0x9ee178ffe1e7da63), "key", "6:0a1[7:0o1{k=s/q=s/5:0s=;};];"},
    {0, 0, 18, 0, 18, UINT64_C(0x9ee178ffe1e7da63), "key", "6:0a1[7:0o1{k=s/q=s/5:0s=t;};];"},
    {0, 1, 0, 1, 17, UINT64_C(0xd9fd6582200984bd), "", "6:0a1[7:0o1{k=s/q=s/5:0s=t;};];"},
    {0, 1, 1, 1, 18, UINT64_C(0x9ee178ffe1e7da63), "", "6:0a1[7:0o1{k=s/q=s/5:0s=t;};];"},
    {0, 1, 2, 1, 3, UINT64_C(0xa922fa18e1c37583), "key", "0:0;"},
    {0, 1, 3, 1, 4, UINT64_C(0x29e493479f20d661), "key", "0:0;"},
    {0, 1, 4, 1, 18, UINT64_C(0x9ee178ffe1e7da63), "key", "6:0a1[7:0o1{k=s/q=s/5:0s=t;};];"},
    {0, 1, 5, 1, 6, UINT64_C(0x231a8cd446ee0ac9), "key", "6:0a0[missing];"},
    {0, 1, 6, 1, 7, UINT64_C(0x942845b4867ca51b), "key", "6:0a0[];"},
    {0, 1, 7, 1, 8, UINT64_C(0x3d1384c085cc7f49), "key", "6:0a1[0:0;];"},
    {0, 1, 8, 1, 9, UINT64_C(0x94a9e4235a7c56a3), "key", "6:0a1[0:0;];"},
    {0, 1, 9, 1, 10, UINT64_C(0x19057312c1477af1), "key", "6:0a1[0:0;];"},
    {0, 1, 10, 1, 11, UINT64_C(0xcbbd21de6c7551fb), "key", "6:0a1[7:0o0{missing};];"},
    {0, 1, 11, 1, 12, UINT64_C(0xa7b21ff24b599cc9), "key", "6:0a1[7:0o0{};];"},
    {0, 1, 12, 1, 18, UINT64_C(0x9ee178ffe1e7da63), "key", "6:0a1[7:0o1{k=/q=s/5:0s=t;};];"},
    {0, 1, 13, 1, 17, UINT64_C(0x68efa027e7058763), "key", "6:0a1[7:0o1{k=s/q=/5:0s=t;};];"},
    {0, 1, 14, 1, 18, UINT64_C(0x9ee178ffe1e7da63), "key", "6:0a1[7:0o1{k=s/q=/5:0s=t;};];"},
    {0, 1, 15, 1, 16, UINT64_C(0x4be45a23fda819b9), "key", "6:0a1[7:0o1{k=s/q=s/0:0;};];"},
    {0, 1, 16, 1, 17, UINT64_C(0x9d1ecc2804a38c93), "key", "6:0a1[7:0o1{k=s/q=s/0:0;};];"},
    {0, 1, 17, 1, 18, UINT64_C(0x9ee178ffe1e7da63), "key", "6:0a1[7:0o1{k=s/q=s/5:0s=;};];"},
    {0, 1, 18, 0, 18, UINT64_C(0x9ee178ffe1e7da63), "key", "6:0a1[7:0o1{k=s/q=s/5:0s=t;};];"},
    {1, 0, 0, 2, 2, UINT64_C(0x08842007b530c95d), "", "0:0;"},
    {1, 0, 1, 2, 3, UINT64_C(0xa922fa18e1c37583), "", "0:0;"},
    {1, 0, 2, 1, 3, UINT64_C(0xa922fa18e1c37583), "key", "0:0;"},
    {1, 0, 3, 1, 4, UINT64_C(0x29e493479f20d661), "key", "0:0;"},
    {1, 0, 4, 1, 5, UINT64_C(0x503c4bb364cbb14b), "key", "0:0;"},
    {1, 0, 5, 1, 6, UINT64_C(0x2225c4d4461dfe79), "key", "7:0o0{missing};"},
    {1, 0, 6, 1, 7, UINT64_C(0x242b85b324f6b6e3), "key", "7:0o0{};"},
    {1, 0, 7, 3, 10, UINT64_C(0x154035d1032f5a9b), "key", "7:0o1{k=/q=/0:0;};"},
    {1, 0, 8, 2, 10, UINT64_C(0x154035d1032f5a9b), "key", "7:0o1{k=a/q=/0:0;};"},
    {1, 0, 9, 2, 11, UINT64_C(0x4b61a128696594d3), "key", "7:0o1{k=a/q=/0:0;};"},
    {1, 0, 10, 1, 11, UINT64_C(0x4b61a128696594d3), "key", "7:0o1{k=a/q=a/0:0;};"},
    {1, 0, 11, 1, 12, UINT64_C(0x7c79a2ab179bd4f1), "key", "7:0o1{k=a/q=a/0:0;};"},
    {1, 0, 12, 2, 14, UINT64_C(0xa516a88d9f1de459), "key", "7:0o1{k=a/q=a/6:0a0[missing];};"},
    {1, 0, 13, 1, 14, UINT64_C(0xa516a88d9f1de459), "key", "7:0o1{k=a/q=a/6:0a0[missing];};"},
    {1, 0, 14, 1, 15, UINT64_C(0xa364d1a55fcb1e6b), "key", "7:0o1{k=a/q=a/6:0a0[];};"},
    {1, 0, 15, 1, 16, UINT64_C(0x6f6e9f01c624a239), "key", "7:0o1{k=a/q=a/6:0a1[0:0;];};"},
    {1, 0, 16, 1, 17, UINT64_C(0x7d9a5103b03f7e13), "key", "7:0o1{k=a/q=a/6:0a1[0:0;];};"},
    {1, 0, 17, 1, 18, UINT64_C(0xacb5ba447be336e3), "key", "7:0o1{k=a/q=a/6:0a1[5:0s=;];};"},
    {1, 0, 18, 0, 18, UINT64_C(0xacb5ba447be336e3), "key", "7:0o1{k=a/q=a/6:0a1[5:0s=t;];};"},
    {1, 1, 0, 1, 17, UINT64_C(0xc5a67b07339990bd), "", "7:0o1{k=a/q=a/6:0a1[5:0s=t;];};"},
    {1, 1, 1, 1, 18, UINT64_C(0xacb5ba447be336e3), "", "7:0o1{k=a/q=a/6:0a1[5:0s=t;];};"},
    {1, 1, 2, 1, 3, UINT64_C(0xa922fa18e1c37583), "key", "0:0;"},
    {1, 1, 3, 1, 4, UINT64_C(0x29e493479f20d661), "key", "0:0;"},
    {1, 1, 4, 1, 5, UINT64_C(0x503c4bb364cbb14b), "key", "0:0;"},
    {1, 1, 5, 1, 6, UINT64_C(0x2225c4d4461dfe79), "key", "7:0o0{missing};"},
    {1, 1, 6, 1, 7, UINT64_C(0x242b85b324f6b6e3), "key", "7:0o0{};"},
    {1, 1, 7, 1, 18, UINT64_C(0xacb5ba447be336e3), "key", "7:0o1{k=/q=a/6:0a1[5:0s=t;];};"},
    {1, 1, 8, 1, 17, UINT64_C(0xe052cbb56cadb10b), "key", "7:0o1{k=a/q=/6:0a1[5:0s=t;];};"},
    {1, 1, 9, 1, 18, UINT64_C(0xacb5ba447be336e3), "key", "7:0o1{k=a/q=/6:0a1[5:0s=t;];};"},
    {1, 1, 10, 1, 11, UINT64_C(0x4b61a128696594d3), "key", "7:0o1{k=a/q=a/0:0;};"},
    {1, 1, 11, 1, 12, UINT64_C(0x7c79a2ab179bd4f1), "key", "7:0o1{k=a/q=a/0:0;};"},
    {1, 1, 12, 1, 18, UINT64_C(0xacb5ba447be336e3), "key", "7:0o1{k=a/q=a/6:0a1[5:0s=t;];};"},
    {1, 1, 13, 1, 14, UINT64_C(0xa516a88d9f1de459), "key", "7:0o1{k=a/q=a/6:0a0[missing];};"},
    {1, 1, 14, 1, 15, UINT64_C(0xa364d1a55fcb1e6b), "key", "7:0o1{k=a/q=a/6:0a0[];};"},
    {1, 1, 15, 1, 16, UINT64_C(0x6f6e9f01c624a239), "key", "7:0o1{k=a/q=a/6:0a1[0:0;];};"},
    {1, 1, 16, 1, 17, UINT64_C(0x7d9a5103b03f7e13), "key", "7:0o1{k=a/q=a/6:0a1[0:0;];};"},
    {1, 1, 17, 1, 18, UINT64_C(0xacb5ba447be336e3), "key", "7:0o1{k=a/q=a/6:0a1[5:0s=;];};"},
    {1, 1, 18, 0, 18, UINT64_C(0xacb5ba447be336e3), "key", "7:0o1{k=a/q=a/6:0a1[5:0s=t;];};"},
};
static void CheckCollectionCopies(LegacyTrace& trace, Context& context) {
    const auto guestBaseline = trace.guestGenerations, heapBaseline = trace.heapGenerations;
    for (const auto& test : CollectionCases) for (unsigned alias = 0; alias < 2; ++alias) {
        Value source{}; ParseLegacy(source, test.source ? R"({"a":["t"]})" : R"([{"s":"t"}])");
        Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &context) == 0, "Register nested legacy Pair source recovery");
        std::vector<LegacySnapshot> sourceAllocations;
        for (const auto& [pointer, generation] : trace.guestGenerations) {
            if (guestBaseline.contains(pointer)) continue;
            const auto* bytes = reinterpret_cast<const unsigned char*>(pointer);
            sourceAllocations.push_back({pointer, generation, {bytes, bytes + trace.guest.at(pointer)}});
        }
        const LegacyNode* sourceNode = nullptr;
        for (const auto& [pointer, size] : trace.heap) {
            const auto* node = reinterpret_cast<const LegacyNode*>(pointer);
            if (!node->nil && node->key == &source) sourceNode = node;
        }
        Require(sourceNode != nullptr, "Nested Pair copies require the actual source callback record");
        const auto sourceGeneration = trace.heapGenerations.at(const_cast<LegacyNode*>(sourceNode));
        const Value sourceValue = source;
        String key{}; _ZN3sce4Json6StringC1EPKc(&key, "key");
        Pair pair; std::memset(&pair, 0xa5, sizeof(pair));
        const auto requests = trace.requests.size(); const auto notices = trace.notifications;
        trace.budget = test.budget; trace.once = test.once;
        if (alias) _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &source);
        else _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &source);
        trace.budget = -1; trace.once = false;
        std::uint64_t requestOrder = UINT64_C(14695981039346656037);
        for (auto i = requests; i < trace.requests.size(); ++i) {
            requestOrder ^= trace.requests[i]; requestOrder *= UINT64_C(1099511628211);
        }
        Require(trace.notifications == notices + test.notifications && trace.requests.size() == requests + test.requests && requestOrder == test.requestOrder,
                "Nested legacy Pair failures must preserve every request, notification and allocation-order fingerprint");
        Require(pair.reserved == UINT64_C(0xa5a5a5a5a5a5a5a5) && pair.value.reserved == UINT32_C(0xa5a5a5a5) && !pair.value.auxiliary && std::strcmp(_ZNK3sce4Json6String5c_strEv(&pair.key), test.key) == 0,
                "Nested legacy copies must preserve inactive bytes, independent metadata and partial key admission");
        std::string shape; std::vector<const Value*> values;
        CollectionShape(pair.value, nullptr, shape, values);
        Require(shape == test.shape, "Nested legacy copies must preserve partial counts, missing sentinels, both object keys, scalar/string contents and child parent identities");
        CheckMap(trace, {{&source, &context}, {&pair.value, &context}});
        const auto calls = context.calls;
        Require(_ZNK3sce4Json5Value10getBooleanEv(&pair.value) == reinterpret_cast<const bool*>(&context.replacement.payload) && context.calls == calls + 1 && context.source == &pair.value && !context.parent,
                "Even an incomplete nested Pair must retain its independently copied callback and borrowed result identity");
        for (const auto* value : values) {
            if (value == &pair.value) continue;
            Require(!value->parent && !value->auxiliary && !*_ZNK3sce4Json5Value10getBooleanEv(value) && context.calls == calls + 1,
                    "Unbound legacy descendants must use typed defaults without inheriting the copied root callback");
        }
        if (alias) _ZN3sce4Json6Object4PairD2Ev(&pair);
        else _ZN3sce4Json6Object4PairD1Ev(&pair);
        CheckMap(trace, {{&source, &context}});
        Require(std::memcmp(&source, &sourceValue, sizeof(source)) == 0 && trace.heapGenerations.at(const_cast<LegacyNode*>(sourceNode)) == sourceGeneration && sourceNode->callback == RecoverLegacy && sourceNode->context == &context,
                "Nested Pair teardown must preserve the source Value and callback-record generation");
        for (const auto& snapshot : sourceAllocations) Require(trace.guestGenerations.at(snapshot.pointer) == snapshot.generation && std::memcmp(snapshot.pointer, snapshot.bytes.data(), snapshot.bytes.size()) == 0,
                "Nested Pair failure and cleanup must preserve all source allocation generations and bytes");
        Require(_ZNK3sce4Json5Value10getBooleanEv(&source) == reinterpret_cast<const bool*>(&context.replacement.payload) && context.source == &source && !context.parent,
                "Nested source recovery must remain usable after copied storage is destroyed");
        _ZN3sce4Json6StringD1Ev(&key); _ZN3sce4Json5ValueD1Ev(&source); CheckMap(trace, {});
        Require(trace.guestGenerations == guestBaseline && trace.heapGenerations == heapBaseline, "Nested legacy copies must restore every original generation in both allocation domains");
    }
}
struct MapReadContext {
    LegacyTrace* trace;
    Value* target;
    Value* copy;
    Context* replacement;
    unsigned mode;
    unsigned reads = 0;
    int registration = -1;
    Pair* pair = nullptr;
    const String* key = nullptr;
    unsigned alias = 0;
};
static void UpdateInheritedMap(void* data) {
    auto& context = *static_cast<MapReadContext*>(data);
    context.registration = _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(context.target, RecoverLegacy, context.replacement);
    if (context.pair) {
        if (context.alias == 1) _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(context.pair, context.key, context.target);
        else _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(context.pair, context.key, context.target);
    } else _ZN3sce4Json5ValueC1ERKS1_(context.copy, context.target);
}
static int APS5_VABI ReadInheritedMap(char& character, void* data) {
    auto& context = *static_cast<MapReadContext*>(data);
    if (!context.reads) {
        if (context.mode) {
            if (context.mode == 2) { context.trace->onNotify = UpdateInheritedMap; context.trace->noticeContext = &context; }
            context.trace->budget = 0; context.trace->once = true;
            Require(!_ZN3sce4Json6MallocEm(8),"Inherited map testing must reject the actual guest allocation");
            context.trace->budget = -1; context.trace->once = false;
        }
        if (context.mode != 2) UpdateInheritedMap(&context);
    }
    if (context.reads >= 4) return static_cast<int>(0x80848101);
    character = "true"[context.reads++]; return 0;
}
static void CheckInheritedMap(LegacyTrace& trace, Context& first, Context& second) {
    const auto guestBaseline = trace.guestGenerations, heapBaseline = trace.heapGenerations;
    for (unsigned shape = 0; shape < 3; ++shape) for (bool existing : {false, true}) for (unsigned mode = 0; mode < 3; ++mode) for (unsigned alias = 0; alias < 3; ++alias) {
        Value target{}, scalarCopy{}, outer{}; Pair pair; std::memset(&pair, 0xa5, sizeof(pair));
        Value& copy = alias ? pair.value : scalarCopy;
        String key{}; if (alias) _ZN3sce4Json6StringC1EPKc(&key, "key");
        ParseLegacy(target, shape == 0 ? "9" : shape == 1 ? "[{\"s\":\"text\"}]" : "{\"a\":[\"text\"]}");
        if (existing) Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&target, RecoverLegacy, &first) == 0, "Prepare an existing legacy mismatch record before inherited failure");
        std::vector<LegacySnapshot> targetAllocations;
        for (const auto& [pointer, generation] : trace.guestGenerations) {
            if (guestBaseline.contains(pointer)) continue;
            const auto* bytes = reinterpret_cast<const unsigned char*>(pointer);
            targetAllocations.push_back({pointer, generation, {bytes, bytes + trace.guest.at(pointer)}});
        }
        const Value targetValue = target;
        _ZN3sce4Json5ValueC1Ev(&outer);
        MapReadContext context{&trace, &target, &copy, &second, mode};
        if (alias) { context.pair = &pair; context.key = &key; context.alias = alias; }
        const auto requests = trace.requests.size(); const auto notices = trace.notifications;
        const int status = _ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5_(&outer, ReadInheritedMap, &context);
        const auto error = static_cast<int>(0x80848102);
        Require(status == (mode ? error : 0) && context.registration == (mode == 1 ? error : 0) && context.reads == 4 && outer.type == 1 && (outer.payload & 0xff) == 1 && outer.auxiliary,
                "Legacy map updates must observe failure only after notifier return without clearing the outer parser error or interrupting its admitted literal");
        auto expected = mode ? std::vector<std::size_t>{8} : std::vector<std::size_t>{};
        if (alias) { expected.push_back(24); expected.push_back(4); }
        if (shape) {
            const auto payload = shape == 1 ? std::vector<std::size_t>{8, 24, 48, 48, 48, 8, 24, 104, 104, 104, 2, 24, 2, 8, 24, 5}
                                            : std::vector<std::size_t>{8, 24, 104, 104, 104, 2, 24, 2, 8, 24, 48, 48, 48, 8, 24, 5};
            expected.insert(expected.end(), payload.begin(), payload.end());
        }
        expected.push_back(24); expected.push_back(32);
        if (alias) Require(pair.reserved == UINT64_C(0xa5a5a5a5a5a5a5a5) && pair.key.text != key.text && std::strcmp(_ZNK3sce4Json6String5c_strEv(&pair.key), "key") == 0,
                "Inherited failure must preserve independent Pair key ownership, complete key contents and inactive padding");
        Require(std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected && trace.notifications == notices + unsigned(mode != 0) && !trace.onNotify,
                "Inherited map registration must preserve actual failure, root-admission and notifier ordering");
        Context* targetContext = mode == 1 ? existing ? &first : nullptr : &second;
        Context* copyContext = mode == 1 ? nullptr : &second;
        CheckMap(trace, {{&target, targetContext}, {&copy, copyContext}});
        Require(!copy.parent && !copy.auxiliary && copy.type == (shape ? shape == 1 ? 6 : 7 : 3) && (shape || copy.payload == 9) && std::memcmp(&target, &targetValue, sizeof(target)) == 0,
                "Inherited callback-map failure must preserve copied payload type and source ownership metadata");
        const auto verifyShape = [&] {
            std::string actual; std::vector<const Value*> values;
            CollectionShape(copy, nullptr, actual, values);
            const char* expectedShape = shape == 0 ? "3:0=9;" : shape == 1 ? "6:0a1[7:0o1{k=s/q=s/5:0s=text;};];" : "7:0o1{k=a/q=a/6:0a1[5:0s=text;];};";
            Require(actual == expectedShape, "Inherited copies must preserve complete nested payloads, independent internal/public keys and unbound descendant parents");
        };
        verifyShape();
        const auto verifyRecovery = [&](const Value& value, Context* expectedContext) {
            const auto firstCalls = first.calls, secondCalls = second.calls;
            const auto* result = _ZNK3sce4Json5Value10getBooleanEv(&value);
            if (expectedContext) Require(result == reinterpret_cast<const bool*>(&expectedContext->replacement.payload) && expectedContext->source == &value && !expectedContext->parent && expectedContext->calls == (expectedContext == &first ? firstCalls : secondCalls) + 1,
                    "Admitted legacy records must retain their exact callback context, receiver and borrowed result");
            else Require(!*result && first.calls == firstCalls && second.calls == secondCalls,
                    "An empty map entry retained after inherited failure must use the typed default without callback dispatch");
        };
        verifyRecovery(target, targetContext); verifyRecovery(copy, copyContext);
        const auto guest = trace.guestGenerations, heap = trace.heapGenerations;
        const auto beforeRetry = trace.requests.size();
        Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&target, RecoverLegacy, &second) == 0 && _ZN3sce4Json5ValueaSERKS1_(&copy, &target) == &copy,
                "Independent registration and copy retry must succeed after the outer failure scope completes");
        Require((shape ? trace.guestGenerations.size() == guest.size() && trace.requests.size() > beforeRetry : trace.guestGenerations == guest && trace.requests.size() == beforeRetry) && trace.heapGenerations == heap,
                "Retry must reuse legacy callback records and retain balanced payload ownership across collection replacement");
        verifyShape();
        CheckMap(trace, {{&target, &second}, {&copy, &second}});
        verifyRecovery(target, &second); verifyRecovery(copy, &second);
        for (const auto& snapshot : targetAllocations) Require(trace.guestGenerations.at(snapshot.pointer) == snapshot.generation && std::memcmp(snapshot.pointer, snapshot.bytes.data(), snapshot.bytes.size()) == 0,
                "Inherited failure, registration retry and copying must preserve the original source auxiliary and fallback generations");
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&outer, "false", 5) == 0 && outer.type == 1 && !(outer.payload & 0xff),
                "The next independent parse must not inherit an already completed callback-map failure");
        if (alias) {
            if (alias == 1) _ZN3sce4Json6Object4PairD1Ev(&pair);
            else _ZN3sce4Json6Object4PairD2Ev(&pair);
            _ZN3sce4Json6StringD1Ev(&key);
        } else _ZN3sce4Json5ValueD1Ev(&copy);
        _ZN3sce4Json5ValueD1Ev(&target); _ZN3sce4Json5ValueD1Ev(&outer); CheckMap(trace, {});
        Require(trace.guestGenerations == guestBaseline && trace.heapGenerations == heapBaseline,
                "Inherited map updates and retries must reclaim every temporary generation in both allocator domains");
    }
}
static int CheckHeapExceptions(unsigned budget) {
    Require(budget <= 2, "Heap exceptions must cover sentinel, entry and complete admission");
    LegacyTrace trace; HeapTrace = &trace; LegacyAllocator allocator;
    Parameter parameter{&allocator, &trace, 16384};
    std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&owner, &parameter) == 0, "Initialize legacy heap exception resources");
    Context context; _ZN3sce4Json5ValueC1Eb(&context.replacement, true);
    Value source{}; ParseLegacy(source);
    const Value sourceValue = source;
    const auto baseline = trace.guestGenerations;
    std::vector<LegacySnapshot> snapshots;
    for (const auto& [pointer, generation] : baseline) {
        const auto* bytes = reinterpret_cast<const unsigned char*>(pointer);
        snapshots.push_back({pointer, generation, {bytes, bytes + trace.guest.at(pointer)}});
    }
    trace.heapBudget = static_cast<int>(budget);
    const auto requests = trace.requests.size();
    bool caught = false;
    try { Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &context) == 0, "Complete registration must succeed"); }
    catch (const std::bad_alloc&) { caught = true; }
    Require(caught == (budget < 2) && owner && trace.heap.size() == budget && trace.guest.size() == baseline.size() + 1 &&
            trace.requests.size() == requests + 1 && trace.requests.back() == 24 && !trace.notifications,
            "Libc exceptions must propagate without guest OOM notifications or reclaiming the admitted map header");
    void* header = nullptr;
    for (const auto& [pointer, generation] : trace.guestGenerations) if (!baseline.contains(pointer)) { Require(!header, "Admission creates only one guest map header"); header = pointer; }
    Require(header && trace.guest.at(header) == 24, "The unpublished header must remain owned in its guest allocator domain");
    const auto headerGeneration = trace.guestGenerations.at(header);
    if (!budget) {
        std::array<unsigned char, 24> expected; expected.fill(0xa5); std::memset(expected.data() + 8, 0, 8);
        Require(std::memcmp(header, expected.data(), expected.size()) == 0 && trace.heap.empty(), "Sentinel failure must leave only the count initialized in the unpublished header");
    } else CheckMap(trace, budget == 1 ? std::map<const Value*, Context*>{} : std::map<const Value*, Context*>{{&source, &context}});
    trace.heapBudget = -1;
    const auto retryRequests = trace.requests.size();
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &context) == 0 &&
            trace.requests.size() == retryRequests + unsigned(!budget) && trace.heap.size() == 2 && trace.guestGenerations.at(header) == headerGeneration,
            "Retry must replace only an unpublished map and reuse an already published sentinel");
    CheckMap(trace, {{&source, &context}});
    const auto calls = context.calls;
    Require(_ZNK3sce4Json5Value10getBooleanEv(&source) == reinterpret_cast<const bool*>(&context.replacement.payload) && context.calls == calls + 1 && context.source == &source && !context.parent,
            "Retried callback metadata must dispatch the original context and borrowed result");
    Require(std::memcmp(&source, &sourceValue, sizeof(source)) == 0, "Heap failure and retry must preserve source metadata ownership");
    for (const auto& snapshot : snapshots) Require(trace.guestGenerations.at(snapshot.pointer) == snapshot.generation &&
            std::memcmp(snapshot.pointer, snapshot.bytes.data(), snapshot.bytes.size()) == 0, "Heap failure must preserve preexisting allocation bytes and generations");
    _ZN3sce4Json5ValueD1Ev(&source); _ZN3sce4Json5ValueD1Ev(&context.replacement);
    Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.heap.empty() && trace.heapGenerations.empty() && trace.guest.size() == unsigned(!budget),
            "Termination must reclaim published map storage while preserving the orphaned unpublished header");
    if (!budget) {
        Require(trace.guestGenerations.at(header) == headerGeneration, "Library cleanup must not falsely claim orphan reclamation");
        FreeLegacy(nullptr, header, &trace);
    }
    Require(trace.guest.empty() && trace.guestGenerations.empty(), "The fixture separately reclaims any orphaned allocation");
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&owner, &parameter) == 0 &&
            _ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.guest.empty() && trace.heap.empty(), "Heap exception cleanup must permit a fresh module generation");
    HeapTrace = nullptr;
    return 0;
}
static int CheckCopyHeapExceptions(unsigned shape, unsigned alias) {
    Require(shape < 4 && alias < 3, "Heap-copy schedules cover scalar, string, nested collections and constructor aliases");
    LegacyTrace trace; HeapTrace = &trace; LegacyAllocator allocator; Parameter parameter{&allocator, &trace, 16384};
    std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&owner, &parameter) == 0, "Initialize copy exception resources");
    Context context; _ZN3sce4Json5ValueC1Eb(&context.replacement, true);
    Value source{}; ParseLegacy(source, shape == 0 ? "9" : shape == 1 ? "\"text\"" : shape == 2 ? "[{\"s\":\"text\"}]" : "{\"a\":[\"text\"]}");
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &context) == 0, "Prepare source callback ownership");
    String key{}; _ZN3sce4Json6StringC1EPKc(&key, "key");
    Pair pair; std::memset(&pair, 0xa5, sizeof(pair)); Value scalar{}; Value& copy = alias ? pair.value : scalar;
    const Value sourceValue = source;
    const auto baseline = trace.guestGenerations, heapBaseline = trace.heapGenerations;
    std::vector<LegacySnapshot> snapshots;
    for (const auto& [pointer, generation] : baseline) {
        if (trace.guest.at(pointer) == 24 && trace.heap.contains(*reinterpret_cast<void**>(pointer))) continue;
        const auto* bytes = reinterpret_cast<const unsigned char*>(pointer);
        snapshots.push_back({pointer, generation, {bytes, bytes + trace.guest.at(pointer)}});
    }
    const auto requests = trace.requests.size(); trace.heapBudget = 0;
    bool caught = false;
    try {
        if (!alias) _ZN3sce4Json5ValueC1ERKS1_(&copy, &source);
        else if (alias == 1) _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &source);
        else _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &source);
    } catch (const std::bad_alloc&) { caught = true; }
    std::vector<std::size_t> expected;
    if (alias) expected = {24, 4};
    const auto payload = shape == 0 ? std::vector<std::size_t>{} : shape == 1 ? std::vector<std::size_t>{8, 24, 5}
        : shape == 2 ? std::vector<std::size_t>{8, 24, 48, 48, 48, 8, 24, 104, 104, 104, 2, 24, 2, 8, 24, 5}
                     : std::vector<std::size_t>{8, 24, 104, 104, 104, 2, 24, 2, 8, 24, 48, 48, 48, 8, 24, 5};
    expected.insert(expected.end(), payload.begin(), payload.end());
    const unsigned retained = shape == 0 ? 0 : shape == 1 ? 3 : 14;
    Require(caught && owner && !trace.notifications && trace.heapGenerations == heapBaseline && trace.guest.size() == baseline.size() + retained + unsigned(alias != 0) * 2 &&
            std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected,
            "Map-entry exceptions must preserve completed copy allocations without constructor rollback or guest OOM notices");
    CheckMap(trace, {{&source, &context}});
    const auto verifyPayload = [&] {
        std::string actual; std::vector<const Value*> values; CollectionShape(copy, nullptr, actual, values);
        const char* expectedShape = shape == 0 ? "3:0=9;" : shape == 1 ? "5:0s=text;" : shape == 2 ? "6:0a1[7:0o1{k=s/q=s/5:0s=text;};];" : "7:0o1{k=a/q=a/6:0a1[5:0s=text;];};";
        Require(actual == expectedShape && !copy.auxiliary, "Heap exceptions must preserve complete payloads and independent callback metadata");
        if (alias) Require(pair.key.text != key.text && std::strcmp(_ZNK3sce4Json6String5c_strEv(&pair.key), "key") == 0 && pair.reserved == UINT64_C(0xa5a5a5a5a5a5a5a5),
                "Failed Pair constructors must retain their independent key and inactive padding");
    };
    verifyPayload();
    const auto calls = context.calls;
    Require(!*_ZNK3sce4Json5Value10getBooleanEv(&copy) && context.calls == calls, "An unpublished copy callback must use the typed default");
    Require(_ZNK3sce4Json5Value10getBooleanEv(&source) == reinterpret_cast<const bool*>(&context.replacement.payload) && context.calls == calls + 1 && context.source == &source,
            "Copy failure must preserve the source callback and borrowed result");
    trace.heapBudget = -1;
    Require(_ZN3sce4Json5ValueaSERKS1_(&copy, &source) == &copy, "Assignment retry must publish the copied callback");
    CheckMap(trace, {{&source, &context}, {&copy, &context}}); verifyPayload();
    Require(_ZNK3sce4Json5Value10getBooleanEv(&copy) == reinterpret_cast<const bool*>(&context.replacement.payload) && context.source == &copy && !context.parent,
            "Retried copies must recover through their own callback identity");
    Require(std::memcmp(&source, &sourceValue, sizeof(source)) == 0, "Copy failure and retry must preserve source Value bytes");
    for (const auto& [pointer, generation] : baseline) Require(trace.guestGenerations.at(pointer) == generation, "Retry must preserve every preexisting allocation generation including shared map storage");
    for (const auto& snapshot : snapshots) Require(trace.guestGenerations.at(snapshot.pointer) == snapshot.generation && std::memcmp(snapshot.pointer, snapshot.bytes.data(), snapshot.bytes.size()) == 0,
            "Copy failure must preserve all preexisting allocation bytes and generations");
    if (!alias) _ZN3sce4Json5ValueD1Ev(&copy);
    else if (alias == 1) _ZN3sce4Json6Object4PairD1Ev(&pair);
    else _ZN3sce4Json6Object4PairD2Ev(&pair);
    _ZN3sce4Json6StringD1Ev(&key); _ZN3sce4Json5ValueD1Ev(&source); _ZN3sce4Json5ValueD1Ev(&context.replacement);
    Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.guest.empty() && trace.heap.empty() && trace.guestGenerations.empty() && trace.heapGenerations.empty(),
            "Failed construction, retry and explicit destruction must reclaim both allocator domains");
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&owner, &parameter) == 0 && _ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && trace.guest.empty() && trace.heap.empty(),
            "Copy exceptions must permit a fresh module generation");
    HeapTrace = nullptr;
    return 0;
}

static int CheckArrayHeapExceptions(unsigned budget, unsigned alias, bool wrapped = false) {
    Require(budget <= 2 && alias < 2, "Array-copy schedules cover both child entries and constructor aliases");
    LegacyTrace trace; HeapTrace = &trace; LegacyAllocator allocator; Parameter parameter{&allocator, &trace, 16384};
    std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&owner, &parameter) == 0, "Initialize array-copy exception resources");
    Context context; _ZN3sce4Json5ValueC1Eb(&context.replacement, true); Value source{}; ParseLegacy(source);
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &context) == 0, "Prepare copied child callbacks");
    Array array{}; _ZN3sce4Json5ArrayC1Ev(&array);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &source); _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &source);
    auto* sourceHeader = reinterpret_cast<LegacyArrayHeader*>(array.items);
    std::map<const Value*, Context*> entries{{&source, &context}};
    for (auto* node = sourceHeader->head->next; node != sourceHeader->head; node = node->next) entries.emplace(&node->value, &context);
    CheckMap(trace, entries);
    const auto baseline = trace.guestGenerations, heapBaseline = trace.heapGenerations;
    const Value sourceValue = source;
    std::vector<LegacySnapshot> snapshots;
    for (const auto& [pointer, generation] : baseline) {
        if (trace.guest.at(pointer) == 24 && trace.heap.contains(*reinterpret_cast<void**>(pointer))) continue;
        const auto* bytes = reinterpret_cast<const unsigned char*>(pointer);
        snapshots.push_back({pointer, generation, {bytes, bytes + trace.guest.at(pointer)}});
    }
    Array directCopy; std::memset(&directCopy, 0xa5, sizeof(directCopy)); Value wrappedCopy; std::memset(&wrappedCopy, 0xa5, sizeof(wrappedCopy));
    const auto requests = trace.requests.size(); trace.heapBudget = static_cast<int>(budget);
    bool caught = false;
    try {
        if (wrapped) {
            if (alias) _ZN3sce4Json5ValueC2ERKNS0_5ArrayE(&wrappedCopy, &array);
            else _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&wrappedCopy, &array);
        } else if (alias) _ZN3sce4Json5ArrayC2ERKS1_(&directCopy, &array);
        else _ZN3sce4Json5ArrayC1ERKS1_(&directCopy, &array);
    } catch (const std::bad_alloc&) { caught = true; }
    if (wrapped) Require(wrappedCopy.type == 6 && !wrappedCopy.parent && !wrappedCopy.auxiliary && trace.guest.contains(reinterpret_cast<void*>(wrappedCopy.payload)),
            "Value wrappers must publish their independent Array handle and type before callback entry failure");
    Array& copy = wrapped ? *reinterpret_cast<Array*>(wrappedCopy.payload) : directCopy;
    std::vector<std::size_t> expected{24, 48, 48, 48}; if (wrapped) expected.insert(expected.begin(), 8); if (budget) expected.push_back(48);
    Require(caught == (budget < 2) && trace.guest.contains(copy.items) && trace.guest.size() == baseline.size() + (budget == 1 ? 5u : 4u) + unsigned(wrapped) &&
            trace.heap.size() == heapBaseline.size() + budget && !trace.notifications && std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected,
            "Interrupted Array construction must publish empty destination storage and retain the temporary list without rollback");
    auto* copyHeader = reinterpret_cast<LegacyArrayHeader*>(copy.items);
    Require(copyHeader->count == (caught ? 0u : 2u) && copyHeader->head && (caught ? copyHeader->head->next == copyHeader->head && copyHeader->head->previous == copyHeader->head : true),
            "Only completed list copying may replace the initial empty destination");
    std::map<void*, std::size_t> orphans;
    if (caught) for (const auto& [pointer, generation] : trace.guestGenerations) {
        if (!baseline.contains(pointer) && pointer != copy.items && pointer != copyHeader->head && (!wrapped || pointer != reinterpret_cast<void*>(wrappedCopy.payload))) {
            Require(trace.guest.at(pointer) == 48, "Only temporary list links become orphaned"); orphans.emplace(pointer, generation);
        }
    }
    Require(orphans.size() == (caught ? budget + 2 : 0), "Failed copies retain the temporary sentinel and admitted child nodes");
    for (const auto& [pointer, generation] : trace.heapGenerations) if (!heapBaseline.contains(pointer)) {
        const auto* node = reinterpret_cast<const LegacyNode*>(pointer);
        Require(!node->nil && node->callback == RecoverLegacy && node->context == &context && node->key->type == 3 && node->key->payload == 9,
                "Completed child callback records must remain independently published even in the orphaned temporary list");
        entries.emplace(node->key, &context);
    }
    CheckMap(trace, entries); trace.heapBudget = -1;
    Require(_ZN3sce4Json5ArrayaSERKS1_(&copy, &array) == &copy && _ZNK3sce4Json5Array4sizeEv(&copy) == 2, "Assignment retry must replace the retained empty destination");
    for (const auto& [pointer, generation] : orphans) Require(trace.guestGenerations.at(pointer) == generation, "Retry must not claim orphaned temporary-list ownership");
    for (const auto& snapshot : snapshots) Require(trace.guestGenerations.at(snapshot.pointer) == snapshot.generation && std::memcmp(snapshot.pointer, snapshot.bytes.data(), snapshot.bytes.size()) == 0,
            "Interrupted copy and retry must preserve source storage bytes and generations");
    Require(std::memcmp(&source, &sourceValue, sizeof(source)) == 0, "Original callback source metadata must remain unchanged");
    const auto* child = _ZNK3sce4Json5Array5frontEv(&copy);
    Require(_ZNK3sce4Json5Value10getBooleanEv(child) == reinterpret_cast<const bool*>(&context.replacement.payload) && context.source == child && !context.parent,
            "Retried children must dispatch through their own callback records");
    if (wrapped) _ZN3sce4Json5ValueD1Ev(&wrappedCopy);
    else _ZN3sce4Json5ArrayD1Ev(&copy);
    _ZN3sce4Json5ArrayD1Ev(&array); _ZN3sce4Json5ValueD1Ev(&source); _ZN3sce4Json5ValueD1Ev(&context.replacement);
    Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.heap.empty() && trace.heapGenerations.empty() && trace.guestGenerations == orphans,
            "Module teardown reclaims callback records but cannot reclaim orphaned caller list nodes");
    for (const auto& [pointer, generation] : orphans) FreeLegacy(nullptr, pointer, &trace);
    Require(trace.guest.empty() && _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&owner, &parameter) == 0 &&
            _ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && trace.guest.empty() && trace.heap.empty(), "Separate fixture reclamation must leave fresh module initialization usable");
    HeapTrace = nullptr;
    return 0;
}

static int CheckObjectHeapExceptions(unsigned budget, unsigned alias, bool wrapped) {
    Require(budget <= 2 && alias < 2, "Object-copy schedules cover both child entries and constructor aliases");
    LegacyTrace trace; HeapTrace = &trace; LegacyAllocator allocator; Parameter parameter{&allocator, &trace, 16384};
    std::uint8_t owner = 0; _ZN3sce4Json11InitializerC1Ev(&owner);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&owner, &parameter) == 0, "Initialize tree-copy exception resources");
    Context context; _ZN3sce4Json5ValueC1Eb(&context.replacement, true); Value source{}, root{};
    ParseLegacy(source); ParseLegacy(root, "{\"a\":9,\"b\":9}");
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &context) == 0, "Prepare source callback records");
    auto* object = reinterpret_cast<Object*>(root.payload);
    auto* sourceHeader = reinterpret_cast<LegacyTreeHeader*>(object->items);
    std::string ignored; std::vector<const Value*> values; CollectionTreeShape(sourceHeader->head->parent, &root, ignored, values);
    std::map<const Value*, Context*> entries{{&source, &context}};
    for (const auto* value : values) {
        _ZN3sce4Json5ValueaSERKS1_(const_cast<Value*>(value), &source); entries.emplace(value, &context);
    }
    CheckMap(trace, entries);
    const auto baseline = trace.guestGenerations, heapBaseline = trace.heapGenerations;
    const Value sourceValue = source, rootValue = root;
    std::vector<LegacySnapshot> snapshots;
    for (const auto& [pointer, generation] : baseline) {
        if (trace.guest.at(pointer) == 24 && trace.heap.contains(*reinterpret_cast<void**>(pointer))) continue;
        const auto* bytes = reinterpret_cast<const unsigned char*>(pointer);
        snapshots.push_back({pointer, generation, {bytes, bytes + trace.guest.at(pointer)}});
    }
    Object direct; std::memset(&direct, 0xa5, sizeof(direct)); Value value; std::memset(&value, 0xa5, sizeof(value));
    const auto requests = trace.requests.size(); trace.heapBudget = static_cast<int>(budget); bool caught = false;
    try {
        if (wrapped) {
            if (alias) _ZN3sce4Json5ValueC2ERKNS0_6ObjectE(&value, object);
            else _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&value, object);
        } else if (alias) _ZN3sce4Json6ObjectC2ERKS1_(&direct, object);
        else _ZN3sce4Json6ObjectC1ERKS1_(&direct, object);
    } catch (const std::bad_alloc&) { caught = true; }
    if (wrapped) Require(value.type == 7 && !value.parent && !value.auxiliary && trace.guest.contains(reinterpret_cast<void*>(value.payload)), "Value constructors must retain the published Object type and independent handle");
    Object& copy = wrapped ? *reinterpret_cast<Object*>(value.payload) : direct;
    std::vector<std::size_t> expected{24, 104, 104, 104, 2, 24, 2};
    if (budget) expected.insert(expected.end(), {104, 2, 24, 2});
    if (wrapped) expected.insert(expected.begin(), 8);
    Require(caught == (budget < 2) && trace.guest.contains(copy.items) && trace.guest.size() == baseline.size() + (budget == 0 ? 7u : budget == 1 ? 11u : 10u) + unsigned(wrapped) &&
            trace.heap.size() == heapBaseline.size() + budget && !trace.notifications && std::vector<std::size_t>(trace.requests.begin() + requests, trace.requests.end()) == expected,
            "Interrupted tree construction must retain the published empty destination and unfinished replacement tree without rollback");
    auto* header = reinterpret_cast<LegacyTreeHeader*>(copy.items);
    Require(header->count == (caught ? 0u : 2u) && header->head && (!caught || header->head->parent == header->head), "Only complete tree copying may publish the replacement root");
    std::map<void*, std::size_t> orphans;
    if (caught) for (const auto& [pointer, generation] : trace.guestGenerations) {
        if (!baseline.contains(pointer) && pointer != copy.items && pointer != header->head && (!wrapped || pointer != reinterpret_cast<void*>(value.payload))) orphans.emplace(pointer, generation);
    }
    Require(orphans.size() == (caught ? budget == 0 ? 5u : 9u : 0u), "Unfinished trees retain their sentinel, nodes and independently allocated internal/public keys");
    for (const auto& [pointer, generation] : trace.heapGenerations) if (!heapBaseline.contains(pointer)) {
        const auto* node = reinterpret_cast<const LegacyNode*>(pointer);
        Require(!node->nil && node->callback == RecoverLegacy && node->context == &context && node->key->type == 3 && node->key->payload == 9,
                "Completed replacement-tree child records remain published after the next entry allocation fails");
        entries.emplace(node->key, &context);
    }
    CheckMap(trace, entries); trace.heapBudget = -1;
    Require(_ZN3sce4Json6ObjectaSERKS1_(&copy, object) == &copy, "Independent assignment retry must publish a complete tree");
    Value view{nullptr, nullptr, reinterpret_cast<std::uintptr_t>(&copy), 0, TypeObject}; std::string actual; std::vector<const Value*> copiedValues;
    CollectionShape(view, nullptr, actual, copiedValues);
    Require(actual == "7:0o2{k=a/q=a/3:0=9;k=b/q=b/3:0=9;};", "Retried trees must preserve both keys, scalar contents and parent links");
    const auto* child = &reinterpret_cast<LegacyTreeHeader*>(copy.items)->head->left->pair.value;
    Require(_ZNK3sce4Json5Value10getBooleanEv(child) == reinterpret_cast<const bool*>(&context.replacement.payload) && context.source == child && !context.parent,
            "Retried tree entries must dispatch through their own callback records");
    for (const auto& [pointer, generation] : orphans) Require(trace.guestGenerations.at(pointer) == generation, "Retry must not reclaim the orphaned replacement tree");
    for (const auto& snapshot : snapshots) Require(trace.guestGenerations.at(snapshot.pointer) == snapshot.generation && std::memcmp(snapshot.pointer, snapshot.bytes.data(), snapshot.bytes.size()) == 0,
            "Failed tree copies and retry must preserve every original allocation and source byte");
    Require(std::memcmp(&source, &sourceValue, sizeof(source)) == 0 && std::memcmp(&root, &rootValue, sizeof(root)) == 0, "Both original Values must retain their metadata ownership");
    if (wrapped) _ZN3sce4Json5ValueD1Ev(&value); else _ZN3sce4Json6ObjectD1Ev(&copy);
    _ZN3sce4Json5ValueD1Ev(&root); _ZN3sce4Json5ValueD1Ev(&source); _ZN3sce4Json5ValueD1Ev(&context.replacement);
    Require(_ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && !owner && trace.heap.empty() && trace.heapGenerations.empty() && trace.guestGenerations == orphans,
            "Module teardown reclaims all callback records while leaving the orphaned caller tree intact");
    for (const auto& [pointer, generation] : orphans) FreeLegacy(nullptr, pointer, &trace);
    Require(trace.guest.empty() && _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&owner, &parameter) == 0 && _ZN3sce4Json11Initializer9terminateEv(&owner) == 0 && trace.guest.empty() && trace.heap.empty(),
            "Separate orphan reclamation must preserve fresh module initialization and cleanup");
    HeapTrace = nullptr;
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 5 && std::strcmp(argv[1], "object-heap") == 0) return CheckObjectHeapExceptions(static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)), static_cast<unsigned>(std::strtoul(argv[3], nullptr, 10)), std::strcmp(argv[4], "wrapped") == 0);
    if (argc == 4 && std::strcmp(argv[1], "value-array-heap") == 0) return CheckArrayHeapExceptions(static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)), static_cast<unsigned>(std::strtoul(argv[3], nullptr, 10)), true);
    if (argc == 4 && std::strcmp(argv[1], "array-heap") == 0) return CheckArrayHeapExceptions(static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)), static_cast<unsigned>(std::strtoul(argv[3], nullptr, 10)));
    if (argc == 3) return CheckCopyHeapExceptions(static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)), static_cast<unsigned>(std::strtoul(argv[2], nullptr, 10)));
    if (argc == 2) return CheckHeapExceptions(static_cast<unsigned>(std::strtoul(argv[1], nullptr, 10)));
    LegacyTrace trace; HeapTrace = &trace; LegacyAllocator allocator;
    Parameter parameter{&allocator, &trace, 16384};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(&initializer, &parameter) == 0 && trace.guest.size() == 11, "Initialize legacy allocation and shared mutex resources");
    const auto baseline = trace.guest.size();
    Context first, second; _ZN3sce4Json5ValueC1Eb(&first.replacement, true); _ZN3sce4Json5ValueC1Eb(&second.replacement, false);
    Value source{}; _ZN3sce4Json5ValueC1El(&source, 9);
    Require(static_cast<unsigned>(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &first)) == 0x80848104 && trace.guest.size() == baseline && trace.heap.empty(), "Legacy mismatch registration must reject a root without auxiliary metadata");
    _ZN3sce4Json5ValueD1Ev(&source); ParseLegacy(source);
    const auto before = trace.guest.size(); trace.budget = 0;
    Require(static_cast<unsigned>(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &first)) == 0x80848102 && trace.notifications == 1 && trace.guest.size() == before && trace.heap.empty(), "Failed map admission must retain the root and permit retry");
    trace.budget = -1;
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &first) == 0, "Register legacy mismatch after failure");
    CheckMap(trace, {{&source, &first}});
    Require(*_ZNK3sce4Json5Value10getBooleanEv(&source) && first.source == &source && !first.parent, "Legacy getters must dispatch the registered borrowed replacement");
    const auto guest = trace.guest.size(); const auto heap = trace.heap.size();
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &second) == 0 && !_ZNK3sce4Json5ValuecvbEv(&source) && second.calls == 1 && trace.guest.size() == guest && trace.heap.size() == heap, "Legacy callback replacement must reuse the map entry and govern boolean conversion");
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, nullptr, nullptr) == 0 && !*_ZNK3sce4Json5Value10getBooleanEv(&source), "A null legacy mismatch callback must disable dispatch without removing the entry");
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&source, RecoverLegacy, &first) == 0, "Restore legacy callback");
    Value copy{}; _ZN3sce4Json5ValueC1ERKS1_(&copy, &source);
    Require(!copy.auxiliary && *_ZNK3sce4Json5Value10getBooleanEv(&copy) && first.source == &copy, "Legacy value copies must propagate map callbacks independently of auxiliary ownership");
    Value assigned{}; ParseLegacy(assigned);
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&assigned, RecoverLegacy, &second) == 0, "Prepare legacy assignment callback");
    auto* auxiliary = assigned.auxiliary;
    _ZN3sce4Json5ValueaSERKS1_(&assigned, &source);
    Require(assigned.auxiliary == auxiliary && *_ZNK3sce4Json5Value10getBooleanEv(&assigned) && first.source == &assigned, "Legacy assignment must copy the source map record while retaining destination metadata");
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&assigned, RecoverLegacy, &second) == 0, "Prepare distinct swap callbacks");
    _ZN3sce4Json5Value4swapERS1_(&source, &assigned);
    Require(!*_ZNK3sce4Json5Value10getBooleanEv(&source) && *_ZNK3sce4Json5Value10getBooleanEv(&assigned) && assigned.auxiliary == auxiliary, "Legacy swap must exchange callback records and retain destination auxiliary ownership");
    CheckMap(trace, {{&source, &second}, {&copy, &first}, {&assigned, &first}});
    _ZN3sce4Json5Value4swapERS1_(&source, &assigned);
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&assigned, RecoverLegacy, &first) == 0, "Restore assignment callback after swap");
    Array array{}; _ZN3sce4Json5ArrayC1Ev(&array);
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &source);
    const auto* element = _ZNK3sce4Json5Array5frontEv(&array);
    Require(*_ZNK3sce4Json5Value10getBooleanEv(element) && first.source == element && !first.parent, "Legacy array append must copy the handler map entry without binding a modern parent");
    CheckMap(trace, {{&source, &first}, {&copy, &first}, {&assigned, &first}, {element, &first}});
    trace.budget = 0;
    _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &source);
    trace.budget = -1;
    Require(_ZNK3sce4Json5Array4sizeEv(&array) == 0, "A failed legacy append must remove the existing tail and its handler record");
    CheckMap(trace, {{&source, &first}, {&copy, &first}, {&assigned, &first}});
    _ZN3sce4Json5ArrayD1Ev(&array);
    Value parsed{}; ParseLegacy(parsed, "[9]");
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&parsed, RecoverLegacy, &first) == 0, "Register legacy parsed-container recovery");
    const auto* child = _ZNK3sce4Json5Array5frontEv(reinterpret_cast<const Array*>(parsed.payload));
    Require(*_ZNK3sce4Json5Value10getBooleanEv(child) && first.source == child && first.parent == &parsed, "Legacy descendant mismatch recovery must use the root map and immediate parent");
    _ZN3sce4Json5ValueD1Ev(&parsed);
    Value null{}; ParseLegacy(null, "null"); NullContext nullContext{&source, &first};
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&null, RecoverLegacyNull, &nullContext) == 0 && *_ZNK3sce4Json5Value10getBooleanEv(&null), "Legacy local null recovery must preserve its pre-guard dispatch");
    _ZN3sce4Json5ValueD1Ev(&null); _ZN3sce4Json5ValueD1Ev(&assigned); _ZN3sce4Json5ValueD1Ev(&copy); _ZN3sce4Json5ValueD1Ev(&source);
    CheckMap(trace, {});
    CheckPairCopies(trace, first);
    CheckCollectionCopies(trace, first);
    CheckInheritedMap(trace, first, second);
    std::array<Value, 32> roots{}; std::map<const Value*, Context*> entries;
    for (unsigned index = 0; index < roots.size(); ++index) {
        auto& root = roots[index * 13 % roots.size()]; ParseLegacy(root);
        Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&root, RecoverLegacy, &first) == 0, "Register legacy tree entry");
        entries.emplace(&root, &first); CheckMap(trace, entries);
    }
    for (unsigned index = 0; index < roots.size(); ++index) {
        auto& root = roots[index * 7 % roots.size()];
        _ZN3sce4Json5ValueD1Ev(&root); entries.erase(&root); CheckMap(trace, entries);
    }
    _ZN3sce4Json5ValueD1Ev(&second.replacement); _ZN3sce4Json5ValueD1Ev(&first.replacement);
    Require(trace.guest.size() == baseline + 1 && trace.heap.size() == 1, "Empty legacy map storage must remain owned until termination");
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.guest.empty() && trace.heap.empty() && trace.guestGenerations.empty() && trace.heapGenerations.empty(), "Termination must release both allocator domains and all legacy map resources");
    HeapTrace = nullptr;
    return 0;
}
