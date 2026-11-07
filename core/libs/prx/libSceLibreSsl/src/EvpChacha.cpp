#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libSceLibreSsl/include/Evp.hpp"

namespace {

constexpr std::size_t ChaChaBlockBytes = 64;

struct ChaChaState {
    std::uint32_t input[16];
    unsigned char keystream[ChaChaBlockBytes];
    unsigned char unused;
};

std::uint32_t LoadLittle32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8 | static_cast<std::uint32_t>(bytes[2]) << 16 |
        static_cast<std::uint32_t>(bytes[3]) << 24;
}

void KeySetup(ChaChaState& state, const unsigned char* key) {
    state.input[0] = 0x61707865;
    state.input[1] = 0x3320646e;
    state.input[2] = 0x79622d32;
    state.input[3] = 0x6b206574;
    for (int index = 0; index < 8; ++index) state.input[4 + index] = LoadLittle32(key + 4 * index);
    state.unused = 0;
}

void IvSetup(ChaChaState& state, const unsigned char* iv) {
    for (int index = 0; index < 4; ++index) state.input[12 + index] = LoadLittle32(iv + 4 * index);
    state.unused = 0;
}

void StoreLittle32(unsigned char* bytes, std::uint32_t value) {
    for (int index = 0; index < 4; ++index, value >>= 8) bytes[index] = static_cast<unsigned char>(value);
}

std::uint32_t RotateLeft(std::uint32_t value, int count) {
    return value << count | value >> (32 - count);
}

void QuarterRound(std::uint32_t (&x)[16], int a, int b, int c, int d) {
    x[a] += x[b];
    x[d] = RotateLeft(x[d] ^ x[a], 16);
    x[c] += x[d];
    x[b] = RotateLeft(x[b] ^ x[c], 12);
    x[a] += x[b];
    x[d] = RotateLeft(x[d] ^ x[a], 8);
    x[c] += x[d];
    x[b] = RotateLeft(x[b] ^ x[c], 7);
}

void NextBlock(ChaChaState& state, unsigned char (&block)[ChaChaBlockBytes]) {
    std::uint32_t x[16];
    std::memcpy(x, state.input, sizeof(x));
    for (int round = 0; round < 10; ++round) {
        QuarterRound(x, 0, 4, 8, 12);
        QuarterRound(x, 1, 5, 9, 13);
        QuarterRound(x, 2, 6, 10, 14);
        QuarterRound(x, 3, 7, 11, 15);
        QuarterRound(x, 0, 5, 10, 15);
        QuarterRound(x, 1, 6, 11, 12);
        QuarterRound(x, 2, 7, 8, 13);
        QuarterRound(x, 3, 4, 9, 14);
    }
    for (int index = 0; index < 16; ++index) StoreLittle32(block + 4 * index, x[index] + state.input[index]);
    if (++state.input[12] == 0) ++state.input[13];
}

void Crypt(ChaChaState& state, unsigned char* out, const unsigned char* in, std::size_t length) {
    std::size_t offset = 0;
    while (state.unused > 0 && offset < length) {
        out[offset] = in[offset] ^ state.keystream[ChaChaBlockBytes - state.unused];
        --state.unused;
        ++offset;
    }
    while (offset < length) {
        unsigned char block[ChaChaBlockBytes];
        NextBlock(state, block);
        const std::size_t count = length - offset < ChaChaBlockBytes ? length - offset : ChaChaBlockBytes;
        for (std::size_t index = 0; index < count; ++index) out[offset + index] = in[offset + index] ^ block[index];
        if (count < ChaChaBlockBytes) {
            std::memcpy(state.keystream, block, ChaChaBlockBytes);
            state.unused = static_cast<unsigned char>(ChaChaBlockBytes - count);
        }
        offset += count;
    }
}

ChaChaState& StateOf(LibreSslEvpCipherCtx* ctx, const char* function) {
    if (!ctx->cipherData) throw std::invalid_argument(std::string(function) + ": context without cipher data");
    return *static_cast<ChaChaState*>(ctx->cipherData);
}

int APS5_VABI ChaChaInit(LibreSslEvpCipherCtx* ctx, const unsigned char* key, const unsigned char* iv, int) {
    auto& state = StateOf(ctx, "chacha20_init");
    if (key) KeySetup(state, key);
    if (iv) IvSetup(state, iv);
    return 1;
}

int APS5_VABI ChaChaCipher(LibreSslEvpCipherCtx* ctx, unsigned char* out, const unsigned char* in, std::size_t length) {
    auto& state = StateOf(ctx, "chacha20_cipher");
    if (length && (!in || !out)) throw std::invalid_argument("chacha20_cipher: null input or output");
    Crypt(state, out, in, length);
    return 1;
}

constexpr LibreSslEvpCipher ChaCha20 {
    .nid = 937,
    .blockSize = 1,
    .keyLength = 32,
    .ivLength = 16,
    .flags = CipherModeStream | CipherFlagCustomIv | CipherFlagAlwaysCallInit,
    .init = ChaChaInit,
    .doCipher = ChaChaCipher,
    .contextSize = static_cast<int>(sizeof(ChaChaState)),
};

}

extern "C" {

const LibreSslEvpCipher* APS5_VABI EVP_chacha20_nid_postfix(void) {
    return &ChaCha20;
}

}
