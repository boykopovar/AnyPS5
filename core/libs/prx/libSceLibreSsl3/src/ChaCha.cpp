#include "prx/libSceLibreSsl3/include/ChaCha.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

#include <mbedtls/poly1305.h>

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr int ChaCha20Nid = 937;
constexpr int ChaCha20Poly1305Nid = 967;
constexpr std::uint64_t CipherFlagDefaultAsn1 = 0x1000;
constexpr std::uint64_t CipherFlagAead = 0x200000;
constexpr std::uint64_t CipherFlagCustomIvLength = 0x400000;
constexpr int ControlSetIvLength = 0x9;
constexpr int ControlGetTag = 0x10;
constexpr int ControlSetTag = 0x11;
constexpr int ControlSetIvFixed = 0x12;
constexpr int ControlGetIvLength = 0x25;
constexpr std::size_t KeyBytes = 32;
constexpr std::size_t BlockBytes = 64;
constexpr std::size_t CounterBytes = 8;
constexpr std::size_t NonceConstantBytes = 4;
constexpr std::size_t NonceBytes = 12;
constexpr std::size_t TagBytes = 16;
constexpr int ChaCha20Poly1305ContextBytes = 384;
constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int TooLarge = 164;

constexpr unsigned char Sigma[16] = {'e', 'x', 'p', 'a', 'n', 'd', ' ', '3', '2', '-', 'b', 'y', 't', 'e', ' ', 'k'};
constexpr unsigned char Tau[16] = {'e', 'x', 'p', 'a', 'n', 'd', ' ', '1', '6', '-', 'b', 'y', 't', 'e', ' ', 'k'};

struct ChaChaState {
    std::uint32_t input[16];
    unsigned char keystream[BlockBytes];
    unsigned char unused;
};

static_assert(sizeof(ChaChaState) == 132);

struct ChaCha20Poly1305State {
    ChaChaState chacha;
    mbedtls_poly1305_context poly1305;
    unsigned char key[KeyBytes];
    unsigned char nonce[NonceBytes];
    std::size_t nonceLength;
    unsigned char tag[TagBytes];
    std::size_t tagLength;
    std::size_t aadLength;
    std::size_t textLength;
    int inAad;
    int started;
};

static_assert(sizeof(ChaCha20Poly1305State) <= ChaCha20Poly1305ContextBytes);

void Wipe(void* data, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(data);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
}

std::uint32_t Load(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8 | static_cast<std::uint32_t>(bytes[2]) << 16 |
           static_cast<std::uint32_t>(bytes[3]) << 24;
}

void Store(unsigned char* bytes, std::uint32_t value) {
    bytes[0] = static_cast<unsigned char>(value);
    bytes[1] = static_cast<unsigned char>(value >> 8);
    bytes[2] = static_cast<unsigned char>(value >> 16);
    bytes[3] = static_cast<unsigned char>(value >> 24);
}

std::uint32_t Rotate(std::uint32_t value, int bits) {
    return value << bits | value >> (32 - bits);
}

void QuarterRound(std::uint32_t* x, int a, int b, int c, int d) {
    x[a] += x[b];
    x[d] = Rotate(x[d] ^ x[a], 16);
    x[c] += x[d];
    x[b] = Rotate(x[b] ^ x[c], 12);
    x[a] += x[b];
    x[d] = Rotate(x[d] ^ x[a], 8);
    x[c] += x[d];
    x[b] = Rotate(x[b] ^ x[c], 7);
}

void SetKey(ChaChaState& state, const unsigned char* key, int bits) {
    const unsigned char* constants = Tau;
    for (int word = 0; word < 4; ++word) state.input[4 + word] = Load(key + 4 * word);
    if (bits == 256) {
        key += 16;
        constants = Sigma;
    }
    for (int word = 0; word < 4; ++word) state.input[8 + word] = Load(key + 4 * word);
    for (int word = 0; word < 4; ++word) state.input[word] = Load(constants + 4 * word);
    state.unused = 0;
}

void SetIv(ChaChaState& state, const unsigned char* iv, const unsigned char* counter) {
    state.input[12] = counter ? Load(counter) : 0;
    state.input[13] = counter ? Load(counter + 4) : 0;
    state.input[14] = Load(iv);
    state.input[15] = Load(iv + 4);
    state.unused = 0;
}

void NextBlock(ChaChaState& state, unsigned char* keystream) {
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
    for (int word = 0; word < 16; ++word) Store(keystream + 4 * word, x[word] + state.input[word]);
    if (++state.input[12] == 0) ++state.input[13];
}

void Crypt(ChaChaState& state, unsigned char* out, const unsigned char* in, std::size_t length, const char* function) {
    if (length && (!in || !out)) throw std::invalid_argument(std::string(function) + ": null input or output");
    if (state.unused > BlockBytes) throw std::invalid_argument(std::string(function) + ": keystream position is outside the block");
    if (state.unused > 0) {
        const unsigned char* keystream = state.keystream + BlockBytes - state.unused;
        const std::size_t count = length < state.unused ? length : state.unused;
        for (std::size_t index = 0; index < count; ++index) out[index] = in[index] ^ keystream[index];
        state.unused = static_cast<unsigned char>(state.unused - count);
        in += count;
        out += count;
        length -= count;
    }
    while (length > 0) {
        unsigned char keystream[BlockBytes];
        NextBlock(state, keystream);
        const std::size_t count = length < BlockBytes ? length : BlockBytes;
        for (std::size_t index = 0; index < count; ++index) out[index] = in[index] ^ keystream[index];
        if (count < BlockBytes) {
            std::memcpy(state.keystream, keystream, BlockBytes);
            state.unused = static_cast<unsigned char>(BlockBytes - count);
        }
        Wipe(keystream, sizeof(keystream));
        in += count;
        out += count;
        length -= count;
    }
}

template <typename TState>
TState& StateOf(LibreSslEvpCipherCtx* ctx, const char* function) {
    if (!ctx) throw std::invalid_argument(std::string(function) + ": null context");
    if (!ctx->cipherData) throw std::invalid_argument(std::string(function) + ": context without cipher data");
    return *static_cast<TState*>(ctx->cipherData);
}

int APS5_VABI ChaCha20Init(LibreSslEvpCipherCtx* ctx, const unsigned char* key, const unsigned char* iv, int) {
    auto& state = StateOf<ChaChaState>(ctx, "chacha_init");
    if (key) SetKey(state, key, ctx->keyLen * 8);
    if (iv) SetIv(state, iv + CounterBytes, iv);
    return 1;
}

int APS5_VABI ChaCha20Cipher(LibreSslEvpCipherCtx* ctx, unsigned char* out, const unsigned char* in, std::size_t length) {
    Crypt(StateOf<ChaChaState>(ctx, "chacha_cipher"), out, in, length, "chacha_cipher");
    return 1;
}

void PolyUpdate(ChaCha20Poly1305State& state, const unsigned char* data, std::size_t length) {
    if (length && !data) throw std::invalid_argument("chacha20_poly1305_cipher: null input");
    if (mbedtls_poly1305_update(&state.poly1305, data, length) != 0) throw std::runtime_error("chacha20_poly1305_cipher: Poly1305 update failed");
}

void PolyPad16(ChaCha20Poly1305State& state, std::size_t length) {
    static constexpr unsigned char Zero[16] = {};
    const std::size_t remainder = length % 16;
    if (remainder) PolyUpdate(state, Zero, 16 - remainder);
}

void PolyLength(ChaCha20Poly1305State& state, std::size_t length) {
    unsigned char bytes[8];
    std::uint64_t value = length;
    for (auto& byte : bytes) {
        byte = static_cast<unsigned char>(value);
        value >>= 8;
    }
    PolyUpdate(state, bytes, sizeof(bytes));
}

bool SameTag(const unsigned char* left, const unsigned char* right, std::size_t length) {
    unsigned char difference = 0;
    for (std::size_t index = 0; index < length; ++index) difference |= left[index] ^ right[index];
    return difference == 0;
}

int APS5_VABI ChaCha20Poly1305Init(LibreSslEvpCipherCtx* ctx, const unsigned char* key, const unsigned char* iv, int) {
    auto& state = StateOf<ChaCha20Poly1305State>(ctx, "chacha20_poly1305_init");
    if (!key && !iv) return 1;
    state.started = 0;
    if (key) std::memcpy(state.key, key, KeyBytes);
    if (iv) {
        if (state.nonceLength == 0 || state.nonceLength > NonceBytes)
            throw std::invalid_argument("chacha20_poly1305_init: nonce length " + std::to_string(state.nonceLength) + " is outside 1 to 12");
        const std::size_t padding = NonceBytes - state.nonceLength;
        std::memset(state.nonce, 0, padding);
        std::memcpy(state.nonce + padding, iv, state.nonceLength);
    }
    return 1;
}

void Start(ChaCha20Poly1305State& state) {
    const std::uint64_t counter = static_cast<std::uint64_t>(Load(state.nonce)) << 32;
    SetKey(state.chacha, state.key, 256);
    SetIv(state.chacha, state.nonce + NonceConstantBytes, nullptr);
    state.chacha.input[12] = static_cast<std::uint32_t>(counter);
    state.chacha.input[13] = static_cast<std::uint32_t>(counter >> 32);
    unsigned char polyKey[KeyBytes] = {};
    Crypt(state.chacha, polyKey, polyKey, sizeof(polyKey), "chacha20_poly1305_cipher");
    mbedtls_poly1305_init(&state.poly1305);
    const int started = mbedtls_poly1305_starts(&state.poly1305, polyKey);
    Wipe(polyKey, sizeof(polyKey));
    if (started != 0) throw std::runtime_error("chacha20_poly1305_cipher: Poly1305 start failed");
    state.chacha.unused = 0;
    state.aadLength = 0;
    state.textLength = 0;
    state.inAad = 0;
    state.started = 1;
}

int APS5_VABI ChaCha20Poly1305Cipher(LibreSslEvpCipherCtx* ctx, unsigned char* out, const unsigned char* in, std::size_t length) {
    auto& state = StateOf<ChaCha20Poly1305State>(ctx, "chacha20_poly1305_cipher");
    if (in && (length == 0 || length > static_cast<std::size_t>(INT_MAX))) return length == 0 ? 0 : -1;
    if (!state.started) Start(state);
    if (length > std::numeric_limits<std::size_t>::max() - state.textLength) {
        LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, TooLarge, __FILE__, __LINE__);
        return -1;
    }
    if (state.textLength > 0 && in && !out) return -1;
    if (state.inAad && (!in || out)) {
        PolyPad16(state, state.aadLength);
        state.inAad = 0;
    }
    if (in) {
        if (!ctx->encrypt || !out) PolyUpdate(state, in, length);
        if (!out) {
            state.aadLength += length;
            state.inAad = 1;
        } else {
            Crypt(state.chacha, out, in, length, "chacha20_poly1305_cipher");
            state.textLength += length;
        }
        if (ctx->encrypt && out) PolyUpdate(state, out, length);
        return static_cast<int>(length);
    }
    PolyPad16(state, state.textLength);
    PolyLength(state, state.aadLength);
    PolyLength(state, state.textLength);
    if (ctx->encrypt) {
        if (mbedtls_poly1305_finish(&state.poly1305, state.tag) != 0) throw std::runtime_error("chacha20_poly1305_cipher: Poly1305 finish failed");
        mbedtls_poly1305_init(&state.poly1305);
        state.tagLength = TagBytes;
    } else {
        if (state.tagLength == 0) return -1;
        unsigned char tag[TagBytes];
        if (mbedtls_poly1305_finish(&state.poly1305, tag) != 0) throw std::runtime_error("chacha20_poly1305_cipher: Poly1305 finish failed");
        mbedtls_poly1305_init(&state.poly1305);
        const bool same = SameTag(tag, state.tag, state.tagLength);
        Wipe(tag, sizeof(tag));
        if (!same) return -1;
    }
    state.started = 0;
    return static_cast<int>(length);
}

int APS5_VABI ChaCha20Poly1305Cleanup(LibreSslEvpCipherCtx* ctx) {
    auto& state = StateOf<ChaCha20Poly1305State>(ctx, "chacha20_poly1305_cleanup");
    Wipe(&state, sizeof(state));
    return 1;
}

int APS5_VABI ChaCha20Poly1305Control(LibreSslEvpCipherCtx* ctx, int type, int arg, void* pointer) {
    auto& state = StateOf<ChaCha20Poly1305State>(ctx, "chacha20_poly1305_ctrl");
    switch (type) {
    case CipherControlInit:
        std::memset(&state, 0, sizeof(state));
        state.nonceLength = NonceBytes;
        return 1;
    case ControlGetIvLength:
        if (state.nonceLength > static_cast<std::size_t>(INT_MAX)) return 0;
        if (!pointer) throw std::invalid_argument("chacha20_poly1305_ctrl: null IV length output");
        *static_cast<int*>(pointer) = static_cast<int>(state.nonceLength);
        return 1;
    case ControlSetIvLength:
        if (arg <= 0 || static_cast<std::size_t>(arg) > NonceBytes) return 0;
        state.nonceLength = static_cast<std::size_t>(arg);
        return 1;
    case ControlSetTag:
        if (ctx->encrypt) return 0;
        if (arg <= 0 || static_cast<std::size_t>(arg) > TagBytes) return 0;
        if (pointer) {
            std::memcpy(state.tag, pointer, static_cast<std::size_t>(arg));
            state.tagLength = static_cast<std::size_t>(arg);
        }
        return 1;
    case ControlGetTag:
        if (!ctx->encrypt) return 0;
        if (arg <= 0 || static_cast<std::size_t>(arg) > state.tagLength) return 0;
        if (!pointer) throw std::invalid_argument("chacha20_poly1305_ctrl: null tag output");
        std::memcpy(pointer, state.tag, static_cast<std::size_t>(arg));
        return 1;
    case ControlSetIvFixed:
        if (arg != static_cast<int>(NonceBytes)) return 0;
        if (!pointer) throw std::invalid_argument("chacha20_poly1305_ctrl: null fixed IV");
        std::memcpy(state.nonce, pointer, NonceBytes);
        return 1;
    default:
        return 0;
    }
}

constexpr LibreSslEvpCipher ChaCha20() {
    LibreSslEvpCipher cipher {};
    cipher.nid = ChaCha20Nid;
    cipher.blockSize = 1;
    cipher.keyLength = static_cast<int>(KeyBytes);
    cipher.ivLength = static_cast<int>(CounterBytes + 8);
    cipher.flags = CipherModeStream | CipherFlagAlwaysCallInit | CipherFlagCustomIv;
    cipher.init = ChaCha20Init;
    cipher.doCipher = ChaCha20Cipher;
    cipher.contextSize = static_cast<int>(sizeof(ChaChaState));
    return cipher;
}

constexpr LibreSslEvpCipher ChaCha20Poly1305() {
    LibreSslEvpCipher cipher {};
    cipher.nid = ChaCha20Poly1305Nid;
    cipher.blockSize = 1;
    cipher.keyLength = static_cast<int>(KeyBytes);
    cipher.ivLength = static_cast<int>(NonceBytes);
    cipher.flags = CipherFlagAlwaysCallInit | CipherFlagCtrlInit | CipherFlagCustomIv | CipherFlagCustomIvLength | CipherFlagAead |
                   CipherFlagCustomCipher | CipherFlagDefaultAsn1;
    cipher.init = ChaCha20Poly1305Init;
    cipher.doCipher = ChaCha20Poly1305Cipher;
    cipher.cleanup = ChaCha20Poly1305Cleanup;
    cipher.contextSize = ChaCha20Poly1305ContextBytes;
    cipher.control = ChaCha20Poly1305Control;
    return cipher;
}

constexpr LibreSslEvpCipher ChaCha20Object = ChaCha20();
constexpr LibreSslEvpCipher ChaCha20Poly1305Object = ChaCha20Poly1305();

} // namespace

extern "C" {

const LibreSslEvpCipher* APS5_VABI EVP_chacha20_nid_postfix() {
    return &ChaCha20Object;
}

const LibreSslEvpCipher* APS5_VABI EVP_chacha20_poly1305_nid_postfix() {
    return &ChaCha20Poly1305Object;
}

}
