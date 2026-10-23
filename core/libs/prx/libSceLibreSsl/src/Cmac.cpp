#include "prx/libSceLibreSsl/include/Cmac.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <stdexcept>
#include <mbedtls/aes.h>

extern "C" {
void* APS5_VABI malloc_nid_postfix(std::size_t);
void* APS5_VABI calloc_nid_postfix(std::size_t, std::size_t);
void APS5_VABI free_nid_postfix(void*);
}

struct LibreSslCmac {
    LibreSslCipherContext cipherContext;
    unsigned char firstSubkey[32];
    unsigned char secondSubkey[32];
    unsigned char previousBlock[32];
    unsigned char lastBlock[32];
    std::int32_t lastLength;
};

static_assert(sizeof(LibreSslCmac) == 304);

namespace {

void Cleanse(void* data, std::size_t length) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    while (length-- != 0) *bytes++ = 0;
}

int Error(int reason) {
    LibreSslPutError_nid_no_patch(6, 0xfff, reason, __FILE__, __LINE__);
    return 0;
}

void RequireContext(const LibreSslCmac* context) {
    if (context == nullptr) throw std::invalid_argument("CMAC: null context");
}

int Cleanup(LibreSslCipherContext& context) {
    if (context.engine != nullptr) NotImplemented_nid_no_patch(__func__);
    if (context.cipher != nullptr) {
        if (context.cipher->cleanup != nullptr && !context.cipher->cleanup(&context)) return 0;
        if (context.cipherData != nullptr) Cleanse(context.cipherData, context.cipher->contextSize);
    }
    free_nid_postfix(context.cipherData);
    Cleanse(&context, sizeof(context));
    return 1;
}

int Control(LibreSslCipherContext& context, int command, int argument) {
    if (context.cipher == nullptr) return Error(131);
    if (context.cipher->control == nullptr) return Error(132);
    const int result = context.cipher->control(&context, command, argument, nullptr);
    return result == -1 ? Error(133) : result;
}

int InitializeCipher(LibreSslCipherContext& context, const LibreSslCipher* cipher, const unsigned char* key, const unsigned char* iv, void* engine) {
    context.encrypt = 1;
    if (context.engine != nullptr || (cipher != nullptr && engine != nullptr)) NotImplemented_nid_no_patch(__func__);
    if (cipher != nullptr) {
        if (cipher->contextSize < 0 || cipher->ivLength < 0) throw std::invalid_argument("CMAC: invalid cipher dimensions");
        if (context.cipher != nullptr) {
            const auto flags = context.flags;
            Cleanup(context);
            context.encrypt = 1;
            context.flags = flags;
        }
        context.cipher = cipher;
        context.cipherData = nullptr;
        if (cipher->contextSize != 0) {
            context.cipherData = calloc_nid_postfix(1, cipher->contextSize);
            if (context.cipherData == nullptr) return Error(65);
        }
        context.keyLength = cipher->keyLength;
        context.flags &= 1;
        if ((cipher->flags & 0x40) != 0 && !Control(context, 0, 0)) return Error(134);
    }
    if (context.cipher == nullptr) return Error(131);
    const auto& method = *context.cipher;
    if (method.blockSize != 1 && method.blockSize != 8 && method.blockSize != 16) return Error(136);
    const auto mode = method.flags & 0xf0007;
    if ((context.flags & 1) == 0 && mode == 0x10002) return Error(170);
    if ((method.flags & 0x10) == 0) {
        if (mode == 2 || mode == 3 || mode == 4 || mode == 5) {
            if (method.ivLength > 16) return Error(102);
            if (mode != 2) context.position = 0;
            if (mode == 5) {
                if (iv != nullptr) std::memcpy(context.iv, iv, method.ivLength);
            } else {
                if (iv != nullptr) std::memcpy(context.originalIv, iv, method.ivLength);
                std::memcpy(context.iv, context.originalIv, method.ivLength);
            }
        } else if (mode != 0 && mode != 1) {
            return 0;
        }
    }
    if (key != nullptr || (method.flags & 0x20) != 0) {
        if (method.init == nullptr) NotImplemented_nid_no_patch(__func__);
        if (!method.init(&context, key, iv, 1)) return 0;
    }
    context.bufferLength = 0;
    context.finalUsed = 0;
    context.blockMask = method.blockSize - 1;
    return 1;
}

int SetKeyLength(LibreSslCipherContext& context, std::size_t length) {
    const int narrowed = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(length));
    if ((context.cipher->flags & 0x80) != 0) return Control(context, 1, narrowed);
    if (context.keyLength == narrowed) return 1;
    if (narrowed > 0 && (context.cipher->flags & 8) != 0) {
        context.keyLength = narrowed;
        return 1;
    }
    return Error(130);
}

int EncryptBlock(LibreSslCmac& context, unsigned char* output, const unsigned char* input) {
    if (context.cipherContext.cipher->cipher == nullptr) NotImplemented_nid_no_patch(__func__);
    return context.cipherContext.cipher->cipher(&context.cipherContext, output, input, context.cipherContext.cipher->blockSize);
}

void DoubleBlock(unsigned char* output, const unsigned char* input, int length) {
    unsigned char carry = 0;
    for (int index = length; index-- > 0;) {
        const auto value = input[index];
        output[index] = static_cast<unsigned char>((value << 1) | carry);
        carry = value >> 7;
    }
    output[length - 1] ^= static_cast<unsigned char>((length == 16 ? 0x87 : 0x1b) & -static_cast<int>(carry));
}

int APS5_VABI AesInit(LibreSslCipherContext* context, const unsigned char* key, const unsigned char*, int encrypt) {
    if (key == nullptr || context->cipherData == nullptr) throw std::invalid_argument("CMAC AES: missing key storage");
    if (context->keyLength != 16 && context->keyLength != 24 && context->keyLength != 32) return Error(143);
    auto* aes = static_cast<mbedtls_aes_context*>(context->cipherData);
    mbedtls_aes_init(aes);
    const int result = encrypt ? mbedtls_aes_setkey_enc(aes, key, context->keyLength * 8) : mbedtls_aes_setkey_dec(aes, key, context->keyLength * 8);
    if (result != 0) throw std::runtime_error("CMAC AES: backend key setup failed");
    return 1;
}

int APS5_VABI AesCipher(LibreSslCipherContext* context, unsigned char* output, const unsigned char* input, std::size_t length) {
    if (context->cipherData == nullptr || (length != 0 && (output == nullptr || input == nullptr))) throw std::invalid_argument("CMAC AES: null data");
    if (length == 0) return 1;
    auto* aes = static_cast<mbedtls_aes_context*>(context->cipherData);
    const int mode = context->encrypt ? MBEDTLS_AES_ENCRYPT : MBEDTLS_AES_DECRYPT;
    const auto complete = length - length % 16;
    if (complete != 0 && mbedtls_aes_crypt_cbc(aes, mode, complete, context->iv, input, output) != 0)
        throw std::runtime_error("CMAC AES: backend encryption failed");
    const auto remaining = length - complete;
    if (remaining == 0) return 1;
    input += complete;
    output += complete;
    unsigned char block[16];
    unsigned char nextIv[16];
    if (context->encrypt) {
        std::memcpy(block, context->iv, 16);
        for (std::size_t index = 0; index < remaining; ++index) block[index] ^= input[index];
        if (mbedtls_aes_crypt_ecb(aes, mode, block, output) != 0) throw std::runtime_error("CMAC AES: backend encryption failed");
        std::memcpy(context->iv, output, 16);
    } else {
        std::memcpy(nextIv, input, 16);
        if (mbedtls_aes_crypt_ecb(aes, mode, input, block) != 0) throw std::runtime_error("CMAC AES: backend decryption failed");
        for (std::size_t index = 0; index < remaining; ++index) output[index] = block[index] ^ context->iv[index];
        std::memcpy(context->iv, nextIv, 16);
    }
    Cleanse(block, sizeof(block));
    Cleanse(nextIv, sizeof(nextIv));
    return 1;
}

const LibreSslCipher aes128{419, 16, 16, 16, 0x5002, AesInit, AesCipher, nullptr, sizeof(mbedtls_aes_context), nullptr, nullptr, nullptr, nullptr};
const LibreSslCipher aes192{423, 16, 24, 16, 0x5002, AesInit, AesCipher, nullptr, sizeof(mbedtls_aes_context), nullptr, nullptr, nullptr, nullptr};
const LibreSslCipher aes256{427, 16, 32, 16, 0x5002, AesInit, AesCipher, nullptr, sizeof(mbedtls_aes_context), nullptr, nullptr, nullptr, nullptr};

}

extern "C" {

LibreSslCmac* APS5_VABI CMAC_CTX_new_nid_postfix() {
    auto* context = static_cast<LibreSslCmac*>(malloc_nid_postfix(sizeof(LibreSslCmac)));
    if (context != nullptr) {
        context->cipherContext = {};
        context->lastLength = -1;
    }
    return context;
}

void APS5_VABI CMAC_CTX_free_nid_postfix(LibreSslCmac* context) {
    if (context == nullptr) return;
    Cleanup(context->cipherContext);
    Cleanse(context, sizeof(*context));
    free_nid_postfix(context);
}

int APS5_VABI CMAC_Init_nid_postfix(LibreSslCmac* context, const void* key, std::size_t keyLength, const LibreSslCipher* cipher, void* engine) {
    RequireContext(context);
    const unsigned char zero[32]{};
    if (key == nullptr && keyLength == 0 && cipher == nullptr && engine == nullptr) {
        if (context->lastLength == -1) return 0;
        if (!InitializeCipher(context->cipherContext, nullptr, nullptr, zero, nullptr)) return 0;
        std::memset(context->previousBlock, 0, context->cipherContext.cipher->blockSize);
        context->lastLength = 0;
        return 1;
    }
    if (cipher != nullptr && !InitializeCipher(context->cipherContext, cipher, nullptr, nullptr, engine)) return 0;
    if (key == nullptr) return 1;
    if (context->cipherContext.cipher == nullptr) return 0;
    if (!SetKeyLength(context->cipherContext, keyLength)) return 0;
    if (!InitializeCipher(context->cipherContext, nullptr, static_cast<const unsigned char*>(key), zero, nullptr)) return 0;
    if (!EncryptBlock(*context, context->previousBlock, zero)) return 0;
    const auto blockSize = context->cipherContext.cipher->blockSize;
    DoubleBlock(context->firstSubkey, context->previousBlock, blockSize);
    DoubleBlock(context->secondSubkey, context->firstSubkey, blockSize);
    Cleanse(context->previousBlock, blockSize);
    if (!InitializeCipher(context->cipherContext, nullptr, nullptr, zero, nullptr)) return 0;
    context->lastLength = 0;
    return 1;
}

int APS5_VABI CMAC_Update_nid_postfix(LibreSslCmac* context, const void* data, std::size_t length) {
    RequireContext(context);
    if (context->lastLength == -1) return 0;
    if (length == 0) return 1;
    if (data == nullptr) throw std::invalid_argument("CMAC_Update: null input");
    const auto* bytes = static_cast<const unsigned char*>(data);
    const auto blockSize = context->cipherContext.cipher->blockSize;
    if (context->lastLength != 0) {
        const auto take = std::min(length, static_cast<std::size_t>(blockSize - context->lastLength));
        std::memcpy(context->lastBlock + context->lastLength, bytes, take);
        context->lastLength += static_cast<std::int32_t>(take);
        bytes += take;
        length -= take;
        if (length == 0) return 1;
        if (!EncryptBlock(*context, context->previousBlock, context->lastBlock)) return 0;
    }
    while (length > static_cast<std::size_t>(blockSize)) {
        if (!EncryptBlock(*context, context->previousBlock, bytes)) return 0;
        bytes += blockSize;
        length -= blockSize;
    }
    std::memcpy(context->lastBlock, bytes, length);
    context->lastLength = static_cast<std::int32_t>(length);
    return 1;
}

int APS5_VABI CMAC_Final_nid_postfix(LibreSslCmac* context, unsigned char* output, std::size_t* length) {
    RequireContext(context);
    if (context->lastLength == -1) return 0;
    if (length == nullptr) throw std::invalid_argument("CMAC_Final: null output length");
    const auto blockSize = context->cipherContext.cipher->blockSize;
    *length = blockSize;
    if (output == nullptr) return 1;
    const bool full = context->lastLength == blockSize;
    if (!full) {
        std::memset(context->lastBlock + context->lastLength, 0, blockSize - context->lastLength);
        context->lastBlock[context->lastLength] = 0x80;
    }
    const auto* subkey = full ? context->firstSubkey : context->secondSubkey;
    for (int index = 0; index < blockSize; ++index) output[index] = context->lastBlock[index] ^ subkey[index];
    if (EncryptBlock(*context, output, output)) return 1;
    Cleanse(output, blockSize);
    return 0;
}

const LibreSslCipher* APS5_VABI EVP_aes_128_cbc_nid_postfix() { return &aes128; }
const LibreSslCipher* APS5_VABI EVP_aes_192_cbc_nid_postfix() { return &aes192; }
const LibreSslCipher* APS5_VABI EVP_aes_256_cbc_nid_postfix() { return &aes256; }

}
