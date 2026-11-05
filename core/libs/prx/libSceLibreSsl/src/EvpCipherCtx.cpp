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

void Wipe(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
}

std::size_t CipherDataBytes(const LibreSslEvpCipher& cipher, const char* function) {
    if (cipher.contextSize < 0) throw std::invalid_argument(std::string(function) + ": cipher context size " + std::to_string(cipher.contextSize) + " is negative");
    return static_cast<std::size_t>(cipher.contextSize);
}

const LibreSslEvpCipher& RequireCipher(const LibreSslEvpCipherCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    if (!ctx->cipher) throw std::invalid_argument(std::string(function) + ": no cipher set");
    return *ctx->cipher;
}

void SetLength(int* length, int value, const char* function) {
    if (!length) throw std::invalid_argument(std::string(function) + ": null output length");
    *length = value;
}

void Copy(void* destination, const void* source, std::size_t size, const char* function) {
    if (!size) return;
    if (!destination || !source) throw std::invalid_argument(std::string(function) + ": null buffer");
    std::memcpy(destination, source, size);
}

int DoCipher(LibreSslEvpCipherCtx* ctx, unsigned char* out, const unsigned char* in, std::size_t length, const char* function) {
    if (!ctx->cipher->doCipher) throw std::invalid_argument(std::string(function) + ": cipher without do_cipher");
    return ctx->cipher->doCipher(ctx, out, in, length);
}

int EncryptUpdate(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl, const char* function) {
    const auto& cipher = RequireCipher(ctx, function);
    if (cipher.flags & CipherFlagCustomCipher) {
        const int result = DoCipher(ctx, out, in, static_cast<std::size_t>(inl), function);
        if (result < 0) return 0;
        SetLength(outl, result, function);
        return 1;
    }
    if (inl <= 0) {
        SetLength(outl, 0, function);
        return inl == 0;
    }
    if (ctx->bufLen == 0 && (inl & ctx->blockMask) == 0) {
        const bool done = DoCipher(ctx, out, in, static_cast<std::size_t>(inl), function);
        SetLength(outl, done ? inl : 0, function);
        return done;
    }
    int buffered = ctx->bufLen;
    const int block = cipher.blockSize;
    if (static_cast<std::size_t>(block) > sizeof(ctx->buf)) {
        SetLength(outl, 0, function);
        return 0;
    }
    if (!block) throw std::invalid_argument(std::string(function) + ": cipher block size changed to 0");
    if (buffered < 0 || buffered > block)
        throw std::invalid_argument(std::string(function) + ": buffered byte count " + std::to_string(buffered) + " is outside 0 to the block size");
    if (buffered) {
        if (block - buffered > inl) {
            Copy(ctx->buf + buffered, in, static_cast<std::size_t>(inl), function);
            ctx->bufLen += inl;
            SetLength(outl, 0, function);
            return 1;
        }
        const int filled = block - buffered;
        Copy(ctx->buf + buffered, in, static_cast<std::size_t>(filled), function);
        if (!DoCipher(ctx, out, ctx->buf, static_cast<std::size_t>(block), function)) return 0;
        inl -= filled;
        in += filled;
        out += block;
        SetLength(outl, block, function);
    } else {
        SetLength(outl, 0, function);
    }
    buffered = inl & (block - 1);
    inl -= buffered;
    if (inl > 0) {
        if (!DoCipher(ctx, out, in, static_cast<std::size_t>(inl), function)) return 0;
        *outl += inl;
    }
    if (buffered) Copy(ctx->buf, in + inl, static_cast<std::size_t>(buffered), function);
    ctx->bufLen = buffered;
    return 1;
}

int EncryptFinal(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl, const char* function) {
    const auto& cipher = RequireCipher(ctx, function);
    if (cipher.flags & CipherFlagCustomCipher) {
        const int result = DoCipher(ctx, out, nullptr, 0, function);
        if (result < 0) return 0;
        SetLength(outl, result, function);
        return 1;
    }
    const auto block = static_cast<unsigned>(cipher.blockSize);
    if (block > sizeof(ctx->buf)) return 0;
    if (block == 1) {
        SetLength(outl, 0, function);
        return 1;
    }
    const auto buffered = static_cast<unsigned>(ctx->bufLen);
    if (ctx->flags & CipherFlagNoPadding) {
        if (buffered) return 0;
        SetLength(outl, 0, function);
        return 1;
    }
    const int pad = static_cast<int>(block - buffered);
    for (unsigned index = buffered; index < block; ++index) ctx->buf[index] = static_cast<unsigned char>(pad);
    const int result = DoCipher(ctx, out, ctx->buf, block, function);
    if (result) SetLength(outl, static_cast<int>(block), function);
    return result;
}

int DecryptUpdate(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl, const char* function) {
    const auto& cipher = RequireCipher(ctx, function);
    if (cipher.flags & CipherFlagCustomCipher) {
        const int result = DoCipher(ctx, out, in, static_cast<std::size_t>(inl), function);
        SetLength(outl, result < 0 ? 0 : result, function);
        return result >= 0;
    }
    if (inl <= 0) {
        SetLength(outl, 0, function);
        return inl == 0;
    }
    if (ctx->flags & CipherFlagNoPadding) return EncryptUpdate(ctx, out, outl, in, inl, function);
    const auto block = static_cast<unsigned>(cipher.blockSize);
    if (block > sizeof(ctx->final)) return 0;
    const bool held = ctx->finalUsed;
    if (held) {
        Copy(out, ctx->final, block, function);
        out += block;
    }
    if (!EncryptUpdate(ctx, out, outl, in, inl, function)) return 0;
    if (block > 1 && !ctx->bufLen) {
        if (*outl < static_cast<int>(block)) throw std::invalid_argument(std::string(function) + ": block mask does not match the block size");
        *outl -= static_cast<int>(block);
        ctx->finalUsed = 1;
        std::memcpy(ctx->final, out + *outl, block);
    } else {
        ctx->finalUsed = 0;
    }
    if (held) *outl += static_cast<int>(block);
    return 1;
}

int DecryptFinal(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl, const char* function) {
    SetLength(outl, 0, function);
    const auto& cipher = RequireCipher(ctx, function);
    if (cipher.flags & CipherFlagCustomCipher) {
        const int result = DoCipher(ctx, out, nullptr, 0, function);
        if (result < 0) return 0;
        *outl = result;
        return 1;
    }
    auto block = static_cast<unsigned>(cipher.blockSize);
    if (ctx->flags & CipherFlagNoPadding) return !ctx->bufLen;
    if (block <= 1) return 1;
    if (ctx->bufLen || !ctx->finalUsed) return 0;
    if (block > sizeof(ctx->final)) return 0;
    const unsigned pad = ctx->final[block - 1];
    if (pad == 0 || pad > block) return 0;
    for (unsigned index = 0; index < pad; ++index)
        if (ctx->final[--block] != pad) return 0;
    const unsigned length = static_cast<unsigned>(cipher.blockSize) - pad;
    Copy(out, ctx->final, length, function);
    *outl = static_cast<int>(length);
    return 1;
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

int APS5_VABI EVP_CipherInit_ex_nid_postfix(LibreSslEvpCipherCtx* ctx, const LibreSslEvpCipher* cipher, void* engine, const unsigned char* key, const unsigned char* iv, int encrypt) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    if (encrypt == -1) encrypt = ctx->encrypt;
    else ctx->encrypt = encrypt = encrypt ? 1 : 0;
    const bool engineCipher = ctx->engine && ctx->cipher && (!cipher || cipher->nid == ctx->cipher->nid);
    if (!engineCipher && cipher) {
        if (engine) NotImplemented_nid_no_patch("EVP_CipherInit_ex: ENGINE implementation");
        if (ctx->cipher) {
            const auto flags = ctx->flags;
            EVP_CIPHER_CTX_cleanup_nid_postfix(ctx);
            ctx->encrypt = encrypt;
            ctx->flags = flags;
        }
        ctx->engine = nullptr;
        ctx->cipher = cipher;
        ctx->cipherData = nullptr;
        if (cipher->contextSize) {
            ctx->cipherData = malloc_nid_postfix(static_cast<std::size_t>(cipher->contextSize));
            if (!ctx->cipherData) return 0;
        }
        ctx->keyLen = cipher->keyLength;
        ctx->flags = 0;
        if ((cipher->flags & CipherFlagCtrlInit) && !EVP_CIPHER_CTX_ctrl_nid_postfix(ctx, CipherControlInit, 0, nullptr)) return 0;
    } else if (!engineCipher && !ctx->cipher) {
        return 0;
    }
    const auto& active = *ctx->cipher;
    if (active.blockSize != 1 && active.blockSize != 8 && active.blockSize != 16) return 0;
    if (!(active.flags & CipherFlagCustomIv)) {
        const auto ivLength = static_cast<std::size_t>(active.ivLength);
        switch (active.flags & CipherModeMask) {
        case CipherModeStream:
        case CipherModeEcb:
            break;
        case CipherModeCfb:
        case CipherModeOfb:
            ctx->num = 0;
            [[fallthrough]];
        case CipherModeCbc:
            if (ivLength > sizeof(ctx->iv)) return 0;
            if (iv) std::memcpy(ctx->oiv, iv, ivLength);
            std::memcpy(ctx->iv, ctx->oiv, ivLength);
            break;
        case CipherModeCtr:
            ctx->num = 0;
            if (iv) {
                if (ivLength > sizeof(ctx->iv)) throw std::invalid_argument(std::string(__func__) + ": CTR IV length " + std::to_string(active.ivLength) + " is outside 0-16");
                std::memcpy(ctx->iv, iv, ivLength);
            }
            break;
        default:
            return 0;
        }
    }
    if (key || (active.flags & CipherFlagAlwaysCallInit)) {
        if (!active.init) throw std::invalid_argument(std::string(__func__) + ": cipher without init");
        if (!active.init(ctx, key, iv, encrypt)) return 0;
    }
    ctx->bufLen = 0;
    ctx->finalUsed = 0;
    ctx->blockMask = active.blockSize - 1;
    return 1;
}

int APS5_VABI EVP_EncryptInit_ex_nid_postfix(LibreSslEvpCipherCtx* ctx, const LibreSslEvpCipher* cipher, void* engine, const unsigned char* key, const unsigned char* iv) {
    return EVP_CipherInit_ex_nid_postfix(ctx, cipher, engine, key, iv, 1);
}

int APS5_VABI EVP_DecryptInit_ex_nid_postfix(LibreSslEvpCipherCtx* ctx, const LibreSslEvpCipher* cipher, void* engine, const unsigned char* key, const unsigned char* iv) {
    return EVP_CipherInit_ex_nid_postfix(ctx, cipher, engine, key, iv, 0);
}

int APS5_VABI EVP_EncryptUpdate_nid_postfix(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl) {
    return EncryptUpdate(ctx, out, outl, in, inl, __func__);
}

int APS5_VABI EVP_DecryptUpdate_nid_postfix(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl) {
    return DecryptUpdate(ctx, out, outl, in, inl, __func__);
}

int APS5_VABI EVP_CipherUpdate_nid_postfix(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    return ctx->encrypt ? EncryptUpdate(ctx, out, outl, in, inl, __func__) : DecryptUpdate(ctx, out, outl, in, inl, __func__);
}

int APS5_VABI EVP_EncryptFinal_ex_nid_postfix(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl) {
    return EncryptFinal(ctx, out, outl, __func__);
}

int APS5_VABI EVP_DecryptFinal_ex_nid_postfix(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl) {
    return DecryptFinal(ctx, out, outl, __func__);
}

int APS5_VABI EVP_CipherFinal_ex_nid_postfix(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    return ctx->encrypt ? EncryptFinal(ctx, out, outl, __func__) : DecryptFinal(ctx, out, outl, __func__);
}

void APS5_VABI EVP_cleanup_nid_postfix(void) {
}

}
