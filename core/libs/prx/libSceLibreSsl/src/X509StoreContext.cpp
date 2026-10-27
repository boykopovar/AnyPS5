#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslX509StoreContext) == 240);
static_assert(offsetof(LibreSslX509StoreContext, chain) == 160);
static_assert(offsetof(LibreSslX509StoreContext, errorDepth) == 180);
static_assert(offsetof(LibreSslX509StoreContext, error) == 184);
static_assert(offsetof(LibreSslX509StoreContext, currentCert) == 192);
static_assert(offsetof(LibreSslX509StoreContext, exData) == 232);

namespace {

LibreSslX509StoreContext* Context(LibreSslX509StoreContext* context, const char* function) {
    if (!context) throw std::invalid_argument(std::string(function) + ": null context");
    return context;
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

}
