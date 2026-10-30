#include "prx/libSceLibreSsl/include/Api.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslCtxInternal) == 464);
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
constexpr int ErrFunctionNone = 0xfff;
constexpr int ShouldNotHaveBeenCalled = 66;
constexpr int SessionIdContextTooLong = 273;
constexpr int InvalidTicketKeysLength = 325;
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

[[noreturn]] void UnsupportedCommand(const char* function, int cmd) {
    NotImplemented_nid_no_patch((std::string(function) + " command " + std::to_string(cmd)).c_str());
}

void PutSslError(int reason) {
    LibreSslPutError_nid_no_patch(ErrLibSsl, ErrFunctionNone, reason);
}

}

extern "C" {

int APS5_VABI SSL_CTX_get_verify_mode_nid_postfix(LibreSslCtx* ctx) {
    return Context(ctx, __func__).verifyMode;
}

void* APS5_VABI SSL_CTX_get_cert_store_nid_postfix(LibreSslCtx* ctx) {
    return Context(ctx, __func__).certStore;
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

std::int64_t APS5_VABI SSL_CTX_ctrl_nid_postfix(LibreSslCtx* ctx, int cmd, std::int64_t larg, void* parg) {
    LibreSslCtxInternal& internal = Internal(ctx, __func__);
    switch (cmd) {
    case GetReadAhead: return internal.readAhead;
    case SetReadAhead: return std::exchange(internal.readAhead, static_cast<int>(larg));
    case SetMsgCallbackArg: internal.msgCallbackArg = parg; return 1;
    case GetMaxCertList: return internal.maxCertList;
    case SetMaxCertList: return std::exchange(internal.maxCertList, larg);
    case SetSessCacheSize: return static_cast<std::int64_t>(std::exchange(internal.sessionCacheSize, static_cast<std::uint64_t>(larg)));
    case GetSessCacheSize: return static_cast<std::int64_t>(internal.sessionCacheSize);
    case SetSessCacheMode: return std::exchange(internal.sessionCacheMode, static_cast<int>(larg));
    case GetSessCacheMode: return internal.sessionCacheMode;
    case SessConnect: return internal.sessConnect;
    case SessConnectGood: return internal.sessConnectGood;
    case SessConnectRenegotiate: return internal.sessConnectRenegotiate;
    case SessAccept: return internal.sessAccept;
    case SessAcceptGood: return internal.sessAcceptGood;
    case SessAcceptRenegotiate: return internal.sessAcceptRenegotiate;
    case SessHit: return internal.sessHit;
    case SessCbHit: return internal.sessCbHit;
    case SessMisses: return internal.sessMiss;
    case SessTimeouts: return internal.sessTimeout;
    case SessCacheFull: return internal.sessCacheFull;
    case Options: return static_cast<std::int64_t>(internal.options |= static_cast<std::uint64_t>(larg));
    case ClearOptions: return static_cast<std::int64_t>(internal.options &= ~static_cast<std::uint64_t>(larg));
    case Mode: return static_cast<std::int64_t>(internal.mode |= static_cast<std::uint64_t>(larg));
    case ClearMode: return static_cast<std::int64_t>(internal.mode &= ~static_cast<std::uint64_t>(larg));
    case SetMaxSendFragment:
        if (larg < MinSendFragment || larg > MaxPlainLength) return 0;
        internal.maxSendFragment = static_cast<unsigned int>(larg);
        return 1;
    case NeedTmpRsa: return 0;
    case SetTmpRsa:
    case SetTmpRsaCb:
    case SetTmpDhCb:
    case SetTmpEcdhCb:
        PutSslError(ShouldNotHaveBeenCalled);
        return 0;
    case SetTlsextServernameArg: internal.tlsextServernameArg = parg; return 1;
    case GetTlsextTicketKeys:
    case SetTlsextTicketKeys: {
        auto* keys = static_cast<unsigned char*>(parg);
        if (!keys) return TicketKeysLength;
        if (larg != TicketKeysLength) {
            PutSslError(InvalidTicketKeysLength);
            return 0;
        }
        if (cmd == SetTlsextTicketKeys) {
            std::memcpy(internal.tlsextTickKeyName, keys, 16);
            std::memcpy(internal.tlsextTickHmacKey, keys + 16, 16);
            std::memcpy(internal.tlsextTickAesKey, keys + 32, 16);
        } else {
            std::memcpy(keys, internal.tlsextTickKeyName, 16);
            std::memcpy(keys + 16, internal.tlsextTickHmacKey, 16);
            std::memcpy(keys + 32, internal.tlsextTickAesKey, 16);
        }
        return 1;
    }
    case SetTlsextStatusReqCbArg: internal.tlsextStatusArg = parg; return 1;
    case GetExtraChainCerts:
        if (!parg) throw std::invalid_argument(std::string(__func__) + ": null extra chain certificates output");
        *static_cast<void**>(parg) = ctx->extraCerts;
        return 1;
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

void APS5_VABI SSL_CTX_set_msg_callback_nid_postfix(LibreSslCtx* ctx, void* callback) {
    Internal(ctx, __func__).msgCallback = callback;
}

void APS5_VABI SSL_CTX_set_verify_nid_postfix(LibreSslCtx* ctx, int mode, void* callback) {
    LibreSslCtxInternal& internal = Internal(ctx, __func__);
    ctx->verifyMode = mode;
    internal.defaultVerifyCallback = callback;
}

int APS5_VABI SSL_CTX_set_session_id_context_nid_postfix(LibreSslCtx* ctx, const unsigned char* sidCtx, unsigned int length) {
    LibreSslCtx& context = Context(ctx, __func__);
    if (length > sizeof context.sidCtx) {
        PutSslError(SessionIdContextTooLong);
        return 0;
    }
    if (!sidCtx && length) throw std::invalid_argument(std::string(__func__) + ": null session id context");
    context.sidCtxLength = length;
    std::copy_n(sidCtx, length, context.sidCtx);
    return 1;
}

}
