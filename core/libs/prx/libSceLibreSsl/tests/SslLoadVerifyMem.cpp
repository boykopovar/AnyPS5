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

long allocations = 0;

void* APS5_VABI Allocate(std::size_t size) { void* p = std::malloc(size ? size : 1); if (p) ++allocations; return p; }
void APS5_VABI Release(void* pointer) { if (pointer) --allocations; std::free(pointer); }
void* APS5_VABI Callocate(std::size_t count, std::size_t size) { void* p = std::calloc(count ? count : 1, size ? size : 1); if (p) ++allocations; return p; }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { if (!pointer) return Allocate(size); return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void RegisterHeap() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release),
        reinterpret_cast<void*>(Callocate), reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(Align),
        reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api.data());
}

template<typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    } catch (...) {
    }
    Require(false, message);
}

void Run() {
    const long baseline = allocations;
    const LibreSslMethod* method = TLS_client_method_nid_postfix();
    Require(method != nullptr, "TLS client method");
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(method);
    Require(ctx != nullptr, "context");
    auto* store = static_cast<LibreSslX509Store*>(ctx->certStore);
    Require(store != nullptr, "store");
    auto* lookups = static_cast<LibreSslStack*>(store->lookupMethods);
    Require(lookups != nullptr && lookups->num == 0, "the store starts without lookups");
    const char pem[] = "-----BEGIN CERTIFICATE-----\n";
    Require(SSL_CTX_load_verify_mem_nid_postfix(ctx, pem, static_cast<int>(sizeof(pem))) == 1, "the buffer is loaded");
    Require(lookups->num == 1, "a memory lookup is added");
    Require(SSL_CTX_load_verify_mem_nid_postfix(ctx, pem, static_cast<int>(sizeof(pem))) == 1, "a second buffer is loaded");
    Require(lookups->num == 1, "the same memory lookup is reused");
    Require(SSL_CTX_load_verify_mem_nid_postfix(ctx, nullptr, 0) == 1, "an empty buffer is accepted");
    Require(SSL_CTX_load_verify_mem_nid_postfix(ctx, nullptr, 4) == 0, "a null buffer with a length fails");
    Require(SSL_CTX_load_verify_mem_nid_postfix(ctx, pem, -1) == 0, "a negative length fails");
    RequireInvalidArgument([&] { SSL_CTX_load_verify_mem_nid_postfix(nullptr, pem, 4); }, "a null context throws");
    SSL_CTX_free_nid_postfix(ctx);
    Require(allocations == baseline, "the loaded buffers are released with the context");
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SSL_CTX_load_verify_mem threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl SSL_CTX_load_verify_mem tests passed");
    return 0;
}
