#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t CleanupCallback = 11;

}

extern "C" {

LibreSslX509StoreContext* APS5_VABI X509_STORE_CTX_new_nid_postfix(void) {
    return static_cast<LibreSslX509StoreContext*>(calloc_nid_postfix(1, sizeof(LibreSslX509StoreContext)));
}

void APS5_VABI X509_STORE_CTX_free_nid_postfix(LibreSslX509StoreContext* context) {
    if (!context) return;
    if (context->callbacks[CleanupCallback]) NotImplemented_nid_no_patch("X509_STORE_CTX_free: cleanup callback");
    if (context->param && !context->parent) NotImplemented_nid_no_patch("X509_STORE_CTX_free: owned verify parameters");
    if (context->tree) NotImplemented_nid_no_patch("X509_STORE_CTX_free: policy tree");
    if (context->chain) NotImplemented_nid_no_patch("X509_STORE_CTX_free: certificate chain");
    if (context->exData) NotImplemented_nid_no_patch("X509_STORE_CTX_free: extra data");
    free_nid_postfix(context);
}

void* APS5_VABI X509_STORE_CTX_get0_param_nid_postfix(LibreSslX509StoreContext* context) {
    if (!context) throw std::invalid_argument("X509_STORE_CTX_get0_param: null context");
    return context->param;
}

int APS5_VABI X509_STORE_up_ref_nid_postfix(LibreSslX509Store* store) {
    if (!store) throw std::invalid_argument("X509_STORE_up_ref: null store");
    std::atomic_ref<std::int32_t> count(store->references);
    auto previous = count.load();
    for (;;) {
        if (previous == std::numeric_limits<std::int32_t>::max()) throw std::overflow_error("X509_STORE_up_ref: reference count overflow");
        if (count.compare_exchange_weak(previous, previous + 1)) return previous > 0;
    }
}

}
