#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

namespace {

constexpr std::uint64_t CipherFlagNoPadding = 0x100;

void Wipe(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
}

std::size_t CipherDataBytes(const LibreSslEvpCipher& cipher, const char* function) {
    if (cipher.contextSize < 0) throw std::invalid_argument(std::string(function) + ": cipher context size " + std::to_string(cipher.contextSize) + " is negative");
    return static_cast<std::size_t>(cipher.contextSize);
}

}

extern "C" {
void APS5_VABI EVP_CIPHER_CTX_init_nid_postfix(LibreSslEvpCipherCtx* ctx) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    std::memset(ctx, 0, sizeof(LibreSslEvpCipherCtx));
}

LibreSslEvpCipherCtx* APS5_VABI EVP_CIPHER_CTX_new_nid_postfix(void) {
    return static_cast<LibreSslEvpCipherCtx*>(calloc_nid_postfix(1, sizeof(LibreSslEvpCipherCtx)));
}

int APS5_VABI EVP_CIPHER_CTX_set_padding_nid_postfix(LibreSslEvpCipherCtx* ctx, int pad) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    if (pad) ctx->flags &= ~CipherFlagNoPadding;
    else ctx->flags |= CipherFlagNoPadding;
    return 1;
}

int APS5_VABI EVP_CIPHER_CTX_cleanup_nid_postfix(LibreSslEvpCipherCtx* ctx) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    if (ctx->engine) NotImplemented_nid_no_patch("EVP_CIPHER_CTX_cleanup: releasing an ENGINE");
    if (ctx->cipher) {
        if (ctx->cipher->cleanup && !ctx->cipher->cleanup(ctx)) return 0;
        if (ctx->cipherData) Wipe(ctx->cipherData, CipherDataBytes(*ctx->cipher, __func__));
    }
    free_nid_postfix(ctx->cipherData);
    Wipe(ctx, sizeof(LibreSslEvpCipherCtx));
    return 1;
}

void APS5_VABI EVP_CIPHER_CTX_free_nid_postfix(LibreSslEvpCipherCtx* ctx) {
    if (!ctx) return;
    EVP_CIPHER_CTX_cleanup_nid_postfix(ctx);
    free_nid_postfix(ctx);
}

int APS5_VABI EVP_CIPHER_CTX_ctrl_nid_postfix(LibreSslEvpCipherCtx* ctx, int type, int arg, void* pointer) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    if (!ctx->cipher || !ctx->cipher->control) return 0;
    const int result = ctx->cipher->control(ctx, type, arg, pointer);
    return result == -1 ? 0 : result;
}

}
