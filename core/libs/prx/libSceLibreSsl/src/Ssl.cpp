#include "prx/libSceLibreSsl/include/SslInternal.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslSslInternal) == 432);
static_assert(offsetof(LibreSslSslInternal, rwstate) == 156);
static_assert(offsetof(LibreSslSslInternal, shutdown) == 168);
static_assert(sizeof(LibreSsl) == 232);
static_assert(offsetof(LibreSsl, rbio) == 16);
static_assert(offsetof(LibreSsl, wbio) == 24);
static_assert(offsetof(LibreSsl, session) == 128);
static_assert(offsetof(LibreSsl, ctx) == 152);
static_assert(offsetof(LibreSsl, verifyResult) == 160);
static_assert(offsetof(LibreSsl, internal) == 224);
static_assert(offsetof(LibreSslSslInternal, options) == 8);
static_assert(offsetof(LibreSslSslInternal, mode) == 16);
static_assert(offsetof(LibreSslSslInternal, handshakeFunc) == 56);
static_assert(offsetof(LibreSslSslInternal, msgCallback) == 64);
static_assert(offsetof(LibreSslSslInternal, msgCallbackArg) == 72);
static_assert(offsetof(LibreSslSslInternal, verifyCallback) == 88);
static_assert(offsetof(LibreSslSslInternal, readAhead) == 212);
static_assert(offsetof(LibreSslSslInternal, aeadReadCtx) == 240);
static_assert(offsetof(LibreSslSslInternal, aeadWriteCtx) == 248);
static_assert(offsetof(LibreSslSslInternal, encWriteCtx) == 256);
static_assert(offsetof(LibreSslSslInternal, writeHash) == 264);
static_assert(offsetof(LibreSslSslInternal, maxCertList) == 296);
static_assert(offsetof(LibreSslSslInternal, state) == 412);
static_assert(offsetof(LibreSsl, method) == 8);
static_assert(offsetof(LibreSsl, server) == 40);
static_assert(offsetof(LibreSsl, s3) == 48);
static_assert(offsetof(LibreSsl, d1) == 56);
static_assert(offsetof(LibreSsl, verifyMode) == 136);
static_assert(offsetof(LibreSsl, maxSendFragment) == 176);
static_assert(offsetof(LibreSsl, encReadCtx) == 208);
static_assert(offsetof(LibreSsl, readHash) == 216);

namespace {

const LibreSsl& Connection(const LibreSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

const LibreSslSslInternal& Internal(const LibreSsl* ssl, const char* function) {
    const LibreSsl& connection = Connection(ssl, function);
    if (!connection.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *connection.internal;
}

struct Ssl3StateInternal {
    unsigned char beforeSendConnectionBinding[1012];
    int sendConnectionBinding;
    unsigned char beforeAlpnSelected[8];
    const unsigned char* alpnSelected;
    unsigned int alpnSelectedLength;
    unsigned char rest[4];
};

struct Ssl3State {
    unsigned char beforeInternal[136];
    const Ssl3StateInternal* internal;
};

struct Dtls1StateInternal {
    unsigned char beforeMtu[636];
    std::uint32_t mtu;
};

struct Dtls1State {
    unsigned char beforeInternal[32];
    Dtls1StateInternal* internal;
};

struct SslMethodInternal {
    int version;
    unsigned char beforeConnect[36];
    void* connect;
    unsigned char rest[112];
};

struct SslMethod {
    unsigned char beforeInternal[40];
    const SslMethodInternal* internal;
};

static_assert(sizeof(Ssl3StateInternal) == 1040);
static_assert(offsetof(Ssl3StateInternal, sendConnectionBinding) == 1012);
static_assert(offsetof(Ssl3StateInternal, alpnSelected) == 1024);
static_assert(offsetof(Ssl3StateInternal, alpnSelectedLength) == 1032);
static_assert(sizeof(Ssl3State) == 144);
static_assert(offsetof(Dtls1StateInternal, mtu) == 636);
static_assert(sizeof(Dtls1State) == 40);
static_assert(sizeof(SslMethodInternal) == 160);
static_assert(offsetof(SslMethodInternal, connect) == 40);
static_assert(sizeof(SslMethod) == 48);

constexpr int ControlSetMessageCallbackArgument = 16;
constexpr int ControlSetMtu = 17;
constexpr int ControlOptions = 32;
constexpr int ControlMode = 33;
constexpr int ControlGetReadAhead = 40;
constexpr int ControlSetReadAhead = 41;
constexpr int ControlGetMaxCertList = 50;
constexpr int ControlSetMaxCertList = 51;
constexpr int ControlSetMaxSendFragment = 52;
constexpr int ControlGetRenegotiationInfoSupport = 76;
constexpr int ControlClearOptions = 77;
constexpr int ControlClearMode = 78;
constexpr int ControlSetTlsExtHostName = 55;
constexpr int Dtls1Version = 0xFEFF;
constexpr int StateConnectBefore = 0x5000;
constexpr std::int64_t DtlsMinimumMtu = 228;
constexpr std::int64_t MinimumSendFragment = 512;
constexpr std::int64_t MaximumPlainLength = 16384;

LibreSsl& Connection(LibreSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

LibreSslSslInternal& Internal(LibreSsl* ssl, const char* function) {
    LibreSsl& connection = Connection(ssl, function);
    if (!connection.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *connection.internal;
}

const SslMethodInternal& Method(const LibreSsl& connection, const char* function) {
    const auto* method = static_cast<const SslMethod*>(connection.method);
    if (!method || !method->internal) throw std::invalid_argument(std::string(function) + ": null method");
    return *method->internal;
}

const Ssl3StateInternal* Ssl3Internal(const LibreSsl& connection, const char* function) {
    const auto* state = static_cast<const Ssl3State*>(connection.s3);
    if (!state) return nullptr;
    if (!state->internal) throw std::invalid_argument(std::string(function) + ": null SSL3 state");
    return state->internal;
}

}

extern "C" {

void* APS5_VABI SSL_get_SSL_CTX_nid_postfix(const LibreSsl* ssl) {
    return Connection(ssl, __func__).ctx;
}

void* APS5_VABI SSL_get_rbio_nid_postfix(const LibreSsl* ssl) {
    return Connection(ssl, __func__).rbio;
}

void* APS5_VABI SSL_get_wbio_nid_postfix(const LibreSsl* ssl) {
    return Connection(ssl, __func__).wbio;
}

int APS5_VABI SSL_version_nid_postfix(const LibreSsl* ssl) {
    return Connection(ssl, __func__).version;
}

std::int64_t APS5_VABI SSL_get_verify_result_nid_postfix(const LibreSsl* ssl) {
    return Connection(ssl, __func__).verifyResult;
}

int APS5_VABI SSL_get_shutdown_nid_postfix(const LibreSsl* ssl) {
    return Internal(ssl, __func__).shutdown;
}

int APS5_VABI SSL_want_nid_postfix(const LibreSsl* ssl) {
    return Internal(ssl, __func__).rwstate;
}

void APS5_VABI SSL_set_verify_nid_postfix(LibreSsl* ssl, int mode, void* callback) {
    LibreSsl& connection = Connection(ssl, __func__);
    if (callback) Internal(ssl, __func__).verifyCallback = callback;
    connection.verifyMode = mode;
}

void APS5_VABI SSL_get0_alpn_selected_nid_postfix(const LibreSsl* ssl, const unsigned char** data, unsigned int* length) {
    if (!data || !length) throw std::invalid_argument(std::string(__func__) + ": null output");
    const Ssl3StateInternal* state = Ssl3Internal(Connection(ssl, __func__), __func__);
    *data = state ? state->alpnSelected : nullptr;
    *length = state ? state->alpnSelectedLength : 0;
}

void APS5_VABI SSL_set_connect_state_nid_postfix(LibreSsl* ssl) {
    LibreSsl& connection = Connection(ssl, __func__);
    LibreSslSslInternal& internal = Internal(ssl, __func__);
    const SslMethodInternal& method = Method(connection, __func__);
    if (connection.encReadCtx || connection.readHash || internal.encWriteCtx || internal.aeadReadCtx || internal.aeadWriteCtx || internal.writeHash)
        NotImplemented_nid_no_patch("SSL_set_connect_state: freeing a live cipher or hash context");
    connection.server = 0;
    internal.shutdown = 0;
    internal.state = StateConnectBefore;
    internal.handshakeFunc = method.connect;
}

std::int64_t APS5_VABI SSL_ctrl_nid_postfix(LibreSsl* ssl, int command, std::int64_t argument, void* pointer) {
    switch (command) {
    case ControlGetReadAhead:
        return Internal(ssl, __func__).readAhead;
    case ControlSetReadAhead: {
        LibreSslSslInternal& internal = Internal(ssl, __func__);
        const int previous = internal.readAhead;
        internal.readAhead = static_cast<int>(argument);
        return previous;
    }
    case ControlSetMessageCallbackArgument:
        Internal(ssl, __func__).msgCallbackArg = pointer;
        return 1;
    case ControlOptions:
        return static_cast<std::int64_t>(Internal(ssl, __func__).options |= static_cast<std::uint64_t>(argument));
    case ControlClearOptions:
        return static_cast<std::int64_t>(Internal(ssl, __func__).options &= ~static_cast<std::uint64_t>(argument));
    case ControlMode:
        return static_cast<std::int64_t>(Internal(ssl, __func__).mode |= static_cast<std::uint64_t>(argument));
    case ControlClearMode:
        return static_cast<std::int64_t>(Internal(ssl, __func__).mode &= ~static_cast<std::uint64_t>(argument));
    case ControlGetMaxCertList:
        return Internal(ssl, __func__).maxCertList;
    case ControlSetMaxCertList: {
        LibreSslSslInternal& internal = Internal(ssl, __func__);
        const std::int64_t previous = internal.maxCertList;
        internal.maxCertList = argument;
        return previous;
    }
    case ControlSetMtu: {
        if (argument < DtlsMinimumMtu) return 0;
        const LibreSsl& connection = Connection(ssl, __func__);
        if (Method(connection, __func__).version != Dtls1Version) return 0;
        const auto* state = static_cast<const Dtls1State*>(connection.d1);
        if (!state || !state->internal) throw std::invalid_argument(std::string(__func__) + ": null DTLS state");
        state->internal->mtu = static_cast<std::uint32_t>(argument);
        return argument;
    }
    case ControlSetMaxSendFragment:
        if (argument < MinimumSendFragment || argument > MaximumPlainLength) return 0;
        Connection(ssl, __func__).maxSendFragment = static_cast<unsigned int>(argument);
        return 1;
    case ControlGetRenegotiationInfoSupport: {
        const Ssl3StateInternal* state = Ssl3Internal(Connection(ssl, __func__), __func__);
        return state ? state->sendConnectionBinding : 0;
    }
    case ControlSetTlsExtHostName:
        return LibreSslSetTlsExtHostName_nid_no_patch(Connection(ssl, __func__), argument, pointer);
    default:
        NotImplemented_nid_no_patch(("SSL_ctrl: command " + std::to_string(command)).c_str());
    }
}

}
