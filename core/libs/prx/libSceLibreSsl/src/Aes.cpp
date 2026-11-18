#include "prx/libSceLibreSsl/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::size_t BlockSize = 16;

static_assert(sizeof(AesKey) == 244);

constexpr std::uint8_t Multiply(std::uint8_t a, std::uint8_t b) {
    std::uint8_t product = 0;
    while (b) {
        if (b & 1) product ^= a;
        a = static_cast<std::uint8_t>((a << 1) ^ ((a & 0x80) ? 0x1b : 0));
        b >>= 1;
    }
    return product;
}

constexpr std::array<std::uint8_t, 256> MakeSubstitution() {
    std::array<std::uint8_t, 256> box {};
    for (int value = 0; value < 256; ++value) {
        const auto element = static_cast<std::uint8_t>(value);
        std::uint8_t inverse = element;
        for (int step = 0; step < 6; ++step) inverse = Multiply(Multiply(inverse, inverse), element);
        inverse = Multiply(inverse, inverse);
        std::uint8_t result = 0x63;
        for (int shift = 0; shift < 5; ++shift)
            result ^= static_cast<std::uint8_t>((inverse << shift) | (inverse >> ((8 - shift) & 7)));
        box[value] = result;
    }
    return box;
}

constexpr std::array<std::uint8_t, 256> MakeInverse(const std::array<std::uint8_t, 256>& box) {
    std::array<std::uint8_t, 256> inverse {};
    for (int value = 0; value < 256; ++value) inverse[box[value]] = static_cast<std::uint8_t>(value);
    return inverse;
}

constexpr auto Substitution = MakeSubstitution();
constexpr auto InverseSubstitution = MakeInverse(Substitution);
static_assert(Substitution[0x00] == 0x63 && Substitution[0x53] == 0xed && Substitution[0xff] == 0x16);

std::uint32_t LoadBig32(const unsigned char* p) {
    return std::uint32_t(p[0]) << 24 | std::uint32_t(p[1]) << 16 | std::uint32_t(p[2]) << 8 | std::uint32_t(p[3]);
}

void StoreBig32(unsigned char* p, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(value >> (24 - 8 * i));
}

std::uint32_t SubstituteWord(std::uint32_t word) {
    return std::uint32_t(Substitution[word >> 24]) << 24 | std::uint32_t(Substitution[(word >> 16) & 0xff]) << 16 |
        std::uint32_t(Substitution[(word >> 8) & 0xff]) << 8 | Substitution[word & 0xff];
}

std::uint32_t InverseMixWord(std::uint32_t word) {
    const std::uint8_t a[4] {std::uint8_t(word >> 24), std::uint8_t(word >> 16), std::uint8_t(word >> 8), std::uint8_t(word)};
    std::uint32_t result = 0;
    for (int row = 0; row < 4; ++row) {
        const std::uint8_t value = Multiply(a[row], 14) ^ Multiply(a[(row + 1) % 4], 11) ^ Multiply(a[(row + 2) % 4], 13) ^ Multiply(a[(row + 3) % 4], 9);
        result |= std::uint32_t(value) << (24 - 8 * row);
    }
    return result;
}

int RoundsForBits(int bits) {
    if (bits == 128) return 10;
    if (bits == 192) return 12;
    if (bits == 256) return 14;
    return 0;
}

void ExpandKey(const unsigned char* userKey, int bits, AesKey& key) {
    const int words = bits / 32;
    const int total = 4 * (key.rounds + 1);
    auto* w = key.roundKeys;
    for (int i = 0; i < words; ++i) w[i] = LoadBig32(userKey + 4 * i);
    std::uint8_t roundConstant = 1;
    for (int i = words; i < total; ++i) {
        std::uint32_t temp = w[i - 1];
        if (i % words == 0) {
            temp = SubstituteWord(temp << 8 | temp >> 24) ^ (std::uint32_t(roundConstant) << 24);
            roundConstant = Multiply(roundConstant, 2);
        } else if (words > 6 && i % words == 4) {
            temp = SubstituteWord(temp);
        }
        w[i] = w[i - words] ^ temp;
    }
}

void InvertKey(AesKey& key) {
    auto* w = key.roundKeys;
    for (int i = 0, j = 4 * key.rounds; i < j; i += 4, j -= 4)
        for (int k = 0; k < 4; ++k) std::swap(w[i + k], w[j + k]);
    for (int i = 4; i < 4 * key.rounds; ++i) w[i] = InverseMixWord(w[i]);
}

bool CopyKeyIn(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

bool CopyKeyOut(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

int SetKey(const unsigned char* userKey, int bits, AesKey* key, bool decrypt, const char* function) {
    if (!userKey || !key) return -1;
    const int rounds = RoundsForBits(bits);
    if (!rounds) return -2;
    std::array<unsigned char, 32> keyBytes {};
    if (!CopyKeyIn(keyBytes.data(), userKey, static_cast<std::size_t>(bits / 8)))
        throw std::invalid_argument(std::string(function) + ": unreadable user key");
    AesKey expanded;
    if (!CopyKeyIn(&expanded, key, sizeof(expanded))) throw std::invalid_argument(std::string(function) + ": unreadable key");
    expanded.rounds = rounds;
    ExpandKey(keyBytes.data(), bits, expanded);
    if (decrypt) InvertKey(expanded);
    if (!CopyKeyOut(key, &expanded, sizeof(expanded))) throw std::invalid_argument(std::string(function) + ": unwritable key");
    return 0;
}

const AesKey& RequireKey(const AesKey* key, const char* function) {
    if (!key) throw std::invalid_argument(std::string(function) + ": null key");
    if (key->rounds != 10 && key->rounds != 12 && key->rounds != 14)
        throw std::invalid_argument(std::string(function) + ": key schedule with " + std::to_string(key->rounds) + " rounds");
    return *key;
}

AesKey ReadKey(const AesKey* key, const char* function) {
    if (!key) throw std::invalid_argument(std::string(function) + ": null key");
    AesKey copy;
    if (!CopyKeyIn(&copy, key, sizeof(copy))) throw std::invalid_argument(std::string(function) + ": unreadable key");
    RequireKey(&copy, function);
    return copy;
}

void RequireBlocks(const void* in, const void* out, std::size_t length, const char* function) {
    if (length && (!in || !out)) throw std::invalid_argument(std::string(function) + ": null input or output");
}

constexpr std::size_t Chunk = 4096;

void ReadGuest(void* destination, const void* source, std::size_t bytes, const char* function, const char* name) {
    if (!CopyKeyIn(destination, source, bytes)) throw std::invalid_argument(std::string(function) + ": unreadable " + name);
}

void WriteGuest(void* destination, const void* source, std::size_t bytes, const char* function, const char* name) {
    if (!CopyKeyOut(destination, source, bytes)) throw std::invalid_argument(std::string(function) + ": unwritable " + name);
}

void WriteChanged(void* destination, const void* before, const void* after, std::size_t bytes, const char* function, const char* name) {
    if (std::memcmp(before, after, bytes)) WriteGuest(destination, after, bytes, function, name);
}

void LoadState(std::uint8_t (&state)[16], const unsigned char* in, const std::uint32_t* roundKey) {
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            state[4 * column + row] = in[4 * column + row] ^ std::uint8_t(roundKey[column] >> (24 - 8 * row));
}

void AddRoundKey(std::uint8_t (&state)[16], const std::uint32_t* roundKey) {
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row) state[4 * column + row] ^= std::uint8_t(roundKey[column] >> (24 - 8 * row));
}

void EncryptBlock(const unsigned char* in, unsigned char* out, const AesKey& key) {
    std::uint8_t state[16];
    LoadState(state, in, key.roundKeys);
    for (int round = 1; round <= key.rounds; ++round) {
        std::uint8_t shifted[16];
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row) shifted[4 * column + row] = Substitution[state[4 * ((column + row) % 4) + row]];
        if (round != key.rounds) {
            for (int column = 0; column < 4; ++column) {
                const std::uint8_t* a = shifted + 4 * column;
                const std::uint8_t mixed[4] {
                    std::uint8_t(Multiply(a[0], 2) ^ Multiply(a[1], 3) ^ a[2] ^ a[3]),
                    std::uint8_t(a[0] ^ Multiply(a[1], 2) ^ Multiply(a[2], 3) ^ a[3]),
                    std::uint8_t(a[0] ^ a[1] ^ Multiply(a[2], 2) ^ Multiply(a[3], 3)),
                    std::uint8_t(Multiply(a[0], 3) ^ a[1] ^ a[2] ^ Multiply(a[3], 2)),
                };
                std::memcpy(state + 4 * column, mixed, 4);
            }
        } else {
            std::memcpy(state, shifted, sizeof(state));
        }
        AddRoundKey(state, key.roundKeys + 4 * round);
    }
    std::memcpy(out, state, sizeof(state));
}

void DecryptBlock(const unsigned char* in, unsigned char* out, const AesKey& key) {
    std::uint8_t state[16];
    LoadState(state, in, key.roundKeys);
    for (int round = 1; round <= key.rounds; ++round) {
        std::uint8_t shifted[16];
        for (int column = 0; column < 4; ++column)
            for (int row = 0; row < 4; ++row) shifted[4 * column + row] = InverseSubstitution[state[4 * ((column + 4 - row) % 4) + row]];
        if (round != key.rounds) {
            for (int column = 0; column < 4; ++column) {
                const std::uint8_t* a = shifted + 4 * column;
                const std::uint8_t mixed[4] {
                    std::uint8_t(Multiply(a[0], 14) ^ Multiply(a[1], 11) ^ Multiply(a[2], 13) ^ Multiply(a[3], 9)),
                    std::uint8_t(Multiply(a[0], 9) ^ Multiply(a[1], 14) ^ Multiply(a[2], 11) ^ Multiply(a[3], 13)),
                    std::uint8_t(Multiply(a[0], 13) ^ Multiply(a[1], 9) ^ Multiply(a[2], 14) ^ Multiply(a[3], 11)),
                    std::uint8_t(Multiply(a[0], 11) ^ Multiply(a[1], 13) ^ Multiply(a[2], 9) ^ Multiply(a[3], 14)),
                };
                std::memcpy(state + 4 * column, mixed, 4);
            }
        } else {
            std::memcpy(state, shifted, sizeof(state));
        }
        AddRoundKey(state, key.roundKeys + 4 * round);
    }
    std::memcpy(out, state, sizeof(state));
}

unsigned RequirePosition(int position, const char* function) {
    if (position < 0 || position >= static_cast<int>(BlockSize))
        throw std::invalid_argument(std::string(function) + ": block position " + std::to_string(position) + " is outside 0-15");
    return static_cast<unsigned>(position);
}

void IncrementCounter(unsigned char* counter) {
    for (int i = 15; i >= 0; --i)
        if (++counter[i]) return;
}
}

extern "C" {

int APS5_VABI AES_set_encrypt_key_nid_postfix(const unsigned char* userKey, int bits, AesKey* key) {
    return SetKey(userKey, bits, key, false, __func__);
}

int APS5_VABI AES_set_decrypt_key_nid_postfix(const unsigned char* userKey, int bits, AesKey* key) {
    return SetKey(userKey, bits, key, true, __func__);
}

void APS5_VABI AES_encrypt_nid_postfix(const unsigned char* in, unsigned char* out, const AesKey* key) {
    const auto& k = RequireKey(key, __func__);
    RequireBlocks(in, out, BlockSize, __func__);
    EncryptBlock(in, out, k);
}

void APS5_VABI AES_decrypt_nid_postfix(const unsigned char* in, unsigned char* out, const AesKey* key) {
    const auto& k = RequireKey(key, __func__);
    RequireBlocks(in, out, BlockSize, __func__);
    DecryptBlock(in, out, k);
}

void APS5_VABI AES_ecb_encrypt_nid_postfix(const unsigned char* in, unsigned char* out, const AesKey* key, int encrypt) {
    if (encrypt == 1) AES_encrypt_nid_postfix(in, out, key);
    else AES_decrypt_nid_postfix(in, out, key);
}

void APS5_VABI AES_cbc_encrypt_nid_postfix(const unsigned char* in, unsigned char* out, std::size_t length, const AesKey* key, unsigned char* ivec, int encrypt) {
    const auto k = ReadKey(key, __func__);
    RequireBlocks(in, out, length, __func__);
    if (!ivec) throw std::invalid_argument(std::string(__func__) + ": null iv");
    if (!length) return;
    unsigned char iv[BlockSize];
    ReadGuest(iv, ivec, BlockSize, __func__, "iv");
    unsigned char source[Chunk];
    unsigned char target[Chunk];
    for (std::size_t done = 0; done < length;) {
        const std::size_t size = std::min(Chunk, length - done);
        const std::size_t padded = (size + BlockSize - 1) / BlockSize * BlockSize;
        ReadGuest(source, in + done, encrypt ? size : padded, __func__, "input");
        for (std::size_t offset = 0; offset < padded; offset += BlockSize) {
            const std::size_t count = std::min(BlockSize, size - offset);
            std::uint8_t block[BlockSize];
            if (encrypt) {
                for (std::size_t n = 0; n < BlockSize; ++n) block[n] = n < count ? source[offset + n] ^ iv[n] : iv[n];
                EncryptBlock(block, target + offset, k);
                std::memcpy(iv, target + offset, BlockSize);
            } else {
                DecryptBlock(source + offset, block, k);
                for (std::size_t n = 0; n < count; ++n) target[offset + n] = block[n] ^ iv[n];
                std::memcpy(iv, source + offset, BlockSize);
            }
        }
        WriteGuest(out + done, target, encrypt ? padded : size, __func__, "output");
        done += size;
    }
    WriteGuest(ivec, iv, BlockSize, __func__, "iv");
}

void APS5_VABI AES_cfb128_encrypt_nid_postfix(const unsigned char* in, unsigned char* out, std::size_t length, const AesKey* key, unsigned char* ivec, int* num, int encrypt) {
    const auto k = ReadKey(key, __func__);
    RequireBlocks(in, out, length, __func__);
    if (!ivec || !num) throw std::invalid_argument(std::string(__func__) + ": null iv or position");
    int position = 0;
    ReadGuest(&position, num, sizeof(position), __func__, "position");
    unsigned n = RequirePosition(position, __func__);
    if (!length) return;
    unsigned char before[BlockSize];
    unsigned char iv[BlockSize];
    ReadGuest(before, ivec, BlockSize, __func__, "iv");
    std::memcpy(iv, before, BlockSize);
    unsigned char source[Chunk];
    unsigned char target[Chunk];
    for (std::size_t done = 0; done < length;) {
        const std::size_t size = std::min(Chunk, length - done);
        ReadGuest(source, in + done, size, __func__, "input");
        for (std::size_t i = 0; i < size; ++i) {
            if (!n) EncryptBlock(iv, iv, k);
            const auto c = source[i];
            target[i] = iv[n] ^ c;
            iv[n] = encrypt ? target[i] : c;
            n = (n + 1) % BlockSize;
        }
        WriteGuest(out + done, target, size, __func__, "output");
        done += size;
    }
    WriteChanged(ivec, before, iv, BlockSize, __func__, "iv");
    const int after = static_cast<int>(n);
    WriteChanged(num, &position, &after, sizeof(after), __func__, "position");
}

void APS5_VABI AES_ctr128_encrypt_nid_postfix(const unsigned char* in, unsigned char* out, std::size_t length, const AesKey* key, unsigned char* ivec, unsigned char* ecountBuffer, unsigned* num) {
    const auto k = ReadKey(key, __func__);
    RequireBlocks(in, out, length, __func__);
    if (!ivec || !ecountBuffer || !num) throw std::invalid_argument(std::string(__func__) + ": null counter, keystream or position");
    unsigned position = 0;
    ReadGuest(&position, num, sizeof(position), __func__, "position");
    if (position >= BlockSize) throw std::invalid_argument(std::string(__func__) + ": block position " + std::to_string(position) + " is outside 0-15");
    if (!length) return;
    unsigned char counterBefore[BlockSize];
    unsigned char counter[BlockSize];
    unsigned char keystreamBefore[BlockSize];
    unsigned char keystream[BlockSize];
    ReadGuest(counterBefore, ivec, BlockSize, __func__, "counter");
    ReadGuest(keystreamBefore, ecountBuffer, BlockSize, __func__, "keystream");
    std::memcpy(counter, counterBefore, BlockSize);
    std::memcpy(keystream, keystreamBefore, BlockSize);
    unsigned n = position;
    unsigned char source[Chunk];
    unsigned char target[Chunk];
    for (std::size_t done = 0; done < length;) {
        const std::size_t size = std::min(Chunk, length - done);
        ReadGuest(source, in + done, size, __func__, "input");
        for (std::size_t i = 0; i < size; ++i) {
            if (!n) {
                EncryptBlock(counter, keystream, k);
                IncrementCounter(counter);
            }
            target[i] = source[i] ^ keystream[n];
            n = (n + 1) % BlockSize;
        }
        WriteGuest(out + done, target, size, __func__, "output");
        done += size;
    }
    WriteChanged(ivec, counterBefore, counter, BlockSize, __func__, "counter");
    WriteChanged(ecountBuffer, keystreamBefore, keystream, BlockSize, __func__, "keystream");
    WriteChanged(num, &position, &n, sizeof(n), __func__, "position");
}

}
