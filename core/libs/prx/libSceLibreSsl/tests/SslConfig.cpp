#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
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

void SslParameters() {
    const LibreSslMethod* method = TLS_client_method_nid_postfix();
    Require(method != nullptr, "TLS client method");
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(method);
    Require(ctx != nullptr, "context");
    LibreSslSsl* ssl = SSL_new_nid_postfix(ctx);
    Require(ssl != nullptr, "ssl");
    Require(ssl->param != nullptr, "verify parameters are allocated");
    Require(SSL_get0_param_nid_postfix(ssl) == static_cast<LibreSslX509VerifyParam*>(ssl->param), "SSL_get0_param returns the connection parameters");
    Require(SSL_get_servername_nid_postfix(ssl, 0) == nullptr, "a fresh connection has no server name");
    Require(SSL_get_servername_nid_postfix(ssl, 1) == nullptr, "another name type is empty");
    ssl->tlsextHostname = static_cast<char*>(malloc_nid_postfix(12));
    Require(ssl->tlsextHostname != nullptr, "server name storage");
    std::memcpy(ssl->tlsextHostname, "example.com", 12);
    Require(std::strcmp(SSL_get_servername_nid_postfix(ssl, 0), "example.com") == 0, "the client server name is returned");
    Require(SSL_get_servername_nid_postfix(ssl, 1) == nullptr, "the server name type is checked first");
    SSL_free_nid_postfix(ssl);
    SSL_CTX_free_nid_postfix(ctx);
}

void AlpnProtocols() {
    const LibreSslMethod* method = TLS_client_method_nid_postfix();
    Require(method != nullptr, "TLS client method");
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(method);
    Require(ctx != nullptr, "context");
    const unsigned char protocols[] = {2, 'h', '2'};
    Require(SSL_CTX_set_alpn_protos_nid_postfix(ctx, protocols, sizeof(protocols)) == 0, "a protocol list is accepted");
    Require(ctx->internal->alpnClientProtoListLen == sizeof(protocols), "the protocol list length is stored");
    Require(std::memcmp(ctx->internal->alpnClientProtoList, protocols, sizeof(protocols)) == 0, "the protocol list is copied");
    const unsigned char replacement[] = {8, 'h', 't', 't', 'p', '/', '1', '.', '1'};
    Require(SSL_CTX_set_alpn_protos_nid_postfix(ctx, replacement, sizeof(replacement)) == 0, "a second list replaces the first");
    Require(ctx->internal->alpnClientProtoListLen == sizeof(replacement), "the replaced list length is stored");
    Require(std::memcmp(ctx->internal->alpnClientProtoList, replacement, sizeof(replacement)) == 0, "the replaced list is copied");
    Require(SSL_CTX_set_alpn_protos_nid_postfix(ctx, nullptr, 3) == 1, "a null list fails");
    Require(SSL_CTX_set_alpn_protos_nid_postfix(ctx, protocols, 0) == 1, "an empty list fails");
    Require(ctx->internal->alpnClientProtoListLen == sizeof(replacement), "a rejected list keeps the previous one");
    RequireInvalidArgument([&] { SSL_CTX_set_alpn_protos_nid_postfix(nullptr, protocols, 3); }, "a null context throws");
    SSL_CTX_free_nid_postfix(ctx);
}

}

int main() {
    RegisterHeap();
    try {
        SslParameters();
        AlpnProtocols();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SSL configuration threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl SSL configuration tests passed");
    return 0;
}
