#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>

namespace {

void* APS5_VABI Allocate(std::size_t bytes) { return std::malloc(bytes ? bytes : 1); }
void APS5_VABI Release(void* block) { std::free(block); }
void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) { return std::calloc(count ? count : 1, bytes ? bytes : 1); }
void* APS5_VABI Reallocate(void* block, std::size_t bytes) { return std::realloc(block, bytes ? bytes : 1); }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10]{
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

constexpr unsigned long ExpectedConnectionTypeNotSetError =
    (static_cast<unsigned long>(SslLayout::ErrorLibrarySsl) << 24) |
    (static_cast<unsigned long>(SslLayout::ErrorFunctionAny) << 12) |
    144UL;

int callbackCalls = 0;
int callbackReturnValue = 1;
LibreSslSsl* callbackReceivedSsl = nullptr;

int APS5_VABI TestHandshakeCallback_nid_no_patch(LibreSslSsl* ssl) {
    ++callbackCalls;
    callbackReceivedSsl = ssl;
    return callbackReturnValue;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_do_handshake check failed at line %u\n", location.line());
    std::abort();
}

template <typename TCall>
void RejectsPreservingErrno(TCall call, std::source_location location = std::source_location::current()) {
    errno = 73;
    bool rejected = false;
    try {
        call();
    } catch (const std::exception&) {
        rejected = errno == 73;
    }
    Require(rejected, location);
}

void TestGuestPointers() {
    RejectsPreservingErrno([&] { SSL_do_handshake_nid_postfix(nullptr); });

    auto* unmappedSsl = reinterpret_cast<LibreSslSsl*>(std::uintptr_t{0x10});
    RejectsPreservingErrno([&] { SSL_do_handshake_nid_postfix(unmappedSsl); });

    LibreSslSsl ssl{};
    RejectsPreservingErrno([&] { SSL_do_handshake_nid_postfix(&ssl); });

    ssl.internal = reinterpret_cast<LibreSslSslInternal*>(std::uintptr_t{0x10});
    RejectsPreservingErrno([&] { SSL_do_handshake_nid_postfix(&ssl); });
}

void TestUnsetHandshakeFunction() {
    ERR_clear_error_nid_postfix();
    LibreSslSsl ssl{};
    LibreSslSslInternal internal{};
    ssl.internal = &internal;
    internal.state = SslLayout::StateConnect | SslLayout::StateBefore;

    Require(SSL_do_handshake_nid_postfix(&ssl) == -1);
    Require(ERR_get_error_nid_postfix() == ExpectedConnectionTypeNotSetError);
    Require(ERR_get_error_nid_postfix() == 0);
}

void TestDispatchAndStates() {
    LibreSslSsl ssl{};
    LibreSslSslInternal internal{};
    ssl.internal = &internal;
    internal.handshakeFunc = reinterpret_cast<void*>(&TestHandshakeCallback_nid_no_patch);

    const int activeStates[] = {
        SslLayout::StateConnect | SslLayout::StateBefore,
        SslLayout::StateAccept | SslLayout::StateBefore,
        SslLayout::StateBefore,
        SslLayout::StateConnectHelloA,
        SslLayout::StateAcceptHelloA,
    };
    const int returns[] = {1, 0, -1};
    for (int state : activeStates) {
        for (int expected : returns) {
            internal.state = state;
            callbackCalls = 0;
            callbackReceivedSsl = nullptr;
            callbackReturnValue = expected;
            Require(SSL_do_handshake_nid_postfix(&ssl) == expected);
            Require(callbackCalls == 1);
            Require(callbackReceivedSsl == &ssl);
        }
    }

    internal.state = SslLayout::StateOk;
    callbackCalls = 0;
    callbackReceivedSsl = nullptr;
    callbackReturnValue = -1;
    Require(SSL_do_handshake_nid_postfix(&ssl) == 1);
    Require(callbackCalls == 0);
    Require(callbackReceivedSsl == nullptr);
}

void TestRenegotiateThrows() {
    LibreSslSsl ssl{};
    LibreSslSslInternal internal{};
    ssl.internal = &internal;
    internal.state = SslLayout::StateOk;
    internal.handshakeFunc = reinterpret_cast<void*>(&TestHandshakeCallback_nid_no_patch);
    SslLayout::Write<int>(&internal, SslLayout::InternalRenegotiate, 1);

    callbackCalls = 0;
    RejectsPreservingErrno([&] { SSL_do_handshake_nid_postfix(&ssl); });
    Require(callbackCalls == 0);
}

void TestParentSslLifecycle() {
    ERR_clear_error_nid_postfix();
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    Require(ctx != nullptr);
    LibreSslSsl* ssl = SSL_new_nid_postfix(ctx);
    Require(ssl != nullptr);

    Require(SSL_do_handshake_nid_postfix(ssl) == -1);
    Require(ERR_get_error_nid_postfix() == ExpectedConnectionTypeNotSetError);

    SSL_set_connect_state_nid_postfix(ssl);
    bool dispatchedToConnect = false;
    try {
        SSL_do_handshake_nid_postfix(ssl);
    } catch (const std::exception&) {
        dispatchedToConnect = true;
    }
    Require(dispatchedToConnect);

    SSL_free_nid_postfix(ssl);
    SSL_CTX_free_nid_postfix(ctx);
}

} // namespace

int main() {
    InitializeHeap();
    TestGuestPointers();
    TestUnsetHandshakeFunction();
    TestDispatchAndStates();
    TestRenegotiateThrows();
    TestParentSslLifecycle();

    std::puts("libSceLibreSsl3 SSL_do_handshake tests passed");
    return 0;
}
