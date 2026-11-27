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

LibreSslSession* NewSession() {
    auto* session = static_cast<LibreSslSession*>(calloc_nid_postfix(1, sizeof(LibreSslSession)));
    Require(session != nullptr, "session");
    session->references = 1;
    session->verifyResult = 42;
    session->tlsextHostname = static_cast<char*>(malloc_nid_postfix(5));
    Require(session->tlsextHostname != nullptr, "server name");
    std::memcpy(session->tlsextHostname, "host", 5);
    session->tlsextTick = static_cast<unsigned char*>(malloc_nid_postfix(3));
    Require(session->tlsextTick != nullptr, "ticket");
    return session;
}

void Run() {
    const long baseline = allocations;
    const LibreSslMethod* method = TLS_client_method_nid_postfix();
    Require(method != nullptr, "TLS client method");
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(method);
    Require(ctx != nullptr, "context");
    LibreSslSsl* ssl = SSL_new_nid_postfix(ctx);
    Require(ssl != nullptr, "ssl");

    LibreSslSession* session = NewSession();
    Require(SSL_set_session_nid_postfix(ssl, session) == 1, "the session is adopted");
    Require(ssl->session == session, "the connection keeps the session");
    Require(session->references == 2, "the adopted session is referenced");
    Require(ssl->verifyResult == 42, "the verification result is copied");
    Require(SSL_set_session_nid_postfix(ssl, session) == 1, "adopting the same session again");
    Require(session->references == 2, "the session is referenced once");
    Require(SSL_set_session_nid_postfix(ssl, nullptr) == 1, "the session is cleared");
    Require(ssl->session == nullptr, "the connection has no session");
    Require(session->references == 1, "the cleared session keeps the caller's reference");
    SSL_SESSION_free_nid_postfix(session);
    SSL_SESSION_free_nid_postfix(nullptr);
    bool threw = false;
    try {
        SSL_set_session_nid_postfix(nullptr, session);
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, "a null connection throws");
    SSL_free_nid_postfix(ssl);
    SSL_CTX_free_nid_postfix(ctx);
    Require(allocations == baseline, "the session storage is released");

    ctx = SSL_CTX_new_nid_postfix(method);
    Require(ctx != nullptr, "second context");
    ssl = SSL_new_nid_postfix(ctx);
    Require(ssl != nullptr, "second ssl");
    session = NewSession();
    Require(SSL_set_session_nid_postfix(ssl, session) == 1, "the session is adopted again");
    SSL_free_nid_postfix(ssl);
    Require(session->references == 1, "SSL_free releases the held reference");
    SSL_SESSION_free_nid_postfix(session);
    SSL_CTX_free_nid_postfix(ctx);
    Require(allocations == baseline, "SSL_free releases the session it held");
}

}

int main() {
    RegisterHeap();
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "SSL session set/free threw: %s\n", error.what());
        return 1;
    }
    std::puts("libSceLibreSsl SSL_set_session and SSL_SESSION_free tests passed");
    return 0;
}
