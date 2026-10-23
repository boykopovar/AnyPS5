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

struct Value { Value* parent; void* auxiliary; std::uint64_t payload; std::uint32_t reserved; std::int32_t type; };
struct Array { void* storage; };
struct Parameter { void* allocator; void* context; std::size_t bufferSize; };
using Handler = const Value& (APS5_VABI*)(std::int32_t, const Value*, const Value*, void*);
using NullHandler = const Value& (APS5_VABI*)(std::int32_t, const Value*, void*);
extern "C" {
void APS5_VABI _ZN3sce4Json11InitializerC1Ev(void*);
int APS5_VABI _ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE(void*, const Parameter*);
int APS5_VABI _ZN3sce4Json11Initializer9terminateEv(void*);
void APS5_VABI _ZN3sce4Json5ValueC1El(Value*, std::int64_t);
void APS5_VABI _ZN3sce4Json5ValueC1Eb(Value*, bool);
void APS5_VABI _ZN3sce4Json5ValueC1Ev(Value*);
void APS5_VABI _ZN3sce4Json5ValueC1ERKS1_(Value*, const Value*);
Value* APS5_VABI _ZN3sce4Json5ValueaSERKS1_(Value*, const Value*);
void APS5_VABI _ZN3sce4Json5ValueD1Ev(Value*);
void APS5_VABI _ZN3sce4Json5Value4swapERS1_(Value*, Value*);
const bool* APS5_VABI _ZNK3sce4Json5Value10getBooleanEv(const Value*);
bool APS5_VABI _ZNK3sce4Json5ValuecvbEv(const Value*);
int APS5_VABI _ZN3sce4Json5Value22setTypeMismatchHandlerEPFRKS1_NS0_9ValueTypeEPS2_S5_PvES6_(Value*, Handler, void*);
int APS5_VABI _ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_(Value*, NullHandler, void*);
int APS5_VABI _ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(Value*, const char*, std::size_t);
void APS5_VABI _ZN3sce4Json5ArrayC1Ev(Array*);
void APS5_VABI _ZN3sce4Json5ArrayD1Ev(Array*);
void APS5_VABI _ZN3sce4Json5Array9push_backERKNS0_5ValueE(Array*, const Value*);
const Value* APS5_VABI _ZNK3sce4Json5Array5frontEv(const Array*);
std::size_t APS5_VABI _ZNK3sce4Json5Array4sizeEv(const Array*);
}
static void Require(bool valid, const char* message) {
    if (!valid) { std::fprintf(stderr, "%s\n", message); std::abort(); }
}
struct LegacyTrace {
    std::map<void*, std::size_t> guest;
    std::map<void*, std::size_t> heap;
    int budget = -1;
    unsigned notifications = 0;
};
static LegacyTrace* HeapTrace = nullptr;
extern "C" const void* ApplicationProcessParameters_nid_no_patch() {
    alignas(8) static const std::uint32_t parameters[]{0, 0, 0, 0, 0x05000000, 0};
    return parameters;
}
extern "C" void* APS5_VABI _Znwm_nid_postfix(std::size_t bytes) {
    void* memory = std::malloc(bytes);
    if (!memory) throw std::bad_alloc();
    std::memset(memory, 0xa5, bytes);
    Require(HeapTrace && bytes == 56, "Legacy map nodes must use the actual libc new allocation boundary");
    HeapTrace->heap.emplace(memory, bytes);
    return memory;
}
extern "C" void APS5_VABI _ZdlPv_nid_postfix(void* memory) {
    Require(HeapTrace && HeapTrace->heap.erase(memory) == 1, "Legacy map nodes must return through libc delete");
    std::free(memory);
}
static void* APS5_VABI AllocateLegacy(void*, std::size_t bytes, void* context) {
    auto& trace = *static_cast<LegacyTrace*>(context);
    if (trace.budget == 0) return nullptr;
    if (trace.budget > 0) --trace.budget;
    void* memory = std::malloc(bytes); Require(memory != nullptr, "Tracking allocator requires host memory");
    std::memset(memory, 0xa5, bytes); trace.guest.emplace(memory, bytes); return memory;
}
static void APS5_VABI FreeLegacy(void*, void* memory, void* context) {
    if (!memory) return;
    Require(static_cast<LegacyTrace*>(context)->guest.erase(memory) == 1, "Legacy metadata must return through its owning allocator");
    std::free(memory);
}
static void APS5_VABI NotifyLegacy(void*, int error, std::size_t bytes, void* context) {
    Require(static_cast<unsigned>(error) == 0x80848102 && (bytes == 24 || bytes == 48), "Legacy allocation failure must identify its map header or array node");
    ++static_cast<LegacyTrace*>(context)->notifications;
}
static void* const LegacyVtable[]{nullptr, nullptr, reinterpret_cast<void*>(AllocateLegacy), reinterpret_cast<void*>(FreeLegacy), reinterpret_cast<void*>(NotifyLegacy)};
struct LegacyAllocator { void* const* vtable = LegacyVtable; };
struct LegacyNode { LegacyNode* parent; LegacyNode* left; LegacyNode* right; const Value* key; Handler callback; void* context; std::uint8_t color; std::uint8_t nil; std::uint8_t reserved[6]; };
struct LegacyHeader { LegacyNode* head; std::size_t count; std::uint64_t reserved; };
static_assert(sizeof(LegacyNode) == 56 && sizeof(LegacyHeader) == 24);
struct Context { Value replacement{}; unsigned calls = 0; const Value* source = nullptr; const Value* parent = nullptr; };
static const Value& APS5_VABI RecoverLegacy(std::int32_t type, const Value* source, const Value* parent, void* context) {
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
        if (trace.heap.contains(head)) { Require(!header, "Legacy registration must use one shared map header"); header = static_cast<const LegacyHeader*>(memory); }
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
        Require(expected != entries.end() && node->key == expected->first && node->callback == RecoverLegacy && node->context == expected->second, "Legacy map entries must preserve sorted keys and callback identity");
        ++expected;
        const auto right = visit(node->right, node);
        Require(left == right, "Legacy tree paths must preserve black height");
        return left + !node->color;
    };
    visit(head->parent, head);
    Require(expected == entries.end() && trace.heap.size() == entries.size() + 1, "Legacy map must retain only its actual sentinel and entry nodes");
}
struct NullContext { const Value* other; Context* replacement; };
static const Value& APS5_VABI RecoverLegacyNull(std::int32_t requested, const Value* parent, void* context) {
    auto& value = *static_cast<NullContext*>(context);
    Require(requested == 1 && !parent && *_ZNK3sce4Json5Value10getBooleanEv(value.other), "Legacy null recovery must allow independent mismatch recovery before the reentry guard");
    return value.replacement->replacement;
}
int main() {
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
    Require(_ZN3sce4Json11Initializer9terminateEv(&initializer) == 0 && trace.guest.empty() && trace.heap.empty(), "Termination must release both allocator domains and all legacy map resources");
    HeapTrace = nullptr;
    return 0;
}
