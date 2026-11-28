#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <stdexcept>
#include <cstdio>
#include <source_location>
#include <cstdlib>

namespace {
bool FailReallocation = false;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_CTX_add1_chain_cert check failed at line %u\n", location.line());
    std::abort();
}
void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) { std::free(pointer); }
void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (FailReallocation) return nullptr;
    return std::realloc(pointer, size);
}
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }
}
int main() {
    std::array<void*, 7> api{reinterpret_cast<void*>(Allocate_nid_no_patch), reinterpret_cast<void*>(Free_nid_no_patch),
        reinterpret_cast<void*>(Callocate_nid_no_patch), reinterpret_cast<void*>(Reallocate_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch), reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch),
        reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api.data());
    LibreSsl3SslCtx context{};
    LibreSsl3X509 first{};
    LibreSsl3X509 second{};
    first.references = 1;
    second.references = 1;
    Require(SSL_CTX_add1_chain_cert_nid_postfix(&context, &first) == 1);
    auto* stack = static_cast<LibreSslStack*>(context.extraCerts);
    Require(stack && stack->num == 1 && stack->data[0] == reinterpret_cast<char*>(&first));
    Require(first.references == 2);
    Require(SSL_CTX_add1_chain_cert_nid_postfix(&context, &second) == 1);
    Require(context.extraCerts == stack);
    Require(stack->num == 2 && stack->data[1] == reinterpret_cast<char*>(&second));
    Require(first.references == 2 && second.references == 2);
    Require(SSL_CTX_add1_chain_cert_nid_postfix(nullptr, &first) == 0);
    Require(SSL_CTX_add1_chain_cert_nid_postfix(&context, nullptr) == 0);

    LibreSsl3SslCtx fullContext{};
    std::array<LibreSsl3X509, 3> entries{};
    for (auto& entry : entries) {
        entry.references = 1;
        Require(SSL_CTX_add1_chain_cert_nid_postfix(&fullContext, &entry) == 1);
    }
    auto* fullStack = static_cast<LibreSslStack*>(fullContext.extraCerts);
    Require(fullStack && fullStack->num == 3);
    LibreSsl3X509 rejected{};
    rejected.references = 1;
    FailReallocation = true;
    Require(SSL_CTX_add1_chain_cert_nid_postfix(&fullContext, &rejected) == 0);
    FailReallocation = false;
    Require(rejected.references == 1 && fullStack->num == 3);
    Require(SSL_CTX_add1_chain_cert_nid_postfix(&fullContext, &rejected) == 1);
    Require(rejected.references == 2 && fullStack->num == 4 &&
        fullStack->data[3] == reinterpret_cast<char*>(&rejected));
    sk_free_nid_postfix(fullStack);
    sk_free_nid_postfix(stack);
    std::puts("libSceLibreSsl3 SSL_CTX_add1_chain_cert tests passed");
}
