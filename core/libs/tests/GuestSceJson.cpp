#include "prx/libSceJson/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace SceJson;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SceJson check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr int ErrorParse = static_cast<int>(0x80848101);
constexpr int ErrorNoMemory = static_cast<int>(0x80848102);
constexpr int ErrorFileOpen = static_cast<int>(0x80848103);
constexpr int ErrorNotRoot = static_cast<int>(0x80848104);
constexpr int ErrorFileBufferSize = static_cast<int>(0x80848105);
constexpr int ErrorNotInitialized = static_cast<int>(0x80848110);
constexpr int ErrorAlreadyInitialized = static_cast<int>(0x80848111);

static long liveAllocations = 0;

static void* APS5_VABI TestAllocate(MemAllocator*, std::size_t size, void* userData) {
    Require(userData == &liveAllocations);
    ++liveAllocations;
    return std::malloc(size);
}

static void APS5_VABI TestDeallocate(MemAllocator*, void* pointer, void* userData) {
    Require(userData == &liveAllocations);
    if (pointer != nullptr) --liveAllocations;
    std::free(pointer);
}

static void* const allocatorVtable[] = {nullptr, nullptr, reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestDeallocate)};
static MemAllocator allocator {allocatorVtable};

static const char* Text(const String* text) {
    return text->capacity > 15 ? text->storage.pointer : text->storage.buffer;
}

static String MakeString(const char* source) {
    String text {};
    const std::size_t size = std::strlen(source);
    text.size = size;
    if (size <= 15) {
        text.capacity = 15;
        std::memcpy(text.storage.buffer, source, size + 1);
    } else {
        text.capacity = size;
        text.storage.pointer = static_cast<char*>(_ZN3sce4Json6MallocEm_nid_no_patch(size + 1));
        std::memcpy(text.storage.pointer, source, size + 1);
    }
    return text;
}

static void FreeString(String& text) {
    if (text.capacity > 15) _ZN3sce4Json4FreeEPv_nid_no_patch(text.storage.pointer);
}

static String EmptyString() {
    return MakeString("");
}

static int Parse(Value& value, const std::string& text) {
    return _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm_nid_no_patch(&value, text.data(), text.size());
}

static std::string Serialize(Value& value) {
    String out = EmptyString();
    Require(_ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(&value, &out) == 0);
    std::string result(Text(&out), out.size);
    FreeString(out);
    return result;
}

static const Value& Member(const Value& value, const char* key) {
    return *_ZNK3sce4Json5ValueixEPKc_nid_no_patch(&value, key);
}

static const Value& Element(const Value& value, std::size_t index) {
    return *_ZNK3sce4Json5ValueixEm_nid_no_patch(&value, index);
}

static void Uninitialized() {
    Require(_ZN3sce4Json6MallocEm_nid_no_patch(16) == nullptr);
    Value value {};
    _ZN3sce4Json5ValueC1Ev_nid_no_patch(&value);
    Require(Parse(value, "1") == ErrorNotInitialized);
    String out = EmptyString();
    Require(_ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(&value, &out) == ErrorNotInitialized);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc_nid_no_patch(&value, "missing.json") == ErrorNotInitialized);
}

static void Document() {
    Value root {};
    _ZN3sce4Json5ValueC1Ev_nid_no_patch(&root);
    const std::string text = " {\"b\": [1, -2, 0.5, 1e2, true, false, null, \"s\\u00e9\\ud83d\\ude00\\/\"],"
                             " \"a\": {\"k\": 18446744073709551615, \"n\": -9223372036854775808, \"m\": -9223372036854775807},"
                             " \"c\": \"x\"} ";
    Require(Parse(root, text) == 0);
    Require(_ZNK3sce4Json5Value7getTypeEv_nid_no_patch(&root) == TypeObject);
    Require(_ZNK3sce4Json5Value5countEv_nid_no_patch(&root) == 3);
    const Value& a = Element(root, 0);
    Require(&a == &Member(root, "a"));
    Require(_ZNK3sce4Json5Value7getTypeEv_nid_no_patch(&Member(a, "k")) == TypeUInteger);
    Require(*_ZNK3sce4Json5Value11getUIntegerEv_nid_no_patch(&Member(a, "k")) == 18446744073709551615ull);
    Require(_ZNK3sce4Json5Value7getTypeEv_nid_no_patch(&Member(a, "n")) == TypeReal);
    Require(*_ZNK3sce4Json5Value7getRealEv_nid_no_patch(&Member(a, "n")) == -9223372036854775808.0);
    Require(_ZNK3sce4Json5Value7getTypeEv_nid_no_patch(&Member(a, "m")) == TypeInteger);
    Require(*_ZNK3sce4Json5Value10getIntegerEv_nid_no_patch(&Member(a, "m")) == -9223372036854775807ll);
    const Value& b = Member(root, "b");
    Require(_ZNK3sce4Json5Value5countEv_nid_no_patch(&b) == 8);
    Require(_ZNK3sce4Json5Value7getTypeEv_nid_no_patch(&Element(b, 0)) == TypeUInteger);
    Require(*_ZNK3sce4Json5Value10getIntegerEv_nid_no_patch(&Element(b, 0)) == 1);
    Require(_ZNK3sce4Json5Value7getTypeEv_nid_no_patch(&Element(b, 1)) == TypeInteger);
    Require(*_ZNK3sce4Json5Value11getUIntegerEv_nid_no_patch(&Element(b, 1)) == static_cast<std::uint64_t>(-2));
    Require(*_ZNK3sce4Json5Value7getRealEv_nid_no_patch(&Element(b, 2)) == 0.5);
    Require(*_ZNK3sce4Json5Value7getRealEv_nid_no_patch(&Element(b, 3)) == 100.0);
    Require(_ZNK3sce4Json5Value7getRealEv_nid_no_patch(&Element(b, 0)) == &_ZN3sce4Json5Value10s_nullrealE);
    Require(*_ZNK3sce4Json5Value10getBooleanEv_nid_no_patch(&Element(b, 4)));
    Require(!_ZNK3sce4Json5ValuecvbEv_nid_no_patch(&Element(b, 5)));
    Require(_ZNK3sce4Json5Value7getTypeEv_nid_no_patch(&Element(b, 6)) == TypeNull);
    Require(std::strcmp(Text(_ZNK3sce4Json5Value9getStringEv_nid_no_patch(&Element(b, 7))), "s\xc3\xa9\xf0\x9f\x98\x80/") == 0);
    Require(_ZNK3sce4Json5Value9getStringEv_nid_no_patch(&Element(b, 0)) == _ZN3sce4Json5Value12s_nullstringE);
    Require(_ZNK3sce4Json5Value7getRootEv_nid_no_patch(&Element(b, 7)) == &root);
    Require(Serialize(root) ==
        "{\"a\":{\"k\":18446744073709551615,\"m\":-9223372036854775807,\"n\":-9223372036854775808},"
        "\"b\":[1,-2,0.500000,100,true,false,null,\"s\xc3\xa9\xf0\x9f\x98\x80\\/\"],\"c\":\"x\"}");
    String out = EmptyString();
    _ZNK3sce4Json5Value8toStringERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(&Element(b, 7), &out);
    _ZNK3sce4Json5Value8toStringERSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(&root, &out);
    Require(std::string(Text(&out), out.size) == "s\xc3\xa9\xf0\x9f\x98\x80/object");
    FreeString(out);

    Value copy {};
    _ZN3sce4Json5ValueC1ERKS1__nid_no_patch(&copy, &root);
    Require(copy.parent == nullptr && copy.root == nullptr);
    Require(Serialize(copy) == Serialize(root));
    String key = MakeString("c");
    Value* c = _ZN3sce4Json5Value10referValueERKSbIcSt11char_traitsIcENS0_8StlAllocIcEEE_nid_no_patch(&copy, &key);
    Require(c != nullptr);
    _ZN3sce4Json5Value3setEd_nid_no_patch(c, -1.25);
    _ZN3sce4Json5Value4swapERS1__nid_no_patch(c, _ZN3sce4Json5Value10referValueEm_nid_no_patch(_ZN3sce4Json5Value10referValueEm_nid_no_patch(&copy, 1), 6));
    Require(_ZNK3sce4Json5Value7getTypeEv_nid_no_patch(c) == TypeNull);
    Require(Serialize(copy).find("null,\"s") == std::string::npos && Serialize(copy).find("-1.250000") != std::string::npos);
    FreeString(key);
    _ZN3sce4Json5ValueD1Ev_nid_no_patch(&copy);
    _ZN3sce4Json5ValueD1Ev_nid_no_patch(&root);
}

static Value fallback {};
static int callbackCalls = 0;
static const Value* callbackParent = nullptr;
static ValueType callbackType = TypeNull;

static const Value& APS5_VABI OnNullAccess(ValueType type, const Value* parent, void* context) {
    Require(context == &callbackCalls);
    ++callbackCalls;
    callbackType = type;
    callbackParent = parent;
    return fallback;
}

static void NullAccess() {
    _ZN3sce4Json5ValueC1El_nid_no_patch(&fallback, 7);
    Value root {};
    _ZN3sce4Json5ValueC1Ev_nid_no_patch(&root);
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6__nid_no_patch(&root, OnNullAccess, &callbackCalls) == ErrorNotRoot);
    Require(Parse(root, "{\"x\": [1]}") == 0);
    const Value& missing = Member(root, "zz");
    Require(&missing == root.root->nullValue);
    Require(_ZNK3sce4Json5Value10getIntegerEv_nid_no_patch(&missing) == &_ZN3sce4Json5Value13s_nullintegerE);
    Value* x = _ZN3sce4Json5Value10referValueEm_nid_no_patch(&root, 0);
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6__nid_no_patch(x, OnNullAccess, &callbackCalls) == ErrorNotRoot);
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6__nid_no_patch(&root, OnNullAccess, &callbackCalls) == 0);
    Require(*_ZNK3sce4Json5Value10getIntegerEv_nid_no_patch(&missing) == 7);
    Require(callbackCalls == 1 && callbackType == TypeInteger && callbackParent == &root);
    Require(&Element(*x, 5) == root.root->nullValue);
    Require(_ZNK3sce4Json5Value9getStringEv_nid_no_patch(&Member(*x, "a")) == _ZN3sce4Json5Value12s_nullstringE);
    Require(Serialize(root) == "{\"x\":[1]}");
    Require(_ZNK3sce4Json5Value10getIntegerEv_nid_no_patch(&missing) == &_ZN3sce4Json5Value13s_nullintegerE);
    _ZN3sce4Json5ValueD1Ev_nid_no_patch(&root);
    _ZN3sce4Json5ValueD1Ev_nid_no_patch(&fallback);
}

static int APS5_VABI FailingRead(char& character, void* context) {
    auto* remaining = static_cast<int*>(context);
    if (*remaining == 0) return -5;
    --*remaining;
    character = ' ';
    return 0;
}

static int serializeChunks = 0;

static int APS5_VABI CountChunks(String& out, void* context) {
    ++serializeChunks;
    out.size = 0;
    Require(context == &serializeChunks);
    return serializeChunks == 3 ? -9 : 0;
}

static void Errors() {
    Value value {};
    _ZN3sce4Json5ValueC1Ev_nid_no_patch(&value);
    Require(Parse(value, "[1,]") == ErrorParse);
    Require(Parse(value, "tru") == ErrorParse);
    Require(Parse(value, "{\"a\" 1}") == ErrorParse);
    Require(Parse(value, "\"a\\x\"") == ErrorParse);
    Require(Parse(value, "\"\\ud800x\"") == ErrorParse);
    Require(Parse(value, "[\"open") == ErrorNoMemory);
    Require(_ZNK3sce4Json5Value7getTypeEv_nid_no_patch(&value) == TypeNull);
    int remaining = 3;
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPFiRcPvES5__nid_no_patch(&value, FailingRead, &remaining) == -5);
    Require(Parse(value, "[1, [2, 3], 4]") == 0);
    String out = EmptyString();
    serializeChunks = 0;
    Require(_ZN3sce4Json5Value9serializeERSbIcSt11char_traitsIcENS0_8StlAllocIcEEEPFiS7_PvES8__nid_no_patch(&value, &out, CountChunks, &serializeChunks) == -9);
    Require(serializeChunks == 3);
    FreeString(out);
    _ZN3sce4Json5ValueD1Ev_nid_no_patch(&value);
}

static int CheckTree(const TreeNode* node, const TreeNode* parent) {
    if (node->isNil) return 1;
    Require(node->parent == parent);
    if (node->color == 0) Require(node->left->color == 1 && node->right->color == 1);
    const int left = CheckTree(node->left, node);
    Require(left == CheckTree(node->right, node));
    return left + (node->color == 1 ? 1 : 0);
}

static void Tree() {
    std::string text = "{";
    std::vector<std::string> keys;
    std::uint32_t seed = 12345;
    for (int index = 0; index < 300; ++index) {
        seed = seed * 1103515245u + 12345u;
        keys.push_back("key" + std::to_string(seed % 100000));
        text += (index ? "," : "") + std::string("\"") + keys.back() + "\":" + std::to_string(index);
    }
    text += "}";
    Value root {};
    _ZN3sce4Json5ValueC1Ev_nid_no_patch(&root);
    Require(Parse(root, text) == 0);
    const Object* object = _ZNK3sce4Json5Value9getObjectEv_nid_no_patch(&root);
    const TreeNode* head = object->head;
    Require(head->isNil == 1 && head->color == 1 && head->parent->color == 1);
    CheckTree(head->parent, head);
    std::size_t count = 0;
    std::string previous;
    for (const TreeNode* node = head->left; node != head; ++count) {
        const std::string key(Text(&node->key), node->key.size);
        Require(count == 0 || previous < key);
        previous = key;
        if (!node->right->isNil) {
            node = node->right;
            while (!node->left->isNil) node = node->left;
        } else {
            const TreeNode* parent = node->parent;
            while (!parent->isNil && node == parent->right) {
                node = parent;
                parent = node->parent;
            }
            node = parent;
        }
    }
    Require(count == object->size && static_cast<int>(count) == _ZNK3sce4Json5Value5countEv_nid_no_patch(&root));
    Value copy {};
    _ZN3sce4Json5ValueC1ERKSt3mapISbIcSt11char_traitsIcENS0_8StlAllocIcEEES1_St4lessIS7_ENS5_ISt4pairIS7_S1_EEEE_nid_no_patch(&copy, object);
    CheckTree(copy.payload.object->head->parent, copy.payload.object->head);
    Require(Serialize(copy) == Serialize(root));
    Value list {};
    _ZN3sce4Json5ValueC1ENS0_9ValueTypeE_nid_no_patch(&list, TypeArray);
    Require(Parse(list, "[1, 2, 3]") == 0);
    Value shorter {};
    _ZN3sce4Json5ValueC1Ev_nid_no_patch(&shorter);
    Require(Parse(shorter, "[\"x\"]") == 0);
    _ZN3sce4Json5Value3setERKSt4listIS1_NS0_8StlAllocIS1_EEE_nid_no_patch(&list, _ZNK3sce4Json5Value8getArrayEv_nid_no_patch(&shorter));
    Require(Serialize(list) == "[\"x\"]");
    _ZN3sce4Json5Value3setERKS1__nid_no_patch(&shorter, &copy);
    Require(_ZNK3sce4Json5Value5countEv_nid_no_patch(&shorter) == static_cast<int>(count));
    _ZN3sce4Json5ValueD1Ev_nid_no_patch(&shorter);
    _ZN3sce4Json5ValueD1Ev_nid_no_patch(&list);
    _ZN3sce4Json5ValueD1Ev_nid_no_patch(&copy);
    _ZN3sce4Json5ValueD1Ev_nid_no_patch(&root);
}

static void Files() {
    const char* path = "guest_sce_json.tmp";
    std::FILE* file = std::fopen(path, "wb");
    Require(file != nullptr);
    std::fputs(" {\"name\": \"value with more than four bytes\", \"list\": [1, 2.5]} ", file);
    std::fclose(file);
    Value value {};
    _ZN3sce4Json5ValueC1Ev_nid_no_patch(&value);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc_nid_no_patch(&value, path) == 0);
    Require(Serialize(value) == "{\"list\":[1,2.500000],\"name\":\"value with more than four bytes\"}");
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc_nid_no_patch(&value, "guest_sce_json_missing.tmp") == ErrorFileOpen);
    _ZN3sce4Json5ValueD1Ev_nid_no_patch(&value);
    std::remove(path);
}

int main() {
    Uninitialized();
    Initializer initializer {};
    _ZN3sce4Json11InitializerC1Ev_nid_no_patch(&initializer);
    InitParameter parameter {&allocator, &liveAllocations, 0};
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE_nid_no_patch(&initializer, &parameter) == 0);
    Initializer second {};
    _ZN3sce4Json11InitializerC1Ev_nid_no_patch(&second);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE_nid_no_patch(&second, &parameter) == ErrorAlreadyInitialized);
    Require(_ZN3sce4Json11Initializer9terminateEv_nid_no_patch(&second) == ErrorNotInitialized);
    Value value {};
    _ZN3sce4Json5ValueC1Ev_nid_no_patch(&value);
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKc_nid_no_patch(&value, "missing.json") == ErrorFileBufferSize);
    Document();
    NullAccess();
    Errors();
    Tree();
    Require(_ZN3sce4Json11Initializer9terminateEv_nid_no_patch(&initializer) == 0);
    Require(liveAllocations == 0);
    parameter.fileBufferSize = 4;
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE_nid_no_patch(&initializer, &parameter) == 0);
    Files();
    _ZN3sce4Json11InitializerD1Ev_nid_no_patch(&initializer);
    Require(_ZN3sce4Json11s_initparamE == nullptr && liveAllocations == 0);
    return 0;
}
