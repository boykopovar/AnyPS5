#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"

#include <limits>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(offsetof(LibreSslSslInternal, exData) == 272);
static_assert(sizeof(LibreSslSslInternal) == 432);
static_assert(offsetof(LibreSslSslInternal, rwstate) == 156);
static_assert(offsetof(LibreSslSslInternal, shutdown) == 168);
static_assert(sizeof(LibreSslSsl) == 232);
static_assert(offsetof(LibreSslSsl, rbio) == 16);
static_assert(offsetof(LibreSslSsl, wbio) == 24);
static_assert(offsetof(LibreSslSsl, session) == 128);
static_assert(offsetof(LibreSslSsl, ctx) == 152);
static_assert(offsetof(LibreSslSsl, verifyResult) == 160);
static_assert(offsetof(LibreSslSsl, internal) == 224);
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
static_assert(offsetof(LibreSslSsl, method) == 8);
static_assert(offsetof(LibreSslSsl, server) == 40);
static_assert(offsetof(LibreSslSsl, s3) == 48);
static_assert(offsetof(LibreSslSsl, d1) == 56);
static_assert(offsetof(LibreSslSsl, verifyMode) == 136);
static_assert(offsetof(LibreSslSsl, maxSendFragment) == 176);
static_assert(offsetof(LibreSslSsl, encReadCtx) == 208);
static_assert(offsetof(LibreSslSsl, readHash) == 216);

namespace {

const LibreSslSsl& Connection(const LibreSslSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

const LibreSslSslInternal& Internal(const LibreSslSsl* ssl, const char* function) {
    const LibreSslSsl& connection = Connection(ssl, function);
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
    unsigned char beforeAccept[28];
    void* accept;
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
static_assert(offsetof(SslMethodInternal, accept) == 32);
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
constexpr int Dtls1Version = 0xFEFF;
constexpr int StateConnectBefore = 0x5000;
constexpr int StateAcceptBefore = 0x6000;
constexpr std::int64_t DtlsMinimumMtu = 228;
constexpr std::int64_t MinimumSendFragment = 512;
constexpr std::int64_t MaximumPlainLength = 16384;

LibreSslSsl& Connection(LibreSslSsl* ssl, const char* function) {
    if (!ssl) throw std::invalid_argument(std::string(function) + ": null connection");
    return *ssl;
}

LibreSslSslInternal& Internal(LibreSslSsl* ssl, const char* function) {
    LibreSslSsl& connection = Connection(ssl, function);
    if (!connection.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *connection.internal;
}

const SslMethodInternal& Method(const LibreSslSsl& connection, const char* function) {
    const auto* method = static_cast<const SslMethod*>(connection.method);
    if (!method || !method->internal) throw std::invalid_argument(std::string(function) + ": null method");
    return *method->internal;
}

const Ssl3StateInternal* Ssl3Internal(const LibreSslSsl& connection, const char* function) {
    const auto* state = static_cast<const Ssl3State*>(connection.s3);
    if (!state) return nullptr;
    if (!state->internal) throw std::invalid_argument(std::string(function) + ": null SSL3 state");
    return state->internal;
}

constexpr int MinimumNodes = 4;
constexpr int CryptoLibrary = 15;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;

LibreSslStack* NewExDataStack() {
    auto* const stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
    if (!stack) return nullptr;
    stack->data = static_cast<char**>(malloc_nid_postfix(MinimumNodes * sizeof(char*)));
    if (!stack->data) {
        free_nid_postfix(stack);
        return nullptr;
    }
    for (int index = 0; index < MinimumNodes; ++index) stack->data[index] = nullptr;
    stack->num = 0;
    stack->sorted = 0;
    stack->numAlloc = MinimumNodes;
    stack->comp = nullptr;
    return stack;
}

void ValidateExDataStack(const LibreSslStack* stack, const char* function) {
    if (!stack->data || stack->num < 0 || stack->numAlloc < MinimumNodes || stack->num > stack->numAlloc) {
        NotImplemented_nid_no_patch(function);
    }
}

bool PushExData(LibreSslStack* stack, void* value, const char* function) {
    ValidateExDataStack(stack, function);
    if (stack->numAlloc - stack->num <= 1) {
        if (stack->numAlloc > std::numeric_limits<int>::max() / 2) NotImplemented_nid_no_patch(function);
        auto* const grown = static_cast<char**>(realloc_nid_postfix(stack->data,
            static_cast<std::size_t>(stack->numAlloc) * 2 * sizeof(char*)));
        if (!grown) return false;
        stack->data = grown;
        stack->numAlloc *= 2;
    }
    stack->data[stack->num] = static_cast<char*>(value);
    ++stack->num;
    stack->sorted = 0;
    return true;
}

}

extern "C" {

void* APS5_VABI SSL_get_SSL_CTX_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).ctx;
}

void* APS5_VABI SSL_get_rbio_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).rbio;
}

void* APS5_VABI SSL_get_wbio_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).wbio;
}

int APS5_VABI SSL_version_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).version;
}

int APS5_VABI SSL_check_private_key_nid_postfix(const LibreSslSsl* ssl) {
    if (!ssl) {
        LibreSslPutError_nid_no_patch(20, 0xfff, 67);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

std::int64_t APS5_VABI SSL_get_verify_result_nid_postfix(const LibreSslSsl* ssl) {
    return Connection(ssl, __func__).verifyResult;
}

int APS5_VABI SSL_get_shutdown_nid_postfix(const LibreSslSsl* ssl) {
    return Internal(ssl, __func__).shutdown;
}

int APS5_VABI SSL_want_nid_postfix(const LibreSslSsl* ssl) {
    return Internal(ssl, __func__).rwstate;
}

void APS5_VABI SSL_set_verify_nid_postfix(LibreSslSsl* ssl, int mode, void* callback) {
    LibreSslSsl& connection = Connection(ssl, __func__);
    if (callback) Internal(ssl, __func__).verifyCallback = callback;
    connection.verifyMode = mode;
}

void APS5_VABI SSL_get0_alpn_selected_nid_postfix(const LibreSslSsl* ssl, const unsigned char** data, unsigned int* length) {
    if (!data || !length) throw std::invalid_argument(std::string(__func__) + ": null output");
    const Ssl3StateInternal* state = Ssl3Internal(Connection(ssl, __func__), __func__);
    *data = state ? state->alpnSelected : nullptr;
    *length = state ? state->alpnSelectedLength : 0;
}

void APS5_VABI SSL_set_connect_state_nid_postfix(LibreSslSsl* ssl) {
    LibreSslSsl& connection = Connection(ssl, __func__);
    LibreSslSslInternal& internal = Internal(ssl, __func__);
    const SslMethodInternal& method = Method(connection, __func__);
    if (connection.encReadCtx || connection.readHash || internal.encWriteCtx || internal.aeadReadCtx || internal.aeadWriteCtx || internal.writeHash)
        NotImplemented_nid_no_patch("SSL_set_connect_state: freeing a live cipher or hash context");
    connection.server = 0;
    internal.shutdown = 0;
    internal.state = StateConnectBefore;
    internal.handshakeFunc = method.connect;
}

void APS5_VABI SSL_set_accept_state_nid_postfix(LibreSslSsl* ssl) {
    LibreSslSsl& connection = Connection(ssl, __func__);
    LibreSslSslInternal& internal = Internal(ssl, __func__);
    const SslMethodInternal& method = Method(connection, __func__);
    if (connection.encReadCtx || connection.readHash || internal.encWriteCtx || internal.aeadReadCtx || internal.aeadWriteCtx || internal.writeHash)
        NotImplemented_nid_no_patch("SSL_set_accept_state: freeing a live cipher or hash context");
    connection.server = 1;
    internal.shutdown = 0;
    internal.state = StateAcceptBefore;
    internal.handshakeFunc = method.accept;
}

std::int64_t APS5_VABI SSL_ctrl_nid_postfix(LibreSslSsl* ssl, int command, std::int64_t argument, void* pointer) {
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
        const LibreSslSsl& connection = Connection(ssl, __func__);
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
    default:
        NotImplemented_nid_no_patch(("SSL_ctrl: command " + std::to_string(command)).c_str());
    }
}

int APS5_VABI SSL_set_ex_data_nid_postfix(LibreSslSsl* ssl, int index, void* value) {
    LibreSslSslInternal& internal = Internal(ssl, __func__);
    auto* stack = static_cast<LibreSslStack*>(internal.exData.stack);
    if (!stack) {
        stack = NewExDataStack();
        if (!stack) {
            LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
            return 0;
        }
        internal.exData.stack = stack;
    } else {
        ValidateExDataStack(stack, __func__);
    }
    while (stack->num <= index) {
        if (!PushExData(stack, nullptr, __func__)) {
            LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
            return 0;
        }
    }
    if (index >= 0 && index < stack->num) stack->data[index] = static_cast<char*>(value);
    return 1;
}

}
