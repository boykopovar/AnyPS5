#include "prx/libSceLibreSsl3/include/AesCcm.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr int Aes128CcmNid = 896;
constexpr std::uint64_t CipherModeCcm = 0x7;
constexpr std::uint64_t CipherFlagCustomCopy = 0x400;
constexpr std::uint64_t CipherFlagDefaultAsn1 = 0x1000;
constexpr std::uint64_t CipherFlagCustomIvLength = 0x400000;
constexpr std::uint64_t CcmFlags = CipherFlagDefaultAsn1 | CipherFlagCustomIv | CipherFlagCustomIvLength | CipherFlagCustomCipher |
    CipherFlagAlwaysCallInit | CipherFlagCtrlInit | CipherFlagCustomCopy | CipherModeCcm;
constexpr int ControlCopy = 0x8;
constexpr int ControlSetIvLength = 0x9;
constexpr int ControlGetTag = 0x10;
constexpr int ControlSetTag = 0x11;
constexpr int ControlSetL = 0x14;
constexpr int ControlGetIvLength = 0x25;
constexpr int AesCcmContextBytes = 336;
constexpr std::size_t BlockBytes = 16;
constexpr unsigned char AdataFlag = 0x40;

struct Ccm128 {
    unsigned char nonce[BlockBytes];
    unsigned char mac[BlockBytes];
    std::uint64_t blocks;
    const AesKey* key;
};

struct CcmState {
    AesKey schedule;
    int keySet;
    int ivSet;
    int tagSet;
    int lengthSet;
    int l;
    int m;
    Ccm128 ccm;
};

static_assert(sizeof(CcmState) <= AesCcmContextBytes);

void Wipe(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
}

int TimingSafeCompare(const unsigned char* first, const unsigned char* second, std::size_t size) {
    unsigned difference = 0;
    for (std::size_t index = 0; index < size; ++index) difference |= first[index] ^ second[index];
    return static_cast<int>(difference);
}

void Block(const Ccm128& ccm, const unsigned char* in, unsigned char* out) {
    if (!ccm.key) throw std::invalid_argument("CRYPTO_ccm128: no key was set, and LibreSSL calls a null block function");
    AES_encrypt_nid_postfix(in, out, ccm.key);
}

void Increment64(unsigned char* counter) {
    for (int index = 15; index >= 8; --index)
        if (++counter[index]) return;
}

std::size_t ReadLength(Ccm128& ccm, unsigned l) {
    std::size_t length = 0;
    for (unsigned index = 15 - l; index < 15; ++index) {
        length |= ccm.nonce[index];
        ccm.nonce[index] = 0;
        length <<= 8;
    }
    length |= ccm.nonce[15];
    ccm.nonce[15] = 1;
    return length;
}

void FinishMac(Ccm128& ccm, unsigned l, unsigned char flags) {
    unsigned char scratch[BlockBytes];
    for (unsigned index = 15 - l; index < BlockBytes; ++index) ccm.nonce[index] = 0;
    Block(ccm, ccm.nonce, scratch);
    for (std::size_t index = 0; index < BlockBytes; ++index) ccm.mac[index] ^= scratch[index];
    ccm.nonce[0] = flags;
}

void CcmInit(Ccm128& ccm, unsigned m, unsigned l, const AesKey* key) {
    std::memset(ccm.nonce, 0, sizeof(ccm.nonce));
    ccm.nonce[0] = static_cast<unsigned char>(((l - 1) & 7) | (((m - 2) / 2) & 7) << 3);
    ccm.blocks = 0;
    ccm.key = key;
}

int CcmSetIv(Ccm128& ccm, const unsigned char* nonce, std::size_t nonceLength, std::size_t messageLength) {
    const unsigned l = ccm.nonce[0] & 7;
    if (nonceLength < 14 - l) return -1;
    if (l >= 3) {
        for (int index = 0; index < 4; ++index) ccm.nonce[8 + index] = static_cast<unsigned char>(static_cast<std::uint64_t>(messageLength) >> (56 - 8 * index));
    } else {
        std::memset(ccm.nonce + 8, 0, 8);
    }
    for (int index = 0; index < 4; ++index) ccm.nonce[12 + index] = static_cast<unsigned char>(messageLength >> (24 - 8 * index));
    ccm.nonce[0] &= static_cast<unsigned char>(~AdataFlag);
    std::memcpy(ccm.nonce + 1, nonce, 14 - l);
    return 0;
}

void CcmAad(Ccm128& ccm, const unsigned char* aad, std::size_t length) {
    if (!length) return;
    ccm.nonce[0] |= AdataFlag;
    Block(ccm, ccm.nonce, ccm.mac);
    ++ccm.blocks;
    unsigned index;
    const auto wide = static_cast<std::uint64_t>(length);
    if (length < 0x10000 - 0x100) {
        ccm.mac[0] ^= static_cast<unsigned char>(length >> 8);
        ccm.mac[1] ^= static_cast<unsigned char>(length);
        index = 2;
    } else if (wide >= std::uint64_t {1} << 32) {
        ccm.mac[0] ^= 0xFF;
        ccm.mac[1] ^= 0xFF;
        for (int byte = 0; byte < 8; ++byte) ccm.mac[2 + byte] ^= static_cast<unsigned char>(wide >> (56 - 8 * byte));
        index = 10;
    } else {
        ccm.mac[0] ^= 0xFF;
        ccm.mac[1] ^= 0xFE;
        for (int byte = 0; byte < 4; ++byte) ccm.mac[2 + byte] ^= static_cast<unsigned char>(wide >> (24 - 8 * byte));
        index = 6;
    }
    do {
        for (; index < BlockBytes && length; ++index, ++aad, --length) ccm.mac[index] ^= *aad;
        Block(ccm, ccm.mac, ccm.mac);
        ++ccm.blocks;
        index = 0;
    } while (length);
}

int CcmEncrypt(Ccm128& ccm, const unsigned char* in, unsigned char* out, std::size_t length) {
    const unsigned char flags = ccm.nonce[0];
    if (!(flags & AdataFlag)) {
        Block(ccm, ccm.nonce, ccm.mac);
        ++ccm.blocks;
    }
    const unsigned l = flags & 7;
    ccm.nonce[0] = static_cast<unsigned char>(l);
    if (ReadLength(ccm, l) != length) return -1;
    ccm.blocks += ((static_cast<std::uint64_t>(length) + 15) >> 3) | 1;
    if (ccm.blocks > std::uint64_t {1} << 61) return -2;
    unsigned char scratch[BlockBytes];
    for (; length >= BlockBytes; in += BlockBytes, out += BlockBytes, length -= BlockBytes) {
        for (std::size_t index = 0; index < BlockBytes; ++index) ccm.mac[index] ^= in[index];
        Block(ccm, ccm.mac, ccm.mac);
        Block(ccm, ccm.nonce, scratch);
        Increment64(ccm.nonce);
        for (std::size_t index = 0; index < BlockBytes; ++index) out[index] = scratch[index] ^ in[index];
    }
    if (length) {
        for (std::size_t index = 0; index < length; ++index) ccm.mac[index] ^= in[index];
        Block(ccm, ccm.mac, ccm.mac);
        Block(ccm, ccm.nonce, scratch);
        for (std::size_t index = 0; index < length; ++index) out[index] = scratch[index] ^ in[index];
    }
    FinishMac(ccm, l, flags);
    return 0;
}

int CcmDecrypt(Ccm128& ccm, const unsigned char* in, unsigned char* out, std::size_t length) {
    const unsigned char flags = ccm.nonce[0];
    if (!(flags & AdataFlag)) Block(ccm, ccm.nonce, ccm.mac);
    const unsigned l = flags & 7;
    ccm.nonce[0] = static_cast<unsigned char>(l);
    if (ReadLength(ccm, l) != length) return -1;
    unsigned char scratch[BlockBytes];
    for (; length >= BlockBytes; in += BlockBytes, out += BlockBytes, length -= BlockBytes) {
        Block(ccm, ccm.nonce, scratch);
        Increment64(ccm.nonce);
        for (std::size_t index = 0; index < BlockBytes; ++index) {
            scratch[index] ^= in[index];
            out[index] = scratch[index];
            ccm.mac[index] ^= scratch[index];
        }
        Block(ccm, ccm.mac, ccm.mac);
    }
    if (length) {
        Block(ccm, ccm.nonce, scratch);
        for (std::size_t index = 0; index < length; ++index) {
            out[index] = scratch[index] ^ in[index];
            ccm.mac[index] ^= out[index];
        }
        Block(ccm, ccm.mac, ccm.mac);
    }
    FinishMac(ccm, l, flags);
    return 0;
}

std::size_t CcmTag(const Ccm128& ccm, unsigned char* tag, std::size_t length) {
    const std::size_t m = ((ccm.nonce[0] >> 3) & 7) * 2 + 2;
    if (length != m) return 0;
    if (!tag) throw std::invalid_argument("CRYPTO_ccm128_tag: null tag");
    std::memcpy(tag, ccm.mac, m);
    return m;
}

CcmState& StateOf(LibreSslEvpCipherCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    if (!ctx->cipherData) throw std::invalid_argument(std::string(function) + ": context without cipher data");
    return *static_cast<CcmState*>(ctx->cipherData);
}

std::size_t NonceBytes(const CcmState& state) {
    return static_cast<std::size_t>(15 - state.l);
}

int APS5_VABI AesCcmControl(LibreSslEvpCipherCtx* ctx, int type, int arg, void* pointer) {
    auto& state = StateOf(ctx, "aes_ccm_ctrl");
    switch (type) {
    case CipherControlInit:
        state.keySet = 0;
        state.ivSet = 0;
        state.l = 8;
        state.m = 12;
        state.tagSet = 0;
        state.lengthSet = 0;
        return 1;
    case ControlGetIvLength:
        if (!pointer) throw std::invalid_argument("aes_ccm_ctrl: null IV length output");
        *static_cast<int*>(pointer) = 15 - state.l;
        return 1;
    case ControlSetIvLength:
    case ControlSetL: {
        const long long l = type == ControlSetIvLength ? 15LL - arg : arg;
        if (l < 2 || l > 8) return 0;
        state.l = static_cast<int>(l);
        return 1;
    }
    case ControlSetTag:
        if ((arg & 1) || arg < 4 || arg > 16) return 0;
        if ((ctx->encrypt && pointer) || (!ctx->encrypt && !pointer)) return 0;
        if (pointer) {
            state.tagSet = 1;
            std::memcpy(ctx->buf, pointer, static_cast<std::size_t>(arg));
        }
        state.m = arg;
        return 1;
    case ControlGetTag:
        if (!ctx->encrypt || !state.tagSet) return 0;
        if (!CcmTag(state.ccm, static_cast<unsigned char*>(pointer), static_cast<std::size_t>(arg))) return 0;
        state.tagSet = 0;
        state.ivSet = 0;
        state.lengthSet = 0;
        return 1;
    case ControlCopy: {
        auto* out = static_cast<LibreSslEvpCipherCtx*>(pointer);
        if (!out) throw std::invalid_argument("aes_ccm_ctrl: null copy destination");
        if (!state.ccm.key) return 1;
        if (state.ccm.key != &state.schedule) return 0;
        auto& copy = StateOf(out, "aes_ccm_ctrl");
        copy.ccm.key = &copy.schedule;
        return 1;
    }
    default:
        return -1;
    }
}

int APS5_VABI AesCcmInitKey(LibreSslEvpCipherCtx* ctx, const unsigned char* key, const unsigned char* iv, int) {
    auto& state = StateOf(ctx, "aes_ccm_init_key");
    if (!iv && !key) return 1;
    if (key) {
        if (AES_set_encrypt_key_nid_postfix(key, ctx->keyLen * 8, &state.schedule) < 0)
            throw std::invalid_argument("aes_ccm_init_key: key length of " + std::to_string(ctx->keyLen) + " bytes");
        CcmInit(state.ccm, static_cast<unsigned>(state.m), static_cast<unsigned>(state.l), &state.schedule);
        state.keySet = 1;
    }
    if (iv) {
        std::memcpy(ctx->iv, iv, NonceBytes(state));
        state.ivSet = 1;
    }
    return 1;
}

int APS5_VABI AesCcmCipher(LibreSslEvpCipherCtx* ctx, unsigned char* out, const unsigned char* in, std::size_t length) {
    auto& state = StateOf(ctx, "aes_ccm_cipher");
    if (length > INT_MAX) return -1;
    if (!state.ivSet && !state.keySet) return -1;
    if (!ctx->encrypt && !state.tagSet) return -1;
    const int done = static_cast<int>(length);
    if (!out) {
        if (!in) {
            if (CcmSetIv(state.ccm, ctx->iv, NonceBytes(state), length)) return -1;
            state.lengthSet = 1;
            return done;
        }
        if (!state.lengthSet && length) return -1;
        CcmAad(state.ccm, in, length);
        return done;
    }
    if (!in) return 0;
    if (!state.lengthSet) {
        if (CcmSetIv(state.ccm, ctx->iv, NonceBytes(state), length)) return -1;
        state.lengthSet = 1;
    }
    if (ctx->encrypt) {
        if (CcmEncrypt(state.ccm, in, out, length)) return -1;
        state.tagSet = 1;
        return done;
    }
    int result = -1;
    if (!CcmDecrypt(state.ccm, in, out, length)) {
        unsigned char tag[BlockBytes];
        const auto tagLength = static_cast<std::size_t>(state.m);
        if (CcmTag(state.ccm, tag, tagLength) && !TimingSafeCompare(tag, ctx->buf, tagLength)) result = done;
    }
    if (result == -1) Wipe(out, length);
    state.ivSet = 0;
    state.tagSet = 0;
    state.lengthSet = 0;
    return result;
}

constexpr LibreSslEvpCipher AesCcm(int nid, int keyLength) {
    LibreSslEvpCipher cipher {};
    cipher.nid = nid;
    cipher.blockSize = 1;
    cipher.keyLength = keyLength;
    cipher.ivLength = 12;
    cipher.flags = CcmFlags;
    cipher.init = AesCcmInitKey;
    cipher.doCipher = AesCcmCipher;
    cipher.contextSize = AesCcmContextBytes;
    cipher.control = AesCcmControl;
    return cipher;
}

constexpr LibreSslEvpCipher Aes128Ccm = AesCcm(Aes128CcmNid, 16);

}

extern "C" {

const LibreSslEvpCipher* APS5_VABI EVP_aes_128_ccm_nid_postfix() {
    return &Aes128Ccm;
}

}
