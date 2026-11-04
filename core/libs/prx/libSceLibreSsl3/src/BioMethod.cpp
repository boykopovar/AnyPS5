#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"

#include <stdexcept>

extern "C" {

LibreSsl::BioMethod* APS5_VABI BIO_meth_new_nid_postfix(int type, const char* name) {
    auto* method = static_cast<LibreSsl::BioMethod*>(calloc_nid_postfix(1, sizeof(LibreSsl::BioMethod)));
    if (!method) return nullptr;
    method->type = type;
    method->name = name;
    return method;
}

void APS5_VABI BIO_meth_free_nid_postfix(LibreSsl::BioMethod* method) {
    free_nid_postfix(method);
}

}
