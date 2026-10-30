#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <source_location>
#include <stdexcept>

using namespace SslLayout;

namespace {

bool failHostAllocations = false;

}

void* operator new(std::size_t size) {
    if (failHostAllocations) throw std::bad_alloc();
    if (void* pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace {

std::set<void*> live;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL SSL lifecycle check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) live.insert(pointer);
    return pointer;
}

void APS5_VABI Release(void* pointer) {
    if (!pointer) return;
    Require(live.erase(pointer) == 1);
    std::free(pointer);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t size) {
    void* pointer = std::calloc(count, size);
    if (pointer) live.insert(pointer);
    return pointer;
}

void* APS5_VABI Reallocate(void* pointer, std::size_t size) {
    if (pointer) Require(live.erase(pointer) == 1);
    void* moved = std::realloc(pointer, size);
    if (moved) live.insert(moved);
    return moved;
}

void* APS5_VABI Align(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

void InitializeHeap() {
    void* const api[10]{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Release), reinterpret_cast<void*>(AllocateZeroed), reinterpret_cast<void*>(Reallocate),
        reinterpret_cast<void*>(Align), reinterpret_cast<void*>(Realign), reinterpret_cast<void*>(PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
}

template <typename TTest>
void Run(TTest test) {
    const std::set<void*> before = live;
    test();
    Require(live == before);
}

constexpr std::int64_t ExArgument = 11;
constexpr unsigned int ErrorLibraryMask = 0xff;
constexpr int SslCtrlOptions = 32;
constexpr int SslCtrlMode = 33;
constexpr int SslCtrlGetReadAhead = 40;
constexpr int SslCtrlSetReadAhead = 41;
constexpr int SslCtrlGetMaxCertList = 50;
constexpr int SslCtrlSetMaxCertList = 51;
constexpr int SslCtrlSetMaxSendFragment = 52;
constexpr int SslCtrlSetMessageCallbackArgument = 16;

template <typename TAction>
void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return;
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

unsigned int Library(std::uint64_t code) { return static_cast<unsigned int>(code >> 24) & ErrorLibraryMask; }
unsigned int Function(std::uint64_t code) { return static_cast<unsigned int>(code >> 12) & 0xfff; }
unsigned int Reason(std::uint64_t code) { return static_cast<unsigned int>(code) & 0xfff; }

void RequireQueuedError(unsigned int function, unsigned int reason, std::source_location location = std::source_location::current()) {
    const std::uint64_t code = ERR_get_error_nid_postfix();
    Require(code != 0 && Library(code) == 20 && Function(code) == function && Reason(code) == reason, location);
    Require(ERR_get_error_nid_postfix() == 0, location);
}

int contextCreated = 0;
int contextReleased = 0;
int sslCreated = 0;
int sslReleased = 0;
void* lastContextCreated = nullptr;
void* lastContextReleased = nullptr;
void* lastSslCreated = nullptr;
void* lastSslReleased = nullptr;
int contextIndex = -1;
int sslIndex = -1;
char exMarker = 0;

int APS5_VABI ContextCreated(void* parent, void* pointer, LibreSslExData*, int index, std::int64_t argl, void* argp) {
    Require(pointer == nullptr && index == contextIndex && argl == ExArgument && argp == &exMarker);
    ++contextCreated;
    lastContextCreated = parent;
    return 1;
}

void APS5_VABI ContextReleased(void* parent, void* pointer, LibreSslExData*, int index, std::int64_t argl, void* argp) {
    Require(pointer == nullptr && index == contextIndex && argl == ExArgument && argp == &exMarker);
    ++contextReleased;
    lastContextReleased = parent;
}

int APS5_VABI SslCreated(void* parent, void* pointer, LibreSslExData*, int index, std::int64_t argl, void* argp) {
    Require(pointer == nullptr && index == sslIndex && argl == ExArgument && argp == &exMarker);
    ++sslCreated;
    lastSslCreated = parent;
    return 1;
}

void APS5_VABI SslReleased(void* parent, void* pointer, LibreSslExData*, int index, std::int64_t argl, void* argp) {
    Require(pointer == nullptr && index == sslIndex && argl == ExArgument && argp == &exMarker);
    ++sslReleased;
    lastSslReleased = parent;
}

void RegisterCallbacks() {
    contextIndex = SSL_CTX_get_ex_new_index_nid_postfix(ExArgument, &exMarker, ContextCreated, nullptr, ContextReleased);
    sslIndex = SSL_get_ex_new_index_nid_postfix(ExArgument, &exMarker, SslCreated, nullptr, SslReleased);
    Require(contextIndex >= 0 && sslIndex >= 0);
}

template <typename TValue>
TValue InternalField(const LibreSsl* ssl, std::size_t offset) {
    return Read<TValue>(ssl->internal, offset);
}

void TestMethodGetters() {
    const LibreSslMethod* client = TLS_client_method_nid_postfix();
    const LibreSslMethod* server = SSLv23_server_method_nid_postfix();
    const LibreSslMethod* datagramClient = DTLSv1_client_method_nid_postfix();
    const LibreSslMethod* datagramServer = DTLSv1_server_method_nid_postfix();
    Require(client && server && datagramClient && datagramServer);
    Require(client == TLS_client_method_nid_postfix());
    Require(client == SSLv23_client_method_nid_postfix());
    Require(server == SSLv23_server_method_nid_postfix());
    Require(datagramClient == DTLSv1_client_method_nid_postfix());
    Require(datagramServer == DTLSv1_server_method_nid_postfix());
    Require(client != server && client != datagramClient && client != datagramServer);
    Require(server != datagramClient && server != datagramServer && datagramClient != datagramServer);
    for (const LibreSslMethod* method : {client, server}) {
        Require(method->internal->version == Tls1_2Version);
        Require(method->internal->minVersion == Tls1Version);
        Require(method->internal->maxVersion == Tls1_2Version);
        Require(method->internal->getTimeout() == DefaultTimeout);
        Require(method->internal->ssl3Enc && method->internal->ssl3Enc->encFlags == 0x17);
    }
    for (const LibreSslMethod* method : {datagramClient, datagramServer}) {
        Require(method->internal->version == Dtls1Version);
        Require(method->internal->minVersion == Dtls1Version);
        Require(method->internal->maxVersion == Dtls1Version);
        Require(method->internal->getTimeout() == DefaultTimeout);
        Require(method->internal->ssl3Enc && method->internal->ssl3Enc->encFlags == 0x01);
    }
    Require(client->internal->ssl3Enc == server->internal->ssl3Enc);
    Require(client->internal->ssl3Enc != datagramClient->internal->ssl3Enc);
    Require(client->internal->sslConnect != client->internal->sslAccept);
    Require(server->internal->sslConnect != server->internal->sslAccept);
    Require(client->internal->sslAccept == server->internal->sslConnect);
    Require(client->internal->sslAccept == datagramClient->internal->sslAccept);
    Require(client->internal->sslConnect != server->internal->sslConnect);
}

void TestMethodSlots() {
    const LibreSslMethod* client = TLS_client_method_nid_postfix();
    const LibreSslMethod* server = SSLv23_server_method_nid_postfix();
    LibreSslCtx* clientContext = SSL_CTX_new_nid_postfix(client);
    LibreSslCtx* serverContext = SSL_CTX_new_nid_postfix(server);
    LibreSsl* clientSsl = SSL_new_nid_postfix(clientContext);
    LibreSsl* serverSsl = SSL_new_nid_postfix(serverContext);
    Require(clientSsl && serverSsl);
    ERR_clear_error_nid_postfix();

    Require(client->internal->sslAccept(clientSsl) == 0);
    RequireQueuedError(78, ErrorShouldNotHaveBeenCalled);
    Require(server->internal->sslConnect(serverSsl) == 0);
    RequireQueuedError(80, ErrorShouldNotHaveBeenCalled);
    Require(client->internal->sslRenegotiate(clientSsl) == 0);
    RequireQueuedError(78, ErrorShouldNotHaveBeenCalled);
    Require(client->internal->sslRenegotiateCheck(clientSsl) == 1);
    Require(ERR_get_error_nid_postfix() == 0);
    Require(client->internal->sslVersion() == 0);
    RequireQueuedError(0xfff, ErrorShouldNotHaveBeenCalled);

    SSL_free_nid_postfix(clientSsl);
    SSL_free_nid_postfix(serverSsl);
    SSL_CTX_free_nid_postfix(clientContext);
    SSL_CTX_free_nid_postfix(serverContext);
}

void TestContextNew() {
    ERR_clear_error_nid_postfix();
    Require(SSL_CTX_new_nid_postfix(nullptr) == nullptr);
    RequireQueuedError(0xfff, ReasonNullSslMethodPassed);

    const int created = contextCreated;
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    Require(ctx != nullptr);
    Require(contextCreated == created + 1 && lastContextCreated == ctx);
    Require(ctx->method == TLS_client_method_nid_postfix());
    Require(ctx->references == 1);
    Require(ctx->sessionTimeout == DefaultTimeout);
    Require(ctx->verifyMode == 0 && ctx->sidCtxLength == 0);
    Require(ctx->cipherList == nullptr && ctx->extraCerts == nullptr);
    Require(ctx->defaultPasswdCallback == nullptr && ctx->defaultPasswdCallbackUserdata == nullptr);
    Require(ctx->certStore != nullptr && static_cast<LibreSslX509Store*>(ctx->certStore)->references == 1);
    auto* param = static_cast<LibreSslX509VerifyParam*>(ctx->param);
    Require(param != nullptr && param->depth == -1 && param->flags == 0 && param->id != nullptr && param->policies == nullptr);
    Require(SSL_CTX_get_cert_store_nid_postfix(ctx) == ctx->certStore);
    Require(SSL_CTX_get_verify_mode_nid_postfix(ctx) == 0);

    const LibreSslCtxInternal& internal = *ctx->internal;
    Require(internal.minVersion == Tls1Version && internal.maxVersion == Tls1_2Version);
    Require(internal.options == OptionLegacyServerConnect && internal.mode == 0);
    Require(internal.sessionCacheMode == SessionCacheServer && internal.sessionCacheSize == SessionCacheSizeDefault);
    Require(internal.maxCertList == MaxCertListDefault && internal.readAhead == 0 && internal.quietShutdown == 0);
    Require(internal.maxSendFragment == MaxPlainLength);
    Require(internal.cert != nullptr && internal.sessions == nullptr && internal.cipherListById == nullptr);
    Require(internal.newSessionCb == nullptr && internal.appVerifyCallback == nullptr && internal.defaultVerifyCallback == nullptr);
    Require(internal.msgCallback == nullptr && internal.infoCallback == nullptr && internal.generateSessionId == nullptr);
    Require(internal.sessConnect == 0 && internal.sessAccept == 0 && internal.sessHit == 0);
    Require(internal.alpnClientProtoList == nullptr && internal.tlsextEcpointformatlist == nullptr && internal.tlsextSupportedgroups == nullptr);
    const auto* clientCa = static_cast<const LibreSslStack*>(internal.clientCa);
    Require(clientCa != nullptr && clientCa->num == 0 && clientCa->numAlloc == 4 && clientCa->data != nullptr && clientCa->comp == nullptr);
    const unsigned char* certificate = static_cast<const unsigned char*>(internal.cert);
    Require(Read<void*>(certificate, 0) == certificate + 80);
    Require(Read<int>(certificate, 248) == 1);

    unsigned char allZero[16] = {};
    Require(std::memcmp(internal.tlsextTickKeyName, allZero, 16) != 0);
    Require(std::memcmp(internal.tlsextTickHmacKey, allZero, 16) != 0);
    Require(std::memcmp(internal.tlsextTickAesKey, allZero, 16) != 0);
    LibreSslCtx* other = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    Require(other != nullptr && other != ctx && other->certStore != ctx->certStore && other->param != ctx->param && other->internal->cert != internal.cert);
    Require(std::memcmp(other->internal->tlsextTickKeyName, internal.tlsextTickKeyName, 16) != 0);
    Require(std::memcmp(other->internal->tlsextTickHmacKey, internal.tlsextTickHmacKey, 16) != 0);
    Require(std::memcmp(other->internal->tlsextTickAesKey, internal.tlsextTickAesKey, 16) != 0);

    LibreSslCtx* datagram = SSL_CTX_new_nid_postfix(DTLSv1_server_method_nid_postfix());
    Require(datagram != nullptr && datagram->method == DTLSv1_server_method_nid_postfix());
    Require(datagram->internal->minVersion == Dtls1Version && datagram->internal->maxVersion == Dtls1Version);

    const int released = contextReleased;
    SSL_CTX_free_nid_postfix(other);
    Require(contextReleased == released + 1 && lastContextReleased == other);
    SSL_CTX_free_nid_postfix(datagram);
    Require(contextReleased == released + 2 && lastContextReleased == datagram);
    SSL_CTX_free_nid_postfix(ctx);
    Require(contextReleased == released + 3 && lastContextReleased == ctx);
    SSL_CTX_free_nid_postfix(nullptr);
    Require(contextReleased == released + 3);
}

void TestContextReferences() {
    const int released = contextReleased;
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    Require(ctx != nullptr);
    LibreSsl* first = SSL_new_nid_postfix(ctx);
    LibreSsl* second = SSL_new_nid_postfix(ctx);
    Require(first != nullptr && second != nullptr && first != second);
    Require(ctx->references == 5);
    SSL_CTX_free_nid_postfix(ctx);
    Require(ctx->references == 4 && contextReleased == released);
    SSL_free_nid_postfix(first);
    Require(ctx->references == 2 && contextReleased == released);
    SSL_free_nid_postfix(second);
    Require(contextReleased == released + 1 && lastContextReleased == ctx);
}

void TestSslNew() {
    ERR_clear_error_nid_postfix();
    Require(SSL_new_nid_postfix(nullptr) == nullptr);
    RequireQueuedError(0xfff, ReasonNullSslCtx);

    LibreSslCtx bare{};
    Require(SSL_new_nid_postfix(&bare) == nullptr);
    RequireQueuedError(0xfff, ReasonSslCtxHasNoDefaultSslVersion);

    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    const int created = sslCreated;
    LibreSsl* ssl = SSL_new_nid_postfix(ctx);
    Require(ssl != nullptr && sslCreated == created + 1 && lastSslCreated == ssl);
    Require(ctx->references == 3);
    Require(ssl->method == ctx->method && ssl->ctx == ctx && ssl->initialCtx == ctx);
    Require(SSL_get_SSL_CTX_nid_postfix(ssl) == ctx);
    Require(ssl->references == 1 && ssl->server == 0);
    Require(ssl->version == Tls1_2Version && ssl->clientVersion == Tls1_2Version && SSL_version_nid_postfix(ssl) == Tls1_2Version);
    Require(ssl->verifyResult == 0 && SSL_get_verify_result_nid_postfix(ssl) == 0);
    Require(ssl->verifyMode == 0 && ssl->sidCtxLength == 0 && ssl->error == 0 && ssl->errorCode == 0);
    Require(ssl->rbio == nullptr && ssl->wbio == nullptr && ssl->bbio == nullptr && ssl->d1 == nullptr);
    Require(SSL_get_rbio_nid_postfix(ssl) == nullptr && SSL_get_wbio_nid_postfix(ssl) == nullptr);
    Require(ssl->session == nullptr && ssl->cipherList == nullptr && ssl->tlsextHostname == nullptr);
    Require(ssl->tlsextStatusType == -1);
    Require(ssl->maxSendFragment == MaxPlainLength);
    Require(ssl->encReadCtx == nullptr && ssl->readHash == nullptr);
    Require(ssl->param != nullptr && ssl->param != ctx->param);
    auto* param = static_cast<LibreSslX509VerifyParam*>(ssl->param);
    Require(param->depth == -1 && param->flags == 0 && param->id != nullptr && param->id != static_cast<LibreSslX509VerifyParam*>(ctx->param)->id);
    Require(ssl->cert != nullptr && ssl->cert != ctx->internal->cert);
    Require(Read<void*>(ssl->cert, 0) == static_cast<unsigned char*>(ssl->cert) + 80 && Read<int>(ssl->cert, 248) == 1);
    Require(ssl->s3 != nullptr && Read<void*>(ssl->s3, Ssl3InternalPointer) != nullptr);
    Require(Read<int>(ssl->s3, Ssl3AlertDispatch) == 0 && Read<int>(Read<void*>(ssl->s3, Ssl3InternalPointer), Ssl3InternalWarnAlert) == 0);

    const LibreSslSslInternal& internal = *ssl->internal;
    Require(InternalField<std::uint16_t>(ssl, InternalMinVersion) == Tls1Version && InternalField<std::uint16_t>(ssl, InternalMaxVersion) == Tls1_2Version);
    Require(internal.options == OptionLegacyServerConnect && internal.mode == 0);
    Require(internal.maxCertList == MaxCertListDefault && internal.readAhead == 0);
    Require(internal.handshakeFunc == nullptr && internal.msgCallback == nullptr && internal.verifyCallback == nullptr);
    Require(internal.rwstate == RwNothing && SSL_want_nid_postfix(ssl) == RwNothing);
    Require(internal.shutdown == 0 && SSL_get_shutdown_nid_postfix(ssl) == 0);
    Require(internal.state == (StateBefore | StateConnect));
    Require(InternalField<int>(ssl, InternalReadState) == StateReadHeader);
    Require(InternalField<int>(ssl, InternalType) == 0 && InternalField<int>(ssl, InternalHit) == 0 && InternalField<int>(ssl, InternalRenegotiate) == 0);
    Require(InternalField<int>(ssl, InternalInHandshake) == 0 && InternalField<unsigned int>(ssl, InternalPacketLength) == 0);
    Require(InternalField<int>(ssl, InternalOcspResponseLength) == -1);
    Require(InternalField<int>(ssl, InternalQuietShutdown) == 0);
    Require(InternalField<void*>(ssl, InternalAlpnList) == nullptr && InternalField<void*>(ssl, InternalEcPointFormats) == nullptr);

    LibreSsl* second = SSL_new_nid_postfix(ctx);
    Require(second != nullptr && second != ssl && second->s3 != ssl->s3 && second->param != ssl->param && second->cert != ssl->cert);

    const int released = sslReleased;
    SSL_free_nid_postfix(second);
    Require(sslReleased == released + 1 && lastSslReleased == second);
    SSL_free_nid_postfix(ssl);
    Require(sslReleased == released + 2 && lastSslReleased == ssl);
    SSL_free_nid_postfix(nullptr);
    Require(sslReleased == released + 2);
    Require(ctx->references == 1);
    SSL_CTX_free_nid_postfix(ctx);

    LibreSslCtx* serverContext = SSL_CTX_new_nid_postfix(SSLv23_server_method_nid_postfix());
    LibreSsl* serverSsl = SSL_new_nid_postfix(serverContext);
    Require(serverSsl->server == 1 && serverSsl->internal->state == (StateBefore | StateAccept));
    SSL_free_nid_postfix(serverSsl);
    SSL_CTX_free_nid_postfix(serverContext);
}

void TestExDataTeardownUnderHostAllocationFailure() {
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    LibreSsl* ssl = SSL_new_nid_postfix(ctx);
    Require(ctx && ssl);
    const int contextBefore = contextReleased;
    const int sslBefore = sslReleased;
    failHostAllocations = true;
    SSL_free_nid_postfix(ssl);
    SSL_CTX_free_nid_postfix(ctx);
    failHostAllocations = false;
    Require(sslReleased == sslBefore + 1 && contextReleased == contextBefore + 1);
    Require(lastSslReleased == ssl && lastContextReleased == ctx);

    auto* stack = static_cast<LibreSslStack*>(calloc_nid_postfix(1, sizeof(LibreSslStack)));
    stack->data = static_cast<char**>(calloc_nid_postfix(4, sizeof(char*)));
    stack->numAlloc = 4;
    LibreSslExData data{stack};
    int owner = 0;
    const int released = sslReleased;
    failHostAllocations = true;
    LibreSslFreeExData_nid_no_patch(ExClassSsl, &owner, &data);
    failHostAllocations = false;
    Require(sslReleased == released + 1 && lastSslReleased == &owner);
    Require(data.stack == nullptr);
}

void TestInheritedDefaults() {
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    constexpr std::int64_t options = 0x00400000 | 0x4;
    constexpr std::int64_t mode = 0x1 | 0x2;
    char callbackArgument = 0;
    unsigned char sidContext[5] = {1, 2, 3, 4, 5};
    auto callback = reinterpret_cast<void*>(0x1000);
    auto verifyCallback = reinterpret_cast<void*>(0x2000);
    auto sessionId = reinterpret_cast<void*>(0x3000);
    SSL_CTX_ctrl_nid_postfix(ctx, SslCtrlOptions, options, nullptr);
    SSL_CTX_ctrl_nid_postfix(ctx, SslCtrlMode, mode, nullptr);
    SSL_CTX_ctrl_nid_postfix(ctx, SslCtrlSetMaxCertList, 7000, nullptr);
    SSL_CTX_ctrl_nid_postfix(ctx, SslCtrlSetReadAhead, 1, nullptr);
    SSL_CTX_ctrl_nid_postfix(ctx, SslCtrlSetMaxSendFragment, 4096, nullptr);
    SSL_CTX_ctrl_nid_postfix(ctx, SslCtrlSetMessageCallbackArgument, 0, &callbackArgument);
    SSL_CTX_set_msg_callback_nid_postfix(ctx, callback);
    SSL_CTX_set_verify_nid_postfix(ctx, 3, verifyCallback);
    SSL_CTX_set_quiet_shutdown_nid_postfix(ctx, 1);
    Require(SSL_CTX_set_session_id_context_nid_postfix(ctx, sidContext, sizeof(sidContext)) == 1);
    ctx->internal->generateSessionId = sessionId;
    ctx->sessionTimeout = 99;

    LibreSsl* ssl = SSL_new_nid_postfix(ctx);
    Require(ssl != nullptr);
    Require(static_cast<std::int64_t>(SSL_ctrl_nid_postfix(ssl, SslCtrlOptions, 0, nullptr)) == options);
    Require(SSL_ctrl_nid_postfix(ssl, SslCtrlMode, 0, nullptr) == mode);
    Require(SSL_ctrl_nid_postfix(ssl, SslCtrlGetMaxCertList, 0, nullptr) == 7000);
    Require(SSL_ctrl_nid_postfix(ssl, SslCtrlGetReadAhead, 0, nullptr) == 1);
    Require(ssl->maxSendFragment == 4096);
    Require(ssl->internal->msgCallback == callback && ssl->internal->msgCallbackArg == &callbackArgument);
    Require(ssl->verifyMode == 3 && ssl->internal->verifyCallback == verifyCallback);
    Require(InternalField<int>(ssl, InternalQuietShutdown) == 1);
    Require(ssl->sidCtxLength == sizeof(sidContext) && std::memcmp(ssl->sidCtx, sidContext, sizeof(sidContext)) == 0);
    Require(InternalField<void*>(ssl, InternalGenerateSessionId) == sessionId);

    SSL_CTX_ctrl_nid_postfix(ctx, SslCtrlOptions, 0x1000, nullptr);
    SSL_CTX_ctrl_nid_postfix(ctx, SslCtrlSetMaxCertList, 1, nullptr);
    SSL_CTX_set_verify_nid_postfix(ctx, 0, nullptr);
    Require(static_cast<std::int64_t>(SSL_ctrl_nid_postfix(ssl, SslCtrlOptions, 0, nullptr)) == options);
    Require(SSL_ctrl_nid_postfix(ssl, SslCtrlGetMaxCertList, 0, nullptr) == 7000);
    Require(ssl->verifyMode == 3);
    SSL_free_nid_postfix(ssl);

    ssl = SSL_new_nid_postfix(ctx);
    Require(static_cast<std::int64_t>(SSL_ctrl_nid_postfix(ssl, SslCtrlOptions, 0, nullptr)) == (options | 0x1000));
    Require(SSL_ctrl_nid_postfix(ssl, SslCtrlGetMaxCertList, 0, nullptr) == 1 && ssl->verifyMode == 0 && ssl->internal->verifyCallback == nullptr);
    SSL_free_nid_postfix(ssl);
    SSL_CTX_free_nid_postfix(ctx);
}

void TestInheritedLists() {
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    auto* alpn = static_cast<unsigned char*>(malloc_nid_postfix(9));
    const unsigned char protocols[9] = {2, 'h', '2', 5, 'h', 't', 't', 'p', '1'};
    std::memcpy(alpn, protocols, sizeof(protocols));
    auto* formats = static_cast<std::uint8_t*>(malloc_nid_postfix(3));
    formats[0] = 0;
    formats[1] = 1;
    formats[2] = 2;
    auto* groups = static_cast<std::uint16_t*>(malloc_nid_postfix(4 * sizeof(std::uint16_t)));
    for (std::uint16_t index = 0; index < 4; ++index) groups[index] = static_cast<std::uint16_t>(23 + index);
    ctx->internal->alpnClientProtoList = alpn;
    ctx->internal->alpnClientProtoListLen = sizeof(protocols);
    ctx->internal->tlsextEcpointformatlist = formats;
    ctx->internal->tlsextEcpointformatlistLength = 3;
    ctx->internal->tlsextSupportedgroups = groups;
    ctx->internal->tlsextSupportedgroupsLength = 4;

    LibreSsl* ssl = SSL_new_nid_postfix(ctx);
    Require(ssl != nullptr);
    const auto* sslAlpn = InternalField<unsigned char*>(ssl, InternalAlpnList);
    Require(sslAlpn != nullptr && sslAlpn != alpn && InternalField<unsigned int>(ssl, InternalAlpnListLength) == sizeof(protocols));
    Require(std::memcmp(sslAlpn, protocols, sizeof(protocols)) == 0);
    const auto* sslFormats = InternalField<std::uint8_t*>(ssl, InternalEcPointFormats);
    Require(sslFormats != nullptr && sslFormats != formats && InternalField<std::uint64_t>(ssl, InternalEcPointFormatLength) == 3);
    Require(sslFormats[0] == 0 && sslFormats[1] == 1 && sslFormats[2] == 2);
    const auto* sslGroups = InternalField<std::uint16_t*>(ssl, InternalSupportedGroups);
    Require(sslGroups != nullptr && sslGroups != groups && InternalField<std::uint64_t>(ssl, InternalSupportedGroupsLength) == 4);
    for (std::uint16_t index = 0; index < 4; ++index) Require(sslGroups[index] == 23 + index);

    SSL_free_nid_postfix(ssl);
    SSL_CTX_free_nid_postfix(ctx);
}

void RequireSocketBio(const LibreSsl* ssl, int descriptor) {
    const auto* bio = static_cast<const LibreSslBio*>(SSL_get_rbio_nid_postfix(ssl));
    Require(bio != nullptr && bio == SSL_get_wbio_nid_postfix(ssl));
    Require(bio->method != nullptr && bio->method->type == BioTypeSocket && std::strcmp(bio->method->name, "socket") == 0);
    Require(bio->init == 1 && bio->shutdown == 0 && bio->num == descriptor && bio->references == 1);
    Require(bio->flags == 0 && bio->retryReason == 0 && bio->callback == nullptr && bio->nextBio == nullptr && bio->previousBio == nullptr);
    Require(bio->numRead == 0 && bio->numWrite == 0 && bio->exData.stack == nullptr && bio->pointer == nullptr);
    Require(SSL_get_fd_nid_postfix(ssl) == descriptor);
}

void TestDescriptors() {
    LibreSslCtx* ctx = SSL_CTX_new_nid_postfix(TLS_client_method_nid_postfix());
    LibreSsl* ssl = SSL_new_nid_postfix(ctx);
    Require(SSL_get_fd_nid_postfix(ssl) == -1);
    RequireThrows([] { SSL_get_fd_nid_postfix(nullptr); });
    RequireThrows([] { SSL_set_fd_nid_postfix(nullptr, 3); });

    Require(SSL_set_fd_nid_postfix(ssl, 7) == 1);
    RequireSocketBio(ssl, 7);
    Require(SSL_set_fd_nid_postfix(ssl, 12) == 1);
    RequireSocketBio(ssl, 12);
    Require(SSL_set_fd_nid_postfix(ssl, 0) == 1);
    RequireSocketBio(ssl, 0);
    Require(SSL_set_fd_nid_postfix(ssl, 0x7fffffff) == 1);
    RequireSocketBio(ssl, 0x7fffffff);

    Require(SSL_set_fd_nid_postfix(ssl, -1) == 1);
    const auto* bio = static_cast<const LibreSslBio*>(SSL_get_rbio_nid_postfix(ssl));
    Require(bio != nullptr && bio->init == 1 && bio->num == -1);
    Require(SSL_get_fd_nid_postfix(ssl) == -1);
    Require(SSL_set_fd_nid_postfix(ssl, -5) == 1);
    Require(SSL_get_fd_nid_postfix(ssl) == -5);

    LibreSsl* other = SSL_new_nid_postfix(ctx);
    Require(SSL_get_fd_nid_postfix(other) == -1);
    Require(SSL_set_fd_nid_postfix(other, 21) == 1);
    Require(SSL_get_fd_nid_postfix(other) == 21 && SSL_get_fd_nid_postfix(ssl) == -5);

    SSL_free_nid_postfix(other);
    SSL_free_nid_postfix(ssl);
    SSL_CTX_free_nid_postfix(ctx);
}

}

int main() {
    InitializeHeap();
    RegisterCallbacks();
    Run(TestMethodGetters);
    Run(TestMethodSlots);
    Run(TestContextNew);
    Run(TestContextReferences);
    Run(TestSslNew);
    Run(TestExDataTeardownUnderHostAllocationFailure);
    Run(TestInheritedDefaults);
    Run(TestInheritedLists);
    Run(TestDescriptors);
    Require(live.empty());
    std::puts("libSceLibreSsl SSL lifecycle tests passed");
    return 0;
}
