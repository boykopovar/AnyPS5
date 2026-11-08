#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <cstddef>
#include <cstdint>

namespace {

struct Method {
    int type;
    int padding;
    const char* name;
    int (APS5_VABI* write)(void*, const char*, int);
    int (APS5_VABI* read)(void*, char*, int);
    void* puts;
    void* gets;
    std::int64_t (APS5_VABI* ctrl)(void*, int, std::int64_t, void*);
    int (APS5_VABI* create)(void*);
    int (APS5_VABI* destroy)(void*);
    void* callbackCtrl;
};

static_assert(sizeof(Method) == 80);
static_assert(offsetof(Method, write) == 16);
static_assert(offsetof(Method, read) == 24);
static_assert(offsetof(Method, ctrl) == 48);
static_assert(offsetof(Method, create) == 56);
static_assert(offsetof(Method, destroy) == 64);

}

extern "C" {

int APS5_VABI BIO_meth_set_write_nid_postfix(void* method, int (APS5_VABI* write)(void*, const char*, int)) {
    if (!method) return 0;
    static_cast<Method*>(method)->write = write;
    return 1;
}

int APS5_VABI BIO_meth_set_read_nid_postfix(void* method, int (APS5_VABI* read)(void*, char*, int)) {
    if (!method) return 0;
    static_cast<Method*>(method)->read = read;
    return 1;
}

int APS5_VABI BIO_meth_set_create_nid_postfix(void* method, int (APS5_VABI* create)(void*)) {
    if (!method) return 0;
    static_cast<Method*>(method)->create = create;
    return 1;
}

int APS5_VABI BIO_meth_set_ctrl_nid_postfix(void* method, std::int64_t (APS5_VABI* ctrl)(void*, int, std::int64_t, void*)) {
    if (!method) return 0;
    static_cast<Method*>(method)->ctrl = ctrl;
    return 1;
}

int APS5_VABI BIO_meth_set_destroy_nid_postfix(void* method, int (APS5_VABI* destroy)(void*)) {
    if (!method) return 0;
    static_cast<Method*>(method)->destroy = destroy;
    return 1;
}

}
