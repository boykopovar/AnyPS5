#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size ? size : 1); }
void APS5_VABI Release(void* pointer) { std::free(pointer); }
void* APS5_VABI Callocate(std::size_t count, std::size_t size) { return std::calloc(count ? count : 1, size ? size : 1); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void RegisterHeap() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release),
        reinterpret_cast<void*>(Callocate), reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align),
        reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

void Run() {
    const LibreSslMethod* method = TLS_client_method_nid_postfix();
    Require(method != nullptr, "TLS client method");
    LibreSslCtx* first = SSL_CTX_new_nid_postfix(method);
    LibreSslCtx* second = SSL_CTX_new_nid_postfix(method);
    Require(first != nullptr && second != nullptr, "contexts");
    Require(first->references == 1 && second->references == 1, "initial context references");
    LibreSslSsl* ssl = SSL_new_nid_postfix(first);
    Require(ssl != nullptr, "ssl");
    Require(ssl->ctx == first, "the connection starts with the creating context");
    Require(first->references == 3, "the connection holds the context and its initial context");
    Require(SSL_set_SSL_CTX_nid_postfix(ssl, second) == second, "the context is switched");
    Require(ssl->ctx == second, "the connection uses the new context");
    Require(second->references == 2, "the new context is referenced");
    Require(first->references == 2, "the replaced context loses one reference");
    Require(SSL_set_SSL_CTX_nid_postfix(ssl, second) == second, "switching to the same context is a no-op");
    Require(second->references == 2, "the same context is not referenced twice");
    Require(SSL_set_SSL_CTX_nid_postfix(ssl, nullptr) == first, "a null context restores the initial one");
    Require(ssl->ctx == first && first->references == 3, "the initial context is referenced again");
    Require(second->references == 1, "the abandoned context is released");
    SSL_free_nid_postfix(ssl);
    Require(first->references <= 2, "SSL_free releases at least the initial context reference");
    SSL_CTX_free_nid_postfix(first);
    SSL_CTX_free_nid_postfix(second);
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SSL_set_SSL_CTX threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl SSL_set_SSL_CTX tests passed");
    return 0;
}
