#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uint64_t CipherModeCcm = 0x7;
constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int BadBlockLength = 136;
constexpr int TooLarge = 164;

const LibreSslEvpCipher& Checked(const LibreSslEvpCipherCtx* context, const int* written, const char* function) {
    if (!context) throw std::invalid_argument(std::string(function) + ": null context");
    if (!context->cipher) throw std::invalid_argument(std::string(function) + ": no cipher set");
    if (!written) throw std::invalid_argument(std::string(function) + ": null output length");
    return *context->cipher;
}

int Mask(int blockSize) {
    return static_cast<int>(static_cast<unsigned>(blockSize) - 1u);
}

void Copy(unsigned char* destination, const unsigned char* source, int length, const char* function) {
    if (length <= 0) return;
    if (!destination || !source) throw std::invalid_argument(std::string(function) + ": null buffer");
    std::memcpy(destination, source, static_cast<std::size_t>(length));
}

int Run(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length, const char* function) {
    const auto& cipher = *context->cipher;
    if (!cipher.doCipher) throw std::invalid_argument(std::string(function) + ": cipher without do_cipher");
    if (cipher.flags & CipherFlagCustomCipher) {
        const int produced = cipher.doCipher(context, out, in, static_cast<std::size_t>(length));
        if (produced < 0) return 0;
        *written = produced;
        return 1;
    }
    if (length > 0 && (!out || !in)) throw std::invalid_argument(std::string(function) + ": null buffer");
    if (!cipher.doCipher(context, out, in, static_cast<std::size_t>(length))) return 0;
    *written = length;
    return 1;
}

int Encrypt(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length, const char* function) {
    const auto& cipher = Checked(context, written, function);
    const int blockSize = cipher.blockSize;
    const int blockMask = Mask(blockSize);
    int partial = context->bufLen;
    int total = 0;
    *written = 0;
    if ((blockSize & blockMask) != 0) return 0;
    if (length < 0) return 0;
    if (length == 0 && (cipher.flags & CipherModeMask) != CipherModeCcm) return 1;
    if (cipher.flags & CipherFlagCustomCipher) return Run(context, out, written, in, length, function);
    if (partial == 0 && (length & blockMask) == 0) return Run(context, out, written, in, length, function);
    if (partial < 0 || partial >= blockSize || static_cast<std::size_t>(blockSize) > sizeof(context->buf)) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, BadBlockLength, __FILE__, __LINE__);
        return 0;
    }
    if (partial > 0) {
        const int needed = blockSize - partial;
        if (needed > length) {
            Copy(context->buf + partial, in, length, function);
            context->bufLen += length;
            return 1;
        }
        if (((length - needed) & ~blockMask) > INT_MAX - blockSize) {
            LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, TooLarge, __FILE__, __LINE__);
            return 0;
        }
        Copy(context->buf + partial, in, needed, function);
        int produced = 0;
        if (!Run(context, out, &produced, context->buf, blockSize, function)) return 0;
        total = produced;
        length -= needed;
        in += needed;
        out += produced;
    }
    partial = length & blockMask;
    length -= partial;
    if (length > 0) {
        if (INT_MAX - length < total) return 0;
        int produced = 0;
        if (!Run(context, out, &produced, in, length, function)) return 0;
        if (INT_MAX - produced < total) return 0;
        total += produced;
    }
    Copy(context->buf, in + length, partial, function);
    context->bufLen = partial;
    *written = total;
    return 1;
}

int Decrypt(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length, const char* function) {
    const auto& cipher = Checked(context, written, function);
    const int blockSize = cipher.blockSize;
    const int blockMask = Mask(blockSize);
    int total = 0;
    *written = 0;
    if ((blockSize & blockMask) != 0) return 0;
    if (length < 0) return 0;
    if (length == 0 && (cipher.flags & CipherModeMask) != CipherModeCcm) return 1;
    if (cipher.flags & CipherFlagCustomCipher) return Run(context, out, written, in, length, function);
    if (context->flags & CipherFlagNoPadding) return Encrypt(context, out, written, in, length, function);
    if (static_cast<std::size_t>(blockSize) > sizeof(context->final)) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, BadBlockLength, __FILE__, __LINE__);
        return 0;
    }
    if (context->finalUsed) {
        if ((length & ~blockMask) > INT_MAX - blockSize) {
            LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, TooLarge, __FILE__, __LINE__);
            return 0;
        }
        Copy(out, context->final, blockSize, function);
        out += blockSize;
        total = blockSize;
    }
    context->finalUsed = 0;
    int produced = 0;
    if (!Encrypt(context, out, &produced, in, length, function)) return 0;
    if (blockSize > 1 && context->bufLen == 0) {
        if (produced < blockSize) return 0;
        produced -= blockSize;
        Copy(context->final, out + produced, blockSize, function);
        context->finalUsed = 1;
    }
    if (produced > INT_MAX - total) return 0;
    total += produced;
    *written = total;
    return 1;
}

}

extern "C" {

int APS5_VABI EVP_EncryptUpdate_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length) {
    return Encrypt(context, out, written, in, length, __func__);
}

int APS5_VABI EVP_DecryptUpdate_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length) {
    return Decrypt(context, out, written, in, length, __func__);
}

int APS5_VABI EVP_CipherUpdate_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const unsigned char* in, int length) {
    if (!context) throw std::invalid_argument(std::string(__func__) + ": null context");
    return context->encrypt ? Encrypt(context, out, written, in, length, __func__) : Decrypt(context, out, written, in, length, __func__);
}

}
