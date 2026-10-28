#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iterator>

static_assert(sizeof(LibreSslX509VerifyParamId) == 56);
static_assert(offsetof(LibreSslX509VerifyParamId, hostFlags) == 8);
static_assert(offsetof(LibreSslX509VerifyParamId, peerName) == 16);
static_assert(offsetof(LibreSslX509VerifyParamId, ip) == 40);
static_assert(sizeof(LibreSslX509VerifyParam) == 64);
static_assert(offsetof(LibreSslX509VerifyParam, flags) == 24);
static_assert(offsetof(LibreSslX509VerifyParam, purpose) == 32);
static_assert(offsetof(LibreSslX509VerifyParam, depth) == 40);
static_assert(offsetof(LibreSslX509VerifyParam, policies) == 48);
static_assert(offsetof(LibreSslX509VerifyParam, id) == 56);
static_assert(sizeof(LibreSslX509Store) == 136);
static_assert(offsetof(LibreSslX509Store, objects) == 8);
static_assert(offsetof(LibreSslX509Store, lookupMethods) == 16);
static_assert(offsetof(LibreSslX509Store, param) == 24);
static_assert(offsetof(LibreSslX509Store, callbacks) == 32);
static_assert(offsetof(LibreSslX509Store, exData) == 120);
static_assert(offsetof(LibreSslX509Store, references) == 128);
static_assert(sizeof(LibreSslX509LookupMethod) == 80);
static_assert(offsetof(LibreSslX509LookupMethod, shutdown) == 32);
static_assert(offsetof(LibreSslX509LookupMethod, ctrl) == 40);
static_assert(sizeof(LibreSslX509Lookup) == 32);
static_assert(offsetof(LibreSslX509Lookup, method) == 8);
static_assert(offsetof(LibreSslX509Lookup, methodData) == 16);
static_assert(offsetof(LibreSslX509Lookup, storeContext) == 24);

namespace {

constexpr int MinimumNodes = 4;
constexpr std::uint64_t PolicyCheck = 0x80;
constexpr std::uint64_t PolicyMask = 0x80 | 0x100 | 0x200 | 0x400;

using Compare = int (APS5_VABI*)(const void*, const void*);

struct Stack {
    std::int32_t num;
    char** data;
    std::int32_t sorted;
    std::int32_t numAlloc;
    Compare comp;
};

static_assert(sizeof(Stack) == 32);
static_assert(offsetof(Stack, data) == 8);
static_assert(offsetof(Stack, numAlloc) == 20);
static_assert(offsetof(Stack, comp) == 24);

int APS5_VABI CompareObjects(const void*, const void*) {
    NotImplemented_nid_no_patch("X509_STORE object comparison");
    return 0;
}

int APS5_VABI LoadFile(LibreSslX509Lookup*, int, const char*, std::int64_t, char**) {
    NotImplemented_nid_no_patch("X509_LOOKUP_file: loading certificates and CRLs from a file");
    return 0;
}

LibreSslX509LookupMethod fileLookup{"Load file into cache", nullptr, nullptr, nullptr, nullptr,
    reinterpret_cast<void*>(LoadFile), nullptr, nullptr, nullptr, nullptr};

Stack* NewStack(Compare compare) {
    auto* stack = static_cast<Stack*>(malloc_nid_postfix(sizeof(Stack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(malloc_nid_postfix(sizeof(char*) * MinimumNodes));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    std::fill_n(stack->data, MinimumNodes, nullptr);
    stack->comp = compare;
    stack->numAlloc = MinimumNodes;
    stack->num = 0;
    stack->sorted = 0;
    return stack;
}

void FreeStack(Stack* stack) {
    if (!stack) return;
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

char** Elements(const Stack* stack, const char* function) {
    if (!stack->data) NotImplemented_nid_no_patch(function);
    return stack->data;
}

bool Push(Stack* stack, void* element, const char* function) {
    if (!stack) return false;
    if (stack->numAlloc <= stack->num + 1) {
        if (stack->numAlloc <= 0 || stack->num < 0) NotImplemented_nid_no_patch(function);
        auto* grown = static_cast<char**>(realloc_nid_postfix(stack->data, static_cast<std::size_t>(stack->numAlloc) * 2 * sizeof(char*)));
        if (!grown) return false;
        stack->data = grown;
        stack->numAlloc *= 2;
    }
    stack->data[stack->num] = static_cast<char*>(element);
    ++stack->num;
    stack->sorted = 0;
    return true;
}

void FreeLookup(LibreSslX509Lookup* lookup, bool shutdown, const char* function) {
    if (!lookup) NotImplemented_nid_no_patch(function);
    const auto* method = lookup->method;
    if (shutdown && method && method->shutdown) NotImplemented_nid_no_patch("X509_LOOKUP shutdown callback");
    if (method && method->free) NotImplemented_nid_no_patch("X509_LOOKUP free callback");
    free_nid_postfix(lookup);
}

bool HoldsElements(const Stack* stack, const char* function) {
    if (!stack || stack->num <= 0) return false;
    char** const elements = Elements(stack, function);
    return std::any_of(elements, elements + stack->num, [](const char* element) { return element != nullptr; });
}

void FreeStrings(Stack* stack, const char* function) {
    if (!stack) return;
    if (stack->num > 0) {
        char** const elements = Elements(stack, function);
        std::for_each(elements, elements + stack->num, [](char* element) { free_nid_postfix(element); });
    }
    FreeStack(stack);
}

LibreSslX509VerifyParam* NewVerifyParam() {
    auto* param = static_cast<LibreSslX509VerifyParam*>(calloc_nid_postfix(1, sizeof(LibreSslX509VerifyParam)));
    if (!param) return nullptr;
    auto* id = static_cast<LibreSslX509VerifyParamId*>(calloc_nid_postfix(1, sizeof(LibreSslX509VerifyParamId)));
    if (!id) {
        free_nid_postfix(param);
        return nullptr;
    }
    param->id = id;
    param->depth = -1;
    return param;
}

void FreeVerifyParam(LibreSslX509VerifyParam* param, const char* function) {
    if (!param) return;
    auto* policies = static_cast<Stack*>(param->policies);
    if (HoldsElements(policies, function)) NotImplemented_nid_no_patch("X509_VERIFY_PARAM policy objects");
    FreeStack(policies);
    auto* id = param->id;
    if (!id) NotImplemented_nid_no_patch(function);
    FreeStrings(static_cast<Stack*>(id->hosts), function);
    free_nid_postfix(id->peerName);
    free_nid_postfix(id->email);
    free_nid_postfix(id->ip);
    free_nid_postfix(id);
    free_nid_postfix(param);
}

} // namespace

extern "C" {

LibreSslX509Store* APS5_VABI X509_STORE_new_nid_postfix(void) {
    auto* store = static_cast<LibreSslX509Store*>(malloc_nid_postfix(sizeof(LibreSslX509Store)));
    if (!store) return nullptr;
    auto* objects = NewStack(CompareObjects);
    auto* lookupMethods = NewStack(nullptr);
    store->objects = objects;
    store->cache = 1;
    store->lookupMethods = lookupMethods;
    store->param = NewVerifyParam();
    if (!store->param) {
        FreeStack(lookupMethods);
        FreeStack(objects);
        free_nid_postfix(store);
        return nullptr;
    }
    std::fill(std::begin(store->callbacks), std::end(store->callbacks), nullptr);
    store->exData = nullptr;
    store->references = 1;
    return store;
}

void APS5_VABI X509_STORE_free_nid_postfix(LibreSslX509Store* store) {
    if (!store) return;
    if (std::atomic_ref<std::int32_t>(store->references).fetch_sub(1) - 1 > 0) return;
    auto* lookupMethods = static_cast<Stack*>(store->lookupMethods);
    if (lookupMethods && lookupMethods->num > 0) {
        char** const lookups = Elements(lookupMethods, __func__);
        for (int index = 0; index < lookupMethods->num; ++index)
            FreeLookup(reinterpret_cast<LibreSslX509Lookup*>(lookups[index]), true, __func__);
    }
    FreeStack(lookupMethods);
    auto* objects = static_cast<Stack*>(store->objects);
    if (HoldsElements(objects, __func__)) NotImplemented_nid_no_patch("X509_STORE_free: stored certificates and CRLs");
    FreeStack(objects);
    FreeStack(static_cast<Stack*>(store->exData));
    FreeVerifyParam(store->param, __func__);
    free_nid_postfix(store);
}

int APS5_VABI X509_STORE_set_flags_nid_postfix(LibreSslX509Store* store, std::uint64_t flags) {
    if (!store || !store->param) NotImplemented_nid_no_patch(__func__);
    store->param->flags |= flags;
    if (flags & PolicyMask) store->param->flags |= PolicyCheck;
    return 1;
}

LibreSslX509LookupMethod* APS5_VABI X509_LOOKUP_file_nid_postfix(void) {
    return &fileLookup;
}

LibreSslX509Lookup* APS5_VABI X509_STORE_add_lookup_nid_postfix(LibreSslX509Store* store, LibreSslX509LookupMethod* method) {
    if (!store) NotImplemented_nid_no_patch(__func__);
    auto* lookupMethods = static_cast<Stack*>(store->lookupMethods);
    if (lookupMethods && lookupMethods->num > 0) {
        char** const lookups = Elements(lookupMethods, __func__);
        for (int index = 0; index < lookupMethods->num; ++index) {
            auto* lookup = reinterpret_cast<LibreSslX509Lookup*>(lookups[index]);
            if (!lookup) NotImplemented_nid_no_patch(__func__);
            if (lookup->method == method) return lookup;
        }
    }
    if (!method) NotImplemented_nid_no_patch(__func__);
    if (method->newItem) NotImplemented_nid_no_patch("X509_LOOKUP new_item callback");
    auto* lookup = static_cast<LibreSslX509Lookup*>(malloc_nid_postfix(sizeof(LibreSslX509Lookup)));
    if (!lookup) return nullptr;
    lookup->init = 0;
    lookup->skip = 0;
    lookup->method = method;
    lookup->methodData = nullptr;
    lookup->storeContext = store;
    if (Push(lookupMethods, lookup, __func__)) return lookup;
    FreeLookup(lookup, false, __func__);
    return nullptr;
}

}
