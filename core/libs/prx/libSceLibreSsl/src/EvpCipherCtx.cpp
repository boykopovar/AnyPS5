#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"

namespace {

constexpr std::size_t CipherField = offsetof(LibreSslEvpCipherCtx, cipher);
constexpr std::size_t EngineField = offsetof(LibreSslEvpCipherCtx, engine);
constexpr std::size_t EncryptField = offsetof(LibreSslEvpCipherCtx, encrypt);
constexpr std::size_t BufLenField = offsetof(LibreSslEvpCipherCtx, bufLen);
constexpr std::size_t OivField = offsetof(LibreSslEvpCipherCtx, oiv);
constexpr std::size_t IvField = offsetof(LibreSslEvpCipherCtx, iv);
constexpr std::size_t NumField = offsetof(LibreSslEvpCipherCtx, num);
constexpr std::size_t KeyLenField = offsetof(LibreSslEvpCipherCtx, keyLen);
constexpr std::size_t FlagsField = offsetof(LibreSslEvpCipherCtx, flags);
constexpr std::size_t CipherDataField = offsetof(LibreSslEvpCipherCtx, cipherData);
constexpr std::size_t FinalUsedField = offsetof(LibreSslEvpCipherCtx, finalUsed);
constexpr std::size_t BlockMaskField = offsetof(LibreSslEvpCipherCtx, blockMask);
constexpr std::size_t IvBytes = sizeof(LibreSslEvpCipherCtx::iv);

void CopyGuestBytesInPreservingErrno(void* hostAddress, const void* guestAddress, std::size_t size, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(hostAddress, guestAddress, size);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void CopyGuestBytesOutPreservingErrno(void* guestAddress, const void* hostAddress, std::size_t size, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(guestAddress, hostAddress, size);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

void RequireGuestWritablePreservingErrno(void* guestAddress, std::size_t size, const char* caller) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(guestAddress, size);
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
}

template <typename T>
T LoadField(const LibreSslEvpCipherCtx* ctx, std::size_t offset, const char* caller) {
    T value {};
    CopyGuestBytesInPreservingErrno(&value, reinterpret_cast<const std::byte*>(ctx) + offset, sizeof(T), caller);
    return value;
}

template <typename T>
void StoreField(LibreSslEvpCipherCtx* ctx, std::size_t offset, const T& value, const char* caller) {
    CopyGuestBytesOutPreservingErrno(reinterpret_cast<std::byte*>(ctx) + offset, &value, sizeof(T), caller);
}

LibreSslEvpCipher LoadCipher(const LibreSslEvpCipher* cipher, const char* caller) {
    LibreSslEvpCipher value {};
    CopyGuestBytesInPreservingErrno(&value, cipher, sizeof(value), caller);
    return value;
}

void StoreIv(LibreSslEvpCipherCtx* ctx, std::size_t offset, const void* source, std::size_t size, const char* caller) {
    unsigned char vector[IvBytes] {};
    CopyGuestBytesInPreservingErrno(vector, source, size, caller);
    CopyGuestBytesOutPreservingErrno(reinterpret_cast<std::byte*>(ctx) + offset, vector, size, caller);
}

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

bool BuffersOverlap(const unsigned char* first, std::size_t firstSize, const unsigned char* second, std::size_t secondSize) {
    if (!first || !second || !firstSize || !secondSize) return false;
    const auto firstStart = reinterpret_cast<std::uintptr_t>(first);
    const auto secondStart = reinterpret_cast<std::uintptr_t>(second);
    if (firstStart < secondStart) return secondStart - firstStart < firstSize;
    return firstStart - secondStart < secondSize;
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
        LibreSslPutError_nid_no_patch(6, 0xfff, 136);
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
        const int remaining = inl - filled;
        const int nextBuffered = remaining & (block - 1);
        const int alignedRemaining = remaining - nextBuffered;
        std::vector<unsigned char> staged;
        const unsigned char* rest = in + filled;
        const std::size_t totalOut = static_cast<std::size_t>(block) + static_cast<std::size_t>(alignedRemaining);
        if (remaining > 0 && out && BuffersOverlap(rest, static_cast<std::size_t>(remaining), out, totalOut)) {
            staged.assign(rest, rest + remaining);
            rest = staged.data();
        }
        if (!DoCipher(ctx, out, ctx->buf, static_cast<std::size_t>(block), function)) return 0;
        out += block;
        SetLength(outl, block, function);
        if (alignedRemaining > 0) {
            if (!DoCipher(ctx, out, rest, static_cast<std::size_t>(alignedRemaining), function)) return 0;
            *outl += alignedRemaining;
        }
        if (nextBuffered) Copy(ctx->buf, rest + alignedRemaining, static_cast<std::size_t>(nextBuffered), function);
        ctx->bufLen = nextBuffered;
        return 1;
    }
    SetLength(outl, 0, function);
    buffered = inl & (block - 1);
    inl -= buffered;
    std::vector<unsigned char> stagedTail;
    const unsigned char* tailSource = in + inl;
    if (buffered && inl > 0 && out && BuffersOverlap(tailSource, static_cast<std::size_t>(buffered), out, static_cast<std::size_t>(inl))) {
        stagedTail.assign(tailSource, tailSource + buffered);
        tailSource = stagedTail.data();
    }
    if (inl > 0) {
        if (!DoCipher(ctx, out, in, static_cast<std::size_t>(inl), function)) return 0;
        *outl += inl;
    }
    if (buffered) Copy(ctx->buf, tailSource, static_cast<std::size_t>(buffered), function);
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
    if (block > sizeof(ctx->buf)) {
        LibreSslPutError_nid_no_patch(6, 0xfff, 136);
        return 0;
    }
    if (block == 1) {
        SetLength(outl, 0, function);
        return 1;
    }
    const auto buffered = static_cast<unsigned>(ctx->bufLen);
    if (ctx->flags & CipherFlagNoPadding) {
        if (buffered) {
            LibreSslPutError_nid_no_patch(6, 0xfff, 138);
            return 0;
        }
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
    if (block > sizeof(ctx->final)) {
        LibreSslPutError_nid_no_patch(6, 0xfff, 136);
        return 0;
    }
    const bool held = ctx->finalUsed;
    unsigned char savedFinal[sizeof(ctx->final)]{};
    if (held) {
        if (!out && block) Copy(out, ctx->final, block, function);
        std::memcpy(savedFinal, ctx->final, block);
    }
    std::vector<unsigned char> stagedIn;
    const unsigned char* input = in;
    if (held && in && out && BuffersOverlap(in, static_cast<std::size_t>(inl), out, static_cast<std::size_t>(block) + static_cast<std::size_t>(inl))) {
        stagedIn.assign(in, in + inl);
        input = stagedIn.data();
    }
    unsigned char* const updateOut = held ? out + block : out;
    if (!EncryptUpdate(ctx, updateOut, outl, input, inl, function)) return 0;
    if (block > 1 && !ctx->bufLen) {
        if (*outl < static_cast<int>(block)) throw std::invalid_argument(std::string(function) + ": block mask does not match the block size");
        *outl -= static_cast<int>(block);
        ctx->finalUsed = 1;
        std::memcpy(ctx->final, updateOut + *outl, block);
    } else {
        ctx->finalUsed = 0;
    }
    if (held) {
        std::memcpy(out, savedFinal, block);
        *outl += static_cast<int>(block);
    }
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
    if (ctx->flags & CipherFlagNoPadding) {
        if (!ctx->bufLen) return 1;
        LibreSslPutError_nid_no_patch(6, 0xfff, 138);
        return 0;
    }
    if (block <= 1) return 1;
    if (ctx->bufLen || !ctx->finalUsed) {
        LibreSslPutError_nid_no_patch(6, 0xfff, 109);
        return 0;
    }
    if (block > sizeof(ctx->final)) {
        LibreSslPutError_nid_no_patch(6, 0xfff, 136);
        return 0;
    }
    const unsigned pad = ctx->final[block - 1];
    if (pad == 0 || pad > block) {
        LibreSslPutError_nid_no_patch(6, 0xfff, 100);
        return 0;
    }
    for (unsigned index = 0; index < pad; ++index)
        if (ctx->final[--block] != pad) {
            LibreSslPutError_nid_no_patch(6, 0xfff, 100);
            return 0;
        }
    const unsigned length = static_cast<unsigned>(cipher.blockSize) - pad;
    Copy(out, ctx->final, length, function);
    *outl = static_cast<int>(length);
    return 1;
}

}

extern "C" {
void APS5_VABI EVP_CIPHER_CTX_init_nid_postfix(LibreSslEvpCipherCtx* ctx) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    LibreSslEvpCipherCtx cleared;
    std::memset(&cleared, 0, sizeof(cleared));
    CopyGuestBytesOutPreservingErrno(ctx, &cleared, sizeof(cleared), __func__);
}

LibreSslEvpCipherCtx* APS5_VABI EVP_CIPHER_CTX_new_nid_postfix(void) {
    return static_cast<LibreSslEvpCipherCtx*>(calloc_nid_postfix(1, sizeof(LibreSslEvpCipherCtx)));
}

int APS5_VABI EVP_CIPHER_CTX_set_padding_nid_postfix(LibreSslEvpCipherCtx* ctx, int pad) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    const auto flags = LoadField<std::uint64_t>(ctx, FlagsField, __func__);
    StoreField(ctx, FlagsField, pad ? flags & ~CipherFlagNoPadding : flags | CipherFlagNoPadding, __func__);
    return 1;
}

int APS5_VABI EVP_CIPHER_CTX_cleanup_nid_postfix(LibreSslEvpCipherCtx* ctx) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    RequireGuestWritablePreservingErrno(ctx, sizeof(LibreSslEvpCipherCtx), __func__);
    if (LoadField<void*>(ctx, EngineField, __func__)) NotImplementedAt_nid_no_patch("EVP_CIPHER_CTX_cleanup: releasing an ENGINE", __FILE__, __LINE__);
    if (const auto* cipher = LoadField<const LibreSslEvpCipher*>(ctx, CipherField, __func__)) {
        const auto active = LoadCipher(cipher, __func__);
        if (active.cleanup && !active.cleanup(ctx)) return 0;
        if (void* data = LoadField<void*>(ctx, CipherDataField, __func__))
            Wipe(data, CipherDataBytes(LoadCipher(LoadField<const LibreSslEvpCipher*>(ctx, CipherField, __func__), __func__), __func__));
    }
    free_nid_postfix(LoadField<void*>(ctx, CipherDataField, __func__));
    LibreSslEvpCipherCtx cleared;
    std::memset(&cleared, 0, sizeof(cleared));
    CopyGuestBytesOutPreservingErrno(ctx, &cleared, sizeof(cleared), __func__);
    return 1;
}

void APS5_VABI EVP_CIPHER_CTX_free_nid_postfix(LibreSslEvpCipherCtx* ctx) {
    if (!ctx) return;
    EVP_CIPHER_CTX_cleanup_nid_postfix(ctx);
    free_nid_postfix(ctx);
}

int APS5_VABI EVP_CIPHER_CTX_ctrl_nid_postfix(LibreSslEvpCipherCtx* ctx, int type, int arg, void* pointer) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    const auto* cipher = LoadField<const LibreSslEvpCipher*>(ctx, CipherField, __func__);
    if (!cipher) {
        LibreSslPutError_nid_no_patch(6, 0xfff, 131);
        return 0;
    }
    const auto active = LoadCipher(cipher, __func__);
    if (!active.control) {
        LibreSslPutError_nid_no_patch(6, 0xfff, 132);
        return 0;
    }
    const int result = active.control(ctx, type, arg, pointer);
    if (result != -1) return result;
    LibreSslPutError_nid_no_patch(6, 0xfff, 133);
    return 0;
}

int APS5_VABI EVP_CipherInit_ex_nid_postfix(LibreSslEvpCipherCtx* ctx, const LibreSslEvpCipher* cipher, void* engine, const unsigned char* key, const unsigned char* iv, int encrypt) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    RequireGuestWritablePreservingErrno(ctx, sizeof(LibreSslEvpCipherCtx), __func__);
    if (encrypt == -1) {
        encrypt = LoadField<int>(ctx, EncryptField, __func__);
    } else {
        encrypt = encrypt ? 1 : 0;
        StoreField(ctx, EncryptField, encrypt, __func__);
    }
    const auto* current = LoadField<const LibreSslEvpCipher*>(ctx, CipherField, __func__);
    const bool engineCipher = LoadField<void*>(ctx, EngineField, __func__) && current && (!cipher || LoadCipher(cipher, __func__).nid == LoadCipher(current, __func__).nid);
    if (!engineCipher && cipher) {
        if (engine) NotImplemented_nid_no_patch("EVP_CipherInit_ex: ENGINE implementation");
        const auto requested = LoadCipher(cipher, __func__);
        if (current) {
            const auto flags = LoadField<std::uint64_t>(ctx, FlagsField, __func__);
            EVP_CIPHER_CTX_cleanup_nid_postfix(ctx);
            StoreField(ctx, EncryptField, encrypt, __func__);
            StoreField(ctx, FlagsField, flags, __func__);
        }
        StoreField<void*>(ctx, EngineField, nullptr, __func__);
        StoreField(ctx, CipherField, cipher, __func__);
        StoreField<void*>(ctx, CipherDataField, nullptr, __func__);
        if (requested.contextSize) {
            void* data = calloc_nid_postfix(1, static_cast<std::size_t>(requested.contextSize));
            StoreField(ctx, CipherDataField, data, __func__);
            if (!data) {
                LibreSslPutError_nid_no_patch(6, 0xfff, 65);
                return 0;
            }
        }
        StoreField(ctx, KeyLenField, requested.keyLength, __func__);
        StoreField(ctx, FlagsField, LoadField<std::uint64_t>(ctx, FlagsField, __func__) & CipherCtxFlagWrapAllow, __func__);
        if ((requested.flags & CipherFlagCtrlInit) && !EVP_CIPHER_CTX_ctrl_nid_postfix(ctx, CipherControlInit, 0, nullptr)) {
            LibreSslPutError_nid_no_patch(6, 0xfff, 134);
            return 0;
        }
    } else if (!engineCipher && !current) {
        LibreSslPutError_nid_no_patch(6, 0xfff, 131);
        return 0;
    }
    const auto active = LoadCipher(LoadField<const LibreSslEvpCipher*>(ctx, CipherField, __func__), __func__);
    if (active.blockSize != 1 && active.blockSize != 8 && active.blockSize != 16) {
        LibreSslPutError_nid_no_patch(6, 0xfff, 136);
        return 0;
    }
    if ((active.flags & CipherModeMask) == CipherModeWrap && !(LoadField<std::uint64_t>(ctx, FlagsField, __func__) & CipherCtxFlagWrapAllow)) return 0;
    if (!(active.flags & CipherFlagCustomIv)) {
        const auto ivLength = static_cast<std::size_t>(active.ivLength);
        switch (active.flags & CipherModeMask) {
        case CipherModeStream:
        case CipherModeEcb:
            break;
        case CipherModeCfb:
        case CipherModeOfb:
            StoreField(ctx, NumField, 0, __func__);
            [[fallthrough]];
        case CipherModeCbc:
            if (ivLength > IvBytes) {
                LibreSslPutError_nid_no_patch(6, 0xfff, 102);
                return 0;
            }
            if (iv) StoreIv(ctx, OivField, iv, ivLength, __func__);
            StoreIv(ctx, IvField, reinterpret_cast<const std::byte*>(ctx) + OivField, ivLength, __func__);
            break;
        case CipherModeCtr:
            StoreField(ctx, NumField, 0, __func__);
            if (iv) {
                if (ivLength > IvBytes) throw std::invalid_argument(std::string(__func__) + ": CTR IV length " + std::to_string(active.ivLength) + " is outside 0-16");
                StoreIv(ctx, IvField, iv, ivLength, __func__);
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
    StoreField(ctx, BufLenField, 0, __func__);
    StoreField(ctx, FinalUsedField, 0, __func__);
    StoreField(ctx, BlockMaskField, active.blockSize - 1, __func__);
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
    return LoadField<int>(ctx, EncryptField, __func__) ? EncryptUpdate(ctx, out, outl, in, inl, __func__) : DecryptUpdate(ctx, out, outl, in, inl, __func__);
}

int APS5_VABI EVP_EncryptFinal_ex_nid_postfix(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl) {
    return EncryptFinal(ctx, out, outl, __func__);
}

int APS5_VABI EVP_DecryptFinal_ex_nid_postfix(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl) {
    return DecryptFinal(ctx, out, outl, __func__);
}

int APS5_VABI EVP_CipherFinal_ex_nid_postfix(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* outl) {
    if (!ctx) throw std::invalid_argument(std::string(__func__) + ": null context");
    return LoadField<int>(ctx, EncryptField, __func__) ? EncryptFinal(ctx, out, outl, __func__) : DecryptFinal(ctx, out, outl, __func__);
}

void APS5_VABI EVP_cleanup_nid_postfix(void) {
}

}
