#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslCtxInternal) == 464);
static_assert(offsetof(LibreSslCtxInternal, newSessionCb) == 24);
static_assert(offsetof(LibreSslCtxInternal, appVerifyCallback) == 48);
static_assert(offsetof(LibreSslCtxInternal, appVerifyArg) == 56);
static_assert(offsetof(LibreSslCtxInternal, clientCertCb) == 64);
static_assert(offsetof(LibreSslCtxInternal, infoCallback) == 88);
static_assert(offsetof(LibreSslCtxInternal, nextProtoSelectCb) == 384);
static_assert(offsetof(LibreSslCtxInternal, nextProtoSelectCbArg) == 392);
static_assert(offsetof(LibreSslCtxInternal, alpnSelectCb) == 400);
static_assert(offsetof(LibreSslCtxInternal, alpnSelectCbArg) == 408);

namespace {

struct CallbackPair {
    void* callback;
    void* arg;
};

static_assert(sizeof(CallbackPair) == 16);
static_assert(offsetof(CallbackPair, callback) == 0);
static_assert(offsetof(CallbackPair, arg) == sizeof(void*));
static_assert(offsetof(LibreSslCtxInternal, appVerifyArg) == offsetof(LibreSslCtxInternal, appVerifyCallback) + sizeof(void*));
static_assert(offsetof(LibreSslCtxInternal, nextProtoSelectCbArg) == offsetof(LibreSslCtxInternal, nextProtoSelectCb) + sizeof(void*));
static_assert(offsetof(LibreSslCtxInternal, alpnSelectCbArg) == offsetof(LibreSslCtxInternal, alpnSelectCb) + sizeof(void*));

LibreSslCtxInternal* ReadInternal(const LibreSslCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    LibreSslCtxInternal* internal = nullptr;
    const auto* source = reinterpret_cast<const std::byte*>(ctx) + offsetof(LibreSslCtx, internal);
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&internal, source, sizeof(internal));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": unreadable context");
    if (!internal) throw std::invalid_argument(std::string(function) + ": null internal state");
    return internal;
}

template<typename TValue>
void WriteInternal(LibreSslCtx* ctx, std::size_t offset, const TValue& value, const char* function) {
    LibreSslCtxInternal* internal = ReadInternal(ctx, function);
    auto* destination = reinterpret_cast<std::byte*>(internal) + offset;
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(destination, sizeof(value));
    const bool copied = writable && GuestCopyOut_nid_no_patch(destination, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": unwritable internal state");
}

}

extern "C" {

void APS5_VABI SSL_CTX_set_info_callback_nid_postfix(LibreSslCtx* ctx, void* callback) {
    WriteInternal(ctx, offsetof(LibreSslCtxInternal, infoCallback), callback, __func__);
}

void APS5_VABI SSL_CTX_sess_set_new_cb_nid_postfix(LibreSslCtx* ctx, void* callback) {
    WriteInternal(ctx, offsetof(LibreSslCtxInternal, newSessionCb), callback, __func__);
}

void APS5_VABI SSL_CTX_set_cert_verify_callback_nid_postfix(LibreSslCtx* ctx, void* callback, void* arg) {
    WriteInternal(ctx, offsetof(LibreSslCtxInternal, appVerifyCallback), CallbackPair{callback, arg}, __func__);
}

void APS5_VABI SSL_CTX_set_client_cert_cb_nid_postfix(LibreSslCtx* ctx, void* callback) {
    WriteInternal(ctx, offsetof(LibreSslCtxInternal, clientCertCb), callback, __func__);
}

void APS5_VABI SSL_CTX_set_next_proto_select_cb_nid_postfix(LibreSslCtx* ctx, void* callback, void* arg) {
    WriteInternal(ctx, offsetof(LibreSslCtxInternal, nextProtoSelectCb), CallbackPair{callback, arg}, __func__);
}

void APS5_VABI SSL_CTX_set_alpn_select_cb_nid_postfix(LibreSslCtx* ctx, void* callback, void* arg) {
    WriteInternal(ctx, offsetof(LibreSslCtxInternal, alpnSelectCb), CallbackPair{callback, arg}, __func__);
}

}
