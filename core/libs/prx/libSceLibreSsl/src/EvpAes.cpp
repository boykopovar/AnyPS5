#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

namespace {

constexpr std::uint64_t CipherFlagCustomCopy = 0x400;
constexpr std::uint64_t CipherFlagDefaultAsn1 = 0x1000;
constexpr std::uint64_t CipherFlagFips = 0x4000;
constexpr std::uint64_t CipherFlagAeadCipher = 0x200000;
constexpr std::uint64_t GcmFlags = CipherFlagFips | CipherFlagAeadCipher | CipherFlagDefaultAsn1 | CipherFlagCustomIv | CipherFlagCustomCipher |
    CipherFlagAlwaysCallInit | CipherFlagCtrlInit | CipherFlagCustomCopy | CipherModeGcm;
constexpr int ControlCopy = 0x8;
constexpr int ControlGcmSetIvLength = 0x9;
constexpr int ControlGcmGetTag = 0x10;
constexpr int ControlGcmSetTag = 0x11;
constexpr int ControlGcmSetIvFixed = 0x12;
constexpr int ControlGcmIvGenerate = 0x13;
constexpr int ControlTlsAad = 0x16;
constexpr int ControlGcmSetIvInvocation = 0x18;
constexpr int TlsAadBytes = 13;
constexpr std::size_t TlsExplicitIvBytes = 8;
constexpr std::size_t TlsTagBytes = 16;
constexpr int AesKeyContextBytes = 264;
constexpr int AesGcmContextBytes = 680;
constexpr std::size_t AesBlockBytes = 16;
constexpr std::uint64_t GcmAadLimit = std::uint64_t {1} << 61;
constexpr std::uint64_t GcmMessageLimit = (std::uint64_t {1} << 36) - 32;

struct GcmState {
    AesKey key;
    int keySet;
    int ivSet;
    unsigned char counter[AesBlockBytes];
    unsigned char keystream[AesBlockBytes];
    unsigned char mask[AesBlockBytes];
    unsigned char hash[AesBlockBytes];
    unsigned char subkey[AesBlockBytes];
    std::uint64_t aadLength;
    std::uint64_t messageLength;
    unsigned messageRemainder;
    unsigned aadRemainder;
    unsigned char* iv;
    int ivLength;
    int tagLength;
    int ivGenerate;
    int tlsAadLength;
};

static_assert(sizeof(AesKey) <= AesKeyContextBytes);
static_assert(sizeof(GcmState) <= AesGcmContextBytes);

void Wipe(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
}

int TimingSafeCompare(const void* first, const void* second, std::size_t size) {
    const auto* firstBytes = static_cast<const volatile unsigned char*>(first);
    const auto* secondBytes = static_cast<const volatile unsigned char*>(second);
    unsigned int difference = 0;
    for (std::size_t index = 0; index < size; ++index) difference |= firstBytes[index] ^ secondBytes[index];
    return static_cast<int>(difference);
}

void Copy(void* destination, const void* source, std::size_t size, const char* function) {
    if (!size) return;
    if (!destination || !source) throw std::invalid_argument(std::string(function) + ": null buffer");
    std::memcpy(destination, source, size);
}

std::uint64_t LoadBig64(const unsigned char* bytes) {
    std::uint64_t value = 0;
    for (int index = 0; index < 8; ++index) value = value << 8 | bytes[index];
    return value;
}

void StoreBig64(unsigned char* bytes, std::uint64_t value) {
    for (int index = 7; index >= 0; --index, value >>= 8) bytes[index] = static_cast<unsigned char>(value);
}

void Multiply(unsigned char (&block)[AesBlockBytes], const unsigned char (&subkey)[AesBlockBytes]) {
    std::uint64_t productHigh = 0;
    std::uint64_t productLow = 0;
    std::uint64_t valueHigh = LoadBig64(subkey);
    std::uint64_t valueLow = LoadBig64(subkey + 8);
    const std::uint64_t factorHigh = LoadBig64(block);
    const std::uint64_t factorLow = LoadBig64(block + 8);
    for (int bit = 0; bit < 128; ++bit) {
        const std::uint64_t word = bit < 64 ? factorHigh : factorLow;
        if ((word >> (63 - bit % 64)) & 1) {
            productHigh ^= valueHigh;
            productLow ^= valueLow;
        }
        const bool carry = valueLow & 1;
        valueLow = valueLow >> 1 | valueHigh << 63;
        valueHigh >>= 1;
        if (carry) valueHigh ^= UINT64_C(0xE100000000000000);
    }
    StoreBig64(block, productHigh);
    StoreBig64(block + 8, productLow);
}

std::uint32_t LoadBig32(const unsigned char* bytes) {
    return std::uint32_t(bytes[0]) << 24 | std::uint32_t(bytes[1]) << 16 | std::uint32_t(bytes[2]) << 8 | bytes[3];
}

void StoreBig32(unsigned char* bytes, std::uint32_t value) {
    for (int index = 3; index >= 0; --index, value >>= 8) bytes[index] = static_cast<unsigned char>(value);
}

void GcmInit(GcmState& state) {
    std::memset(state.counter, 0, sizeof(state.counter));
    std::memset(state.keystream, 0, sizeof(state.keystream));
    std::memset(state.mask, 0, sizeof(state.mask));
    std::memset(state.hash, 0, sizeof(state.hash));
    std::memset(state.subkey, 0, sizeof(state.subkey));
    state.aadLength = 0;
    state.messageLength = 0;
    state.messageRemainder = 0;
    state.aadRemainder = 0;
    AES_encrypt_nid_postfix(state.subkey, state.subkey, &state.key);
}

void GcmSetIv(GcmState& state, const unsigned char* iv, std::size_t length) {
    std::memset(state.counter, 0, sizeof(state.counter));
    std::memset(state.hash, 0, sizeof(state.hash));
    state.aadLength = 0;
    state.messageLength = 0;
    state.aadRemainder = 0;
    state.messageRemainder = 0;
    if (length && !iv) throw std::invalid_argument("CRYPTO_gcm128_setiv: null IV");
    std::uint32_t counter;
    if (length == 12) {
        std::memcpy(state.counter, iv, 12);
        state.counter[15] = 1;
        counter = 1;
    } else {
        const std::uint64_t bits = static_cast<std::uint64_t>(length) << 3;
        for (; length; ) {
            const std::size_t part = length < AesBlockBytes ? length : AesBlockBytes;
            for (std::size_t index = 0; index < part; ++index) state.counter[index] ^= iv[index];
            Multiply(state.counter, state.subkey);
            iv += part;
            length -= part;
        }
        unsigned char lengthBlock[8];
        StoreBig64(lengthBlock, bits);
        for (std::size_t index = 0; index < 8; ++index) state.counter[8 + index] ^= lengthBlock[index];
        Multiply(state.counter, state.subkey);
        counter = LoadBig32(state.counter + 12);
    }
    AES_encrypt_nid_postfix(state.counter, state.mask, &state.key);
    StoreBig32(state.counter + 12, counter + 1);
}

int GcmAad(GcmState& state, const unsigned char* aad, std::size_t length) {
    if (state.messageLength) return -2;
    const std::uint64_t total = state.aadLength + length;
    if (total > GcmAadLimit || total < length) return -1;
    if (length && !aad) throw std::invalid_argument("CRYPTO_gcm128_aad: null AAD");
    state.aadLength = total;
    unsigned position = state.aadRemainder;
    for (std::size_t index = 0; index < length; ++index) {
        state.hash[position] ^= aad[index];
        position = (position + 1) % AesBlockBytes;
        if (!position) Multiply(state.hash, state.subkey);
    }
    state.aadRemainder = position;
    return 0;
}

int GcmCrypt(GcmState& state, const unsigned char* in, unsigned char* out, std::size_t length, bool encrypt) {
    const std::uint64_t total = state.messageLength + length;
    if (total > GcmMessageLimit || total < length) return -1;
    if (length && (!in || !out)) throw std::invalid_argument(std::string(encrypt ? "CRYPTO_gcm128_encrypt" : "CRYPTO_gcm128_decrypt") + ": null input or output");
    state.messageLength = total;
    if (state.aadRemainder) {
        Multiply(state.hash, state.subkey);
        state.aadRemainder = 0;
    }
    std::uint32_t counter = LoadBig32(state.counter + 12);
    unsigned position = state.messageRemainder;
    for (std::size_t index = 0; index < length; ++index) {
        if (!position) {
            AES_encrypt_nid_postfix(state.counter, state.keystream, &state.key);
            StoreBig32(state.counter + 12, ++counter);
        }
        const unsigned char input = in[index];
        const unsigned char output = input ^ state.keystream[position];
        out[index] = output;
        state.hash[position] ^= encrypt ? output : input;
        position = (position + 1) % AesBlockBytes;
        if (!position) Multiply(state.hash, state.subkey);
    }
    state.messageRemainder = position;
    return 0;
}

int GcmFinish(GcmState& state, const unsigned char* tag, std::size_t length) {
    if (state.messageRemainder || state.aadRemainder) Multiply(state.hash, state.subkey);
    unsigned char lengths[AesBlockBytes];
    StoreBig64(lengths, state.aadLength << 3);
    StoreBig64(lengths + 8, state.messageLength << 3);
    for (std::size_t index = 0; index < AesBlockBytes; ++index) state.hash[index] ^= lengths[index];
    Multiply(state.hash, state.subkey);
    for (std::size_t index = 0; index < AesBlockBytes; ++index) state.hash[index] ^= state.mask[index];
    if (!tag || length > AesBlockBytes) return -1;
    return TimingSafeCompare(state.hash, tag, length);
}

void GcmTag(GcmState& state, unsigned char* tag, std::size_t length) {
    GcmFinish(state, nullptr, 0);
    Copy(tag, state.hash, length < AesBlockBytes ? length : AesBlockBytes, "CRYPTO_gcm128_tag");
}

void IncrementInvocation(unsigned char* counter) {
    for (int index = 7; index >= 0; --index)
        if (++counter[index]) return;
}

GcmState& GcmStateOf(LibreSslEvpCipherCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    if (!ctx->cipherData) throw std::invalid_argument(std::string(function) + ": context without cipher data");
    return *static_cast<GcmState*>(ctx->cipherData);
}

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

int APS5_VABI AesGcmCleanup(LibreSslEvpCipherCtx* ctx) {
    auto& state = GcmStateOf(ctx, "aes_gcm_cleanup");
    if (state.iv != ctx->iv) free_nid_postfix(state.iv);
    Wipe(&state, sizeof(state));
    return 1;
}

int APS5_VABI AesGcmControl(LibreSslEvpCipherCtx* ctx, int type, int arg, void* pointer) {
    auto& state = GcmStateOf(ctx, "aes_gcm_ctrl");
    auto* bytes = static_cast<unsigned char*>(pointer);
    switch (type) {
    case CipherControlInit:
        if (!ctx->cipher) throw std::invalid_argument("aes_gcm_ctrl: no cipher set");
        state.keySet = 0;
        state.ivSet = 0;
        state.ivLength = ctx->cipher->ivLength;
        state.iv = ctx->iv;
        state.tagLength = -1;
        state.ivGenerate = 0;
        state.tlsAadLength = -1;
        return 1;
    case ControlGcmSetIvLength:
        if (arg <= 0) return 0;
        if (arg > static_cast<int>(sizeof(ctx->iv)) && arg > state.ivLength) {
            if (state.iv != ctx->iv) free_nid_postfix(state.iv);
            state.iv = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(arg)));
            if (!state.iv) return 0;
        }
        state.ivLength = arg;
        return 1;
    case ControlGcmSetTag:
        if (arg <= 0 || arg > 16 || ctx->encrypt) return 0;
        Copy(ctx->buf, bytes, static_cast<std::size_t>(arg), "aes_gcm_ctrl");
        state.tagLength = arg;
        return 1;
    case ControlGcmGetTag:
        if (arg <= 0 || arg > 16 || !ctx->encrypt || state.tagLength < 0) return 0;
        Copy(bytes, ctx->buf, static_cast<std::size_t>(arg), "aes_gcm_ctrl");
        return 1;
    case ControlGcmSetIvFixed:
        if (arg == -1) {
            Copy(state.iv, bytes, static_cast<std::size_t>(state.ivLength), "aes_gcm_ctrl");
            state.ivGenerate = 1;
            return 1;
        }
        if (arg < 4 || state.ivLength - arg < 8) return 0;
        Copy(state.iv, bytes, static_cast<std::size_t>(arg), "aes_gcm_ctrl");
        if (ctx->encrypt) HostEntropy_nid_no_patch(state.iv + arg, static_cast<std::size_t>(state.ivLength - arg));
        state.ivGenerate = 1;
        return 1;
    case ControlGcmIvGenerate:
        if (!state.ivGenerate || !state.keySet) return 0;
        if (state.ivLength < 8) throw std::invalid_argument("aes_gcm_ctrl: generating an IV shorter than 8 bytes");
        GcmSetIv(state, state.iv, static_cast<std::size_t>(state.ivLength));
        if (arg <= 0 || arg > state.ivLength) arg = state.ivLength;
        Copy(bytes, state.iv + state.ivLength - arg, static_cast<std::size_t>(arg), "aes_gcm_ctrl");
        IncrementInvocation(state.iv + state.ivLength - 8);
        state.ivSet = 1;
        return 1;
    case ControlGcmSetIvInvocation:
        if (!state.ivGenerate || !state.keySet || ctx->encrypt) return 0;
        if (arg < 0 || arg > state.ivLength) throw std::invalid_argument("aes_gcm_ctrl: invocation field of " + std::to_string(arg) + " bytes is outside the IV");
        Copy(state.iv + state.ivLength - arg, bytes, static_cast<std::size_t>(arg), "aes_gcm_ctrl");
        GcmSetIv(state, state.iv, static_cast<std::size_t>(state.ivLength));
        state.ivSet = 1;
        return 1;
    case ControlTlsAad: {
        if (arg != TlsAadBytes) return 0;
        Copy(ctx->buf, bytes, TlsAadBytes, "aes_gcm_ctrl");
        state.tlsAadLength = arg;
        unsigned length = static_cast<unsigned>(ctx->buf[arg - 2]) << 8 | ctx->buf[arg - 1];
        if (length < TlsExplicitIvBytes) return 0;
        length -= TlsExplicitIvBytes;
        if (!ctx->encrypt) {
            if (length < TlsTagBytes) return 0;
            length -= TlsTagBytes;
        }
        ctx->buf[arg - 2] = static_cast<unsigned char>(length >> 8);
        ctx->buf[arg - 1] = static_cast<unsigned char>(length);
        return static_cast<int>(TlsTagBytes);
    }
    case ControlCopy: {
        auto* out = static_cast<LibreSslEvpCipherCtx*>(pointer);
        auto& copy = GcmStateOf(out, "aes_gcm_ctrl");
        if (state.iv == ctx->iv) {
            copy.iv = out->iv;
            return 1;
        }
        copy.iv = static_cast<unsigned char*>(malloc_nid_postfix(static_cast<std::size_t>(state.ivLength)));
        if (!copy.iv) return 0;
        std::memcpy(copy.iv, state.iv, static_cast<std::size_t>(state.ivLength));
        return 1;
    }
    default:
        return -1;
    }
}

int APS5_VABI AesGcmInitKey(LibreSslEvpCipherCtx* ctx, const unsigned char* key, const unsigned char* iv, int) {
    auto& state = GcmStateOf(ctx, "aes_gcm_init_key");
    if (!iv && !key) return 1;
    const auto ivLength = static_cast<std::size_t>(state.ivLength);
    if (key) {
        if (AES_set_encrypt_key_nid_postfix(key, ctx->keyLen * 8, &state.key) < 0)
            throw std::invalid_argument("aes_gcm_init_key: key length of " + std::to_string(ctx->keyLen) + " bytes");
        GcmInit(state);
        if (!iv && state.ivSet) iv = state.iv;
        if (iv) {
            GcmSetIv(state, iv, ivLength);
            state.ivSet = 1;
        }
        state.keySet = 1;
        return 1;
    }
    if (state.keySet) GcmSetIv(state, iv, ivLength);
    else Copy(state.iv, iv, ivLength, "aes_gcm_init_key");
    state.ivSet = 1;
    state.ivGenerate = 0;
    return 1;
}

int AesGcmTlsCipher(LibreSslEvpCipherCtx* ctx, GcmState& state, unsigned char* out, const unsigned char* in, std::size_t length) {
    if (out != in || length < TlsExplicitIvBytes + TlsTagBytes) return -1;
    int result = -1;
    const int control = ctx->encrypt ? ControlGcmIvGenerate : ControlGcmSetIvInvocation;
    if (EVP_CIPHER_CTX_ctrl_nid_postfix(ctx, control, static_cast<int>(TlsExplicitIvBytes), out) > 0 &&
        !GcmAad(state, ctx->buf, static_cast<std::size_t>(state.tlsAadLength))) {
        in += TlsExplicitIvBytes;
        out += TlsExplicitIvBytes;
        length -= TlsExplicitIvBytes + TlsTagBytes;
        if (ctx->encrypt) {
            if (!GcmCrypt(state, in, out, length, true)) {
                GcmTag(state, out + length, TlsTagBytes);
                result = static_cast<int>(length + TlsExplicitIvBytes + TlsTagBytes);
            }
        } else if (!GcmCrypt(state, in, out, length, false)) {
            GcmTag(state, ctx->buf, TlsTagBytes);
            if (TimingSafeCompare(ctx->buf, in + length, TlsTagBytes)) Wipe(out, length);
            else result = static_cast<int>(length);
        }
    }
    state.ivSet = 0;
    state.tlsAadLength = -1;
    return result;
}

int APS5_VABI AesGcmCipher(LibreSslEvpCipherCtx* ctx, unsigned char* out, const unsigned char* in, std::size_t length) {
    auto& state = GcmStateOf(ctx, "aes_gcm_cipher");
    if (!state.keySet) return -1;
    if (state.tlsAadLength >= 0) return AesGcmTlsCipher(ctx, state, out, in, length);
    if (!state.ivSet) return -1;
    if (in) {
        const int failed = out ? GcmCrypt(state, in, out, length, ctx->encrypt) : GcmAad(state, in, length);
        return failed ? -1 : static_cast<int>(length);
    }
    if (!ctx->encrypt) {
        if (state.tagLength < 0) return -1;
        if (GcmFinish(state, ctx->buf, static_cast<std::size_t>(state.tagLength))) return -1;
        state.ivSet = 0;
        return 0;
    }
    GcmTag(state, ctx->buf, AesBlockBytes);
    state.tagLength = static_cast<int>(AesBlockBytes);
    state.ivSet = 0;
    return 0;
}

constexpr LibreSslEvpCipher AesGcm(int nid, int keyLength) {
    LibreSslEvpCipher cipher {};
    cipher.nid = nid;
    cipher.blockSize = 1;
    cipher.keyLength = keyLength;
    cipher.ivLength = 12;
    cipher.flags = GcmFlags;
    cipher.init = AesGcmInitKey;
    cipher.doCipher = AesGcmCipher;
    cipher.cleanup = AesGcmCleanup;
    cipher.contextSize = AesGcmContextBytes;
    cipher.control = AesGcmControl;
    return cipher;
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
constexpr LibreSslEvpCipher Aes128Gcm = AesGcm(895, 16);
constexpr LibreSslEvpCipher Aes192Gcm = AesGcm(898, 24);
constexpr LibreSslEvpCipher Aes256Gcm = AesGcm(901, 32);

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

const LibreSslEvpCipher* APS5_VABI EVP_aes_128_gcm_nid_postfix(void) {
    return &Aes128Gcm;
}

const LibreSslEvpCipher* APS5_VABI EVP_aes_192_gcm_nid_postfix(void) {
    return &Aes192Gcm;
}

const LibreSslEvpCipher* APS5_VABI EVP_aes_256_gcm_nid_postfix(void) {
    return &Aes256Gcm;
}

}
