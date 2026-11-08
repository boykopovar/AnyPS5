#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <source_location>
#include <cstdlib>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIO_meth setter check failed at line %u\n", location.line());
    std::abort();
}
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
int APS5_VABI Write(void*, const char*, int) { return 1; }
int APS5_VABI Read(void*, char*, int) { return 2; }
int APS5_VABI Create(void*) { return 3; }
int APS5_VABI Destroy(void*) { return 4; }
std::int64_t APS5_VABI Ctrl(void*, int, std::int64_t, void*) { return 5; }
}

int main() {
    Method method{};
    Require(BIO_meth_set_write_nid_postfix(&method, Write) == 1 && method.write == Write);
    Require(BIO_meth_set_read_nid_postfix(&method, Read) == 1 && method.read == Read);
    Require(BIO_meth_set_create_nid_postfix(&method, Create) == 1 && method.create == Create);
    Require(BIO_meth_set_ctrl_nid_postfix(&method, Ctrl) == 1 && method.ctrl == Ctrl);
    Require(BIO_meth_set_destroy_nid_postfix(&method, Destroy) == 1 && method.destroy == Destroy);
    Require(BIO_meth_set_write_nid_postfix(nullptr, Write) == 0);
    std::puts("libSceLibreSsl3 BIO_meth setter tests passed");
}
