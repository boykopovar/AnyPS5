#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslX509StoreContext) == 240);
static_assert(offsetof(LibreSslX509StoreContext, chain) == 160);
static_assert(offsetof(LibreSslX509StoreContext, errorDepth) == 180);
static_assert(offsetof(LibreSslX509StoreContext, error) == 184);
static_assert(offsetof(LibreSslX509StoreContext, currentCert) == 192);
static_assert(offsetof(LibreSslX509StoreContext, exData) == 232);
static_assert(offsetof(LibreSslX509, references) == 28);

namespace {

LibreSslX509StoreContext* Context(LibreSslX509StoreContext* context, const char* function) {
    if (!context) throw std::invalid_argument(std::string(function) + ": null context");
    return context;
}

constexpr int MinimumNodes = 4;

void FreeStack(LibreSslStack* stack) {
    free_nid_postfix(stack->data);
    free_nid_postfix(stack);
}

LibreSslStack* Duplicate(const LibreSslStack* source) {
    auto* const stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(malloc_nid_postfix(MinimumNodes * sizeof(char*)));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    for (int index = 0; index < MinimumNodes; ++index) stack->data[index] = nullptr;
    stack->comp = source->comp;
    stack->numAlloc = MinimumNodes;
    stack->num = 0;
    stack->sorted = 0;
    auto* const data = static_cast<char**>(realloc_nid_postfix(stack->data, static_cast<std::size_t>(source->numAlloc) * sizeof(char*)));
    if (!data) {
        FreeStack(stack);
        return nullptr;
    }
    stack->data = data;
    stack->num = source->num;
    if (source->num > 0) std::memcpy(stack->data, source->data, sizeof(char*) * static_cast<std::size_t>(source->num));
    stack->sorted = source->sorted;
    stack->numAlloc = source->numAlloc;
    stack->comp = source->comp;
    return stack;
}

}

extern "C" {

int APS5_VABI X509_STORE_CTX_get_error_nid_postfix(LibreSslX509StoreContext* context) {
    return Context(context, __func__)->error;
}

void APS5_VABI X509_STORE_CTX_set_error_nid_postfix(LibreSslX509StoreContext* context, int error) {
    Context(context, __func__)->error = error;
}

int APS5_VABI X509_STORE_CTX_get_error_depth_nid_postfix(LibreSslX509StoreContext* context) {
    return Context(context, __func__)->errorDepth;
}

void* APS5_VABI X509_STORE_CTX_get_current_cert_nid_postfix(LibreSslX509StoreContext* context) {
    return Context(context, __func__)->currentCert;
}

void* APS5_VABI X509_STORE_CTX_get_chain_nid_postfix(LibreSslX509StoreContext* context) {
    return Context(context, __func__)->chain;
}

void* APS5_VABI X509_STORE_CTX_get_ex_data_nid_postfix(LibreSslX509StoreContext* context, int index) {
    const auto* const data = static_cast<const LibreSslStack*>(Context(context, __func__)->exData);
    if (!data || index >= sk_num_nid_postfix(data)) return nullptr;
    return sk_value_nid_postfix(data, index);
}

LibreSslStack* APS5_VABI X509_STORE_CTX_get1_chain_nid_postfix(LibreSslX509StoreContext* context) {
    const auto* const chain = static_cast<const LibreSslStack*>(Context(context, __func__)->chain);
    if (!chain) return nullptr;
    if (chain->num < 0 || chain->num > chain->numAlloc || (chain->num > 0 && !chain->data)) throw std::invalid_argument(std::string(__func__) + ": malformed chain stack");
    for (int index = 0; index < chain->num; ++index) {
        if (!chain->data[index]) throw std::invalid_argument(std::string(__func__) + ": null certificate in the chain");
    }
    auto* const copy = Duplicate(chain);
    if (!copy) return nullptr;
    for (int index = 0; index < copy->num; ++index) {
        auto* const certificate = reinterpret_cast<LibreSslX509*>(copy->data[index]);
        std::atomic_ref<std::int32_t>(certificate->references).fetch_add(1);
    }
    return copy;
}

}
