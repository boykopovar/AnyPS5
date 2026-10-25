#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

namespace {

constexpr std::uint64_t CipherFlagFips = 0x4000;
constexpr int AesKeyContextBytes = 264;
constexpr std::size_t AesBlockBytes = 16;

static_assert(sizeof(AesKey) <= AesKeyContextBytes);

AesKey& KeyOf(LibreSslEvpCipherCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    if (!ctx->cipherData) throw std::invalid_argument(std::string(function) + ": context without cipher data");
    return *static_cast<AesKey*>(ctx->cipherData);
}

int APS5_VABI AesInitKey(LibreSslEvpCipherCtx* ctx, const unsigned char* key, const unsigned char*, int) {
    auto& schedule = KeyOf(ctx, "aes_init_key");
    return AES_set_encrypt_key_nid_postfix(key, ctx->keyLen * 8, &schedule) < 0 ? 0 : 1;
}

int APS5_VABI AesCfb8Cipher(LibreSslEvpCipherCtx* ctx, unsigned char* out, const unsigned char* in, std::size_t length) {
    const auto& key = KeyOf(ctx, "aes_cfb8_cipher");
    if (length && (!in || !out)) throw std::invalid_argument("aes_cfb8_cipher: null input or output");
    for (std::size_t index = 0; index < length; ++index) {
        unsigned char stream[AesBlockBytes];
        AES_encrypt_nid_postfix(ctx->iv, stream, &key);
        const unsigned char input = in[index];
        const unsigned char output = input ^ stream[0];
        out[index] = output;
        std::memmove(ctx->iv, ctx->iv + 1, AesBlockBytes - 1);
        ctx->iv[AesBlockBytes - 1] = ctx->encrypt ? output : input;
    }
    return 1;
}

int APS5_VABI AesCtrCipher(LibreSslEvpCipherCtx* ctx, unsigned char* out, const unsigned char* in, std::size_t length) {
    const auto& key = KeyOf(ctx, "aes_ctr_cipher");
    auto position = static_cast<unsigned>(ctx->num);
    AES_ctr128_encrypt_nid_postfix(in, out, length, &key, ctx->iv, ctx->buf, &position);
    ctx->num = static_cast<int>(position);
    return 1;
}

constexpr LibreSslEvpCipher AesCipher(int nid, int keyLength, std::uint64_t mode, int (APS5_VABI* doCipher)(LibreSslEvpCipherCtx*, unsigned char*, const unsigned char*, std::size_t)) {
    LibreSslEvpCipher cipher {};
    cipher.nid = nid;
    cipher.blockSize = 1;
    cipher.keyLength = keyLength;
    cipher.ivLength = 16;
    cipher.flags = CipherFlagFips | mode;
    cipher.init = AesInitKey;
    cipher.doCipher = doCipher;
    cipher.contextSize = AesKeyContextBytes;
    return cipher;
}

constexpr LibreSslEvpCipher Aes128Cfb8 = AesCipher(653, 16, CipherModeCfb, AesCfb8Cipher);
constexpr LibreSslEvpCipher Aes192Cfb8 = AesCipher(654, 24, CipherModeCfb, AesCfb8Cipher);
constexpr LibreSslEvpCipher Aes256Cfb8 = AesCipher(655, 32, CipherModeCfb, AesCfb8Cipher);
constexpr LibreSslEvpCipher Aes128Ctr = AesCipher(904, 16, CipherModeCtr, AesCtrCipher);
constexpr LibreSslEvpCipher Aes192Ctr = AesCipher(905, 24, CipherModeCtr, AesCtrCipher);
constexpr LibreSslEvpCipher Aes256Ctr = AesCipher(906, 32, CipherModeCtr, AesCtrCipher);

}

extern "C" {

const LibreSslEvpCipher* APS5_VABI EVP_aes_128_cfb8_nid_postfix(void) {
    return &Aes128Cfb8;
}

const LibreSslEvpCipher* APS5_VABI EVP_aes_192_cfb8_nid_postfix(void) {
    return &Aes192Cfb8;
}

const LibreSslEvpCipher* APS5_VABI EVP_aes_256_cfb8_nid_postfix(void) {
    return &Aes256Cfb8;
}

const LibreSslEvpCipher* APS5_VABI EVP_aes_128_ctr_nid_postfix(void) {
    return &Aes128Ctr;
}

const LibreSslEvpCipher* APS5_VABI EVP_aes_192_ctr_nid_postfix(void) {
    return &Aes192Ctr;
}

const LibreSslEvpCipher* APS5_VABI EVP_aes_256_ctr_nid_postfix(void) {
    return &Aes256Ctr;
}

}
