#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct String { void* storage; };
struct Pair { String key; std::uint64_t reserved; Value value; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; std::int32_t mode; std::uint32_t reserved[3]; };
using NullHandler = const Value& (APS5_VABI*)(std::int32_t, const Value*, void*);
using MismatchHandler = const Value& (APS5_VABI*)(std::int32_t, const Value*, const Value*, void*);
using FloatHandler = Value* (APS5_VABI*)(Value*, std::int32_t, double, const Value*, void*, bool*);
using ElementHandler = void (APS5_VABI*)(int, const String*, const std::size_t*, const Value*, const Value*, void*);
extern "C" {
void APS5_VABI _ZN3sce4Json5ValueC1El(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5Value3setEd(Value*, double);
const std::int64_t* APS5_VABI _ZNK3sce4Json5Value10getIntegerEv(const Value*);
const Value* APS5_VABI _ZNK3sce4Json5ValueixEPKc(const Value*, const char*);
int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(Value*, NullHandler, void*);
int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value*, MismatchHandler, void*);
int APS5_VABI _ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(Value*, FloatHandler, void*);
int APS5_VABI _ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(Value*, ElementHandler, void*);
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
int APS5_VABI _ZN3sce4Json5Value9serializeERNS0_6StringE(Value*, String*);
void APS5_VABI _ZN3sce4Json6StringC1EPKc(String*, const char*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
void APS5_VABI _ZN3sce4Json6Object4PairC1Ev(Pair*);
void APS5_VABI _ZN3sce4Json6Object4PairC2Ev(Pair*);
void APS5_VABI _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(Pair*, const String*, const Value*);
void APS5_VABI _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(Pair*, const String*, const Value*);
void APS5_VABI _ZN3sce4Json6Object4PairD1Ev(Pair*);
void APS5_VABI _ZN3sce4Json6Object4PairD2Ev(Pair*);
}
static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct PairTrace {
    std::map<void*, std::size_t> live;
    std::size_t generation = 0;
    std::vector<std::size_t> requests;
    std::vector<std::size_t> releases;
    int budget = -1;
    bool once = false;
    unsigned notices = 0;
};
static void* APS5_VABI AllocatePair(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<PairTrace*>(context);
    trace.requests.push_back(bytes);
    if (trace.budget == 0) { if (trace.once) trace.budget = -1; return nullptr; }
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes);
    Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes);
    Require(trace.live.emplace(memory, ++trace.generation).second, "Pair storage must have an independently owned allocation generation");
    return memory;
}
static void APS5_VABI FreePair(void*, void* memory, void* context) {
    auto& trace = *static_cast<PairTrace*>(context);
    trace.releases.push_back(memory ? trace.live.at(memory) : 0);
    if (!memory) return;
    Require(trace.live.erase(memory) == 1, "Pair teardown must release each allocation through its owner exactly once");
    std::free(memory);
}
static void APS5_VABI NotifyPair(void*, int error, std::size_t size, void* context) {
    auto& trace = *static_cast<PairTrace*>(context);
    Require(static_cast<unsigned>(error) == 0x80848102 && trace.requests.back() == size, "Pair notification must identify the failed allocation request");
    ++trace.notices;
}
static void* const PairVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocatePair), reinterpret_cast<void*>(FreePair), reinterpret_cast<void*>(NotifyPair)};
struct PairAllocator { void* const* vtable = PairVtable; };
static std::string Serialize(Value& value) {
    String output{}; _ZN3sce4Json6StringC1EPKc(&output, "");
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&value, &output) == 0, "Constructed pair values must remain serializable");
    std::string text = _ZNK3sce4Json6String5c_strEv(&output);
    _ZN3sce4Json6StringD1Ev(&output);
    return text;
}
static void CheckMetadata(const Pair& pair, std::uint64_t pattern) {
    Require(pair.reserved == pattern, "Pair construction must preserve the eight inactive bytes between key and value");
    Require(!pair.value.parent && !pair.value.auxiliary && pair.value.reserved == static_cast<std::uint32_t>(pattern), "Pair value construction must initialize ownership without overwriting inactive reserved bytes");
}
struct InspectArrayLink { InspectArrayLink* previous; InspectArrayLink* next; Value value; };
struct InspectArray { InspectArrayLink* head; std::size_t count; std::uint64_t reserved; Value* owner; };
struct InspectObjectLink { InspectObjectLink* parent; InspectObjectLink* left; InspectObjectLink* right; unsigned char key[24]; Pair pair; std::uint8_t color; std::uint8_t nil; unsigned char reserved[6]; };
struct InspectObject { InspectObjectLink* head; std::size_t count; std::uint64_t reserved; Value* owner; };
static_assert(sizeof(InspectArrayLink) == 48 && offsetof(InspectArrayLink, value) == 16);
static_assert(sizeof(InspectObjectLink) == 104 && offsetof(InspectObjectLink, nil) == 97 && offsetof(InspectObjectLink, pair) == 48);
static void Shape(Value& value, const Value* parent, std::string& output);
static int Identity(const Value* actual, const Value* expected) { return actual ? (actual == expected ? 1 : 2) : 0; }
static void TreeShape(InspectObjectLink* node, const Value* parent, std::string& output) {
    if (node->nil) return;
    TreeShape(node->left, parent, output);
    Shape(node->pair.value, parent, output);
    TreeShape(node->right, parent, output);
}
static void Shape(Value& value, const Value* parent, std::string& output) {
    output += std::to_string(value.type) + ":" + std::to_string(Identity(value.parent, parent));
    if (value.type == 6) {
        auto* header = *reinterpret_cast<InspectArray**>(value.payload);
        if (!header) { output += "x"; return; }
        output += "a" + std::to_string(header->count) + ":" + std::to_string(Identity(header->owner, &value)) + "[";
        if (!header->head) { output += "missing]"; return; }
        for (auto* node = header->head->next; node != header->head; node = node->next) Shape(node->value, &value, output);
        output += "]";
    }
    if (value.type == 7) {
        auto* header = *reinterpret_cast<InspectObject**>(value.payload);
        if (!header) { output += "x"; return; }
        output += "o" + std::to_string(header->count) + ":" + std::to_string(Identity(header->owner, &value)) + "{";
        if (!header->head) { output += "missing}"; return; }
        TreeShape(header->head->parent, &value, output);
        output += "}";
    }
    output += ";";
}
static std::uint64_t Fingerprint(const std::vector<std::size_t>& values, std::size_t first, std::size_t end, std::size_t generation = 0) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (auto i = first; i < end; ++i) {
        hash ^= values[i] ? values[i] - generation : 0;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
struct Expected {
    unsigned source;
    bool once;
    int budget;
    int type;
    const char* key;
    const char* text;
    const char* shape;
    unsigned notifications;
    std::size_t requests;
    std::uint64_t requestOrder;
    std::size_t constructionReleases;
    std::uint64_t constructionOrder;
    std::size_t destructionReleases;
    std::uint64_t destructionOrder;
};
static const Expected Cases[]{
    {0, 0, 0, 0, "", "null", "0:0;", 1, 1, UINT64_C(0xaf63d54c8601e0a7), 0, UINT64_C(0xcbf29ce484222325), 0, UINT64_C(0xcbf29ce484222325)},
    {0, 0, 1, 0, "", "null", "0:0;", 0, 1, UINT64_C(0xaf63d54c8601e0a7), 0, UINT64_C(0xcbf29ce484222325), 1, UINT64_C(0xaf63bc4c8601b62c)},
    {0, 1, 0, 0, "", "null", "0:0;", 1, 1, UINT64_C(0xaf63d54c8601e0a7), 0, UINT64_C(0xcbf29ce484222325), 0, UINT64_C(0xcbf29ce484222325)},
    {0, 1, 1, 0, "", "null", "0:0;", 0, 1, UINT64_C(0xaf63d54c8601e0a7), 0, UINT64_C(0xcbf29ce484222325), 1, UINT64_C(0xaf63bc4c8601b62c)},
    {1, 0, 0, 0, "", "null", "0:0;", 1, 1, UINT64_C(0xaf63d54c8601e0a7), 0, UINT64_C(0xcbf29ce484222325), 0, UINT64_C(0xcbf29ce484222325)},
    {1, 0, 1, 0, "", "null", "0:0;", 1, 2, UINT64_C(0x08841407b530b4f9), 0, UINT64_C(0xcbf29ce484222325), 1, UINT64_C(0xaf63bc4c8601b62c)},
    {1, 0, 2, 0, "key", "null", "0:0;", 0, 2, UINT64_C(0x08841407b530b4f9), 0, UINT64_C(0xcbf29ce484222325), 2, UINT64_C(0x08395307b4f1348c)},
    {1, 1, 0, 0, "", "null", "0:0;", 1, 1, UINT64_C(0xaf63d54c8601e0a7), 0, UINT64_C(0xcbf29ce484222325), 0, UINT64_C(0xcbf29ce484222325)},
    {1, 1, 1, 0, "", "null", "0:0;", 1, 2, UINT64_C(0x08841407b530b4f9), 0, UINT64_C(0xcbf29ce484222325), 1, UINT64_C(0xaf63bc4c8601b62c)},
    {1, 1, 2, 0, "key", "null", "0:0;", 0, 2, UINT64_C(0x08841407b530b4f9), 0, UINT64_C(0xcbf29ce484222325), 2, UINT64_C(0x08395307b4f1348c)},
    {2, 0, 0, 0, "", "null", "0:0;", 2, 2, UINT64_C(0x08842007b530c95d), 0, UINT64_C(0xcbf29ce484222325), 0, UINT64_C(0xcbf29ce484222325)},
    {2, 0, 1, 0, "", "null", "0:0;", 2, 3, UINT64_C(0xa922fa18e1c37583), 0, UINT64_C(0xcbf29ce484222325), 1, UINT64_C(0xaf63bc4c8601b62c)},
    {2, 0, 2, 0, "key", "null", "0:0;", 1, 3, UINT64_C(0xa922fa18e1c37583), 0, UINT64_C(0xcbf29ce484222325), 2, UINT64_C(0x08395307b4f1348c)},
    {2, 0, 3, 0, "key", "null", "0:0;", 1, 4, UINT64_C(0x29e493479f20d661), 1, UINT64_C(0xaf63be4c8601b992), 2, UINT64_C(0x08395307b4f1348c)},
    {2, 0, 4, 5, "key", "\"\"", "5:0;", 1, 5, UINT64_C(0x503caab364cc52b8), 0, UINT64_C(0xcbf29ce484222325), 4, UINT64_C(0xb345225e3644edb5)},
    {2, 0, 5, 5, "key", "\"recovery\"", "5:0;", 0, 5, UINT64_C(0x503caab364cc52b8), 0, UINT64_C(0xcbf29ce484222325), 5, UINT64_C(0x3f3fcc261a338844)},
    {2, 1, 0, 5, "", "\"recovery\"", "5:0;", 1, 4, UINT64_C(0x919fd647d9d25bc2), 0, UINT64_C(0xcbf29ce484222325), 3, UINT64_C(0xe1efc21870f169c3)},
    {2, 1, 1, 5, "", "\"recovery\"", "5:0;", 1, 5, UINT64_C(0x503caab364cc52b8), 0, UINT64_C(0xcbf29ce484222325), 4, UINT64_C(0xb345225e3644edb5)},
    {2, 1, 2, 0, "key", "null", "0:0;", 1, 3, UINT64_C(0xa922fa18e1c37583), 0, UINT64_C(0xcbf29ce484222325), 2, UINT64_C(0x08395307b4f1348c)},
    {2, 1, 3, 0, "key", "null", "0:0;", 1, 4, UINT64_C(0x29e493479f20d661), 1, UINT64_C(0xaf63be4c8601b992), 2, UINT64_C(0x08395307b4f1348c)},
    {2, 1, 4, 5, "key", "\"\"", "5:0;", 1, 5, UINT64_C(0x503caab364cc52b8), 0, UINT64_C(0xcbf29ce484222325), 4, UINT64_C(0xb345225e3644edb5)},
    {2, 1, 5, 5, "key", "\"recovery\"", "5:0;", 0, 5, UINT64_C(0x503caab364cc52b8), 0, UINT64_C(0xcbf29ce484222325), 5, UINT64_C(0x3f3fcc261a338844)},
    {3, 0, 0, 0, "", "null", "0:0;", 2, 2, UINT64_C(0x08842007b530c95d), 0, UINT64_C(0xcbf29ce484222325), 0, UINT64_C(0xcbf29ce484222325)},
    {3, 0, 1, 0, "", "null", "0:0;", 2, 3, UINT64_C(0xa922fa18e1c37583), 0, UINT64_C(0xcbf29ce484222325), 1, UINT64_C(0xaf63bc4c8601b62c)},
    {3, 0, 2, 0, "key", "null", "0:0;", 1, 3, UINT64_C(0xa922fa18e1c37583), 0, UINT64_C(0xcbf29ce484222325), 2, UINT64_C(0x08395307b4f1348c)},
    {3, 0, 3, 0, "key", "null", "0:0;", 1, 4, UINT64_C(0x29e49b479f20e3f9), 1, UINT64_C(0xaf63be4c8601b992), 2, UINT64_C(0x08395307b4f1348c)},
    {3, 0, 4, 0, "key", "null", "0:0;", 1, 5, UINT64_C(0x50576bb364e2af63), 2, UINT64_C(0x0824ed07b4dfde30), 2, UINT64_C(0x08395307b4f1348c)},
    {3, 0, 5, 7, "key", "{}", "7:0o0:1{};", 1, 6, UINT64_C(0x673b44d46d2fced9), 1, UINT64_C(0xaf63bd4c8601b7df), 5, UINT64_C(0x3f3fcc261a338844)},
    {3, 0, 6, 7, "key", "{}", "7:0o0:1{};", 1, 7, UINT64_C(0x9984a5f5883c36c3), 1, UINT64_C(0xaf63bb4c8601b479), 5, UINT64_C(0x3f3fcc261a338844)},
    {3, 0, 7, 7, "key", "{}", "7:0o0:1{};", 12, 19, UINT64_C(0x57b5704e320be159), 2, UINT64_C(0x08218e07b4dd06ec), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 8, 7, "key", "{}", "7:0o0:1{};", 12, 20, UINT64_C(0xf367fb55d54c9203), 3, UINT64_C(0x1e87e8189340ae76), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 9, 7, "key", "{}", "7:0o0:1{};", 13, 22, UINT64_C(0xf5aab40101cc37d3), 4, UINT64_C(0xf241d4ba207d8e95), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 10, 7, "key", "{}", "7:0o0:1{};", 12, 22, UINT64_C(0xf5aab40101cc37d3), 5, UINT64_C(0x7e6b1d36db19d80d), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 11, 7, "key", "{}", "7:0o0:1{};", 13, 24, UINT64_C(0xfe76783938049f49), 6, UINT64_C(0x74e8056682af8fd0), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 12, 7, "key", "{}", "7:0o0:1{};", 12, 24, UINT64_C(0xfe76783938049f49), 7, UINT64_C(0x7693bb7c7546b046), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 13, 7, "key", "{}", "7:0o0:1{};", 13, 26, UINT64_C(0xde319778ed12258b), 8, UINT64_C(0x4468dd3409107207), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 14, 7, "key", "{}", "7:0o0:1{};", 12, 26, UINT64_C(0xde319778ed12258b), 9, UINT64_C(0xad55aca26461f6ad), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 15, 7, "key", "{}", "7:0o0:1{};", 12, 27, UINT64_C(0xc7ec986f14c73639), 10, UINT64_C(0x4235c494c06acdf8), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 16, 7, "key", "{}", "7:0o0:1{};", 14, 30, UINT64_C(0x8a81bd5371368fab), 12, UINT64_C(0x57e8bc377377a1a8), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 17, 7, "key", "{}", "7:0o0:1{};", 13, 30, UINT64_C(0x8a81bd5371368fab), 13, UINT64_C(0x1268d2826954245d), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 18, 7, "key", "{}", "7:0o0:1{};", 13, 31, UINT64_C(0x96301b105a1ce429), 13, UINT64_C(0xf35671decbb938c3), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 19, 7, "key", "{}", "7:0o0:1{};", 12, 31, UINT64_C(0x96301b105a1ce429), 14, UINT64_C(0x0cab5608e565adf6), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 20, 7, "key", "{}", "7:0o0:1{};", 12, 32, UINT64_C(0x3bc91ee93ed610f9), 15, UINT64_C(0x794fa5db173e30f0), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 21, 7, "key", "{}", "7:0o0:1{};", 11, 32, UINT64_C(0x3bc91ee93ed610f9), 16, UINT64_C(0x41fe94923964f0cb), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 22, 7, "key", "{}", "7:0o0:1{};", 11, 33, UINT64_C(0x8b101e49bc1d3b51), 17, UINT64_C(0xbbeef34e524f8b27), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 23, 7, "key", "{}", "7:0o0:1{};", 10, 33, UINT64_C(0x8b101e49bc1d3b51), 18, UINT64_C(0x655747e04dd5b78a), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 24, 7, "key", "{}", "7:0o0:1{};", 10, 34, UINT64_C(0x4670bb37ce1cd743), 19, UINT64_C(0x575dab813e787668), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 25, 7, "key", "{}", "7:0o0:1{};", 11, 36, UINT64_C(0xbdb571159b86105b), 21, UINT64_C(0xad7895c02a7368e9), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 26, 7, "key", "{}", "7:0o0:1{};", 10, 36, UINT64_C(0xbdb571159b86105b), 22, UINT64_C(0xca7465d28bfd5461), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 27, 7, "key", "{}", "7:0o0:1{};", 10, 37, UINT64_C(0x44d2b5c68fb25c01), 22, UINT64_C(0xad57f867f29b0856), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 28, 7, "key", "{}", "7:0o0:1{};", 9, 37, UINT64_C(0x44d2b5c68fb25c01), 23, UINT64_C(0xa4f6f9a06ded132c), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {3, 0, 29, 7, "key", "{\"\":null}", "7:0o1:1{0:0;};", 9, 38, UINT64_C(0x98134cae02268541), 23, UINT64_C(0xa4f6f9a06ded132c), 6, UINT64_C(0x5883fada9831d117)},
    {3, 0, 30, 7, "key", "{\"\":null}", "7:0o1:1{0:0;};", 10, 40, UINT64_C(0xef7eec8809648b01), 23, UINT64_C(0xa4f6f9a06ded132c), 7, UINT64_C(0x6c521e5e6410ac8b)},
    {3, 0, 31, 7, "key", "{\"\":null}", "7:0o1:1{0:0;};", 10, 41, UINT64_C(0xe9e59080116a59f1), 23, UINT64_C(0xa4f6f9a06ded132c), 8, UINT64_C(0x86a3a0747ab65d86)},
    {3, 0, 32, 7, "key", "{\"a\":null}", "7:0o1:1{0:0;};", 9, 41, UINT64_C(0xe9e59080116a59f1), 23, UINT64_C(0xa4f6f9a06ded132c), 9, UINT64_C(0xacf3512bbda582e0)},
    {3, 0, 33, 7, "key", "{\"a\":null}", "7:0o1:1{0:0;};", 9, 42, UINT64_C(0xeb3e6815095e9583), 24, UINT64_C(0x7e7974b5e6a575d5), 9, UINT64_C(0xacf3512bbda582e0)},
    {3, 0, 34, 7, "key", "{\"a\":[]}", "7:0o1:1{6:0a0:1[missing]};", 10, 44, UINT64_C(0xfbc65d9e64e2dd2b), 24, UINT64_C(0x0d47adecfbfc430c), 11, UINT64_C(0xf238d2bde87a012f)},
    {3, 0, 35, 7, "key", "{\"a\":[]}", "7:0o1:1{6:0a0:1[];};", 9, 44, UINT64_C(0xfbc65d9e64e2dd2b), 24, UINT64_C(0x0d47adecfbfc430c), 12, UINT64_C(0xaf25e2b8d279d422)},
    {3, 0, 36, 7, "key", "{\"a\":[]}", "7:0o1:1{6:0a0:1[];};", 9, 45, UINT64_C(0x62622be92d64ec21), 24, UINT64_C(0x88b696849119ed00), 12, UINT64_C(0xaf25e2b8d279d422)},
    {3, 0, 37, 7, "key", "{\"a\":[]}", "7:0o1:1{6:1a0:1[];};", 9, 46, UINT64_C(0xd5087cc731249313), 26, UINT64_C(0x945498db768529bf), 12, UINT64_C(0x1402fa0bcc4f6376)},
    {3, 0, 38, 7, "key", "{\"a\":[]}", "7:0o1:1{6:1a0:1[];};", 9, 47, UINT64_C(0xdb649185d9c21c79), 26, UINT64_C(0xf414310c6d6a89e5), 12, UINT64_C(0x1402fa0bcc4f6376)},
    {3, 0, 39, 7, "key", "{\"a\":[]}", "7:0o1:1{6:1a0:1[];};", 9, 48, UINT64_C(0x82d36a62e335fbeb), 27, UINT64_C(0x02643bbfdf0d87ea), 12, UINT64_C(0x11ebb0aab788d892)},
    {3, 0, 40, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 8, 48, UINT64_C(0x82d36a62e335fbeb), 27, UINT64_C(0x02643bbfdf0d87ea), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 41, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 8, 49, UINT64_C(0xb283cbd1b5b5f209), 28, UINT64_C(0xa0a087072c09fde7), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 42, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 8, 50, UINT64_C(0x8a9162aba4d6acf3), 29, UINT64_C(0x9c20542712c0a0ef), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 43, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 9, 52, UINT64_C(0x50f37a8bb3884709), 30, UINT64_C(0xcfe6f26979584abc), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 44, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 8, 52, UINT64_C(0x50f37a8bb3884709), 31, UINT64_C(0xf18fa8ba86856bca), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 45, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 9, 54, UINT64_C(0x62ceafb6eec98ffb), 32, UINT64_C(0xa11b393a4792a7cd), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 46, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 8, 54, UINT64_C(0x62ceafb6eec98ffb), 33, UINT64_C(0x9fda7bb9fe326757), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 47, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 8, 55, UINT64_C(0x6b6a32eab66db2f9), 34, UINT64_C(0x7cc8e9986e8bf23c), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 48, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 8, 56, UINT64_C(0xaecb93612cbf336b), 35, UINT64_C(0x684ccd1b16fbce2e), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 49, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 8, 57, UINT64_C(0x4858d152b25ec04b), 36, UINT64_C(0xf3d7ebb38a437d51), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 50, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 7, 57, UINT64_C(0x4858d152b25ec04b), 37, UINT64_C(0x3c9b54e75e043a07), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 51, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 7, 58, UINT64_C(0xfa7380bc30541f43), 38, UINT64_C(0x557d299a96c25a7a), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 52, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 6, 58, UINT64_C(0xfa7380bc30541f43), 39, UINT64_C(0x11fc770274e9a8ea), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 53, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 6, 59, UINT64_C(0x72407261f2c46ee9), 40, UINT64_C(0x8be5dea685297ad5), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 54, 7, "key", "{\"a\":[true]}", "7:0o1:1{6:1a1:1[1:1;];};", 5, 59, UINT64_C(0x72407261f2c46ee9), 41, UINT64_C(0xc570cb729193a42f), 13, UINT64_C(0xe364508a410c0844)},
    {3, 0, 55, 7, "key", "{\"a\":[true],\"\":null}", "7:0o2:1{6:1a1:1[1:1;];0:0;};", 7, 62, UINT64_C(0xdd828768b0673cb1), 41, UINT64_C(0xc570cb729193a42f), 14, UINT64_C(0xd51537a7c0e9393d)},
    {3, 0, 56, 7, "key", "{\"a\":[true],\"\":null}", "7:0o2:1{6:1a1:1[1:1;];0:0;};", 6, 62, UINT64_C(0xdd828768b0673cb1), 41, UINT64_C(0xc570cb729193a42f), 15, UINT64_C(0x42cc3f104f6e4243)},
    {3, 0, 57, 7, "key", "{\"a\":[true],\"\":null}", "7:0o2:1{6:1a1:1[1:1;];0:0;};", 6, 63, UINT64_C(0x6d02e9bcd600b499), 41, UINT64_C(0xc570cb729193a42f), 16, UINT64_C(0x2a0391b8de74bd18)},
    {3, 0, 58, 7, "key", "{\"a\":[true],\"k\":null}", "7:0o2:1{6:1a1:1[1:1;];0:0;};", 5, 63, UINT64_C(0x6d02e9bcd600b499), 41, UINT64_C(0xc570cb729193a42f), 17, UINT64_C(0xb4aa802afc47ece0)},
    {3, 0, 59, 7, "key", "{\"a\":[true],\"k\":null}", "7:0o2:1{6:1a1:1[1:1;];0:0;};", 5, 64, UINT64_C(0x5f482a0d0dc8019b), 42, UINT64_C(0x65054fd4e246057e), 17, UINT64_C(0xb4aa802afc47ece0)},
    {3, 0, 60, 7, "key", "{\"a\":[true],\"k\":\"\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 5, 65, UINT64_C(0x4b4d917e57f86132), 41, UINT64_C(0xc570cb729193a42f), 19, UINT64_C(0x62909e1cec7ae1d3)},
    {3, 0, 61, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 4, 65, UINT64_C(0x4b4d917e57f86132), 41, UINT64_C(0xc570cb729193a42f), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 62, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 4, 66, UINT64_C(0x972352fcc3940a32), 42, UINT64_C(0x28714bacd96afc31), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 63, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 4, 67, UINT64_C(0x9ac6fafb0fd4419a), 43, UINT64_C(0x642ec0bd91056886), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 64, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 4, 68, UINT64_C(0x06fc689c01f51722), 44, UINT64_C(0xa43d450f6a509362), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 65, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 5, 70, UINT64_C(0x0bea874fe70c8b8c), 45, UINT64_C(0x1fceee1263cab1b3), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 66, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 4, 70, UINT64_C(0x0bea874fe70c8b8c), 46, UINT64_C(0xc764841a6cf73959), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 67, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 5, 72, UINT64_C(0x5cf6bc91b66abd26), 47, UINT64_C(0xca2d698260e6ceec), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 68, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 4, 72, UINT64_C(0x5cf6bc91b66abd26), 48, UINT64_C(0x176b5b2a4ffd504a), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 69, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 4, 73, UINT64_C(0x78e6de7db4f012a6), 49, UINT64_C(0xfce7936c92f12f1b), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 70, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 4, 74, UINT64_C(0x783a50ac559170ce), 50, UINT64_C(0x91a5fbcf95a76d5d), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 71, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 5, 76, UINT64_C(0x5a15fa6e26c98802), 52, UINT64_C(0x23a18c828327ce44), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 72, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 5, 77, UINT64_C(0xa82a00a0586d91aa), 52, UINT64_C(0xcae276259cf4c86c), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 73, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 4, 77, UINT64_C(0xa82a00a0586d91aa), 53, UINT64_C(0xeaa902eb2ee886e5), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 74, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 4, 78, UINT64_C(0x626f4aa7f91ef32c), 54, UINT64_C(0x486d2771cfef87e7), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 75, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 3, 78, UINT64_C(0x626f4aa7f91ef32c), 55, UINT64_C(0xb7e4a214cb3bfcc8), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 76, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 3, 79, UINT64_C(0xb91abd6c8103cf8a), 56, UINT64_C(0x7862cb1cd7eccd70), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 77, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 2, 79, UINT64_C(0xb91abd6c8103cf8a), 57, UINT64_C(0x77989b1f55d4e559), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 78, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 2, 80, UINT64_C(0x8b4ae15f38aa07ee), 58, UINT64_C(0x7d721ed202be022f), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 79, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 2, 81, UINT64_C(0x5a447acd48eac8b2), 59, UINT64_C(0x4bb22e9ffd64c5f0), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 80, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 2, 82, UINT64_C(0x4e1a64d2e7c37f86), 61, UINT64_C(0x80ae7b36aecc0210), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 81, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 2, 83, UINT64_C(0x7a59405fd132616a), 61, UINT64_C(0x7f28d8f6eb248efd), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 82, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 1, 83, UINT64_C(0x7a59405fd132616a), 62, UINT64_C(0x048d19db3da2b56b), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 0, 83, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 3, 86, UINT64_C(0x9f5b0f8c3ddf0238), 62, UINT64_C(0x048d19db3da2b56b), 21, UINT64_C(0x690a605d38c59743)},
    {3, 0, 84, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 2, 86, UINT64_C(0x9f5b0f8c3ddf0238), 62, UINT64_C(0x048d19db3da2b56b), 22, UINT64_C(0x6a3f1a2a443276c5)},
    {3, 0, 85, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 2, 87, UINT64_C(0xa6a9474d21df7fca), 62, UINT64_C(0x048d19db3da2b56b), 23, UINT64_C(0x3f954ced238070f8)},
    {3, 0, 86, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 1, 87, UINT64_C(0xa6a9474d21df7fca), 62, UINT64_C(0x048d19db3da2b56b), 24, UINT64_C(0x601715e2f78af79c)},
    {3, 0, 87, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 1, 88, UINT64_C(0x112412108ec65a9e), 63, UINT64_C(0x333ba418287f24ee), 24, UINT64_C(0x601715e2f78af79c)},
    {3, 0, 88, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 1, 89, UINT64_C(0xe6a5a8229b0c9002), 64, UINT64_C(0x72c6e24c51035e34), 24, UINT64_C(0x601715e2f78af79c)},
    {3, 0, 89, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 90, UINT64_C(0xf80cd4cd7658e9c6), 63, UINT64_C(0xe3718003aacfdf89), 27, UINT64_C(0x23c931a0f86ee070)},
    {3, 0, 90, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 91, UINT64_C(0xd6b74720191512aa), 63, UINT64_C(0x859ce136ed5a19e3), 27, UINT64_C(0x23c931a0f86ee070)},
    {3, 0, 91, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 1, 92, UINT64_C(0xee80658a9ece807e), 65, UINT64_C(0x6974aedd6bbd4bfe), 27, UINT64_C(0xb55d4a66e9e7e0ec)},
    {3, 0, 92, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 1, 93, UINT64_C(0x12aca08bd8e3a562), 65, UINT64_C(0x7cb2eddc0608127a), 27, UINT64_C(0xb55d4a66e9e7e0ec)},
    {3, 0, 93, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 3, 96, UINT64_C(0x3c333c9bf74f1224), 66, UINT64_C(0x154d3274d8faa6c5), 27, UINT64_C(0x998885b959d4fc68)},
    {3, 0, 94, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 4, 98, UINT64_C(0xb940cbe38e8e601e), 67, UINT64_C(0xae9fccd09bb2a1a9), 27, UINT64_C(0x998885b959d4fc68)},
    {3, 0, 95, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 3, 98, UINT64_C(0xb940cbe38e8e601e), 68, UINT64_C(0x61f4292581fe1020), 27, UINT64_C(0x998885b959d4fc68)},
    {3, 0, 96, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 4, 100, UINT64_C(0xfcbcf9f4cc7993e0), 69, UINT64_C(0xe79fa9bc314eb694), 27, UINT64_C(0x998885b959d4fc68)},
    {3, 0, 97, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 3, 100, UINT64_C(0xfcbcf9f4cc7993e0), 70, UINT64_C(0x588604dbc9a84b89), 27, UINT64_C(0x998885b959d4fc68)},
    {3, 0, 98, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 3, 101, UINT64_C(0x30f9e0f7075fc2b6), 71, UINT64_C(0x920d6877a5ad485b), 27, UINT64_C(0x998885b959d4fc68)},
    {3, 0, 99, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 2, 101, UINT64_C(0x30f9e0f7075fc2b6), 72, UINT64_C(0xf5adbbea7893fef0), 27, UINT64_C(0x998885b959d4fc68)},
    {3, 0, 100, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 2, 102, UINT64_C(0x9729f5c186b3bae0), 73, UINT64_C(0x29f0f6fd5ed356c0), 27, UINT64_C(0x998885b959d4fc68)},
    {3, 0, 101, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 1, 102, UINT64_C(0x9729f5c186b3bae0), 74, UINT64_C(0xa9a8d50e804943ed), 27, UINT64_C(0x998885b959d4fc68)},
    {3, 0, 102, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":0}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:0;};};", 2, 104, UINT64_C(0x234227d767437cfa), 74, UINT64_C(0xa9a8d50e804943ed), 28, UINT64_C(0x5765ff51e1564dec)},
    {3, 0, 103, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":0}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:0;};};", 1, 104, UINT64_C(0x234227d767437cfa), 74, UINT64_C(0xa9a8d50e804943ed), 29, UINT64_C(0x4d11975435094ccb)},
    {3, 0, 104, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":0}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:0;};};", 1, 105, UINT64_C(0x2ce6ab0477ad5968), 74, UINT64_C(0xa9a8d50e804943ed), 30, UINT64_C(0xb50667d7d86f8d99)},
    {3, 0, 105, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 0, 105, UINT64_C(0x2ce6ab0477ad5968), 74, UINT64_C(0xa9a8d50e804943ed), 31, UINT64_C(0x98a23224cfb0ffe0)},
    {3, 1, 0, 7, "", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 104, UINT64_C(0xa89112e8bdfbac92), 74, UINT64_C(0x6ea88119e6b74a5d), 29, UINT64_C(0x15bba93be47766f3)},
    {3, 1, 1, 7, "", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 105, UINT64_C(0x2ce6ab0477ad5968), 74, UINT64_C(0x3a169d78c5b81187), 30, UINT64_C(0xcddf81110b348447)},
    {3, 1, 2, 0, "key", "null", "0:0;", 1, 3, UINT64_C(0xa922fa18e1c37583), 0, UINT64_C(0xcbf29ce484222325), 2, UINT64_C(0x08395307b4f1348c)},
    {3, 1, 3, 0, "key", "null", "0:0;", 1, 4, UINT64_C(0x29e49b479f20e3f9), 1, UINT64_C(0xaf63be4c8601b992), 2, UINT64_C(0x08395307b4f1348c)},
    {3, 1, 4, 0, "key", "null", "0:0;", 1, 5, UINT64_C(0x50576bb364e2af63), 2, UINT64_C(0x0824ed07b4dfde30), 2, UINT64_C(0x08395307b4f1348c)},
    {3, 1, 5, 7, "key", "{}", "7:0o0:1{};", 1, 6, UINT64_C(0x673b44d46d2fced9), 1, UINT64_C(0xaf63bd4c8601b7df), 5, UINT64_C(0x3f3fcc261a338844)},
    {3, 1, 6, 7, "key", "{}", "7:0o0:1{};", 1, 7, UINT64_C(0x9984a5f5883c36c3), 1, UINT64_C(0xaf63bb4c8601b479), 5, UINT64_C(0x3f3fcc261a338844)},
    {3, 1, 7, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 91, UINT64_C(0x60517b283d4e7f28), 59, UINT64_C(0x70305a3ea5d29b0b), 31, UINT64_C(0x1987bbdf4f551fe8)},
    {3, 1, 8, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 92, UINT64_C(0x2cdb62dd8baca92e), 60, UINT64_C(0x19657326d928cdd8), 31, UINT64_C(0x2c299448de62a072)},
    {3, 1, 9, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 105, UINT64_C(0x2ce6ab0477ad5968), 73, UINT64_C(0x1fc7c6ebd1b98ec6), 31, UINT64_C(0xe32cffe4946de372)},
    {3, 1, 10, 7, "key", "{\"\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 98, UINT64_C(0x27dfd47a22924cde), 68, UINT64_C(0xad21aae5eb3e9d5c), 29, UINT64_C(0xe76f73f6e413690f)},
    {3, 1, 11, 7, "key", "{\"\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x54436514d18ad928), 71, UINT64_C(0xfeaee9cf685870dc), 30, UINT64_C(0xfad1497156057d51)},
    {3, 1, 12, 7, "key", "{\"\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 100, UINT64_C(0x0ec693ac9e264c48), 70, UINT64_C(0x46f3d395149d83b7), 29, UINT64_C(0x9292c45666d5f087)},
    {3, 1, 13, 7, "key", "{\"\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 103, UINT64_C(0x998db6ea3f64f750), 72, UINT64_C(0x6a8506724de1c15f), 30, UINT64_C(0x51fafc33547ec99a)},
    {3, 1, 14, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 94, UINT64_C(0xdd430a82fe7fe43e), 62, UINT64_C(0x6c1ea5a1d32dce85), 31, UINT64_C(0xaa062e38a46dc362)},
    {3, 1, 15, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 95, UINT64_C(0x020439d52e8973d0), 63, UINT64_C(0x6916ebb1bcc7d741), 31, UINT64_C(0xe99cd0eb7f04b0c0)},
    {3, 1, 16, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 105, UINT64_C(0x2ce6ab0477ad5968), 73, UINT64_C(0x010e7d553fd5eda0), 31, UINT64_C(0xe32cffe4946de372)},
    {3, 1, 17, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 104, UINT64_C(0x7d3ce9fb1cdc6896), 73, UINT64_C(0x1798f8572a342022), 31, UINT64_C(0xe70b60a34f05f80c)},
    {3, 1, 18, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 105, UINT64_C(0x2ce6ab0477ad5968), 73, UINT64_C(0x8df3952bc81fb19a), 31, UINT64_C(0xe32cffe4946de372)},
    {3, 1, 19, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 100, UINT64_C(0x2dfc24178ad786ae), 70, UINT64_C(0xab432aea1b343eb1), 29, UINT64_C(0x3bd25fc07522b6c7)},
    {3, 1, 20, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 101, UINT64_C(0xb1577d1dfe0dc2be), 71, UINT64_C(0xdd3a2a6eb7fab273), 29, UINT64_C(0xa4dacec9da60a18d)},
    {3, 1, 21, 7, "key", "{\"\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 99, UINT64_C(0x4f117cd47c5b07a8), 70, UINT64_C(0x68097b7d7ba01328), 28, UINT64_C(0x9d231f94a3d8505f)},
    {3, 1, 22, 7, "key", "{\"\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 101, UINT64_C(0xaf7fa9cbc785a706), 71, UINT64_C(0xdd3a2a6eb7fab273), 29, UINT64_C(0xa4dacec9da60a18d)},
    {3, 1, 23, 7, "key", "{\"a\":null,\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{0:0;5:1;7:1o1:1{3:1;};};", 1, 93, UINT64_C(0xfe8c535b61975fc0), 65, UINT64_C(0x52dcce1da8492cfc), 27, UINT64_C(0x6b2d1037cf396c3a)},
    {3, 1, 24, 7, "key", "{\"a\":null,\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{0:0;5:1;7:1o1:1{3:1;};};", 1, 94, UINT64_C(0x49656a6b416c8aae), 66, UINT64_C(0xb6023d8fb31fa833), 27, UINT64_C(0x351d6894476b65d4)},
    {3, 1, 25, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x5b8d98fa549824d6), 71, UINT64_C(0x3236647f1b32c90a), 30, UINT64_C(0x4c922b10d249c073)},
    {3, 1, 26, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 101, UINT64_C(0xfd645a891ab048d8), 71, UINT64_C(0x441133cd64aeff34), 30, UINT64_C(0x4bdf8fc323423212)},
    {3, 1, 27, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x5b8d98fa549824d6), 71, UINT64_C(0x6997d907cb99bf48), 30, UINT64_C(0x4c922b10d249c073)},
    {3, 1, 28, 7, "key", "{\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o2:1{5:1;7:1o1:1{3:1;};};", 1, 94, UINT64_C(0xc58789bf9039d336), 70, UINT64_C(0xa480724f4c9ef05f), 23, UINT64_C(0xaefbe7fa88ebe124)},
    {3, 1, 29, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x5b8d98fa549824d6), 72, UINT64_C(0xe344b830f3a11d8a), 29, UINT64_C(0xab5acbf8e6ca0bdf)},
    {3, 1, 30, 7, "key", "{\"\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 101, UINT64_C(0xaf7fa9cbc785a706), 72, UINT64_C(0x902c140387af47d3), 28, UINT64_C(0x66b09ba8d6eda091)},
    {3, 1, 31, 7, "key", "{\"\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x5b8d98fa549824d6), 72, UINT64_C(0xe344b830f3a11d8a), 29, UINT64_C(0xab5acbf8e6ca0bdf)},
    {3, 1, 32, 7, "key", "{\"a\":null,\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{0:0;5:1;7:1o1:1{3:1;};};", 1, 98, UINT64_C(0x63e791635b56fa46), 70, UINT64_C(0x8a8854f75fca9d93), 27, UINT64_C(0x2dfa80357ea0bdb4)},
    {3, 1, 33, 7, "key", "{\"a\":null,\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{0:0;5:1;7:1o1:1{3:1;};};", 1, 99, UINT64_C(0x4a646907627eac30), 71, UINT64_C(0x18b59e7d5b2fe673), 27, UINT64_C(0x2c0f7f6633f03dbe)},
    {3, 1, 34, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x5b8d98fa549824d6), 71, UINT64_C(0x8291d9ce0eae8d1d), 30, UINT64_C(0xeda7450c41cedbee)},
    {3, 1, 35, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 101, UINT64_C(0xfd5972cc2d7ea8a8), 71, UINT64_C(0x931494827e967b6a), 30, UINT64_C(0x52b5b7d7887eb988)},
    {3, 1, 36, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:0a0:1[];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x5b8d98fa549824d6), 71, UINT64_C(0xa4496da3e2ecc4d3), 30, UINT64_C(0x5c17b3d1c0a1b56e)},
    {3, 1, 37, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a0:1[];5:1;7:1o1:1{3:1;};};", 1, 103, UINT64_C(0x7bf409aa512991e0), 73, UINT64_C(0x1091b5eee814c0e9), 30, UINT64_C(0x21d27a28fc8ef91c)},
    {3, 1, 38, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a0:1[];5:1;7:1o1:1{3:1;};};", 1, 104, UINT64_C(0xe05d289941f12cfe), 73, UINT64_C(0xf799f9b52163552a), 30, UINT64_C(0xe93f3e2fb84426fe)},
    {3, 1, 39, 7, "key", "{\"a\":[],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a0:1[];5:1;7:1o1:1{3:1;};};", 1, 105, UINT64_C(0x2ce6ab0477ad5968), 74, UINT64_C(0xf82ec970d3cfc2e0), 30, UINT64_C(0x11bb3330ad0999c0)},
    {3, 1, 40, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 100, UINT64_C(0x835a6ff205dd6146), 68, UINT64_C(0x3b1c5be6adc3631c), 31, UINT64_C(0x9c1079f29a41bd24)},
    {3, 1, 41, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 101, UINT64_C(0x8d63785d9cbce268), 69, UINT64_C(0xa8cdc57a4d527c5c), 31, UINT64_C(0x520bc292ee5b0f92)},
    {3, 1, 42, 7, "key", "{\"a\":[true],\"\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 98, UINT64_C(0x520ba6b37d1f237e), 68, UINT64_C(0x2d7baac348149c62), 29, UINT64_C(0x8098f4d3abda36e7)},
    {3, 1, 43, 7, "key", "{\"a\":[true],\"\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0xec8ebd121f055228), 71, UINT64_C(0x2b515550cedda7c8), 30, UINT64_C(0x86928d977d93f2bb)},
    {3, 1, 44, 7, "key", "{\"a\":[true],\"\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 100, UINT64_C(0x66382e2716717628), 70, UINT64_C(0x0bdc83cc3c8b5cef), 29, UINT64_C(0x4ecf792831a2b853)},
    {3, 1, 45, 7, "key", "{\"a\":[true],\"\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 103, UINT64_C(0xe8acd57ecd9d0bd8), 72, UINT64_C(0xc0509a5f17c137f9), 30, UINT64_C(0xd78e7d4db72bdd00)},
    {3, 1, 46, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x2a58dcccc2fcfd9e), 70, UINT64_C(0x8d2f336da17fdc39), 31, UINT64_C(0x413ebc4e90581e70)},
    {3, 1, 47, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 103, UINT64_C(0x03a8c5ca4270b260), 71, UINT64_C(0xa46bf71b697ba90f), 31, UINT64_C(0x117b52679e50447a)},
    {3, 1, 48, 7, "key", "{\"k\":\"\",\"a\":[true],\"o\":{\"q\":1}}", "7:0o3:1{5:0;6:1a1:1[1:1;];7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x684c99dda1421b91), 72, UINT64_C(0xe793b57af9534fae), 29, UINT64_C(0x67733b1248cb0261)},
    {3, 1, 49, 7, "key", "{\"k\":\"\",\"a\":[true],\"o\":{\"q\":1}}", "7:0o3:1{5:0;6:1a1:1[1:1;];7:1o1:1{3:1;};};", 1, 103, UINT64_C(0xc103fd077d12a831), 73, UINT64_C(0x2fb728bc07b7ae3c), 29, UINT64_C(0xe2affcd4663837ef)},
    {3, 1, 50, 7, "key", "{\"a\":[true],\"\":\"\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:0;7:1o1:1{3:1;};};", 1, 101, UINT64_C(0xf9b8778ce5892e03), 72, UINT64_C(0xa6bcc4a926c9b17f), 28, UINT64_C(0xfe73d9bce6994e15)},
    {3, 1, 51, 7, "key", "{\"a\":[true],\"\":\"\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:0;7:1o1:1{3:1;};};", 1, 103, UINT64_C(0xba3b00c1c283e5e9), 73, UINT64_C(0x2fb728bc07b7ae3c), 29, UINT64_C(0xd1fe98172432957d)},
    {3, 1, 52, 7, "key", "{\"a\":[true],\"k\":null,\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];0:0;7:1o1:1{3:1;};};", 1, 101, UINT64_C(0xa5355b8a98f6cb8b), 72, UINT64_C(0xa6bcc4a926c9b17f), 28, UINT64_C(0xfe73d9bce6994e15)},
    {3, 1, 53, 7, "key", "{\"a\":[true],\"k\":null,\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];0:0;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0xe52e3935b858e4b9), 73, UINT64_C(0x48dbc55354a5d5f7), 28, UINT64_C(0xffb0dab42fe56888)},
    {3, 1, 54, 7, "key", "{\"a\":[true],\"o\":{\"q\":1}}", "7:0o2:1{6:1a1:1[1:1;];7:1o1:1{3:1;};};", 1, 99, UINT64_C(0x9796ad14651215a3), 74, UINT64_C(0x95d5583f8bb4071a), 24, UINT64_C(0xd729a5f6b715fe63)},
    {3, 1, 55, 7, "key", "{\"a\":[true],\"k\":\"\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:0;7:1o1:1{3:1;};};", 1, 104, UINT64_C(0x555db4043c851841), 74, UINT64_C(0x54851835c64759d3), 29, UINT64_C(0xe734f45587bfefcb)},
    {3, 1, 56, 7, "key", "{\"a\":[true],\"\":\"\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:0;7:1o1:1{3:1;};};", 1, 103, UINT64_C(0xba3b00c1c283e5e9), 74, UINT64_C(0xb84e1822ab185956), 28, UINT64_C(0xb37adbd00a14818f)},
    {3, 1, 57, 7, "key", "{\"a\":[true],\"\":\"\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:0;7:1o1:1{3:1;};};", 1, 104, UINT64_C(0x555db4043c851841), 74, UINT64_C(0x54851835c64759d3), 29, UINT64_C(0xe734f45587bfefcb)},
    {3, 1, 58, 7, "key", "{\"a\":[true],\"k\":null,\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];0:0;7:1o1:1{3:1;};};", 1, 103, UINT64_C(0x70181eb04816bf33), 74, UINT64_C(0xb84e1822ab185956), 28, UINT64_C(0xb37adbd00a14818f)},
    {3, 1, 59, 7, "key", "{\"a\":[true],\"k\":null,\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];0:0;7:1o1:1{3:1;};};", 1, 104, UINT64_C(0x555db4043c851841), 75, UINT64_C(0x93238bb2f717715c), 28, UINT64_C(0x6b0001fee18a4342)},
    {3, 1, 60, 7, "key", "{\"a\":[true],\"k\":\"\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 105, UINT64_C(0x2ce6ab0477ad5968), 74, UINT64_C(0x152a73fddccffd18), 30, UINT64_C(0x2919611d528894b6)},
    {3, 1, 61, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 91, UINT64_C(0xc4be6f24764b02dc), 59, UINT64_C(0x70b92cf6f5e020be), 31, UINT64_C(0xb62ceac18acff7bb)},
    {3, 1, 62, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 92, UINT64_C(0x61f826a2990473dc), 60, UINT64_C(0xf32be6d03cbedc94), 31, UINT64_C(0xde4498e1728d6c04)},
    {3, 1, 63, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 93, UINT64_C(0x969ed331086ff354), 61, UINT64_C(0xd551a80e1affde03), 31, UINT64_C(0xaf4ee52420b1c109)},
    {3, 1, 64, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 98, UINT64_C(0x79853297e30e1d4c), 68, UINT64_C(0xb65d101d0531c0a2), 29, UINT64_C(0x46c716fcc1328a3d)},
    {3, 1, 65, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x12ef72ce739f4d0e), 71, UINT64_C(0xb4e6bb7515f3cf72), 30, UINT64_C(0x99227e2156785871)},
    {3, 1, 66, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 100, UINT64_C(0x6f83d649fe8c8c72), 70, UINT64_C(0x44eac57908774015), 29, UINT64_C(0x5fcef1cfacefbfb7)},
    {3, 1, 67, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 103, UINT64_C(0x2804f6f1e71ce3f8), 72, UINT64_C(0xb02c45a9887be1a2), 30, UINT64_C(0xf770764b4ef7ecf9)},
    {3, 1, 68, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 94, UINT64_C(0x7c26769eabcb532c), 62, UINT64_C(0x73f92b18a24d6d95), 31, UINT64_C(0x824ac31fee78d5ba)},
    {3, 1, 69, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 95, UINT64_C(0xafbd5cdea9c2bcac), 63, UINT64_C(0xb0539f5485162014), 31, UINT64_C(0xdbe2448368048507)},
    {3, 1, 70, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 96, UINT64_C(0xb33822e607e26de4), 64, UINT64_C(0x33f9c22d193bedf6), 31, UINT64_C(0x51382e7a7c032910)},
    {3, 1, 71, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 104, UINT64_C(0xc06e6a557e87d880), 73, UINT64_C(0x7a0b8e235a97a118), 31, UINT64_C(0xd26bf9954e0a7e12)},
    {3, 1, 72, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 105, UINT64_C(0x2ce6ab0477ad5968), 73, UINT64_C(0x61bb4f31921812e7), 31, UINT64_C(0x2e08c6c7d56c3047)},
    {3, 1, 73, 7, "key", "{\"o\":{},\"a\":[true],\"k\":\"recovery\"}", "7:0o3:1{7:0o0:1{};6:1a1:1[1:1;];5:1;};", 1, 89, UINT64_C(0x109ea5036371589a), 62, UINT64_C(0x340e380554d0657b), 26, UINT64_C(0xd9113597a2d80f61)},
    {3, 1, 74, 7, "key", "{\"o\":{},\"a\":[true],\"k\":\"recovery\"}", "7:0o3:1{7:0o0:1{};6:1a1:1[1:1;];5:1;};", 1, 90, UINT64_C(0x9244c3a01dcace7c), 63, UINT64_C(0xd24b3c75b921bd74), 26, UINT64_C(0x1c37ca450bfc8a3d)},
    {3, 1, 75, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 88, UINT64_C(0x9f631208c5a8d666), 62, UINT64_C(0xe93b1055211f4f47), 25, UINT64_C(0xd4a2aa2952606b15)},
    {3, 1, 76, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 90, UINT64_C(0xd3b8a67d8b3576f8), 63, UINT64_C(0xd24b3c75b921bd74), 26, UINT64_C(0xf4e67f641f73ecad)},
    {3, 1, 77, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 1, 82, UINT64_C(0xe0e7732a58e74766), 57, UINT64_C(0x77989b1f55d4e559), 24, UINT64_C(0x607bb464128767f0)},
    {3, 1, 78, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 1, 83, UINT64_C(0x7d5bc46063a8c1e2), 58, UINT64_C(0x7d721ed202be022f), 24, UINT64_C(0x6c0528f9d1ca95c4)},
    {3, 1, 79, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 1, 84, UINT64_C(0x0c778e767eab97ae), 59, UINT64_C(0x4bb22e9ffd64c5f0), 24, UINT64_C(0xedf2404c46b20bf4)},
    {3, 1, 80, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 90, UINT64_C(0x304cacc2dca590f6), 63, UINT64_C(0x29777f1b2af8f733), 27, UINT64_C(0x756f2f41adc7104a)},
    {3, 1, 81, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 91, UINT64_C(0xd6b74720191512aa), 63, UINT64_C(0xb522ebbedf5074e6), 27, UINT64_C(0x122bfe467a81181b)},
    {3, 1, 82, 7, "key", "{\"a\":[true],\"k\":\"recovery\"}", "7:0o2:1{6:1a1:1[1:1;];5:1;};", 1, 83, UINT64_C(0x7a59405fd132616a), 62, UINT64_C(0x048d19db3da2b56b), 20, UINT64_C(0x1a0725f3cd62f080)},
    {3, 1, 83, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 91, UINT64_C(0xd6b74720191512aa), 64, UINT64_C(0x72c6e24c51035e34), 26, UINT64_C(0x4fe2311fc08c62d1)},
    {3, 1, 84, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 90, UINT64_C(0xd3b8a67d8b3576f8), 64, UINT64_C(0x0c961ea2c6d9bdd0), 25, UINT64_C(0xb192d70ba7d71d53)},
    {3, 1, 85, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 91, UINT64_C(0xd6b74720191512aa), 64, UINT64_C(0x72c6e24c51035e34), 26, UINT64_C(0x4fe2311fc08c62d1)},
    {3, 1, 86, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 1, 87, UINT64_C(0xa6a9474d21df7fca), 62, UINT64_C(0x048d19db3da2b56b), 24, UINT64_C(0x601715e2f78af79c)},
    {3, 1, 87, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 1, 88, UINT64_C(0x112412108ec65a9e), 63, UINT64_C(0x333ba418287f24ee), 24, UINT64_C(0x601715e2f78af79c)},
    {3, 1, 88, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":null}", "7:0o3:1{6:1a1:1[1:1;];5:1;0:0;};", 1, 89, UINT64_C(0xe6a5a8229b0c9002), 64, UINT64_C(0x72c6e24c51035e34), 24, UINT64_C(0x601715e2f78af79c)},
    {3, 1, 89, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 90, UINT64_C(0xf80cd4cd7658e9c6), 63, UINT64_C(0xe3718003aacfdf89), 27, UINT64_C(0x23c931a0f86ee070)},
    {3, 1, 90, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:0o0:1{};};", 1, 91, UINT64_C(0xd6b74720191512aa), 63, UINT64_C(0x859ce136ed5a19e3), 27, UINT64_C(0x23c931a0f86ee070)},
    {3, 1, 91, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 1, 92, UINT64_C(0xee80658a9ece807e), 65, UINT64_C(0x6974aedd6bbd4bfe), 27, UINT64_C(0xb55d4a66e9e7e0ec)},
    {3, 1, 92, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 1, 93, UINT64_C(0x12aca08bd8e3a562), 65, UINT64_C(0x7cb2eddc0608127a), 27, UINT64_C(0xb55d4a66e9e7e0ec)},
    {3, 1, 93, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 98, UINT64_C(0xfc0a73e323c44ba4), 68, UINT64_C(0x61f4292581fe1020), 29, UINT64_C(0x32b15a146cdd8b6f)},
    {3, 1, 94, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 102, UINT64_C(0x2c63d677b55e3dfa), 71, UINT64_C(0xd9342f88c76e636d), 30, UINT64_C(0xf08facd70b71fb88)},
    {3, 1, 95, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 100, UINT64_C(0xfdcd03f4cd60d80e), 70, UINT64_C(0x588604dbc9a84b89), 29, UINT64_C(0x176bd9a267ef7e3b)},
    {3, 1, 96, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 1, 103, UINT64_C(0x9c1dc1d99d63a20c), 72, UINT64_C(0xf5adbbea7893fef0), 30, UINT64_C(0x584f1dfc2923487b)},
    {3, 1, 97, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":0}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:0;};};", 1, 103, UINT64_C(0xc99430c7bfe41ee4), 72, UINT64_C(0xf5adbbea7893fef0), 30, UINT64_C(0x584f1dfc2923487b)},
    {3, 1, 98, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":0}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:0;};};", 1, 104, UINT64_C(0x238cd5d76782dd1e), 73, UINT64_C(0x29f0f6fd5ed356c0), 30, UINT64_C(0x404b79f3610714de)},
    {3, 1, 99, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":0}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:0;};};", 1, 102, UINT64_C(0x985bf7c187b7d7dc), 72, UINT64_C(0xf5adbbea7893fef0), 29, UINT64_C(0x79af5c5dc615f4f7)},
    {3, 1, 100, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":0}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:0;};};", 1, 104, UINT64_C(0x234227d767437cfa), 73, UINT64_C(0x29f0f6fd5ed356c0), 30, UINT64_C(0x404b79f3610714de)},
    {3, 1, 101, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o0:1{};};", 1, 102, UINT64_C(0x9729f5c186b3bae0), 74, UINT64_C(0xa9a8d50e804943ed), 27, UINT64_C(0x998885b959d4fc68)},
    {3, 1, 102, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":0}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:0;};};", 1, 105, UINT64_C(0x2ce6ab0477ad5968), 74, UINT64_C(0xa9a8d50e804943ed), 30, UINT64_C(0xb50667d7d86f8d99)},
    {3, 1, 103, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":0}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:0;};};", 1, 104, UINT64_C(0x234227d767437cfa), 74, UINT64_C(0xa9a8d50e804943ed), 29, UINT64_C(0x4d11975435094ccb)},
    {3, 1, 104, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"\":0}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:0;};};", 1, 105, UINT64_C(0x2ce6ab0477ad5968), 74, UINT64_C(0xa9a8d50e804943ed), 30, UINT64_C(0xb50667d7d86f8d99)},
    {3, 1, 105, 7, "key", "{\"a\":[true],\"k\":\"recovery\",\"o\":{\"q\":1}}", "7:0o3:1{6:1a1:1[1:1;];5:1;7:1o1:1{3:1;};};", 0, 105, UINT64_C(0x2ce6ab0477ad5968), 74, UINT64_C(0xa9a8d50e804943ed), 31, UINT64_C(0x98a23224cfb0ffe0)},
    {4, 0, 0, 0, "", "null", "0:0;", 2, 2, UINT64_C(0x08842007b530c95d), 0, UINT64_C(0xcbf29ce484222325), 0, UINT64_C(0xcbf29ce484222325)},
    {4, 0, 1, 0, "", "null", "0:0;", 2, 3, UINT64_C(0xa922fa18e1c37583), 0, UINT64_C(0xcbf29ce484222325), 1, UINT64_C(0xaf63bc4c8601b62c)},
    {4, 0, 2, 0, "key", "null", "0:0;", 1, 3, UINT64_C(0xa922fa18e1c37583), 0, UINT64_C(0xcbf29ce484222325), 2, UINT64_C(0x08395307b4f1348c)},
    {4, 0, 3, 0, "key", "null", "0:0;", 1, 4, UINT64_C(0x29e49b479f20e3f9), 1, UINT64_C(0xaf63be4c8601b992), 2, UINT64_C(0x08395307b4f1348c)},
    {4, 0, 4, 6, "key", "[]", "6:0a0:1[missing]", 2, 6, UINT64_C(0x67f9d4d46dd1ec91), 1, UINT64_C(0xaf63bd4c8601b7df), 4, UINT64_C(0xb345225e3644edb5)},
    {4, 0, 5, 6, "key", "[]", "6:0a0:1[];", 1, 6, UINT64_C(0x67f9d4d46dd1ec91), 1, UINT64_C(0xaf63bd4c8601b7df), 5, UINT64_C(0x3f3fcc261a338844)},
    {4, 0, 6, 6, "key", "[]", "6:0a0:1[];", 1, 7, UINT64_C(0x7f7145f69bb51593), 1, UINT64_C(0xaf63bb4c8601b479), 5, UINT64_C(0x3f3fcc261a338844)},
    {4, 0, 7, 6, "key", "[]", "6:0a0:1[];", 4, 11, UINT64_C(0x0ef52d9e6a1d5a93), 2, UINT64_C(0x08218e07b4dd06ec), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {4, 0, 8, 6, "key", "[]", "6:0a0:1[];", 4, 12, UINT64_C(0xa2e7cea7d9800249), 3, UINT64_C(0x1e87e8189340ae76), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {4, 0, 9, 6, "key", "[]", "6:0a0:1[];", 4, 13, UINT64_C(0x1fe4f8370a5a89cb), 4, UINT64_C(0xf241d4ba207d8e95), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {4, 0, 10, 6, "key", "[]", "6:0a0:1[];", 3, 13, UINT64_C(0x1fe4f8370a5a89cb), 5, UINT64_C(0x7e6b1d36db19d80d), 5, UINT64_C(0x18d2da27b6b8ff60)},
    {4, 0, 11, 6, "key", "[null]", "6:0a1:1[0:0;];", 3, 14, UINT64_C(0x8d23b086984bf3e9), 5, UINT64_C(0x7e6b1d36db19d80d), 6, UINT64_C(0xe5658011eb170905)},
    {4, 0, 12, 6, "key", "[null]", "6:0a1:1[0:0;];", 3, 15, UINT64_C(0x4ef6f5b452adc8bb), 6, UINT64_C(0xe60984573090fde3), 6, UINT64_C(0xe5658011eb170905)},
    {4, 0, 13, 6, "key", "[[]]", "6:0a1:1[6:0a0:1[missing]];", 4, 17, UINT64_C(0x9b59153f217aca13), 6, UINT64_C(0xfe81bb0663d6faef), 8, UINT64_C(0xd594f473ed6d03b0)},
    {4, 0, 14, 6, "key", "[[]]", "6:0a1:1[6:0a0:1[];];", 3, 17, UINT64_C(0x9b59153f217aca13), 6, UINT64_C(0xfe81bb0663d6faef), 9, UINT64_C(0x0f060cdee7c6d79c)},
    {4, 0, 15, 6, "key", "[[]]", "6:0a1:1[6:0a0:1[];];", 3, 18, UINT64_C(0xa2f3e5456d9fe0d9), 6, UINT64_C(0x02c88c156775a22c), 9, UINT64_C(0x0f060cdee7c6d79c)},
    {4, 0, 16, 6, "key", "[[]]", "6:0a1:1[6:1a0:1[];];", 3, 19, UINT64_C(0xdf5265fa0b5e084b), 8, UINT64_C(0x7640b8a62d1fce5a), 9, UINT64_C(0x3a62d1866b18afc8)},
    {4, 0, 17, 6, "key", "[[]]", "6:0a1:1[6:1a0:1[];];", 3, 20, UINT64_C(0x927dfae12a29b7e1), 8, UINT64_C(0x1700d06a36def24f), 9, UINT64_C(0x3a62d1866b18afc8)},
    {4, 0, 18, 6, "key", "[[]]", "6:0a1:1[6:1a0:1[];];", 4, 22, UINT64_C(0xe866f0c62e93f421), 9, UINT64_C(0xf24b4415cd4b36d9), 9, UINT64_C(0x08d2d2ce6ebd7ac4)},
    {4, 0, 19, 6, "key", "[[]]", "6:0a1:1[6:1a0:1[];];", 4, 23, UINT64_C(0xaa96fa88c4c89fbb), 10, UINT64_C(0x8d143888e63f9b66), 9, UINT64_C(0x08d2d2ce6ebd7ac4)},
    {4, 0, 20, 6, "key", "[[]]", "6:0a1:1[6:1a0:1[];];", 3, 23, UINT64_C(0xaa96fa88c4c89fbb), 11, UINT64_C(0xaa5c60bc7d9fe37a), 9, UINT64_C(0x08d2d2ce6ebd7ac4)},
    {4, 0, 21, 6, "key", "[[null]]", "6:0a1:1[6:1a1:1[0:0;];];", 3, 24, UINT64_C(0xa746b16660fe9f39), 11, UINT64_C(0xaa5c60bc7d9fe37a), 10, UINT64_C(0x340b2d803261506f)},
    {4, 0, 22, 6, "key", "[[null]]", "6:0a1:1[6:1a1:1[0:0;];];", 3, 25, UINT64_C(0xc63af9f71f110db3), 12, UINT64_C(0xac32a94474c4bc64), 10, UINT64_C(0x340b2d803261506f)},
    {4, 0, 23, 6, "key", "[[\"\"]]", "6:0a1:1[6:1a1:1[5:1;];];", 3, 26, UINT64_C(0xdf2316e9c5861876), 11, UINT64_C(0xaa5c60bc7d9fe37a), 12, UINT64_C(0xc1928666ae5d4a6a)},
    {4, 0, 24, 6, "key", "[[\"recovery\"]]", "6:0a1:1[6:1a1:1[5:1;];];", 2, 26, UINT64_C(0xdf2316e9c5861876), 11, UINT64_C(0xaa5c60bc7d9fe37a), 13, UINT64_C(0xc70e457618281d24)},
    {4, 0, 25, 6, "key", "[[\"recovery\"]]", "6:0a1:1[6:1a1:1[5:1;];];", 2, 27, UINT64_C(0xae82353aa2adaf82), 12, UINT64_C(0x283aa048ed3b7e47), 13, UINT64_C(0xc70e457618281d24)},
    {4, 0, 26, 6, "key", "[[\"recovery\"]]", "6:0a1:1[6:1a1:1[5:1;];];", 2, 28, UINT64_C(0x33fb34a26c513b2e), 13, UINT64_C(0xc3114ee2a85daebf), 13, UINT64_C(0xc70e457618281d24)},
    {4, 0, 27, 6, "key", "[[\"recovery\"]]", "6:0a1:1[6:1a1:1[5:1;];];", 1, 28, UINT64_C(0x33fb34a26c513b2e), 14, UINT64_C(0x0e819e1d9dff465c), 13, UINT64_C(0xc70e457618281d24)},
    {4, 0, 28, 6, "key", "[[\"recovery\"],null]", "6:0a2:1[6:1a1:1[5:1;];0:0;];", 1, 29, UINT64_C(0xa51595fe0e078192), 14, UINT64_C(0x0e819e1d9dff465c), 14, UINT64_C(0xf7ddd57b3ac52386)},
    {4, 0, 29, 6, "key", "[[\"recovery\"],null]", "6:0a2:1[6:1a1:1[5:1;];0:0;];", 1, 30, UINT64_C(0x8b2f90b1d6c16176), 15, UINT64_C(0x27a4fc9d713e1dcf), 14, UINT64_C(0xf7ddd57b3ac52386)},
    {4, 0, 30, 6, "key", "[[\"recovery\"],null]", "6:0a2:1[6:1a1:1[5:1;];0:0;];", 1, 31, UINT64_C(0x4333fc2fea9805fa), 16, UINT64_C(0xb34108ccff78652b), 14, UINT64_C(0xf7ddd57b3ac52386)},
    {4, 0, 31, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:0o0:1{};];", 1, 32, UINT64_C(0xc95b5f6ba051f16e), 15, UINT64_C(0x60ba726eb8e9eafe), 17, UINT64_C(0x1fbdcc46db94b748)},
    {4, 0, 32, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:0o0:1{};];", 1, 33, UINT64_C(0x783429e16b3c8d32), 15, UINT64_C(0xcdedec10fab123de), 17, UINT64_C(0x1fbdcc46db94b748)},
    {4, 0, 33, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 1, 34, UINT64_C(0x7d303c0937e3b596), 17, UINT64_C(0x0e22e134963af1ff), 17, UINT64_C(0xc47810ca7ea08204)},
    {4, 0, 34, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 1, 35, UINT64_C(0x9cac01a9f7ee3e9a), 17, UINT64_C(0x9d9d28d543e28d8d), 17, UINT64_C(0xc47810ca7ea08204)},
    {4, 0, 35, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 3, 38, UINT64_C(0x449d737b6d7045cc), 18, UINT64_C(0x634cab1e73430dc8), 17, UINT64_C(0x6bb402bf755397c8)},
    {4, 0, 36, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 4, 40, UINT64_C(0xd8f447b1c5fb9216), 19, UINT64_C(0xe73f82bc8b648d68), 17, UINT64_C(0x6bb402bf755397c8)},
    {4, 0, 37, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 3, 40, UINT64_C(0xd8f447b1c5fb9216), 20, UINT64_C(0xa643db5d08c58deb), 17, UINT64_C(0x6bb402bf755397c8)},
    {4, 0, 38, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 4, 42, UINT64_C(0x502e87ffe0865028), 21, UINT64_C(0xf96ee5bab261b4ef), 17, UINT64_C(0x6bb402bf755397c8)},
    {4, 0, 39, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 3, 42, UINT64_C(0x502e87ffe0865028), 22, UINT64_C(0x3d36dc7057ad8e3c), 17, UINT64_C(0x6bb402bf755397c8)},
    {4, 0, 40, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 3, 43, UINT64_C(0x916885ca66ca7fee), 23, UINT64_C(0x8993257e27b863ee), 17, UINT64_C(0x6bb402bf755397c8)},
    {4, 0, 41, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 2, 43, UINT64_C(0x916885ca66ca7fee), 24, UINT64_C(0xb355554d556a2233), 17, UINT64_C(0x6bb402bf755397c8)},
    {4, 0, 42, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 2, 44, UINT64_C(0xde8cf0eca99ecd68), 25, UINT64_C(0xc52002f46dcf2203), 17, UINT64_C(0x6bb402bf755397c8)},
    {4, 0, 43, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 1, 44, UINT64_C(0xde8cf0eca99ecd68), 26, UINT64_C(0x3628a04973bf9fa0), 17, UINT64_C(0x6bb402bf755397c8)},
    {4, 0, 44, 6, "key", "[[\"recovery\"],{\"\":0}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:0;};];", 2, 46, UINT64_C(0x2e23e78c9567bb32), 26, UINT64_C(0x3628a04973bf9fa0), 18, UINT64_C(0xed49e4670d08ef12)},
    {4, 0, 45, 6, "key", "[[\"recovery\"],{\"\":0}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:0;};];", 1, 46, UINT64_C(0x2e23e78c9567bb32), 26, UINT64_C(0x3628a04973bf9fa0), 19, UINT64_C(0xc217b532e08d04bf)},
    {4, 0, 46, 6, "key", "[[\"recovery\"],{\"\":0}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:0;};];", 1, 47, UINT64_C(0xcebda3e1df431290), 26, UINT64_C(0x3628a04973bf9fa0), 20, UINT64_C(0xdda94b1d3104c283)},
    {4, 0, 47, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 0, 47, UINT64_C(0xcebda3e1df431290), 26, UINT64_C(0x3628a04973bf9fa0), 21, UINT64_C(0xf0d02532e8d6f88c)},
    {4, 1, 0, 6, "", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 46, UINT64_C(0x92dd27940b88cdee), 26, UINT64_C(0x709eb1bd3f80136c), 19, UINT64_C(0xcc473bf941fe55b3)},
    {4, 1, 1, 6, "", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 47, UINT64_C(0xcebda3e1df431290), 26, UINT64_C(0xdc138678b8467c04), 20, UINT64_C(0x39553f39ddffd6fd)},
    {4, 1, 2, 0, "key", "null", "0:0;", 1, 3, UINT64_C(0xa922fa18e1c37583), 0, UINT64_C(0xcbf29ce484222325), 2, UINT64_C(0x08395307b4f1348c)},
    {4, 1, 3, 0, "key", "null", "0:0;", 1, 4, UINT64_C(0x29e49b479f20e3f9), 1, UINT64_C(0xaf63be4c8601b992), 2, UINT64_C(0x08395307b4f1348c)},
    {4, 1, 4, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 47, UINT64_C(0xcebda3e1df431290), 25, UINT64_C(0x61ac871f77e3e9ef), 21, UINT64_C(0x745e4c543ea53008)},
    {4, 1, 5, 6, "key", "[]", "6:0a0:1[];", 1, 6, UINT64_C(0x67f9d4d46dd1ec91), 1, UINT64_C(0xaf63bd4c8601b7df), 5, UINT64_C(0x3f3fcc261a338844)},
    {4, 1, 6, 6, "key", "[]", "6:0a0:1[];", 1, 7, UINT64_C(0x7f7145f69bb51593), 1, UINT64_C(0xaf63bb4c8601b479), 5, UINT64_C(0x3f3fcc261a338844)},
    {4, 1, 7, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 43, UINT64_C(0x62d08462f86429b0), 21, UINT64_C(0xbc4aea29c30224e9), 21, UINT64_C(0x2195d3c69dbd95a4)},
    {4, 1, 8, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 44, UINT64_C(0x7887af0e0f014fba), 22, UINT64_C(0x0e0620c2831b2fa8), 21, UINT64_C(0xd4097f69bb24ded4)},
    {4, 1, 9, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 47, UINT64_C(0xcebda3e1df431290), 25, UINT64_C(0xe9abf74807bfdead), 21, UINT64_C(0xc78a1d641ccded9c)},
    {4, 1, 10, 6, "key", "[{\"k\":1}]", "6:0a1:1[7:1o1:1{3:1;};];", 1, 34, UINT64_C(0xaca4dcad768758b3), 20, UINT64_C(0x2b1254f33a770e0f), 13, UINT64_C(0x45a62b3550081448)},
    {4, 1, 11, 6, "key", "[null,{\"k\":1}]", "6:0a2:1[0:0;7:1o1:1{3:1;};];", 1, 35, UINT64_C(0x10f4d932a9856119), 20, UINT64_C(0xf63d69101e63889e), 14, UINT64_C(0x8afec3ac50f976fd)},
    {4, 1, 12, 6, "key", "[null,{\"k\":1}]", "6:0a2:1[0:0;7:1o1:1{3:1;};];", 1, 36, UINT64_C(0x463f5c38b808c643), 21, UINT64_C(0xb1e86ad690faa15b), 14, UINT64_C(0x4363778309ad8d0d)},
    {4, 1, 13, 6, "key", "[[],{\"k\":1}]", "6:0a2:1[6:0a0:1[];7:1o1:1{3:1;};];", 1, 39, UINT64_C(0xa97deac3ff265a29), 21, UINT64_C(0x1282acbd55a15693), 17, UINT64_C(0xe511760c18ca0850)},
    {4, 1, 14, 6, "key", "[[],{\"k\":1}]", "6:0a2:1[6:0a0:1[];7:1o1:1{3:1;};];", 1, 38, UINT64_C(0xd16edb648a6ce1ab), 21, UINT64_C(0xd366c3eec13151bd), 17, UINT64_C(0xa892f032cc56afb4)},
    {4, 1, 15, 6, "key", "[[],{\"k\":1}]", "6:0a2:1[6:0a0:1[];7:1o1:1{3:1;};];", 1, 39, UINT64_C(0xa97deac3ff265a29), 21, UINT64_C(0x430b89b128d98bc9), 17, UINT64_C(0x0ffc238e12d8a034)},
    {4, 1, 16, 6, "key", "[[],{\"k\":1}]", "6:0a2:1[6:1a0:1[];7:1o1:1{3:1;};];", 1, 40, UINT64_C(0x064800083d112433), 23, UINT64_C(0x7e3dde20e2c4af28), 17, UINT64_C(0xa9c98a3ab9b643c0)},
    {4, 1, 17, 6, "key", "[[],{\"k\":1}]", "6:0a2:1[6:1a0:1[];7:1o1:1{3:1;};];", 1, 41, UINT64_C(0x960821877b217001), 23, UINT64_C(0xc7fcea40891a9322), 17, UINT64_C(0xae16f39d760e7cc8)},
    {4, 1, 18, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 46, UINT64_C(0x4adbffdc69541eea), 24, UINT64_C(0x76857573ce5936df), 21, UINT64_C(0xae785b5857c280a0)},
    {4, 1, 19, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 47, UINT64_C(0xcebda3e1df431290), 25, UINT64_C(0x4b847b2b3084d143), 21, UINT64_C(0xd558d4feac336e68)},
    {4, 1, 20, 6, "key", "[[],{\"k\":1}]", "6:0a2:1[6:1a0:1[];7:1o1:1{3:1;};];", 1, 44, UINT64_C(0x4e84d7d79cb12b43), 26, UINT64_C(0x2827871680da954c), 17, UINT64_C(0x282bda8ef28461dc)},
    {4, 1, 21, 6, "key", "[[null],{\"k\":1}]", "6:0a2:1[6:1a1:1[0:0;];7:1o1:1{3:1;};];", 1, 45, UINT64_C(0x50892a76fb509509), 26, UINT64_C(0x2fe37edc7395b8a5), 18, UINT64_C(0x9b143889fe3f908f)},
    {4, 1, 22, 6, "key", "[[null],{\"k\":1}]", "6:0a2:1[6:1a1:1[0:0;];7:1o1:1{3:1;};];", 1, 46, UINT64_C(0x3e337cb4623144cb), 27, UINT64_C(0xf0778577f32c1518), 18, UINT64_C(0x76545b682a66d80f)},
    {4, 1, 23, 6, "key", "[[\"\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 47, UINT64_C(0xcebda3e1df431290), 26, UINT64_C(0xba5d9fa327aa54c7), 20, UINT64_C(0x152a5133a69aacd2)},
    {4, 1, 24, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 43, UINT64_C(0x545e9aeceaf07070), 21, UINT64_C(0xb358f17a3ddbd0f3), 21, UINT64_C(0x7f7246030eab6d5c)},
    {4, 1, 25, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 44, UINT64_C(0x5de102f5bf79df84), 22, UINT64_C(0x6d16a137cb9e81de), 21, UINT64_C(0xc94584b061292514)},
    {4, 1, 26, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 45, UINT64_C(0xfdf3d577ce49b8f8), 23, UINT64_C(0x7116aa28dc5729ce), 21, UINT64_C(0x7fb420974764fe44)},
    {4, 1, 27, 6, "key", "[[\"recovery\"]]", "6:0a1:1[6:1a1:1[5:1;];];", 1, 28, UINT64_C(0x33fb34a26c513b2e), 14, UINT64_C(0x0e819e1d9dff465c), 13, UINT64_C(0xc70e457618281d24)},
    {4, 1, 28, 6, "key", "[[\"recovery\"],null]", "6:0a2:1[6:1a1:1[5:1;];0:0;];", 1, 29, UINT64_C(0xa51595fe0e078192), 14, UINT64_C(0x0e819e1d9dff465c), 14, UINT64_C(0xf7ddd57b3ac52386)},
    {4, 1, 29, 6, "key", "[[\"recovery\"],null]", "6:0a2:1[6:1a1:1[5:1;];0:0;];", 1, 30, UINT64_C(0x8b2f90b1d6c16176), 15, UINT64_C(0x27a4fc9d713e1dcf), 14, UINT64_C(0xf7ddd57b3ac52386)},
    {4, 1, 30, 6, "key", "[[\"recovery\"],null]", "6:0a2:1[6:1a1:1[5:1;];0:0;];", 1, 31, UINT64_C(0x4333fc2fea9805fa), 16, UINT64_C(0xb34108ccff78652b), 14, UINT64_C(0xf7ddd57b3ac52386)},
    {4, 1, 31, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:0o0:1{};];", 1, 32, UINT64_C(0xc95b5f6ba051f16e), 15, UINT64_C(0x60ba726eb8e9eafe), 17, UINT64_C(0x1fbdcc46db94b748)},
    {4, 1, 32, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:0o0:1{};];", 1, 33, UINT64_C(0x783429e16b3c8d32), 15, UINT64_C(0xcdedec10fab123de), 17, UINT64_C(0x1fbdcc46db94b748)},
    {4, 1, 33, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 1, 34, UINT64_C(0x7d303c0937e3b596), 17, UINT64_C(0x0e22e134963af1ff), 17, UINT64_C(0xc47810ca7ea08204)},
    {4, 1, 34, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 1, 35, UINT64_C(0x9cac01a9f7ee3e9a), 17, UINT64_C(0x9d9d28d543e28d8d), 17, UINT64_C(0xc47810ca7ea08204)},
    {4, 1, 35, 6, "key", "[[\"recovery\"],{\"\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 40, UINT64_C(0xa3938fb1a75a386c), 20, UINT64_C(0xa643db5d08c58deb), 19, UINT64_C(0xfa57cf84a8b53e9b)},
    {4, 1, 36, 6, "key", "[[\"recovery\"],{\"\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 44, UINT64_C(0x9e7a111eaeebebf2), 23, UINT64_C(0xb411294cf9d6b5fc), 20, UINT64_C(0x76cdc90ed9ee8f5a)},
    {4, 1, 37, 6, "key", "[[\"recovery\"],{\"\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 42, UINT64_C(0x4eb201ffdf431a66), 22, UINT64_C(0x3d36dc7057ad8e3c), 19, UINT64_C(0xb4b45f3eec4e7aa7)},
    {4, 1, 38, 6, "key", "[[\"recovery\"],{\"\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 1, 45, UINT64_C(0x32b0d42505b4e994), 24, UINT64_C(0xb355554d556a2233), 20, UINT64_C(0xe8452972970cdff5)},
    {4, 1, 39, 6, "key", "[[\"recovery\"],{\"k\":0}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:0;};];", 1, 45, UINT64_C(0x939601fb7cc7322c), 24, UINT64_C(0xb355554d556a2233), 20, UINT64_C(0xe8452972970cdff5)},
    {4, 1, 40, 6, "key", "[[\"recovery\"],{\"k\":0}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:0;};];", 1, 46, UINT64_C(0x2e38658c95793a56), 25, UINT64_C(0xc52002f46dcf2203), 20, UINT64_C(0x69a2acc0a7d1c9c0)},
    {4, 1, 41, 6, "key", "[[\"recovery\"],{\"\":0}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:0;};];", 1, 44, UINT64_C(0xdf1b42ecaa175e04), 24, UINT64_C(0xb355554d556a2233), 19, UINT64_C(0x957fcb3b059cab5b)},
    {4, 1, 42, 6, "key", "[[\"recovery\"],{\"\":0}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:0;};];", 1, 46, UINT64_C(0x2e23e78c9567bb32), 25, UINT64_C(0xc52002f46dcf2203), 20, UINT64_C(0x69a2acc0a7d1c9c0)},
    {4, 1, 43, 6, "key", "[[\"recovery\"],{}]", "6:0a2:1[6:1a1:1[5:1;];7:1o0:1{};];", 1, 44, UINT64_C(0xde8cf0eca99ecd68), 26, UINT64_C(0x3628a04973bf9fa0), 17, UINT64_C(0x6bb402bf755397c8)},
    {4, 1, 44, 6, "key", "[[\"recovery\"],{\"k\":0}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:0;};];", 1, 47, UINT64_C(0xcebda3e1df431290), 26, UINT64_C(0x3628a04973bf9fa0), 20, UINT64_C(0xdda94b1d3104c283)},
    {4, 1, 45, 6, "key", "[[\"recovery\"],{\"\":0}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:0;};];", 1, 46, UINT64_C(0x2e23e78c9567bb32), 26, UINT64_C(0x3628a04973bf9fa0), 19, UINT64_C(0xc217b532e08d04bf)},
    {4, 1, 46, 6, "key", "[[\"recovery\"],{\"\":0}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:0;};];", 1, 47, UINT64_C(0xcebda3e1df431290), 26, UINT64_C(0x3628a04973bf9fa0), 20, UINT64_C(0xdda94b1d3104c283)},
    {4, 1, 47, 6, "key", "[[\"recovery\"],{\"k\":1}]", "6:0a2:1[6:1a1:1[5:1;];7:1o1:1{3:1;};];", 0, 47, UINT64_C(0xcebda3e1df431290), 26, UINT64_C(0x3628a04973bf9fa0), 21, UINT64_C(0xf0d02532e8d6f88c)},
};
struct PairCallbackContext {
    Value replacement{};
    const Value* receiver = nullptr;
    unsigned nulls = 0, mismatches = 0, floats = 0, elements = 0;
};
static const Value& APS5_VABI NullCopy(std::int32_t type, const Value* parent, void* context) {
    Require(type == 2 && !parent, "Copied null recovery must preserve requested type and parent ABI");
    auto& state = *static_cast<PairCallbackContext*>(context); ++state.nulls;
    return state.replacement;
}
static const Value& APS5_VABI MismatchCopy(std::int32_t type, const Value* self, const Value* parent, void* context) {
    auto& state = *static_cast<PairCallbackContext*>(context);
    Require(type == 2 && self == state.receiver && !parent, "Copied mismatch recovery must preserve receiver, type and parent ABI");
    ++state.mismatches;
    return state.replacement;
}
static Value* APS5_VABI FloatCopy(Value* result, std::int32_t function, double number, const Value* parent, void* context, bool* error) {
    Require(function == 1 && number == std::numeric_limits<double>::infinity() && !parent && !error, "Copied float recovery must preserve its assignment ABI");
    auto& state = *static_cast<PairCallbackContext*>(context); ++state.floats;
    _ZN3sce4Json5ValueC1El(result, static_cast<std::int64_t>(state.replacement.payload));
    return result;
}
static void APS5_VABI ElementCopy(int error, const String* key, const std::size_t* index, const Value* parent, const Value* self, void* context) {
    auto& state = *static_cast<PairCallbackContext*>(context);
    Require(static_cast<unsigned>(error) == 0x80848115 && key && !index && !parent && self == state.receiver && std::strcmp(_ZNK3sce4Json6String5c_strEv(key), "missing") == 0, "Copied element recovery must retain its exact key, receiver and parent ABI");
    ++state.elements;
}
static void Register(Value& value, PairCallbackContext& context) {
    context.receiver = &value;
    Require(_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(&value, NullCopy, &context) == 0, "Register source null recovery");
    Require(_ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(&value, MismatchCopy, &context) == 0, "Register source mismatch recovery");
    Require(_ZN3sce4Json5Value22setSpecialFloatHandlerEPFKS1_NS0_12FunctionTypeEdPS2_PvPbES5_(&value, FloatCopy, &context) == 0, "Register source float recovery");
    Require(_ZN3sce4Json5Value30setElementAccessFailureHandlerEPFviPKNS0_6StringEPKmPKS1_S8_PvES9_(&value, ElementCopy, &context) == 0, "Register source element recovery");
}
struct MetadataSnapshot {
    std::array<std::byte, 72> bytes;
    std::array<void*, 5> allocations;
    std::array<std::size_t, 5> generations;
};
static MetadataSnapshot Capture(const Value& value, const PairTrace& trace) {
    MetadataSnapshot snapshot{};
    const auto* words = static_cast<void* const*>(value.auxiliary);
    snapshot.allocations = {value.auxiliary, words[2], words[3], words[4], words[5]};
    std::memcpy(snapshot.bytes.data(), value.auxiliary, snapshot.bytes.size());
    for (unsigned i = 0; i < snapshot.allocations.size(); ++i) snapshot.generations[i] = trace.live.at(snapshot.allocations[i]);
    return snapshot;
}
static void Verify(const Value& value, const MetadataSnapshot& snapshot, const PairTrace& trace) {
    Require(value.auxiliary == snapshot.allocations[0] && std::memcmp(value.auxiliary, snapshot.bytes.data(), snapshot.bytes.size()) == 0, "Copying must preserve complete source/destination auxiliary bytes and handler/context identities");
    for (unsigned i = 0; i < snapshot.allocations.size(); ++i) Require(trace.live.at(snapshot.allocations[i]) == snapshot.generations[i], "Copying must preserve auxiliary, fallback and all three callback-record allocation generations");
    Require(static_cast<const Value*>(snapshot.allocations[1])->parent == &value, "Copied callback metadata must retain its original fallback owner");
}
int main() {
    PairTrace trace; PairAllocator allocator; Parameter parameter{&allocator, &trace, 8192, 2, {}};
    std::uint8_t initializer = 0; _ZN3sce4Json11InitializerC1Ev(&initializer);
    Require(_ZN3sce4Json11Initializer10initializeEPKNS0_14InitParameter2E(&initializer, &parameter) == 0, "Initialize pair allocation contracts");
    const char* documents[]{"", "null", "\"recovery\"", "{\"k\":\"recovery\",\"o\":{\"q\":1},\"a\":[true]}", "[[\"recovery\"],{\"k\":1}]"};
    const auto moduleBaseline = trace.live;
    for (const auto& expected : Cases) for (bool base : {false, true}) for (bool callbackSource : {false, true}) {
        String key{}; _ZN3sce4Json6StringC1EPKc(&key, "key");
        Value source{}; _ZN3sce4Json5ValueC1Ev(&source);
        const char* document = documents[expected.source];
        if (expected.source) Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&source, document, std::strlen(document)) == 0, "Construct independent pair source");
        PairCallbackContext context;
        _ZN3sce4Json5ValueC1El(&context.replacement, 77);
        MetadataSnapshot sourceMetadata{};
        if (callbackSource) { Register(source, context); sourceMetadata = Capture(source, trace); }
        const auto sourceText = Serialize(source);
        const auto baseline = trace.live;
        Pair pair; std::memset(&pair, 0xa5, sizeof(pair));
        const auto first = trace.requests.size(), freeFirst = trace.releases.size(), generation = trace.generation;
        const auto notifications = trace.notices;
        trace.budget = expected.budget; trace.once = expected.once;
        if (!expected.source) {
            if (base) _ZN3sce4Json6Object4PairC2Ev(&pair);
            else _ZN3sce4Json6Object4PairC1Ev(&pair);
        } else {
            if (base) _ZN3sce4Json6Object4PairC2ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &source);
            else _ZN3sce4Json6Object4PairC1ERKNS0_6StringERKNS0_5ValueE(&pair, &key, &source);
        }
        const auto end = trace.requests.size(), freeEnd = trace.releases.size();
        const auto errors = trace.notices - notifications;
        trace.budget = -1; trace.once = false;
        CheckMetadata(pair, UINT64_C(0xa5a5a5a5a5a5a5a5));
        std::string shape; Shape(pair.value, nullptr, shape);
        const auto text = Serialize(pair.value);
        Require(pair.value.type == expected.type && shape == expected.shape && text == expected.text &&
                std::strcmp(_ZNK3sce4Json6String5c_strEv(&pair.key), expected.key) == 0,
                "Partial pair construction must preserve reference contents, collection counts, child parents and storage owners");
        Require(end - first == expected.requests && Fingerprint(trace.requests, first, end) == expected.requestOrder && errors == expected.notifications,
                "Pair construction must preserve exact allocation request order and notifications at every failure point");
        Require(freeEnd - freeFirst == expected.constructionReleases && Fingerprint(trace.releases, freeFirst, freeEnd, generation) == expected.constructionOrder,
                "Failed pair copies must preserve exact temporary release identities and observable null frees");
        const auto teardown = trace.releases.size();
        if (base) _ZN3sce4Json6Object4PairD2Ev(&pair);
        else _ZN3sce4Json6Object4PairD1Ev(&pair);
        Require(trace.releases.size() - teardown == expected.destructionReleases && Fingerprint(trace.releases, teardown, trace.releases.size(), generation) == expected.destructionOrder,
                "Pair destruction must release the value before its key in the reference allocation-identity order");
        Require(trace.live == baseline && Serialize(source) == sourceText, "Partial pair cleanup must preserve every source allocation generation and value");
        if (callbackSource) {
            Verify(source, sourceMetadata, trace);
            const auto* borrowed = _ZNK3sce4Json5Value10getIntegerEv(&source);
            Require(borrowed == reinterpret_cast<const std::int64_t*>(&context.replacement.payload) && *borrowed == 77,
                    "Source null/mismatch callbacks must retain their borrowed result identity after failed copies are destroyed");
            const bool nullSource = source.type == 0, objectSource = source.type == 7;
            if (objectSource) _ZNK3sce4Json5ValueixEPKc(&source, "missing");
            _ZN3sce4Json5Value3setEd(&source, std::numeric_limits<double>::infinity());
            Require(source.type == 2 && source.payload == 77 && context.nulls == unsigned(nullSource) && context.mismatches == unsigned(!nullSource) && context.elements == unsigned(objectSource) && context.floats == 1,
                    "Every partial-copy failure must leave the source's four independent handlers usable and correctly bound");
            Verify(source, sourceMetadata, trace);
        }
        _ZN3sce4Json5ValueD1Ev(&source); _ZN3sce4Json5ValueD1Ev(&context.replacement); _ZN3sce4Json6StringD1Ev(&key);
        Require(trace.live == moduleBaseline, "Pair failure schedules must leave only the original module resources");
    }
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.live.empty(), "Release pair allocation resources");
    return 0;
}
