#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* storage; };
struct Object { void* storage; };
struct Iterator { void* state; };
struct Pair { String key; std::uint64_t reserved; Value value; };
struct StoredText { const char* characters; std::uint32_t length; std::uint32_t capacity; std::uint64_t reserved; };
struct TreeNode { TreeNode* parent; TreeNode* left; TreeNode* right; StoredText key; Pair pair; std::uint8_t color; std::uint8_t nil; std::uint8_t reserved[6]; };
struct TreeHeader { TreeNode* head; std::size_t count; std::uint64_t reserved; Value* owner; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
static_assert(sizeof(TreeNode) == 104 && offsetof(TreeNode, pair) == 48 && offsetof(TreeNode, color) == 96);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5Value3setEl(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6StringE(Value*, const String*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(Value*, const Object*);
std::size_t APS5_VABI _ZNK3sce4Json5Value5countEv(const Value*);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value*, String*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEm(const Value*, std::size_t);
void APS5_VABI _ZN3sce4Json6ObjectC1Ev(Object*);
void APS5_VABI _ZN3sce4Json6ObjectC1ERKS1_(Object*, const Object*);
void APS5_VABI _ZN3sce4Json6ObjectD1Ev(Object*);
Object* APS5_VABI _ZN3sce4Json6ObjectaSERKS1_(Object*, const Object*);
void APS5_VABI _ZN3sce4Json6Object5clearEv(Object*);
std::size_t APS5_VABI _ZNK3sce4Json6Object4sizeEv(const Object*);
Value* APS5_VABI _ZN3sce4Json6ObjectixERKNS0_6StringE(Object*, const String*);
void APS5_VABI _ZN3sce4Json6Object5eraseERKNS0_6StringE(Object*, const String*);
Iterator* APS5_VABI _ZNK3sce4Json6Object5beginEv(Iterator*, const Object*);
Iterator* APS5_VABI _ZN3sce4Json6Object6insertERKNS1_4PairE(Iterator*, Object*, const Pair*);
void APS5_VABI _ZN3sce4Json6Object8iteratorC1Ev(Iterator*);
Iterator* APS5_VABI _ZN3sce4Json6Object8iteratoraSERKS2_(Iterator*, const Iterator*);
Iterator* APS5_VABI _ZNK3sce4Json6Object3endEv(Iterator*, const Object*);
Iterator* APS5_VABI _ZNK3sce4Json6Object4findERKNS0_6StringE(Iterator*, const Object*, const String*);
const Pair* APS5_VABI _ZNK3sce4Json6Object8iteratordeEv(const Iterator*);
Iterator* APS5_VABI _ZN3sce4Json6Object8iteratorppEv(Iterator*);
void APS5_VABI _ZN3sce4Json6Object8iteratorC1ERKS2_(Iterator*, const Iterator*);
void APS5_VABI _ZN3sce4Json6Object8iteratorD1Ev(Iterator*);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringC1Ev(String*);
String* APS5_VABI _ZN3sce4Json6String6appendEPKcm(String*, const char*, std::size_t);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct ObjectAllocationTrace {
    std::map<void*, std::size_t> live;
    std::vector<std::size_t> requests;
    std::set<void*> headers;
    int budget = -1;
    bool failOnce = false;
    unsigned notifications = 0;
};
static void* APS5_VABI AllocateObject(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ObjectAllocationTrace*>(context); trace.requests.push_back(bytes);
    if (trace.budget == 0) { if (trace.failOnce) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.live.emplace(memory, bytes); return memory;
}
static void APS5_VABI FreeObject(void*, void* memory, void* context) {
    if (!memory) return;
    auto& trace = *static_cast<ObjectAllocationTrace*>(context);
    if (trace.headers.erase(memory)) {
        const auto* words = static_cast<const std::uint64_t*>(memory);
        Require(words[0] == 0xffffffff && words[1] == UINT64_MAX, "Object teardown must poison the header before freeing it");
    }
    Require(trace.live.erase(memory) == 1, "Object resources must return through their owning allocator"); std::free(memory);
}
static void APS5_VABI NotifyObject(void*, int error, std::size_t bytes, void* context) {
    auto& trace = *static_cast<ObjectAllocationTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == bytes, "Object notification must identify the failed allocation"); ++trace.notifications;
}
static void* const ObjectVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateObject), reinterpret_cast<void*>(FreeObject), reinterpret_cast<void*>(NotifyObject)};
struct ObjectAllocator { void* const* vtable = ObjectVtable; };
static bool Owned(const ObjectAllocationTrace& trace, const void* memory, std::size_t bytes) {
    const auto found = trace.live.find(const_cast<void*>(memory)); return found != trace.live.end() && found->second == bytes;
}
static Value* Entry(Object& object, const char* key) {
    String text{}; _ZN3sce4Json6StringC1EPKc(&text, key);
    auto* result = _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &text);
    _ZN3sce4Json6StringD1Ev(&text); return result;
}
static void CheckTree(const Object& object, ObjectAllocationTrace& trace, const std::map<std::string, Value*>& entries) {
    const auto* header = static_cast<const TreeHeader*>(object.storage);
    Require(Owned(trace, header, 32) && Owned(trace, header->head, 104) && !header->owner && header->count == entries.size(), "Object header and sentinel must be actual allocator-owned resources");
    trace.headers.insert(object.storage);
    auto* head = header->head;
    Require(head->nil == 1 && head->color == 0, "Sentinel flags must mark a black nil node");
    std::vector<TreeNode*> nodes;
    std::function<unsigned(TreeNode*, TreeNode*)> visit = [&](TreeNode* node, TreeNode* parent) -> unsigned {
        if (node == head) return 1;
        Require(Owned(trace, node, 104) && !node->nil && node->color <= 1 && node->parent == parent, "Every tree node must retain its native allocation and parent link");
        Require(node->color == 0 || (node->left->color == 0 && node->right->color == 0), "Red nodes must have black children");
        const auto left = visit(node->left, node); nodes.push_back(node); const auto right = visit(node->right, node);
        Require(left == right, "Every path must retain the same black height");
        Require(Owned(trace, node->pair.key.storage, 24) && !node->pair.value.parent && !node->pair.value.auxiliary, "Node pairs must expose native string and value ownership");
        const auto found = entries.find(node->key.characters);
        Require(found != entries.end() && found->second == &node->pair.value && std::strcmp(node->key.characters, _ZNK3sce4Json6String5c_strEv(&node->pair.key)) == 0, "Rotations must preserve entry identity and both key representations");
        return left + (node->color == 0);
    };
    Require(head->parent->color == 0, "Tree root must remain black"); visit(head->parent, head);
    Require(nodes.size() == entries.size() && (nodes.empty() ? head->left == head && head->right == head : head->left == nodes.front() && head->right == nodes.back()), "Sentinel endpoints must match the ordered entries");
    auto expected = entries.begin();
    for (const auto* node : nodes) { Require(expected->first == node->key.characters, "Tree iteration must preserve key order"); ++expected; }
}
static std::vector<std::string> Keys(const Object& object) {
    Iterator current{}, end{}; _ZNK3sce4Json6Object5beginEv(&current, &object); _ZNK3sce4Json6Object3endEv(&end, &object);
    std::vector<std::string> keys;
    while (*static_cast<void**>(current.state) != *static_cast<void**>(end.state)) {
        keys.emplace_back(_ZNK3sce4Json6String5c_strEv(&_ZNK3sce4Json6Object8iteratordeEv(&current)->key));
        _ZN3sce4Json6Object8iteratorppEv(&current);
    }
    _ZN3sce4Json6Object8iteratorD1Ev(&current); _ZN3sce4Json6Object8iteratorD1Ev(&end); return keys;
}
static void CheckSerialized(Value& value, const char* expected) {
    String output{}; _ZN3sce4Json6StringC1Ev(&output);
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0 && std::strcmp(_ZNK3sce4Json6String5c_strEv(&output), expected) == 0,
            "Object keys and string values must retain their distinct binary serialization behavior");
    _ZN3sce4Json6StringD1Ev(&output);
}
static void CheckBinaryKeys() {
    String key{}; _ZN3sce4Json6StringC1EPKc(&key, "a");
    const char suffix[]{0, 'b'}; _ZN3sce4Json6String6appendEPKcm(&key, suffix, sizeof(suffix));
    Value value{}; _ZN3sce4Json5ValueC1ERKNS0_6StringE(&value, &key);
    CheckSerialized(value, R"("a\u0000b")"); _ZN3sce4Json5ValueD1Ev(&value);
    Object object{}; _ZN3sce4Json6ObjectC1Ev(&object); _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &key);
    _ZN3sce4Json5ValueC1ERKNS0_6ObjectE(&value, &object); CheckSerialized(value, R"({"a":null})");
    _ZN3sce4Json5ValueD1Ev(&value); _ZN3sce4Json6ObjectD1Ev(&object); _ZN3sce4Json6StringD1Ev(&key);
    _ZN3sce4Json5ValueC1Ev(&value);
    const char input[] = R"({"a":1,"a\u0000suffix":2})";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, input, sizeof(input) - 1) == 0 && _ZNK3sce4Json5Value5countEv(&value) == 2,
            "The inspected parser drops the decoded zero byte without treating the two keys as duplicates");
    CheckSerialized(value, R"({"a":1,"asuffix":2})"); _ZN3sce4Json5ValueD1Ev(&value);
    _ZN3sce4Json5ValueC1Ev(&value);
    const char string[] = R"("a\u0000b")";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value, string, sizeof(string) - 1) == 0, "Parse a zero-byte escape in a string value");
    CheckSerialized(value, R"("ab")"); _ZN3sce4Json5ValueD1Ev(&value);
}
static void CheckFailures(ObjectAllocationTrace& trace, const Value* moduleFallback) {
    const auto baseline = trace.live.size();
    for (bool once : {false, true}) for (int budget = 0; budget <= 8; ++budget) {
        Object object{}; _ZN3sce4Json6ObjectC1Ev(&object); Entry(object, "a"); Entry(object, "c");
        trace.headers.insert(object.storage);
        String text{}; _ZN3sce4Json6StringC1EPKc(&text, "b");
        trace.budget = budget; trace.failOnce = once;
        const auto notifications = trace.notifications;
        auto* result = _ZN3sce4Json6ObjectixERKNS0_6StringE(&object, &text);
        trace.budget = -1;
        const bool inserted = once ? budget != 4 : budget >= 5;
        const auto expected = inserted ? std::size_t{3} : std::size_t{2};
        Require(_ZNK3sce4Json6Object4sizeEv(&object) == expected, "Failed object insertion must retain the observed partial entry count");
        const bool fallback = budget < 7;
        Require((result == moduleFallback) == fallback, "Insertion must preserve its operation-level versus late-assignment failure result");
        std::vector<std::string> expectedKeys{"a", "c"};
        if (inserted) {
            if (once && (budget == 0 || budget == 2)) expectedKeys = {"", "a", "c"};
            else expectedKeys = {"a", budget == 8 ? "b" : "", "c"};
        }
        Require(Keys(object) == expectedKeys, "Partial allocation failures must preserve the observed public keys and ordering");
        static constexpr unsigned persistentNotifications[]{3, 3, 3, 2, 1, 2, 1, 1, 0};
        Require(trace.notifications - notifications == (once ? static_cast<unsigned>(budget < 8) : persistentNotifications[budget]), "Object failures must notify for each real failed request");
        _ZN3sce4Json6StringD1Ev(&text); _ZN3sce4Json6ObjectD1Ev(&object);
        Require(trace.live.size() == baseline, "All partial insertion states must be destructible without leaks");
    }
    trace.failOnce = false;
}
static std::string AssignmentContents(const Object& object) {
    std::string result;
    auto* head = static_cast<const TreeHeader*>(object.storage)->head;
    auto* node = head->left;
    while (node != head) {
        result += node->pair.key.storage ? static_cast<const StoredText*>(node->pair.key.storage)->characters : "~";
        result += ":" + std::to_string(node->pair.value.type) + ":" + std::to_string(node->pair.value.payload) + ";";
        if (!node->right->nil) { node = node->right; while (!node->left->nil) node = node->left; }
        else { auto* parent = node->parent; while (!parent->nil && node == parent->right) { node = parent; parent = parent->parent; } node = parent; }
    }
    return result;
}
static void CheckInsertions(ObjectAllocationTrace& trace) {
    struct Case { bool duplicate; bool once; int budget; std::size_t count; unsigned notifications; bool state; const char* result; const char* contents; };
    const Case cases[]{
        {0, 0, -1, 3, 0, 1, "b", "a:2:11;b:2:22;c:2:33;"},
        {0, 0, 0, 2, 4, 0, "NONE", "a:2:11;c:2:33;"},
        {0, 0, 1, 2, 3, 1, "END", "a:2:11;c:2:33;"},
        {0, 0, 2, 2, 3, 1, "END", "a:2:11;c:2:33;"},
        {0, 0, 3, 2, 4, 1, "END", "a:2:11;c:2:33;"},
        {0, 0, 4, 2, 3, 1, "END", "a:2:11;c:2:33;"},
        {0, 0, 5, 2, 2, 1, "END", "a:2:11;c:2:33;"},
        {0, 0, 6, 2, 2, 1, "END", "a:2:11;c:2:33;"},
        {0, 0, 7, 2, 1, 1, "END", "a:2:11;c:2:33;"},
        {0, 0, 8, 3, 2, 1, "NULL", "a:2:11;~:0:11936128518282651045;c:2:33;"},
        {0, 0, 9, 3, 1, 1, "NULL", "a:2:11;~:0:11936128518282651045;c:2:33;"},
        {0, 0, 10, 3, 1, 1, "", "a:2:11;:0:11936128518282651045;c:2:33;"},
        {0, 0, 11, 3, 0, 1, "b", "a:2:11;b:2:22;c:2:33;"},
        {0, 0, 12, 3, 0, 1, "b", "a:2:11;b:2:22;c:2:33;"},
        {0, 1, -1, 3, 0, 1, "b", "a:2:11;b:2:22;c:2:33;"},
        {0, 1, 0, 3, 1, 0, "NONE", "a:2:11;b:2:22;c:2:33;"},
        {0, 1, 1, 3, 1, 1, "b", "b:0:11936128518282651045;a:2:11;c:2:33;"},
        {0, 1, 2, 3, 1, 1, "NULL", "a:2:11;~:0:11936128518282651045;c:2:33;"},
        {0, 1, 3, 3, 1, 1, "", "a:2:11;:0:11936128518282651045;c:2:33;"},
        {0, 1, 4, 3, 1, 1, "b", "b:0:11936128518282651045;a:2:11;c:2:33;"},
        {0, 1, 5, 3, 1, 1, "NULL", "a:2:11;~:0:11936128518282651045;c:2:33;"},
        {0, 1, 6, 3, 1, 1, "", "a:2:11;:0:11936128518282651045;c:2:33;"},
        {0, 1, 7, 2, 1, 1, "END", "a:2:11;c:2:33;"},
        {0, 1, 8, 3, 1, 1, "b", "a:2:11;b:0:11936128518282651045;c:2:33;"},
        {0, 1, 9, 3, 1, 1, "NULL", "a:2:11;~:0:11936128518282651045;c:2:33;"},
        {0, 1, 10, 3, 1, 1, "", "a:2:11;:0:11936128518282651045;c:2:33;"},
        {0, 1, 11, 3, 0, 1, "b", "a:2:11;b:2:22;c:2:33;"},
        {0, 1, 12, 3, 0, 1, "b", "a:2:11;b:2:22;c:2:33;"},
        {1, 0, -1, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 0, 0, 2, 4, 0, "NONE", "a:2:11;c:2:33;"},
        {1, 0, 1, 2, 3, 1, "END", "a:2:11;c:2:33;"},
        {1, 0, 2, 2, 3, 1, "END", "a:2:11;c:2:33;"},
        {1, 0, 3, 2, 4, 1, "END", "a:2:11;c:2:33;"},
        {1, 0, 4, 2, 3, 1, "END", "a:2:11;c:2:33;"},
        {1, 0, 5, 2, 1, 1, "a", "a:2:11;c:2:33;"},
        {1, 0, 6, 2, 1, 1, "a", "a:2:11;c:2:33;"},
        {1, 0, 7, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 0, 8, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 0, 9, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 0, 10, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 0, 11, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 0, 12, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, -1, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, 0, 2, 1, 0, "NONE", "a:2:11;c:2:33;"},
        {1, 1, 1, 3, 1, 1, "a", "a:0:11936128518282651045;a:2:11;c:2:33;"},
        {1, 1, 2, 2, 1, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, 3, 2, 1, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, 4, 3, 1, 1, "a", "a:0:11936128518282651045;a:2:11;c:2:33;"},
        {1, 1, 5, 2, 1, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, 6, 2, 1, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, 7, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, 8, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, 9, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, 10, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, 11, 2, 0, 1, "a", "a:2:11;c:2:33;"},
        {1, 1, 12, 2, 0, 1, "a", "a:2:11;c:2:33;"},
    };
    const auto baseline = trace.live.size();
    for (const auto& test : cases) {
        Object object{}; _ZN3sce4Json6ObjectC1Ev(&object);
        auto* first = Entry(object, "a"); _ZN3sce4Json5Value3setEl(first, 11);
        _ZN3sce4Json5Value3setEl(Entry(object, "c"), 33);
        Pair pair{}; _ZN3sce4Json6StringC1EPKc(&pair.key, test.duplicate ? "a" : "b");
        _ZN3sce4Json5ValueC1Ev(&pair.value); _ZN3sce4Json5Value3setEl(&pair.value, 22);
        const auto notifications = trace.notifications;
        trace.budget = test.budget; trace.failOnce = test.once;
        Iterator result{};
        Require(_ZN3sce4Json6Object6insertERKNS1_4PairE(&result, &object, &pair) == &result, "Insertion must return its result iterator");
        trace.budget = -1; trace.failOnce = false;
        Require((result.state != nullptr) == test.state, "Insertion must preserve independent iterator allocation failure");
        auto* node = result.state ? *static_cast<TreeNode**>(result.state) : nullptr;
        const char* key = !node ? "NONE" : node->nil ? "END" : node->pair.key.storage ? static_cast<const StoredText*>(node->pair.key.storage)->characters : "NULL";
        Require(std::strcmp(key, test.result) == 0, "Insertion must return the reference entry or end node on failure");
        Require(_ZNK3sce4Json6Object4sizeEv(&object) == test.count && AssignmentContents(object) == test.contents, "Insertion must preserve partial keys, inactive payloads and duplicate values");
        Require(trace.notifications - notifications == test.notifications, "Insertion must notify every failed owned allocation");
        Require(first->type == 2 && first->payload == 11 && pair.value.type == 2 && pair.value.payload == 22, "Insertion must preserve existing entries and its input value");
        _ZN3sce4Json6Object8iteratorD1Ev(&result); _ZN3sce4Json6StringD1Ev(&pair.key); _ZN3sce4Json5ValueD1Ev(&pair.value); _ZN3sce4Json6ObjectD1Ev(&object);
        Require(trace.live.size() == baseline, "Insertion failure must release temporary allocations and iterator state");
    }
}
static void CheckIteratorFailures(ObjectAllocationTrace& trace) {
    const auto baseline = trace.live.size();
    Object object{}; _ZN3sce4Json6ObjectC1Ev(&object);
    Entry(object, "a");
    String key{}; _ZN3sce4Json6StringC1EPKc(&key, "a");
    Iterator source{}; _ZNK3sce4Json6Object5beginEv(&source, &object);
    const auto retained = trace.live.size();
    for (unsigned operation = 0; operation < 5; ++operation) {
        const auto requests = trace.requests.size();
        const auto notifications = trace.notifications;
        Iterator failed{}; trace.budget = 0;
        switch (operation) {
            case 0: _ZN3sce4Json6Object8iteratorC1Ev(&failed); break;
            case 1: _ZN3sce4Json6Object8iteratorC1ERKS2_(&failed, &source); break;
            case 2: _ZNK3sce4Json6Object5beginEv(&failed, &object); break;
            case 3: _ZNK3sce4Json6Object3endEv(&failed, &object); break;
            case 4: _ZNK3sce4Json6Object4findERKNS0_6StringE(&failed, &object, &key); break;
        }
        trace.budget = -1;
        Require(!failed.state && trace.live.size() == retained, "Failed iterator allocation must retain no state or borrowed resources");
        Require(trace.requests.size() == requests + 1 && trace.requests.back() == 8 && trace.notifications == notifications + 1, "Iterator failure must report its actual eight-byte allocation");
        _ZN3sce4Json6Object8iteratorD1Ev(&failed);
    }
    Iterator absent{}, copy{}, target{}, end{};
    const auto requests = trace.requests.size();
    trace.budget = 0;
    _ZN3sce4Json6Object8iteratorC1ERKS2_(&copy, &absent);
    Require(_ZN3sce4Json6Object8iteratoraSERKS2_(&absent, &source) == &absent && !absent.state && !copy.state && trace.requests.size() == requests, "Copying absent state and assigning into absent state must not allocate");
    trace.budget = -1;
    _ZN3sce4Json6Object8iteratorC1ERKS2_(&target, &source);
    _ZNK3sce4Json6Object3endEv(&end, &object);
    auto* storage = target.state;
    trace.budget = 0;
    _ZN3sce4Json6Object8iteratoraSERKS2_(&target, &end);
    Require(target.state == storage && *static_cast<void**>(target.state) == static_cast<TreeHeader*>(object.storage)->head, "Assignment from end must preserve owned iterator state and copy the sentinel");
    _ZN3sce4Json6Object8iteratoraSERKS2_(&target, &absent);
    Require(!target.state && trace.live.size() == retained + 1, "Assignment from absent state must release the destination iterator state");
    trace.budget = -1;
    _ZN3sce4Json6Object8iteratorD1Ev(&end); _ZN3sce4Json6Object8iteratorD1Ev(&target);
    Object missing{};
    _ZNK3sce4Json6Object5beginEv(&target, &missing);
    Require(Owned(trace, target.state, 8) && !*static_cast<void**>(target.state), "A missing object must produce an allocated iterator with no position");
    _ZN3sce4Json6Object8iteratorD1Ev(&target);
    _ZN3sce4Json6Object8iteratorD1Ev(&source); _ZN3sce4Json6StringD1Ev(&key); _ZN3sce4Json6ObjectD1Ev(&object);
    Require(trace.live.size() == baseline, "Iterator failure and assignment paths must release all owned state");
}
static void CheckNestedAssignment(ObjectAllocationTrace& trace) {
    const auto baseline = trace.live.size();
    Value source{}, destination{};
    _ZN3sce4Json5ValueC1Ev(&source); _ZN3sce4Json5ValueC1Ev(&destination);
    const char input[] = "{\"nested\":{\"x\":1},\"array\":[2],\"text\":\"abc\",\"bool\":true,\"real\":1.25,\"nil\":null}";
    const char previous[] = "{\"old\":0}";
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&source, input, sizeof(input) - 1) == 0, "Parse nested assignment source");
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&destination, previous, sizeof(previous) - 1) == 0, "Parse nested assignment destination");
    auto* target = reinterpret_cast<Object*>(destination.payload);
    _ZN3sce4Json6ObjectaSERKS1_(target, reinterpret_cast<const Object*>(source.payload));
    String expected{}, actual{};
    _ZN3sce4Json6StringC1Ev(&expected); _ZN3sce4Json6StringC1Ev(&actual);
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&source, &expected) == 0 && _ZN3sce4Json5Value9serializeERNS0_6StringE(&destination, &actual) == 0, "Serialize nested assignment results");
    Require(std::strcmp(_ZNK3sce4Json6String5c_strEv(&expected), _ZNK3sce4Json6String5c_strEv(&actual)) == 0, "Assignment must preserve nested collections, strings and scalar types");
    auto* header = static_cast<TreeHeader*>(target->storage);
    Require(header->owner == &destination, "Assigned root object must retain its owner");
    Iterator iterator{}; _ZNK3sce4Json6Object5beginEv(&iterator, target);
    for (std::size_t index = 0; index < header->count; ++index) {
        const auto* pair = _ZNK3sce4Json6Object8iteratordeEv(&iterator);
        Require(pair->value.parent == &destination, "Assigned entries must bind to the destination value");
        if (pair->value.type == 6 || pair->value.type == 7) {
            auto* nestedStorage = *reinterpret_cast<void* const*>(pair->value.payload);
            Value* owner = nullptr; std::memcpy(&owner, static_cast<const char*>(nestedStorage) + 24, sizeof(owner));
            Require(owner == &pair->value, "Nested collection ownership must bind to the assigned entry");
        }
        _ZN3sce4Json6Object8iteratorppEv(&iterator);
    }
    _ZN3sce4Json6Object8iteratorD1Ev(&iterator);
    _ZN3sce4Json6StringD1Ev(&actual); _ZN3sce4Json6StringD1Ev(&expected);
    _ZN3sce4Json5ValueD1Ev(&destination); _ZN3sce4Json5ValueD1Ev(&source);
    Require(trace.live.size() == baseline, "Nested assignment must release all resources");
}
static std::string NestedObjectFailureState(const Value& value, const Value* parent = nullptr) {
    std::string state = std::to_string(value.type) + (value.parent == parent ? "p" : value.parent ? "x" : "-");
    if (value.type == 0) return state;
    if (value.type < 5) return state + ":" + std::to_string(value.payload);
    if (!value.payload) return state + "!";
    const auto* header = *reinterpret_cast<void* const*>(value.payload);
    if (!header) return state + "?";
    if (value.type == 5) {
        const auto* text = static_cast<const StoredText*>(header);
        return state + ":" + std::string(text->characters, text->length);
    }
    const auto* words = static_cast<const std::uintptr_t*>(header);
    state += words[3] == reinterpret_cast<std::uintptr_t>(&value) ? "o" : "!";
    state += std::to_string(words[1]) + "[";
    if (!words[0]) return state + "?]";
    if (value.type == 6) {
        const auto* head = reinterpret_cast<const std::uintptr_t*>(words[0]);
        const auto* node = reinterpret_cast<const std::uintptr_t*>(head[1]);
        unsigned steps = 0;
        while (node != head) {
            Require(++steps < 32, "Nested test array links must reach their sentinel");
            state += NestedObjectFailureState(*reinterpret_cast<const Value*>(node + 2), &value) + ";";
            node = reinterpret_cast<const std::uintptr_t*>(node[1]);
        }
    } else {
        Require(value.type == 7, "Nested test collection must be an array or object");
        const auto* head = reinterpret_cast<const TreeNode*>(words[0]);
        unsigned steps = 0;
        std::function<void(const TreeNode*)> visit = [&](const TreeNode* node) {
            if (node == head) return;
            Require(node && ++steps < 32, "Nested test tree links must reach their sentinel");
            visit(node->left);
            state += node->pair.key.storage ? _ZNK3sce4Json6String5c_strEv(&node->pair.key) : "~";
            state += "=" + NestedObjectFailureState(node->pair.value, &value) + ";";
            visit(node->right);
        };
        visit(head->parent);
    }
    return state + "]";
}
static void CheckNestedFailureCases(ObjectAllocationTrace& trace) {
    static const char* const states[]{
        "7po1[z=3p:99;]",
        "7po0[]",
        "7po1[~=0-;]",
        "7po1[=0-;]",
        "7po1[a=0-;]",
        "7po1[a=5p:;]",
        "7po1[a=5p:abc;]",
        "7po2[a=5p:abc;~=0-;]",
        "7po2[a=5p:abc;=0-;]",
        "7po2[a=5p:abc;b=0-;]",
        "7po2[a=5p:abc;b=7-o0[];]",
        "7po2[a=5p:abc;b=7po0[];]",
        "7po2[a=5p:abc;b=7po1[~=0-;];]",
        "7po2[a=5p:abc;b=7po1[=0-;];]",
        "7po2[a=5p:abc;b=7po1[x=0-;];]",
        "7po2[a=5p:abc;b=7po1[x=5p:;];]",
        "7po2[a=5p:abc;b=7po1[x=5p:def;];]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];~=0-;]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];=0-;]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];c=0-;]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];c=6-o0[?];]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];c=6-o0[];]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];c=6po0[];]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po3[~=5p:abc;b=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po3[=5p:abc;b=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po3[a=5-:;b=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po3[~=5-:;b=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po3[=5-:;b=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po3[a=0-;b=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po2[b=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po3[a=5p:;b=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;~=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;=7po1[x=5p:def;];c=6po1[3p:42;];]",
        "7po3[b=7-o0[];a=5p:abc;c=6po1[3p:42;];]",
        "7po3[a=5p:abc;~=7-o0[];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;=7-o0[];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=0-;c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7-o0[];c=6po1[3p:42;];]",
        "7po2[a=5p:abc;c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7po0[];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7po1[~=5p:def;];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7po1[=5p:def;];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7po1[x=5-:;];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7po1[~=5-:;];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7po1[=5-:;];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7po1[x=0-;];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7po1[x=5p:;];c=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];~=6po1[3p:42;];]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];=6po1[3p:42;];]",
        "7po3[c=6-o0[];a=5p:abc;b=7po1[x=5p:def;];]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];~=6-o0[];]",
        "7po3[a=5p:abc;b=7po1[x=5p:def;];=6-o0[];]",
        "7p?",
        "7p!0[?]",
        "7p!0[]",
        "7p!3[~=0-;]",
        "7p!3[=0-;]",
        "7p!3[b=0-;]",
        "7p!3[b=7-o0[];]",
        "7p!3[b=7-o1[~=0-;];]",
        "7p!3[b=7-o1[=0-;];]",
        "7p!3[b=7-o1[x=0-;];]",
        "7p!3[b=7-o1[x=5p:;];]",
        "7p!3[b=7-o1[x=5p:def;];]",
        "7p!3[~=0-;b=7-o1[x=5p:def;];]",
        "7p!3[=0-;b=7-o1[x=5p:def;];]",
        "7p!3[a=0-;b=7-o1[x=5p:def;];]",
        "7p!3[a=5-:;b=7-o1[x=5p:def;];]",
        "7p!3[a=5-:abc;b=7-o1[x=5p:def;];]",
        "7p!3[a=5-:abc;b=7-o1[x=5p:def;];~=0-;]",
        "7p!3[a=5-:abc;b=7-o1[x=5p:def;];=0-;]",
        "7p!3[a=5-:abc;b=7-o1[x=5p:def;];c=0-;]",
        "7p!3[a=5-:abc;b=7-o1[x=5p:def;];c=6-o0[?];]",
        "7p!3[a=5-:abc;b=7-o1[x=5p:def;];c=6-o0[];]",
        "7p!3[a=5-:abc;b=7-o1[x=5p:def;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;~=7-o1[x=5p:def;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;=7-o1[x=5p:def;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=0-;c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=7-o0[];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=7-o1[~=5p:def;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=7-o1[=5p:def;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=7-o1[x=5-:;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=7-o1[~=5-:;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=7-o1[=5-:;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=7-o1[x=0-;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=7-o1[x=5p:;];c=6-o1[3p:42;];]",
        "7p!3[b=7-o1[x=5p:def;];c=6-o1[3p:42;];]",
        "7p!3[~=5-:abc;b=7-o1[x=5p:def;];c=6-o1[3p:42;];]",
        "7p!3[=5-:abc;b=7-o1[x=5p:def;];c=6-o1[3p:42;];]",
        "7p!3[a=0-;b=7-o1[x=5p:def;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:;b=7-o1[x=5p:def;];c=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=7-o1[x=5p:def;];~=6-o1[3p:42;];]",
        "7p!3[a=5-:abc;b=7-o1[x=5p:def;];=6-o1[3p:42;];]",
    };
    struct Case { bool constructor; bool once; int budget; unsigned notifications; std::size_t requests; unsigned state; };
    const Case cases[]{
        {0, 0, 0, 1, 1, 0},
        {0, 0, 1, 1, 2, 0},
        {0, 0, 2, 12, 14, 1},
        {0, 0, 3, 12, 15, 1},
        {0, 0, 4, 12, 16, 1},
        {0, 0, 5, 13, 18, 1},
        {0, 0, 6, 12, 18, 1},
        {0, 0, 7, 13, 20, 1},
        {0, 0, 8, 12, 20, 1},
        {0, 0, 9, 12, 21, 1},
        {0, 0, 10, 12, 22, 1},
        {0, 0, 11, 12, 23, 1},
        {0, 0, 12, 11, 23, 1},
        {0, 0, 13, 11, 24, 1},
        {0, 0, 14, 10, 24, 1},
        {0, 0, 15, 10, 25, 1},
        {0, 0, 16, 9, 25, 1},
        {0, 0, 17, 9, 26, 2},
        {0, 0, 18, 10, 28, 2},
        {0, 0, 19, 10, 29, 3},
        {0, 0, 20, 9, 29, 4},
        {0, 0, 21, 9, 30, 4},
        {0, 0, 22, 9, 31, 5},
        {0, 0, 23, 8, 31, 6},
        {0, 0, 24, 8, 32, 6},
        {0, 0, 25, 8, 33, 6},
        {0, 0, 26, 8, 34, 6},
        {0, 0, 27, 9, 36, 6},
        {0, 0, 28, 8, 36, 6},
        {0, 0, 29, 9, 38, 6},
        {0, 0, 30, 8, 38, 6},
        {0, 0, 31, 8, 39, 6},
        {0, 0, 32, 8, 40, 6},
        {0, 0, 33, 9, 42, 6},
        {0, 0, 34, 9, 43, 6},
        {0, 0, 35, 8, 43, 6},
        {0, 0, 36, 8, 44, 6},
        {0, 0, 37, 7, 44, 6},
        {0, 0, 38, 7, 45, 6},
        {0, 0, 39, 6, 45, 6},
        {0, 0, 40, 6, 46, 6},
        {0, 0, 41, 6, 47, 6},
        {0, 0, 42, 6, 48, 6},
        {0, 0, 43, 6, 49, 6},
        {0, 0, 44, 5, 49, 6},
        {0, 0, 45, 7, 52, 7},
        {0, 0, 46, 6, 52, 7},
        {0, 0, 47, 6, 53, 8},
        {0, 0, 48, 5, 53, 9},
        {0, 0, 49, 5, 54, 9},
        {0, 0, 50, 5, 55, 9},
        {0, 0, 51, 5, 56, 10},
        {0, 0, 52, 5, 57, 10},
        {0, 0, 53, 5, 58, 11},
        {0, 0, 54, 5, 59, 11},
        {0, 0, 55, 8, 63, 11},
        {0, 0, 56, 8, 64, 11},
        {0, 0, 57, 8, 65, 11},
        {0, 0, 58, 9, 67, 11},
        {0, 0, 59, 8, 67, 11},
        {0, 0, 60, 9, 69, 11},
        {0, 0, 61, 8, 69, 11},
        {0, 0, 62, 8, 70, 11},
        {0, 0, 63, 8, 71, 11},
        {0, 0, 64, 8, 72, 11},
        {0, 0, 65, 7, 72, 11},
        {0, 0, 66, 7, 73, 11},
        {0, 0, 67, 6, 73, 11},
        {0, 0, 68, 6, 74, 11},
        {0, 0, 69, 5, 74, 11},
        {0, 0, 70, 7, 77, 12},
        {0, 0, 71, 6, 77, 12},
        {0, 0, 72, 6, 78, 13},
        {0, 0, 73, 5, 78, 14},
        {0, 0, 74, 5, 79, 14},
        {0, 0, 75, 5, 80, 15},
        {0, 0, 76, 4, 80, 16},
        {0, 0, 77, 4, 81, 16},
        {0, 0, 78, 5, 83, 16},
        {0, 0, 79, 4, 83, 16},
        {0, 0, 80, 5, 85, 16},
        {0, 0, 81, 4, 85, 16},
        {0, 0, 82, 5, 87, 16},
        {0, 0, 83, 4, 87, 16},
        {0, 0, 84, 4, 88, 16},
        {0, 0, 85, 6, 91, 16},
        {0, 0, 86, 5, 91, 16},
        {0, 0, 87, 5, 92, 16},
        {0, 0, 88, 4, 92, 16},
        {0, 0, 89, 4, 93, 16},
        {0, 0, 90, 3, 93, 16},
        {0, 0, 91, 3, 94, 16},
        {0, 0, 92, 2, 94, 16},
        {0, 0, 93, 2, 95, 16},
        {0, 0, 94, 3, 97, 16},
        {0, 0, 95, 2, 97, 16},
        {0, 0, 96, 2, 98, 16},
        {0, 0, 97, 1, 98, 16},
        {0, 0, 98, 3, 101, 17},
        {0, 0, 99, 2, 101, 17},
        {0, 0, 100, 2, 102, 18},
        {0, 0, 101, 1, 102, 19},
        {0, 0, 102, 1, 103, 19},
        {0, 0, 103, 2, 105, 20},
        {0, 0, 104, 1, 105, 21},
        {0, 0, 105, 1, 106, 21},
        {0, 0, 106, 1, 107, 22},
        {0, 0, 107, 1, 108, 22},
        {0, 0, 108, 1, 109, 22},
        {0, 0, 109, 0, 109, 23},
        {0, 1, 0, 1, 1, 0},
        {0, 1, 1, 1, 2, 0},
        {0, 1, 2, 1, 104, 23},
        {0, 1, 3, 1, 105, 23},
        {0, 1, 4, 1, 102, 24},
        {0, 1, 5, 1, 106, 25},
        {0, 1, 6, 1, 104, 24},
        {0, 1, 7, 1, 107, 25},
        {0, 1, 8, 1, 106, 23},
        {0, 1, 9, 1, 107, 23},
        {0, 1, 10, 1, 106, 26},
        {0, 1, 11, 1, 107, 26},
        {0, 1, 12, 1, 105, 27},
        {0, 1, 13, 1, 107, 28},
        {0, 1, 14, 1, 105, 29},
        {0, 1, 15, 1, 106, 29},
        {0, 1, 16, 1, 103, 30},
        {0, 1, 17, 1, 108, 26},
        {0, 1, 18, 1, 107, 27},
        {0, 1, 19, 1, 108, 28},
        {0, 1, 20, 1, 107, 29},
        {0, 1, 21, 1, 108, 29},
        {0, 1, 22, 1, 109, 31},
        {0, 1, 23, 1, 95, 23},
        {0, 1, 24, 1, 96, 23},
        {0, 1, 25, 1, 97, 23},
        {0, 1, 26, 1, 102, 32},
        {0, 1, 27, 1, 106, 33},
        {0, 1, 28, 1, 104, 32},
        {0, 1, 29, 1, 107, 33},
        {0, 1, 30, 1, 98, 23},
        {0, 1, 31, 1, 99, 23},
        {0, 1, 32, 1, 100, 23},
        {0, 1, 33, 1, 108, 23},
        {0, 1, 34, 1, 109, 23},
        {0, 1, 35, 1, 84, 34},
        {0, 1, 36, 1, 85, 34},
        {0, 1, 37, 1, 83, 35},
        {0, 1, 38, 1, 85, 36},
        {0, 1, 39, 1, 77, 37},
        {0, 1, 40, 1, 78, 37},
        {0, 1, 41, 1, 79, 37},
        {0, 1, 42, 1, 85, 38},
        {0, 1, 43, 1, 86, 38},
        {0, 1, 44, 1, 78, 39},
        {0, 1, 45, 1, 86, 38},
        {0, 1, 46, 1, 85, 35},
        {0, 1, 47, 1, 86, 36},
        {0, 1, 48, 1, 82, 37},
        {0, 1, 49, 1, 83, 37},
        {0, 1, 50, 1, 84, 37},
        {0, 1, 51, 1, 85, 38},
        {0, 1, 52, 1, 86, 38},
        {0, 1, 53, 1, 87, 40},
        {0, 1, 54, 1, 88, 40},
        {0, 1, 55, 1, 104, 23},
        {0, 1, 56, 1, 105, 23},
        {0, 1, 57, 1, 102, 41},
        {0, 1, 58, 1, 106, 42},
        {0, 1, 59, 1, 104, 41},
        {0, 1, 60, 1, 107, 42},
        {0, 1, 61, 1, 106, 23},
        {0, 1, 62, 1, 107, 23},
        {0, 1, 63, 1, 106, 43},
        {0, 1, 64, 1, 107, 43},
        {0, 1, 65, 1, 105, 44},
        {0, 1, 66, 1, 107, 45},
        {0, 1, 67, 1, 105, 46},
        {0, 1, 68, 1, 106, 46},
        {0, 1, 69, 1, 103, 40},
        {0, 1, 70, 1, 108, 43},
        {0, 1, 71, 1, 107, 44},
        {0, 1, 72, 1, 108, 45},
        {0, 1, 73, 1, 107, 46},
        {0, 1, 74, 1, 108, 46},
        {0, 1, 75, 1, 109, 47},
        {0, 1, 76, 1, 95, 23},
        {0, 1, 77, 1, 96, 23},
        {0, 1, 78, 1, 109, 23},
        {0, 1, 79, 1, 102, 48},
        {0, 1, 80, 1, 106, 49},
        {0, 1, 81, 1, 104, 48},
        {0, 1, 82, 1, 107, 49},
        {0, 1, 83, 1, 98, 23},
        {0, 1, 84, 1, 99, 23},
        {0, 1, 85, 1, 109, 23},
        {0, 1, 86, 1, 108, 23},
        {0, 1, 87, 1, 109, 23},
        {0, 1, 88, 1, 104, 50},
        {0, 1, 89, 1, 105, 50},
        {0, 1, 90, 1, 103, 51},
        {0, 1, 91, 1, 105, 52},
        {0, 1, 92, 1, 97, 19},
        {0, 1, 93, 1, 98, 19},
        {0, 1, 94, 1, 106, 21},
        {0, 1, 95, 1, 105, 21},
        {0, 1, 96, 1, 106, 21},
        {0, 1, 97, 1, 98, 16},
        {0, 1, 98, 1, 106, 21},
        {0, 1, 99, 1, 105, 51},
        {0, 1, 100, 1, 106, 52},
        {0, 1, 101, 1, 102, 19},
        {0, 1, 102, 1, 103, 19},
        {0, 1, 103, 1, 106, 21},
        {0, 1, 104, 1, 105, 21},
        {0, 1, 105, 1, 106, 21},
        {0, 1, 106, 1, 107, 22},
        {0, 1, 107, 1, 108, 22},
        {0, 1, 108, 1, 109, 22},
        {0, 1, 109, 0, 109, 23},
        {1, 0, 0, 1, 1, 53},
        {1, 0, 1, 2, 3, 54},
        {1, 0, 2, 1, 3, 54},
        {1, 0, 3, 1, 4, 55},
        {1, 0, 4, 5, 9, 56},
        {1, 0, 5, 4, 9, 56},
        {1, 0, 6, 4, 10, 57},
        {1, 0, 7, 3, 10, 58},
        {1, 0, 8, 3, 11, 58},
        {1, 0, 9, 3, 12, 58},
        {1, 0, 10, 3, 13, 59},
        {1, 0, 11, 3, 14, 59},
        {1, 0, 12, 6, 18, 59},
        {1, 0, 13, 6, 19, 59},
        {1, 0, 14, 6, 20, 59},
        {1, 0, 15, 7, 22, 59},
        {1, 0, 16, 6, 22, 59},
        {1, 0, 17, 7, 24, 59},
        {1, 0, 18, 6, 24, 59},
        {1, 0, 19, 6, 25, 59},
        {1, 0, 20, 6, 26, 59},
        {1, 0, 21, 6, 27, 59},
        {1, 0, 22, 5, 27, 59},
        {1, 0, 23, 5, 28, 59},
        {1, 0, 24, 4, 28, 59},
        {1, 0, 25, 4, 29, 59},
        {1, 0, 26, 3, 29, 59},
        {1, 0, 27, 5, 32, 60},
        {1, 0, 28, 4, 32, 60},
        {1, 0, 29, 4, 33, 61},
        {1, 0, 30, 3, 33, 62},
        {1, 0, 31, 3, 34, 62},
        {1, 0, 32, 3, 35, 63},
        {1, 0, 33, 2, 35, 64},
        {1, 0, 34, 4, 38, 65},
        {1, 0, 35, 3, 38, 65},
        {1, 0, 36, 3, 39, 66},
        {1, 0, 37, 2, 39, 67},
        {1, 0, 38, 2, 40, 67},
        {1, 0, 39, 2, 41, 68},
        {1, 0, 40, 1, 41, 69},
        {1, 0, 41, 3, 44, 70},
        {1, 0, 42, 2, 44, 70},
        {1, 0, 43, 2, 45, 71},
        {1, 0, 44, 1, 45, 72},
        {1, 0, 45, 1, 46, 72},
        {1, 0, 46, 2, 48, 73},
        {1, 0, 47, 1, 48, 74},
        {1, 0, 48, 1, 49, 74},
        {1, 0, 49, 1, 50, 74},
        {1, 0, 50, 0, 50, 75},
        {1, 1, 0, 1, 1, 53},
        {1, 1, 1, 1, 50, 75},
        {1, 1, 2, 1, 3, 54},
        {1, 1, 3, 1, 4, 55},
        {1, 1, 4, 1, 50, 75},
        {1, 1, 5, 1, 49, 76},
        {1, 1, 6, 1, 50, 77},
        {1, 1, 7, 1, 25, 78},
        {1, 1, 8, 1, 26, 78},
        {1, 1, 9, 1, 27, 78},
        {1, 1, 10, 1, 28, 79},
        {1, 1, 11, 1, 29, 79},
        {1, 1, 12, 1, 45, 75},
        {1, 1, 13, 1, 46, 75},
        {1, 1, 14, 1, 43, 80},
        {1, 1, 15, 1, 47, 81},
        {1, 1, 16, 1, 45, 80},
        {1, 1, 17, 1, 48, 81},
        {1, 1, 18, 1, 47, 75},
        {1, 1, 19, 1, 48, 75},
        {1, 1, 20, 1, 47, 82},
        {1, 1, 21, 1, 48, 82},
        {1, 1, 22, 1, 46, 83},
        {1, 1, 23, 1, 48, 84},
        {1, 1, 24, 1, 46, 85},
        {1, 1, 25, 1, 47, 85},
        {1, 1, 26, 1, 44, 79},
        {1, 1, 27, 1, 49, 82},
        {1, 1, 28, 1, 48, 83},
        {1, 1, 29, 1, 49, 84},
        {1, 1, 30, 1, 48, 85},
        {1, 1, 31, 1, 49, 85},
        {1, 1, 32, 1, 50, 86},
        {1, 1, 33, 1, 44, 87},
        {1, 1, 34, 1, 50, 75},
        {1, 1, 35, 1, 49, 88},
        {1, 1, 36, 1, 50, 89},
        {1, 1, 37, 1, 48, 90},
        {1, 1, 38, 1, 49, 90},
        {1, 1, 39, 1, 50, 91},
        {1, 1, 40, 1, 41, 69},
        {1, 1, 41, 1, 50, 75},
        {1, 1, 42, 1, 49, 92},
        {1, 1, 43, 1, 50, 93},
        {1, 1, 44, 1, 45, 72},
        {1, 1, 45, 1, 46, 72},
        {1, 1, 46, 1, 50, 75},
        {1, 1, 47, 1, 48, 74},
        {1, 1, 48, 1, 49, 74},
        {1, 1, 49, 1, 50, 74},
        {1, 1, 50, 0, 50, 75},
    };
    const auto baseline = trace.live.size();
    for (const auto& test : cases) {
        Value source{}, destination{};
        _ZN3sce4Json5ValueC1Ev(&source); _ZN3sce4Json5ValueC1Ev(&destination);
        const char input[] = "{\"a\":\"abc\",\"b\":{\"x\":\"def\"},\"c\":[42]}";
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&source, input, sizeof(input) - 1) == 0, "Prepare nested failure source");
        Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&destination, "{\"z\":99}", 8) == 0, "Prepare nested failure destination");
        const auto sourceState = NestedObjectFailureState(source);
        Object copy{};
        auto* sourceObject = reinterpret_cast<const Object*>(source.payload);
        const auto notifications = trace.notifications;
        const auto requests = trace.requests.size();
        trace.budget = test.budget; trace.failOnce = test.once;
        if (test.constructor) _ZN3sce4Json6ObjectC1ERKS1_(&copy, sourceObject);
        else _ZN3sce4Json6ObjectaSERKS1_(reinterpret_cast<Object*>(destination.payload), sourceObject);
        trace.budget = -1; trace.failOnce = false;
        std::string actual;
        if (test.constructor) {
            Value proxy{}; proxy.type = 7; proxy.payload = reinterpret_cast<std::uintptr_t>(&copy);
            actual = NestedObjectFailureState(proxy);
        } else actual = NestedObjectFailureState(destination);
        const bool matches = actual == states[test.state] && trace.notifications - notifications == test.notifications && trace.requests.size() - requests == test.requests;
        if (!matches) std::fprintf(stderr, "Nested object constructor=%d once=%d budget=%d expected=%s actual=%s requests=%zu notifications=%u\n", test.constructor, test.once, test.budget, states[test.state], actual.c_str(), trace.requests.size() - requests, trace.notifications - notifications);
        Require(matches, "Nested object failure must retain the reference state, ownership and allocation accounting");
        Require(NestedObjectFailureState(source) == sourceState, "Nested allocation failures must preserve source contents and ownership");
        if (test.constructor) _ZN3sce4Json6ObjectD1Ev(&copy);
        _ZN3sce4Json5ValueD1Ev(&destination); _ZN3sce4Json5ValueD1Ev(&source);
        Require(trace.live.size() == baseline, "Nested object failure must reclaim every admitted resource");
    }
}
static void CheckAssignments(ObjectAllocationTrace& trace) {
    struct Case { bool once; int budget; std::size_t count; unsigned notifications; const char* contents; };
    const Case cases[]{
        {0, -1, 2, 0, "a:2:11;c:2:33;"},
        {0, 0, 1, 1, "z:2:99;"},
        {0, 1, 1, 1, "z:2:99;"},
        {0, 2, 0, 6, ""},
        {0, 3, 0, 7, ""},
        {0, 4, 0, 6, ""},
        {0, 5, 0, 7, ""},
        {0, 6, 0, 6, ""},
        {0, 7, 0, 6, ""},
        {0, 8, 0, 5, ""},
        {0, 9, 0, 5, ""},
        {0, 10, 0, 4, ""},
        {0, 11, 1, 4, "~:2:0;"},
        {0, 12, 1, 4, "~:2:0;"},
        {0, 13, 1, 4, ":2:0;"},
        {0, 14, 1, 3, "a:2:11;"},
        {0, 15, 1, 4, "a:2:11;"},
        {0, 16, 1, 3, "a:2:11;"},
        {0, 17, 1, 4, "a:2:11;"},
        {0, 18, 1, 3, "a:2:11;"},
        {0, 19, 1, 3, "a:2:11;"},
        {0, 20, 1, 2, "a:2:11;"},
        {0, 21, 1, 2, "a:2:11;"},
        {0, 22, 1, 1, "a:2:11;"},
        {0, 23, 2, 2, "a:2:11;~:2:0;"},
        {0, 24, 2, 1, "a:2:11;~:2:0;"},
        {0, 25, 2, 1, "a:2:11;:2:0;"},
        {0, 26, 2, 0, "a:2:11;c:2:33;"},
        {1, -1, 2, 0, "a:2:11;c:2:33;"},
        {1, 0, 1, 1, "z:2:99;"},
        {1, 1, 1, 1, "z:2:99;"},
        {1, 2, 2, 1, "~:2:11;c:2:33;"},
        {1, 3, 2, 1, ":2:11;c:2:33;"},
        {1, 4, 2, 1, "~:2:11;c:2:33;"},
        {1, 5, 2, 1, ":2:11;c:2:33;"},
        {1, 6, 2, 1, "a:2:0;c:2:33;"},
        {1, 7, 2, 1, "a:2:0;c:2:33;"},
        {1, 8, 2, 1, "~:2:0;c:2:33;"},
        {1, 9, 2, 1, ":2:0;c:2:33;"},
        {1, 10, 1, 1, "c:2:33;"},
        {1, 11, 2, 1, "a:2:0;c:2:33;"},
        {1, 12, 2, 1, "~:2:0;c:2:33;"},
        {1, 13, 2, 1, ":2:0;c:2:33;"},
        {1, 14, 2, 1, "a:2:11;~:2:33;"},
        {1, 15, 2, 1, "a:2:11;:2:33;"},
        {1, 16, 2, 1, "a:2:11;~:2:33;"},
        {1, 17, 2, 1, "a:2:11;:2:33;"},
        {1, 18, 2, 1, "c:2:0;a:2:11;"},
        {1, 19, 2, 1, "c:2:0;a:2:11;"},
        {1, 20, 2, 1, "a:2:11;~:2:0;"},
        {1, 21, 2, 1, "a:2:11;:2:0;"},
        {1, 22, 1, 1, "a:2:11;"},
        {1, 23, 2, 1, "a:2:11;c:2:0;"},
        {1, 24, 2, 1, "a:2:11;~:2:0;"},
        {1, 25, 2, 1, "a:2:11;:2:0;"},
        {1, 26, 2, 0, "a:2:11;c:2:33;"},
    };
    const auto baseline = trace.live.size();
    for (const auto& test : cases) {
        Object source{}, destination{};
        _ZN3sce4Json6ObjectC1Ev(&source); _ZN3sce4Json6ObjectC1Ev(&destination);
        _ZN3sce4Json5Value3setEl(Entry(source, "a"), 11);
        _ZN3sce4Json5Value3setEl(Entry(source, "c"), 33);
        _ZN3sce4Json5Value3setEl(Entry(destination, "z"), 99);
        Value owner{};
        static_cast<TreeHeader*>(destination.storage)->owner = &owner;
        auto* previous = destination.storage;
        trace.headers.insert(previous);
        const auto notifications = trace.notifications;
        trace.budget = test.budget; trace.failOnce = test.once;
        Require(_ZN3sce4Json6ObjectaSERKS1_(&destination, &source) == &destination, "Object assignment must return its destination");
        trace.budget = -1; trace.failOnce = false;
        Require((destination.storage == previous) == (test.budget == 0 || test.budget == 1), "Only initial replacement allocation failures preserve the old object header");
        Require(static_cast<TreeHeader*>(destination.storage)->owner == &owner, "Replacement trees must retain their destination owner");
        Require(_ZNK3sce4Json6Object4sizeEv(&destination) == test.count && AssignmentContents(destination) == test.contents, "Assignment failure must retain the reference partial entries and values");
        Require(trace.notifications - notifications == test.notifications, "Assignment must report each failed owned allocation");
        Require(AssignmentContents(source) == "a:2:11;c:2:33;", "Assignment must preserve the source");
        trace.headers.insert(destination.storage);
        _ZN3sce4Json6ObjectD1Ev(&destination); _ZN3sce4Json6ObjectD1Ev(&source);
        Require(trace.live.size() == baseline, "Assignment failure must reclaim temporary and replaced resources");
    }
    Object object{}; _ZN3sce4Json6ObjectC1Ev(&object);
    _ZN3sce4Json5Value3setEl(Entry(object, "a"), 11);
    auto* previous = object.storage;
    trace.headers.insert(previous);
    _ZN3sce4Json6ObjectaSERKS1_(&object, &object);
    Require(object.storage != previous && AssignmentContents(object) == "a:2:11;", "Modern self-assignment must rebuild while preserving values");
    trace.headers.insert(object.storage);
    _ZN3sce4Json6ObjectD1Ev(&object);
    Require(trace.live.size() == baseline, "Self-assignment must release its previous tree");
}
int main() {
    ObjectAllocationTrace trace; ObjectAllocator allocator;
    Parameter parameter{&allocator, &trace, 16384, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize object allocation contract");
    const auto baseline = trace.live.size();
    for (int budget : {0, 1}) {
        Object partial{}; trace.budget = budget; _ZN3sce4Json6ObjectC1Ev(&partial); trace.budget = -1;
        Require(!partial.storage && _ZNK3sce4Json6Object4sizeEv(&partial) == 0 && trace.live.size() == baseline, "Failed object construction must reclaim partial header ownership");
        _ZN3sce4Json6ObjectD1Ev(&partial);
    }
    Object object{}; _ZN3sce4Json6ObjectC1Ev(&object);
    std::map<std::string, Value*> entries;
    CheckTree(object, trace, entries);
    for (unsigned index = 0; index < 32; ++index) {
        char key[3]; std::snprintf(key, sizeof(key), "%02u", index * 13 % 32);
        auto* value = Entry(object, key); _ZN3sce4Json5Value3setEl(value, index);
        entries.emplace(key, value); CheckTree(object, trace, entries);
    }
    Iterator current{}, copy{}; _ZNK3sce4Json6Object5beginEv(&current, &object); _ZN3sce4Json6Object8iteratorC1ERKS2_(&copy, &current);
    Require(Owned(trace, current.state, 8) && Owned(trace, copy.state, 8) && current.state != copy.state && *static_cast<void**>(current.state) == *static_cast<void**>(copy.state), "Object iterator copies must own one independent node pointer");
    _ZN3sce4Json6Object8iteratorD1Ev(&copy); _ZN3sce4Json6Object8iteratorD1Ev(&current);
    const auto beforeCopies = trace.live.size();
    for (int budget = 0; budget <= 10; ++budget) {
        Object duplicate{}; trace.budget = budget; _ZN3sce4Json6ObjectC1ERKS1_(&duplicate, &object); trace.budget = -1;
        Require(_ZNK3sce4Json6Object4sizeEv(&duplicate) == (budget >= 4 ? 32 : 0), "Partial tree copies retain the source count once the root allocation succeeds");
        if (duplicate.storage) trace.headers.insert(duplicate.storage);
        _ZN3sce4Json6ObjectD1Ev(&duplicate); Require(trace.live.size() == beforeCopies, "Partial tree copies must release their actual nodes");
    }
    for (unsigned index = 0; index < 32; ++index) {
        char key[3]; std::snprintf(key, sizeof(key), "%02u", index * 7 % 32);
        String text{}; _ZN3sce4Json6StringC1EPKc(&text, key);
        const auto requests = trace.requests.size();
        const auto notifications = trace.notifications;
        trace.budget = 0;
        _ZN3sce4Json6Object5eraseERKNS0_6StringE(&object, &text);
        _ZN3sce4Json6Object5eraseERKNS0_6StringE(&object, &text);
        trace.budget = -1;
        Require(trace.requests.size() == requests && trace.notifications == notifications, "Existing and missing key erasure must work without allocating or reporting allocation failure");
        _ZN3sce4Json6StringD1Ev(&text);
        entries.erase(key); CheckTree(object, trace, entries);
    }
    _ZN3sce4Json6Object5clearEv(&object); _ZN3sce4Json6ObjectD1Ev(&object);
    Require(trace.live.size() == baseline, "Tree operations must release all owned resources");
    Value null{}; _ZN3sce4Json5ValueC1Ev(&null);
    const auto* moduleFallback = _ZNK3sce4Json5ValueixEm(&null, 0);
    _ZN3sce4Json5ValueD1Ev(&null);
    CheckFailures(trace, moduleFallback);
    CheckIteratorFailures(trace);
    CheckInsertions(trace);
    CheckAssignments(trace);
    CheckNestedAssignment(trace);
    CheckNestedFailureCases(trace);
    CheckBinaryKeys();
    Require(trace.live.size() == baseline, "Binary-key operations must release their storage");
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Terminate object allocation contract");
    return 0;
}
