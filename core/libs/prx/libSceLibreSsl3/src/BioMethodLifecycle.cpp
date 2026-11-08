#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libc/libc.hpp"

namespace {
struct Method {
    int type;
    int padding;
    const char* name;
    unsigned char rest[64];
};
static_assert(sizeof(Method) == 80);
static_assert(offsetof(Method, name) == 8);
}

extern "C" {

void* APS5_VABI BIO_meth_new_nid_postfix(int type, const char* name) {
    auto* method = static_cast<Method*>(calloc_nid_postfix(1, sizeof(Method)));
    if (!method) return nullptr;
    method->type = type;
    method->name = name;
    return method;
}

void APS5_VABI BIO_meth_free_nid_postfix(void* method) {
    free_nid_postfix(method);
}

}
