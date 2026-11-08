#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <cstddef>
#include <stdexcept>

namespace {

struct Context {
    int flags;
    int padding;
    void* issuer;
    void* subject;
    void* request;
    void* crl;
    void* databaseMethod;
    void* database;
};

static_assert(sizeof(Context) == 56);
static_assert(offsetof(Context, issuer) == 8);
static_assert(offsetof(Context, subject) == 16);
static_assert(offsetof(Context, request) == 24);
static_assert(offsetof(Context, crl) == 32);
static_assert(offsetof(Context, databaseMethod) == 40);
static_assert(offsetof(Context, database) == 48);

}

extern "C" void APS5_VABI X509V3_set_ctx_nid_postfix(void* context, void* issuer, void* subject, void* request, void* crl, int flags) {
    if (!context) throw std::invalid_argument("X509V3_set_ctx: null context");
    auto* value = static_cast<Context*>(context);
    value->flags = flags;
    value->issuer = issuer;
    value->subject = subject;
    value->request = request;
    value->crl = crl;
}
