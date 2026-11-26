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
    Require(SSL_CTX_set1_groups_list_nid_postfix(ctx, "P-256:X25519") == 1, "a named group list is accepted");
    Require(ctx->internal->tlsextSupportedgroupsLength == 2, "both groups are stored");
    Require(ctx->internal->tlsextSupportedgroups[0] == 23 && ctx->internal->tlsextSupportedgroups[1] == 29,
            "the groups use the TLS identifiers");
    Require(SSL_CTX_set1_groups_list_nid_postfix(ctx, "secp384r1") == 1, "a short name is accepted");
    Require(ctx->internal->tlsextSupportedgroupsLength == 1 && ctx->internal->tlsextSupportedgroups[0] == 24,
            "the replaced list is stored");
    Require(SSL_CTX_set1_groups_list_nid_postfix(ctx, "P-999") == 0, "an unknown group fails");
    Require(ctx->internal->tlsextSupportedgroupsLength == 1 && ctx->internal->tlsextSupportedgroups[0] == 24,
            "a failed list keeps the previous one");
    Require(SSL_CTX_set1_groups_list_nid_postfix(ctx, "") == 0, "an empty list fails");
    RequireInvalidArgument([&] { SSL_CTX_set1_groups_list_nid_postfix(ctx, nullptr); }, "a null list throws");
    RequireInvalidArgument([&] { SSL_CTX_set1_groups_list_nid_postfix(nullptr, "P-256"); }, "a null context throws");
    SSL_CTX_free_nid_postfix(ctx);
    Require(allocations == baseline, "the group list is released with the context");
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SSL_CTX_set1_groups_list threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl SSL_CTX_set1_groups_list tests passed");
    return 0;
}
