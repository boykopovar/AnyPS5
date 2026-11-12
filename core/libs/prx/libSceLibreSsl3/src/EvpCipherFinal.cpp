#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int BadDecrypt = 100;
constexpr int WrongFinalBlockLength = 109;
constexpr int BadBlockLength = 136;
constexpr int DataNotMultipleOfBlockLength = 138;

const LibreSslEvpCipher& Checked(const LibreSslEvpCipherCtx* context, const int* written, const char* function) {
    if (!context) throw std::invalid_argument(std::string(function) + ": null context");
    if (!context->cipher) throw std::invalid_argument(std::string(function) + ": no cipher set");
    if (!written) throw std::invalid_argument(std::string(function) + ": null output length");
    return *context->cipher;
}

int Fail(int reason) {
    LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, reason, __FILE__, __LINE__);
    return 0;
}

int Custom(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const char* function) {
    const auto& cipher = *context->cipher;
    if (!cipher.doCipher) throw std::invalid_argument(std::string(function) + ": cipher without do_cipher");
    const int produced = cipher.doCipher(context, out, nullptr, 0);
    if (produced < 0) return 0;
    *written = produced;
    return 1;
}

int Encrypt(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const char* function) {
    const auto& cipher = Checked(context, written, function);
    const int blockSize = cipher.blockSize;
    const int partial = context->bufLen;
    *written = 0;
    if (cipher.flags & CipherFlagCustomCipher) return Custom(context, out, written, function);
    if (partial < 0 || partial >= blockSize || static_cast<std::size_t>(blockSize) > sizeof(context->buf)) return Fail(BadBlockLength);
    if (blockSize == 1) return 1;
    if (context->flags & CipherFlagNoPadding) return partial != 0 ? Fail(DataNotMultipleOfBlockLength) : 1;
    if (!cipher.doCipher) throw std::invalid_argument(std::string(function) + ": cipher without do_cipher");
    if (!out) throw std::invalid_argument(std::string(function) + ": null output");
    const int pad = blockSize - partial;
    std::memset(context->buf + partial, pad, static_cast<std::size_t>(pad));
    if (!cipher.doCipher(context, out, context->buf, static_cast<std::size_t>(blockSize))) return 0;
    *written = blockSize;
    return 1;
}

int Decrypt(LibreSslEvpCipherCtx* context, unsigned char* out, int* written, const char* function) {
    const auto& cipher = Checked(context, written, function);
    const int blockSize = cipher.blockSize;
    const int partial = context->bufLen;
    *written = 0;
    if (cipher.flags & CipherFlagCustomCipher) return Custom(context, out, written, function);
    if (context->flags & CipherFlagNoPadding) return partial != 0 ? Fail(DataNotMultipleOfBlockLength) : 1;
    if (blockSize == 1) return 1;
    if (partial != 0 || !context->finalUsed) return Fail(WrongFinalBlockLength);
    if (static_cast<std::size_t>(blockSize) > sizeof(context->final)) return Fail(BadBlockLength);
    if (blockSize <= 0) throw std::invalid_argument(std::string(function) + ": held block with block size " + std::to_string(blockSize));
    const int pad = context->final[blockSize - 1];
    if (pad <= 0 || pad > blockSize) return Fail(BadDecrypt);
    const int plain = blockSize - pad;
    for (int index = plain; index < blockSize; ++index)
        if (context->final[index] != pad) return Fail(BadDecrypt);
    if (plain > 0) {
        if (!out) throw std::invalid_argument(std::string(function) + ": null output");
        std::memcpy(out, context->final, static_cast<std::size_t>(plain));
    }
    *written = plain;
    return 1;
}

}

extern "C" {

int APS5_VABI EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written) {
    return Encrypt(context, out, written, __func__);
}

int APS5_VABI EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written) {
    return Decrypt(context, out, written, __func__);
}

int APS5_VABI EVP_CipherFinal_ex_nid_disambig1_nid_postfix(LibreSslEvpCipherCtx* context, unsigned char* out, int* written) {
    if (!context) throw std::invalid_argument(std::string(__func__) + ": null context");
    return context->encrypt ? Encrypt(context, out, written, __func__) : Decrypt(context, out, written, __func__);
}

}
