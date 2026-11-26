#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslCtxInternal) == 464);
static_assert(offsetof(LibreSslCtxInternal, exData) == 248);
static_assert(offsetof(LibreSslCtxInternal, quietShutdown) == 292);
static_assert(sizeof(LibreSslCtx) == 120);
static_assert(offsetof(LibreSslCtx, certStore) == 16);
static_assert(offsetof(LibreSslCtx, verifyMode) == 48);
static_assert(offsetof(LibreSslCtx, defaultPasswdCallback) == 96);
static_assert(offsetof(LibreSslCtx, defaultPasswdCallbackUserdata) == 104);
static_assert(offsetof(LibreSslCtx, internal) == 112);
static_assert(offsetof(LibreSslCtx, extraCerts) == 40);
static_assert(offsetof(LibreSslCtx, sidCtxLength) == 52);
static_assert(offsetof(LibreSslCtx, sidCtx) == 56);
static_assert(offsetof(LibreSslCtxInternal, options) == 8);
static_assert(offsetof(LibreSslCtxInternal, mode) == 16);
static_assert(offsetof(LibreSslCtxInternal, msgCallback) == 96);
static_assert(offsetof(LibreSslCtxInternal, msgCallbackArg) == 104);
static_assert(offsetof(LibreSslCtxInternal, defaultVerifyCallback) == 112);
static_assert(offsetof(LibreSslCtxInternal, tlsextServernameCallback) == 128);
static_assert(offsetof(LibreSslCtxInternal, tlsextServernameArg) == 136);
static_assert(offsetof(LibreSslCtxInternal, tlsextTicketKeyCb) == 144);
static_assert(offsetof(LibreSslCtxInternal, tlsextStatusCb) == 152);
static_assert(offsetof(LibreSslCtxInternal, tlsextStatusArg) == 160);
static_assert(offsetof(LibreSslCtxInternal, sessionCacheSize) == 176);
static_assert(offsetof(LibreSslCtxInternal, cert) == 264);
static_assert(offsetof(LibreSslCtxInternal, sessionCacheMode) == 200);
static_assert(offsetof(LibreSslCtxInternal, sessConnect) == 204);
static_assert(offsetof(LibreSslCtxInternal, sessConnectRenegotiate) == 208);
static_assert(offsetof(LibreSslCtxInternal, sessConnectGood) == 212);
static_assert(offsetof(LibreSslCtxInternal, sessAccept) == 216);
static_assert(offsetof(LibreSslCtxInternal, sessAcceptRenegotiate) == 220);
static_assert(offsetof(LibreSslCtxInternal, sessAcceptGood) == 224);
static_assert(offsetof(LibreSslCtxInternal, sessMiss) == 228);
static_assert(offsetof(LibreSslCtxInternal, sessTimeout) == 232);
static_assert(offsetof(LibreSslCtxInternal, sessCacheFull) == 236);
static_assert(offsetof(LibreSslCtxInternal, sessHit) == 240);
static_assert(offsetof(LibreSslCtxInternal, sessCbHit) == 244);
static_assert(offsetof(LibreSslCtxInternal, exData) == 248);
static_assert(sizeof(LibreSslExData) == sizeof(void*));
static_assert(offsetof(LibreSslExData, stack) == 0);
static_assert(offsetof(LibreSslCtxInternal, maxCertList) == 280);
static_assert(offsetof(LibreSslCtxInternal, readAhead) == 288);
static_assert(offsetof(LibreSslCtxInternal, maxSendFragment) == 296);
static_assert(offsetof(LibreSslCtxInternal, tlsextTickKeyName) == 312);
static_assert(offsetof(LibreSslCtxInternal, tlsextTickHmacKey) == 328);
static_assert(offsetof(LibreSslCtxInternal, tlsextTickAesKey) == 344);

namespace {

enum Command : int {
    NeedTmpRsa = 1,
    SetTmpRsa = 2,
    SetTmpDh = 3,
    SetTmpEcdh = 4,
    SetTmpRsaCb = 5,
    SetTmpDhCb = 6,
    SetTmpEcdhCb = 7,
    ExtraChainCert = 14,
    SetMsgCallback = 15,
    SetMsgCallbackArg = 16,
    SessNumber = 20,
    SessConnect = 21,
    SessConnectGood = 22,
    SessConnectRenegotiate = 23,
    SessAccept = 24,
    SessAcceptGood = 25,
    SessAcceptRenegotiate = 26,
    SessHit = 27,
    SessCbHit = 28,
    SessMisses = 29,
    SessTimeouts = 30,
    SessCacheFull = 31,
    Options = 32,
    Mode = 33,
    GetReadAhead = 40,
    SetReadAhead = 41,
    SetSessCacheSize = 42,
    GetSessCacheSize = 43,
    SetSessCacheMode = 44,
    GetSessCacheMode = 45,
    GetMaxCertList = 50,
    SetMaxCertList = 51,
    SetMaxSendFragment = 52,
    SetTlsextServernameCb = 53,
    SetTlsextServernameArg = 54,
    GetTlsextTicketKeys = 58,
    SetTlsextTicketKeys = 59,
    SetTlsextStatusReqCb = 63,
    SetTlsextStatusReqCbArg = 64,
    SetTlsextTicketKeyCb = 72,
    ClearOptions = 77,
    ClearMode = 78,
    GetExtraChainCerts = 82,
    ClearExtraChainCerts = 83,
    SetGroups = 91,
    SetGroupsList = 92,
    SetEcdhAuto = 94,
    SetDhAuto = 118,
};

constexpr int ErrLibSsl = 20;
constexpr int ErrLibEvp = 6;
constexpr int ErrLibAsn1 = 13;
constexpr int ErrFunctionNone = 0xfff;
constexpr int ErrMallocFailure = 65;
constexpr int ErrEvpLibrary = 6;
constexpr int ErrUnsupportedAlgorithm = 156;
constexpr int ErrUnknownPublicKeyType = 163;
constexpr int ShouldNotHaveBeenCalled = 66;
constexpr int SessionIdContextTooLong = 273;
constexpr int InvalidTicketKeysLength = 325;
constexpr int NoCertificateAssigned = 177;
constexpr std::int64_t TicketKeysLength = 48;
constexpr std::int64_t MinSendFragment = 512;
constexpr std::int64_t MaxPlainLength = 16384;

LibreSslCtx& Context(LibreSslCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    return *ctx;
}

LibreSslCtxInternal& Internal(LibreSslCtx* ctx, const char* function) {
    LibreSslCtx& context = Context(ctx, function);
    if (!context.internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return *context.internal;
}

void CopyGuestBytesInPreservingErrno(void* hostAddress, const void* guestAddress, std::size_t size, const char* caller) {
    const bool ok = GuestCopyIn_nid_no_patch(hostAddress, guestAddress, size);
    if (!ok) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void CopyGuestBytesOutPreservingErrno(void* guestAddress, const void* hostAddress, std::size_t size, const char* caller) {
    const bool ok = GuestCopyOut_nid_no_patch(guestAddress, hostAddress, size);
    if (!ok) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void RequireGuestWritablePreservingErrno(void* guestAddress, std::size_t size, const char* caller) {
    const bool ok = GuestWritable_nid_no_patch(guestAddress, size);
    if (!ok) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

template <typename TObject, typename TField>
std::size_t FieldOffset(TField TObject::* field) {
    static const TObject probe {};
    return static_cast<std::size_t>(reinterpret_cast<const unsigned char*>(&(probe.*field)) - reinterpret_cast<const unsigned char*>(&probe));
}

template <typename TObject, typename TField>
const unsigned char* FieldBytes(const TObject* object, TField TObject::* field) {
    return reinterpret_cast<const unsigned char*>(object) + FieldOffset(field);
}

template <typename TObject, typename TField>
unsigned char* FieldBytes(TObject* object, TField TObject::* field) {
    return reinterpret_cast<unsigned char*>(object) + FieldOffset(field);
}

template <typename TObject, typename TField>
TField ReadField(const TObject* object, TField TObject::* field, const char* caller) {
    TField value {};
    CopyGuestBytesInPreservingErrno(&value, FieldBytes(object, field), sizeof(TField), caller);
    return value;
}

template <typename TObject, typename TField>
void WriteField(TObject* object, TField TObject::* field, const std::type_identity_t<TField>& value, const char* caller) {
    CopyGuestBytesOutPreservingErrno(FieldBytes(object, field), &value, sizeof(TField), caller);
}

template <typename TObject, typename TField>
TField ExchangeField(TObject* object, TField TObject::* field, const std::type_identity_t<TField>& value, const char* caller) {
    const TField previous = ReadField(object, field, caller);
    WriteField(object, field, value, caller);
    return previous;
}

LibreSslCtxInternal* GuestInternal(const LibreSslCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    auto* internal = ReadField(ctx, &LibreSslCtx::internal, function);
    if (!internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return internal;
}

[[noreturn]] void UnsupportedCommand(const char* function, int cmd) {
    NotImplemented_nid_no_patch((std::string(function) + " command " + std::to_string(cmd)).c_str());
}

int SetCtxExData_nid_no_patch(LibreSslCtxInternal& internal, int index, void* value) {
    constexpr int InitialCapacity = 4;
    constexpr int CryptoLibrary = 15;
    constexpr int AnyFunction = 0xfff;
    constexpr int MallocFailure = 65;
    auto* stack = static_cast<LibreSslStack*>(internal.exData);
    if (!stack) {
        stack = static_cast<LibreSslStack*>(malloc_nid_postfix(sizeof(LibreSslStack)));
        if (!stack) {
            LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
            return 0;
        }
        stack->num = 0;
        stack->sorted = 0;
        stack->numAlloc = InitialCapacity;
        stack->comp = nullptr;
        stack->data = static_cast<char**>(malloc_nid_postfix(InitialCapacity * sizeof(char*)));
        if (!stack->data) {
            free_nid_postfix(stack);
            LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
            return 0;
        }
        std::memset(stack->data, 0, InitialCapacity * sizeof(char*));
        internal.exData = stack;
    } else if (stack->num < 0 || stack->numAlloc <= 0 || stack->num >= stack->numAlloc || !stack->data) {
        NotImplemented_nid_no_patch("SSL_CTX_set_ex_data: malformed ex-data stack");
    }

    int count = stack->num;
    while (count <= index) {
        if (stack->numAlloc <= stack->num + 1) {
            if (stack->numAlloc > INT_MAX / 2) NotImplemented_nid_no_patch("SSL_CTX_set_ex_data: unrepresentable stack capacity");
            auto** data = static_cast<char**>(reallocarray_nid_postfix(stack->data, static_cast<std::size_t>(stack->numAlloc), 2 * sizeof(char*)));
            if (!data) {
                LibreSslPutError_nid_no_patch(CryptoLibrary, AnyFunction, MallocFailure);
                return 0;
            }
            stack->data = data;
            stack->numAlloc *= 2;
        }
        stack->data[stack->num++] = nullptr;
        stack->sorted = 0;
        ++count;
    }
    if (index >= 0) stack->data[index] = static_cast<char*>(value);
    return 1;
}

void PutSslError(int reason) {
    LibreSslPutError_nid_no_patch(ErrLibSsl, ErrFunctionNone, reason);
}

}

extern "C" {

int APS5_VABI SSL_CTX_use_certificate_nid_postfix(LibreSslCtx* ctx, void* certificate) {
    if (certificate == nullptr) {
        PutSslError(67);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_get_verify_mode_nid_postfix(LibreSslCtx* ctx) {
    return Context(ctx, __func__).verifyMode;
}

void* APS5_VABI SSL_CTX_get_cert_store_nid_postfix(LibreSslCtx* ctx) {
    return Context(ctx, __func__).certStore;
}

void APS5_VABI SSL_CTX_set_cert_store_nid_postfix(LibreSslCtx* ctx, LibreSslX509Store* store) {
    LibreSslCtx& context = Context(ctx, __func__);
    X509_STORE_free_nid_postfix(static_cast<LibreSslX509Store*>(context.certStore));
    context.certStore = store;
}

int APS5_VABI SSL_CTX_load_verify_locations_nid_postfix(LibreSslCtx* ctx, const char* file, const char* path) {
    Context(ctx, __func__);
    if (file || path) NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI SSL_CTX_set_quiet_shutdown_nid_postfix(LibreSslCtx* ctx, int mode) {
    LibreSslCtx& context = Context(ctx, __func__);
    if (!context.internal) throw std::invalid_argument(std::string(__func__) + ": null internal state");
    context.internal->quietShutdown = mode;
}

void APS5_VABI SSL_CTX_set_default_passwd_cb_nid_postfix(LibreSslCtx* ctx, void* callback) {
    Context(ctx, __func__).defaultPasswdCallback = callback;
}

void APS5_VABI SSL_CTX_set_default_passwd_cb_userdata_nid_postfix(LibreSslCtx* ctx, void* userdata) {
    Context(ctx, __func__).defaultPasswdCallbackUserdata = userdata;
}

int APS5_VABI SSL_CTX_set_ex_data_nid_postfix(LibreSslCtx* ctx, int index, void* value) {
    return SetCtxExData_nid_no_patch(Internal(ctx, __func__), index, value);
}

int APS5_VABI SSL_CTX_use_PrivateKey_ASN1_nid_postfix(int type, LibreSslCtx*, const unsigned char*, std::int64_t) {
    if (type != 0) NotImplemented_nid_no_patch(__func__);

    void* const pkey = EVP_PKEY_new_nid_postfix();
    if (!pkey) {
        LibreSslPutError_nid_no_patch(ErrLibEvp, ErrFunctionNone, ErrMallocFailure);
        LibreSslPutError_nid_no_patch(ErrLibAsn1, ErrFunctionNone, ErrEvpLibrary);
        PutSslError(13);
        return 0;
    }

    LibreSslPutError_nid_no_patch(ErrLibEvp, ErrFunctionNone, ErrUnsupportedAlgorithm);
    LibreSslPutError_nid_no_patch(ErrLibAsn1, ErrFunctionNone, ErrUnknownPublicKeyType);
    EVP_PKEY_free_nid_postfix(pkey);
    PutSslError(13);
    return 0;
}

std::int64_t APS5_VABI SSL_CTX_ctrl_nid_postfix(LibreSslCtx* ctx, int cmd, std::int64_t larg, void* parg) {
    auto* internal = GuestInternal(ctx, __func__);
    using CtxInternal = LibreSslCtxInternal;
    switch (cmd) {
    case GetReadAhead: return ReadField(internal, &CtxInternal::readAhead, __func__);
    case SetReadAhead: return ExchangeField(internal, &CtxInternal::readAhead, static_cast<int>(larg), __func__);
    case SetMsgCallbackArg: WriteField(internal, &CtxInternal::msgCallbackArg, parg, __func__); return 1;
    case GetMaxCertList: return ReadField(internal, &CtxInternal::maxCertList, __func__);
    case SetMaxCertList: return ExchangeField(internal, &CtxInternal::maxCertList, larg, __func__);
    case SetSessCacheSize: return static_cast<std::int64_t>(ExchangeField(internal, &CtxInternal::sessionCacheSize, static_cast<std::uint64_t>(larg), __func__));
    case GetSessCacheSize: return static_cast<std::int64_t>(ReadField(internal, &CtxInternal::sessionCacheSize, __func__));
    case SetSessCacheMode: return ExchangeField(internal, &CtxInternal::sessionCacheMode, static_cast<int>(larg), __func__);
    case GetSessCacheMode: return ReadField(internal, &CtxInternal::sessionCacheMode, __func__);
    case SessConnect: return ReadField(internal, &CtxInternal::sessConnect, __func__);
    case SessConnectGood: return ReadField(internal, &CtxInternal::sessConnectGood, __func__);
    case SessConnectRenegotiate: return ReadField(internal, &CtxInternal::sessConnectRenegotiate, __func__);
    case SessAccept: return ReadField(internal, &CtxInternal::sessAccept, __func__);
    case SessAcceptGood: return ReadField(internal, &CtxInternal::sessAcceptGood, __func__);
    case SessAcceptRenegotiate: return ReadField(internal, &CtxInternal::sessAcceptRenegotiate, __func__);
    case SessHit: return ReadField(internal, &CtxInternal::sessHit, __func__);
    case SessCbHit: return ReadField(internal, &CtxInternal::sessCbHit, __func__);
    case SessMisses: return ReadField(internal, &CtxInternal::sessMiss, __func__);
    case SessTimeouts: return ReadField(internal, &CtxInternal::sessTimeout, __func__);
    case SessCacheFull: return ReadField(internal, &CtxInternal::sessCacheFull, __func__);
    case Options: {
        const auto value = ReadField(internal, &CtxInternal::options, __func__) | static_cast<std::uint64_t>(larg);
        WriteField(internal, &CtxInternal::options, value, __func__);
        return static_cast<std::int64_t>(value);
    }
    case ClearOptions: {
        const auto value = ReadField(internal, &CtxInternal::options, __func__) & ~static_cast<std::uint64_t>(larg);
        WriteField(internal, &CtxInternal::options, value, __func__);
        return static_cast<std::int64_t>(value);
    }
    case Mode: {
        const auto value = ReadField(internal, &CtxInternal::mode, __func__) | static_cast<std::uint64_t>(larg);
        WriteField(internal, &CtxInternal::mode, value, __func__);
        return static_cast<std::int64_t>(value);
    }
    case ClearMode: {
        const auto value = ReadField(internal, &CtxInternal::mode, __func__) & ~static_cast<std::uint64_t>(larg);
        WriteField(internal, &CtxInternal::mode, value, __func__);
        return static_cast<std::int64_t>(value);
    }
    case SetMaxSendFragment:
        if (larg < MinSendFragment || larg > MaxPlainLength) return 0;
        WriteField(internal, &CtxInternal::maxSendFragment, static_cast<unsigned int>(larg), __func__);
        return 1;
    case NeedTmpRsa: return 0;
    case SetTmpRsa:
    case SetTmpRsaCb:
    case SetTmpDhCb:
    case SetTmpEcdhCb:
        PutSslError(ShouldNotHaveBeenCalled);
        return 0;
    case SetTlsextServernameArg: WriteField(internal, &CtxInternal::tlsextServernameArg, parg, __func__); return 1;
    case GetTlsextTicketKeys:
    case SetTlsextTicketKeys: {
        auto* keys = static_cast<unsigned char*>(parg);
        if (!keys) return TicketKeysLength;
        if (larg != TicketKeysLength) {
            PutSslError(InvalidTicketKeysLength);
            return 0;
        }
        unsigned char local[TicketKeysLength];
        if (cmd == SetTlsextTicketKeys) {
            CopyGuestBytesInPreservingErrno(local, keys, sizeof(local), __func__);
            RequireGuestWritablePreservingErrno(FieldBytes(internal, &CtxInternal::tlsextTickKeyName), sizeof(local), __func__);
            CopyGuestBytesOutPreservingErrno(FieldBytes(internal, &CtxInternal::tlsextTickKeyName), local, 16, __func__);
            CopyGuestBytesOutPreservingErrno(FieldBytes(internal, &CtxInternal::tlsextTickHmacKey), local + 16, 16, __func__);
            CopyGuestBytesOutPreservingErrno(FieldBytes(internal, &CtxInternal::tlsextTickAesKey), local + 32, 16, __func__);
        } else {
            CopyGuestBytesInPreservingErrno(local, FieldBytes(internal, &CtxInternal::tlsextTickKeyName), 16, __func__);
            CopyGuestBytesInPreservingErrno(local + 16, FieldBytes(internal, &CtxInternal::tlsextTickHmacKey), 16, __func__);
            CopyGuestBytesInPreservingErrno(local + 32, FieldBytes(internal, &CtxInternal::tlsextTickAesKey), 16, __func__);
            CopyGuestBytesOutPreservingErrno(keys, local, sizeof(local), __func__);
        }
        return 1;
    }
    case SetTlsextStatusReqCbArg: WriteField(internal, &CtxInternal::tlsextStatusArg, parg, __func__); return 1;
    case GetExtraChainCerts: {
        if (!parg) throw std::invalid_argument(std::string(__func__) + ": null extra chain certificates output");
        const auto extraCerts = ReadField(ctx, &LibreSslCtx::extraCerts, __func__);
        CopyGuestBytesOutPreservingErrno(parg, &extraCerts, sizeof(extraCerts), __func__);
        return 1;
    }
    case SetTmpDh:
    case SetTmpEcdh:
    case ExtraChainCert:
    case SessNumber:
    case ClearExtraChainCerts:
    case SetGroups:
    case SetGroupsList:
    case SetEcdhAuto:
    case SetDhAuto: UnsupportedCommand(__func__, cmd);
    default: return 0;
    }
}

std::int64_t APS5_VABI SSL_CTX_callback_ctrl_nid_postfix(LibreSslCtx* ctx, int cmd, void* callback) {
    LibreSslCtxInternal& internal = Internal(ctx, __func__);
    switch (cmd) {
    case SetMsgCallback: internal.msgCallback = callback; return 1;
    case SetTmpRsaCb:
        PutSslError(ShouldNotHaveBeenCalled);
        return 0;
    case SetTmpDhCb:
    case SetTmpEcdhCb: UnsupportedCommand(__func__, cmd);
    case SetTlsextServernameCb: internal.tlsextServernameCallback = callback; return 1;
    case SetTlsextStatusReqCb: internal.tlsextStatusCb = callback; return 1;
    case SetTlsextTicketKeyCb: internal.tlsextTicketKeyCb = callback; return 1;
    default: return 0;
    }
}

int APS5_VABI SSL_CTX_check_private_key_nid_postfix(const LibreSslCtx* ctx) {
    if (!ctx) {
        PutSslError(NoCertificateAssigned);
        return 0;
    }
    if (!ReadField(GuestInternal(ctx, __func__), &LibreSslCtxInternal::cert, __func__)) {
        PutSslError(NoCertificateAssigned);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI SSL_CTX_set_msg_callback_nid_postfix(LibreSslCtx* ctx, void* callback) {
    Internal(ctx, __func__).msgCallback = callback;
}

void APS5_VABI SSL_CTX_set_verify_nid_postfix(LibreSslCtx* ctx, int mode, void* callback) {
    auto* internal = GuestInternal(ctx, __func__);
    RequireGuestWritablePreservingErrno(FieldBytes(internal, &LibreSslCtxInternal::defaultVerifyCallback), sizeof(void*), __func__);
    WriteField(ctx, &LibreSslCtx::verifyMode, mode, __func__);
    WriteField(internal, &LibreSslCtxInternal::defaultVerifyCallback, callback, __func__);
}

int APS5_VABI SSL_CTX_add_client_CA_nid_postfix(LibreSslCtx* ctx, void* certificate) {
    Internal(ctx, __func__);
    if (!certificate) return 0;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_set_session_id_context_nid_postfix(LibreSslCtx* ctx, const unsigned char* sidCtx, unsigned int length) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    unsigned char local[sizeof(LibreSslCtx::sidCtx)];
    if (length > sizeof local) {
        PutSslError(SessionIdContextTooLong);
        return 0;
    }
    if (!sidCtx && length) throw std::invalid_argument(std::string(__func__) + ": null session id context");
    CopyGuestBytesInPreservingErrno(local, sidCtx, length, __func__);
    RequireGuestWritablePreservingErrno(FieldBytes(ctx, &LibreSslCtx::sidCtx), length, __func__);
    WriteField(ctx, &LibreSslCtx::sidCtxLength, length, __func__);
    CopyGuestBytesOutPreservingErrno(FieldBytes(ctx, &LibreSslCtx::sidCtx), local, length, __func__);
    return 1;
}

void* APS5_VABI SSL_CTX_get_ex_data_nid_postfix(const LibreSslCtx* ctx, int index) {
    const auto* const data = static_cast<const LibreSslStack*>(ReadField(GuestInternal(ctx, __func__), &LibreSslCtxInternal::exData, __func__));
    if (!data || index >= sk_num_nid_postfix(data)) return nullptr;
    return sk_value_nid_postfix(data, index);
}

}
