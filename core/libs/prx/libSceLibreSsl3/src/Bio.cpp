#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"

#include <stdexcept>

namespace {

template<typename T>
T& Checked(T* object) {
    if (!object) throw std::invalid_argument("LibreSSL3 BIO operation requires an object");
    return *object;
}

} // namespace

extern "C" {

LibreSsl3BioMethod* APS5_VABI BIO_meth_new_nid_postfix(int type, const char* name) {
    auto* method = static_cast<LibreSsl3BioMethod*>(calloc_nid_postfix(1, sizeof(LibreSsl3BioMethod)));
    if (method) {
        method->type = type;
        method->name = name;
    }
    return method;
}

void APS5_VABI BIO_meth_free_nid_postfix(LibreSsl3BioMethod* method) {
    free_nid_postfix(method);
}

int APS5_VABI BIO_meth_set_write_nid_postfix(LibreSsl3BioMethod* method, LibreSsl3BioWrite callback) {
    Checked(method).write = callback;
    return 1;
}

int APS5_VABI BIO_meth_set_read_nid_postfix(LibreSsl3BioMethod* method, LibreSsl3BioRead callback) {
    Checked(method).read = callback;
    return 1;
}

int APS5_VABI BIO_meth_set_ctrl_nid_postfix(LibreSsl3BioMethod* method, LibreSsl3BioControl callback) {
    Checked(method).control = callback;
    return 1;
}

int APS5_VABI BIO_meth_set_create_nid_postfix(LibreSsl3BioMethod* method, LibreSsl3BioLifecycle callback) {
    Checked(method).create = callback;
    return 1;
}

int APS5_VABI BIO_meth_set_destroy_nid_postfix(LibreSsl3BioMethod* method, LibreSsl3BioLifecycle callback) {
    Checked(method).destroy = callback;
    return 1;
}

void* APS5_VABI BIO_get_data_nid_postfix(LibreSsl3Bio* bio) {
    return Checked(bio).data;
}

void APS5_VABI BIO_set_data_nid_postfix(LibreSsl3Bio* bio, void* data) {
    Checked(bio).data = data;
}

void APS5_VABI BIO_set_init_nid_postfix(LibreSsl3Bio* bio, int initialized) {
    Checked(bio).initialized = initialized;
}

int APS5_VABI BIO_get_shutdown_nid_postfix(LibreSsl3Bio* bio) {
    return Checked(bio).shutdown;
}

void APS5_VABI BIO_set_shutdown_nid_postfix(LibreSsl3Bio* bio, int shutdown) {
    Checked(bio).shutdown = shutdown;
}

void APS5_VABI BIO_set_flags_nid_postfix(LibreSsl3Bio* bio, int flags) {
    auto& value = Checked(bio);
    value.flags |= flags;
}

void APS5_VABI BIO_clear_flags_nid_postfix(LibreSsl3Bio* bio, int flags) {
    auto& value = Checked(bio);
    value.flags &= ~flags;
}

}
