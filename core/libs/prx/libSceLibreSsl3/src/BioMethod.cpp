#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"

#include <cerrno>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

namespace {

template <typename T>
void WriteField(T* field, const T& value, const char* function) {
    const bool copied = GuestCopyOut_nid_no_patch(field, &value, sizeof(T));
    if (!copied) throw std::invalid_argument(std::string(function) + " requires a writable method");
}

}

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

int APS5_VABI BIO_meth_set_write_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::write) write) {
    if (!method) throw std::invalid_argument("BIO_meth_set_write requires a method");
    WriteField(&method->write, write, "BIO_meth_set_write");
    return 1;
}

int APS5_VABI BIO_meth_set_read_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::read) read) {
    if (!method) throw std::invalid_argument("BIO_meth_set_read requires a method");
    WriteField(&method->read, read, "BIO_meth_set_read");
    return 1;
}

int APS5_VABI BIO_meth_set_ctrl_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::control) ctrl) {
    if (!method) throw std::invalid_argument("BIO_meth_set_ctrl requires a method");
    WriteField(&method->control, ctrl, "BIO_meth_set_ctrl");
    return 1;
}

int APS5_VABI BIO_meth_set_create_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::create) create) {
    if (!method) throw std::invalid_argument("BIO_meth_set_create requires a method");
    WriteField(&method->create, create, "BIO_meth_set_create");
    return 1;
}

int APS5_VABI BIO_meth_set_destroy_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::destroy) destroy) {
    if (!method) throw std::invalid_argument("BIO_meth_set_destroy requires a method");
    WriteField(&method->destroy, destroy, "BIO_meth_set_destroy");
    return 1;
}

}
