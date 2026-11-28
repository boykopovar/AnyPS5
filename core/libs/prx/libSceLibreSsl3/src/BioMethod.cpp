#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* function) {
    if (!bytes) return;
    ErrnoGuard saved;
    GuestArena::HostWrite guard(destination, bytes);
    if (!destination || !guard.Open() || !GuestWritable_nid_no_patch(destination, bytes)
        || !GuestCopyOut_nid_no_patch(destination, source, bytes)) {
        throw std::invalid_argument(std::string(function) + " requires a writable method");
    }
}

template <typename T>
void WriteField(LibreSsl::BioMethod* method, std::size_t offset, const T& value, const char* function) {
    CopyOut(reinterpret_cast<unsigned char*>(method) + offset, &value, sizeof(T), function);
}

}

extern "C" {

LibreSsl::BioMethod* APS5_VABI BIO_meth_new_nid_postfix(int type, const char* name) {
    auto* method = static_cast<LibreSsl::BioMethod*>(calloc_nid_postfix(1, sizeof(LibreSsl::BioMethod)));
    if (!method) return nullptr;
    try {
        LibreSsl::BioMethod staged{};
        staged.type = type;
        staged.name = name;
        CopyOut(method, &staged, sizeof(staged), "BIO_meth_new");
    } catch (...) {
        free_nid_postfix(method);
        throw;
    }
    return method;
}

void APS5_VABI BIO_meth_free_nid_postfix(LibreSsl::BioMethod* method) {
    free_nid_postfix(method);
}

int APS5_VABI BIO_meth_set_write_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::write) write) {
    if (!method) throw std::invalid_argument("BIO_meth_set_write requires a method");
    WriteField(method, offsetof(LibreSsl::BioMethod, write), write, "BIO_meth_set_write");
    return 1;
}

int APS5_VABI BIO_meth_set_read_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::read) read) {
    if (!method) throw std::invalid_argument("BIO_meth_set_read requires a method");
    WriteField(method, offsetof(LibreSsl::BioMethod, read), read, "BIO_meth_set_read");
    return 1;
}

int APS5_VABI BIO_meth_set_ctrl_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::control) ctrl) {
    if (!method) throw std::invalid_argument("BIO_meth_set_ctrl requires a method");
    WriteField(method, offsetof(LibreSsl::BioMethod, control), ctrl, "BIO_meth_set_ctrl");
    return 1;
}

int APS5_VABI BIO_meth_set_create_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::create) create) {
    if (!method) throw std::invalid_argument("BIO_meth_set_create requires a method");
    WriteField(method, offsetof(LibreSsl::BioMethod, create), create, "BIO_meth_set_create");
    return 1;
}

int APS5_VABI BIO_meth_set_destroy_nid_postfix(LibreSsl::BioMethod* method, decltype(LibreSsl::BioMethod::destroy) destroy) {
    if (!method) throw std::invalid_argument("BIO_meth_set_destroy requires a method");
    WriteField(method, offsetof(LibreSsl::BioMethod, destroy), destroy, "BIO_meth_set_destroy");
    return 1;
}

}
