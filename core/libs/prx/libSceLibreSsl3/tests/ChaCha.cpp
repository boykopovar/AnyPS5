#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Cipher.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <mbedtls/chacha20.h>
#include <mbedtls/chachapoly.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <random>
#include <source_location>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

constexpr int SetIvLength = 0x9;
constexpr int GetTag = 0x10;
constexpr int SetTag = 0x11;
constexpr int SetIvFixed = 0x12;
constexpr int GetIvLength = 0x25;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 ChaCha check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) {
    return std::malloc(size);
}

void APS5_VABI Free(void* memory) {
    std::free(memory);
}

void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    return std::calloc(count, size);
}

void* APS5_VABI Reallocate(void* memory, std::size_t size) {
    return std::realloc(memory, size);
}

void* APS5_VABI Align(std::size_t, std::size_t size) {
    return std::malloc(size);
}

void* APS5_VABI Realign(void* memory, std::size_t size, std::size_t) {
    return std::realloc(memory, size);
}

int APS5_VABI PosixAlign(void** memory, std::size_t, std::size_t size) {
    *memory = std::malloc(size);
    return *memory ? 0 : 12;
}

const char Sunscreen[] = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";

Bytes Text() {
    return Bytes(Sunscreen, Sunscreen + sizeof(Sunscreen) - 1);
}

Bytes Sequence(unsigned char first, std::size_t length) {
    Bytes bytes(length);
    for (std::size_t index = 0; index < length; ++index) bytes[index] = static_cast<unsigned char>(first + index);
    return bytes;
}

Bytes Random(std::mt19937& random, std::size_t length) {
    Bytes bytes(length);
    for (auto& byte : bytes) byte = static_cast<unsigned char>(random());
    return bytes;
}

Bytes Stream(const Bytes& key, const Bytes& nonce, std::uint32_t counter, const Bytes& input) {
    Bytes output(input.size());
    Require(mbedtls_chacha20_crypt(key.data(), nonce.data(), counter, input.size(), input.data(), output.data()) == 0);
    return output;
}

Bytes EvpIv(std::uint64_t counter, const Bytes& iv) {
    Bytes bytes(16);
    for (int index = 0; index < 8; ++index) bytes[index] = static_cast<unsigned char>(counter >> (8 * index));
    std::memcpy(bytes.data() + 8, iv.data(), 8);
    return bytes;
}

Bytes IetfNonce(std::uint32_t high, const Bytes& iv) {
    Bytes nonce(12);
    for (int index = 0; index < 4; ++index) nonce[index] = static_cast<unsigned char>(high >> (8 * index));
    std::memcpy(nonce.data() + 4, iv.data(), 8);
    return nonce;
}

struct Sealed {
    Bytes text;
    Bytes tag;
};

Sealed Seal(const Bytes& key, const Bytes& nonce, const Bytes& aad, const Bytes& plain) {
    mbedtls_chachapoly_context context;
    mbedtls_chachapoly_init(&context);
    Require(mbedtls_chachapoly_setkey(&context, key.data()) == 0);
    Sealed sealed {Bytes(plain.size()), Bytes(16)};
    Require(mbedtls_chachapoly_encrypt_and_tag(&context, plain.size(), nonce.data(), aad.data(), aad.size(), plain.data(), sealed.text.data(),
                                               sealed.tag.data()) == 0);
    mbedtls_chachapoly_free(&context);
    return sealed;
}

class Context {
public:
    Context() : context(EVP_CIPHER_CTX_new_nid_postfix()) {
        Require(context != nullptr);
    }
    ~Context() {
        EVP_CIPHER_CTX_free_nid_postfix(context);
    }
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    LibreSslEvpCipherCtx* get() const {
        return context;
    }

private:
    LibreSslEvpCipherCtx* context;
};

Bytes Update(LibreSslEvpCipherCtx* ctx, bool encrypt, const Bytes& input, std::size_t offset, std::size_t length) {
    Bytes output(length);
    int written = -1;
    const int ok = encrypt ? EVP_EncryptUpdate_nid_postfix(ctx, output.data(), &written, input.data() + offset, static_cast<int>(length))
                           : EVP_DecryptUpdate_nid_postfix(ctx, output.data(), &written, input.data() + offset, static_cast<int>(length));
    Require(ok == 1 && written == static_cast<int>(length));
    return output;
}

void Aad(LibreSslEvpCipherCtx* ctx, bool encrypt, const Bytes& aad, std::size_t offset, std::size_t length) {
    int written = -1;
    const int ok = encrypt ? EVP_EncryptUpdate_nid_postfix(ctx, nullptr, &written, aad.data() + offset, static_cast<int>(length))
                           : EVP_DecryptUpdate_nid_postfix(ctx, nullptr, &written, aad.data() + offset, static_cast<int>(length));
    Require(ok == 1 && written == static_cast<int>(length));
}

void CheckObjects() {
    const LibreSslEvpCipher* stream = EVP_chacha20_nid_postfix();
    Require(stream == EVP_chacha20_nid_postfix());
    Require(stream->nid == 937 && stream->blockSize == 1 && stream->keyLength == 32 && stream->ivLength == 16);
    Require(stream->flags == 0x30 && stream->contextSize == 132);
    Require(stream->init && stream->doCipher && !stream->cleanup && !stream->control);
    const auto* streamView = reinterpret_cast<const LibreSsl3EvpCipher*>(stream);
    Require(EVP_CIPHER_key_length_nid_postfix(streamView) == 32 && EVP_CIPHER_iv_length_nid_postfix(streamView) == 16);

    const LibreSslEvpCipher* aead = EVP_chacha20_poly1305_nid_postfix();
    Require(aead == EVP_chacha20_poly1305_nid_postfix());
    Require(aead->nid == 967 && aead->blockSize == 1 && aead->keyLength == 32 && aead->ivLength == 12);
    Require(aead->flags == 0x701070 && aead->contextSize == 384);
    Require(aead->init && aead->doCipher && aead->cleanup && aead->control);
    const auto* aeadView = reinterpret_cast<const LibreSsl3EvpCipher*>(aead);
    Require(EVP_CIPHER_key_length_nid_postfix(aeadView) == 32 && EVP_CIPHER_iv_length_nid_postfix(aeadView) == 12);
}

Bytes FromHex(const char* hex) {
    Bytes bytes;
    for (; hex[0] != 0 && hex[1] != 0; hex += 2) bytes.push_back(static_cast<unsigned char>(std::stoul(std::string(hex, 2), nullptr, 16)));
    return bytes;
}

void CheckStreamVector() {
    const Bytes key = Sequence(0, 32);
    const Bytes iv = {0x00, 0x00, 0x00, 0x4a, 0x00, 0x00, 0x00, 0x00};
    const Bytes plain = Text();
    const Bytes expected = Stream(key, IetfNonce(0, iv), 1, plain);
    const Bytes rfcStart = {0x6e, 0x2e, 0x35, 0x9a, 0x25, 0x68, 0xf9, 0x80, 0x41, 0xba, 0x07, 0x28, 0xdd, 0x0d, 0x69, 0x81};
    Require(std::memcmp(expected.data(), rfcStart.data(), rfcStart.size()) == 0);
    Require(expected == FromHex("6e2e359a2568f98041ba0728dd0d6981e97e7aec1d4360c20a27afccfd9fae0bf91b65c5524733ab8f593dabcd62b3571639d624e65152ab8f530c359f0861d807ca0dbf500d6a6156a38e088a22b65e52bc514d16ccf806818ce91ab77937365af90bbf74a35be6b40b8eedf2785e42874d"));
    Context context;
    const Bytes evpIv = EvpIv(1, iv);
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_chacha20_nid_postfix(), nullptr, key.data(), evpIv.data()) == 1);
    Require(Update(context.get(), true, plain, 0, plain.size()) == expected);
    int written = -1;
    unsigned char tail[1];
    Require(EVP_EncryptFinal_ex_nid_postfix(context.get(), tail, &written) == 1 && written == 0);
    Require(EVP_DecryptInit_ex_nid_postfix(context.get(), nullptr, nullptr, nullptr, evpIv.data()) == 1);
    Require(Update(context.get(), false, expected, 0, expected.size()) == plain);
    Require(EVP_DecryptFinal_ex_nid_postfix(context.get(), tail, &written) == 1 && written == 0);
}

void CheckStreamChunks() {
    std::mt19937 random(11);
    for (int round = 0; round < 200; ++round) {
        const Bytes key = Random(random, 32);
        const Bytes iv = Random(random, 8);
        const std::uint32_t low = random();
        const std::uint32_t high = random();
        const Bytes plain = Random(random, 1 + random() % 1000);
        const Bytes expected = Stream(key, IetfNonce(high, iv), low, plain);
        const bool carries = static_cast<std::uint64_t>(low) + (plain.size() + 63) / 64 > 0xffffffffULL;
        Context context;
        const Bytes evpIv = EvpIv(static_cast<std::uint64_t>(high) << 32 | low, iv);
        Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_chacha20_nid_postfix(), nullptr, key.data(), evpIv.data()) == 1);
        Bytes output;
        for (std::size_t offset = 0; offset < plain.size();) {
            const std::size_t length = std::min<std::size_t>(plain.size() - offset, random() % 140);
            const Bytes part = Update(context.get(), true, plain, offset, length);
            output.insert(output.end(), part.begin(), part.end());
            offset += length;
        }
        if (!carries) Require(output == expected);
    }
}

void CheckCounterCarry() {
    const Bytes key = Sequence(0x40, 32);
    const Bytes iv = Sequence(0x90, 8);
    const Bytes zeros(128, 0);
    const Bytes first = Stream(key, IetfNonce(0x01020304, iv), 0xffffffff, Bytes(64, 0));
    const Bytes second = Stream(key, IetfNonce(0x01020305, iv), 0, Bytes(64, 0));
    Context context;
    const Bytes evpIv = EvpIv(0x01020304ffffffffULL, iv);
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_chacha20_nid_postfix(), nullptr, key.data(), evpIv.data()) == 1);
    const Bytes output = Update(context.get(), true, zeros, 0, zeros.size());
    Require(std::memcmp(output.data(), first.data(), 64) == 0);
    Require(std::memcmp(output.data() + 64, second.data(), 64) == 0);
}

void CheckStreamReinit() {
    const Bytes key = Sequence(0x10, 32);
    const Bytes iv = Sequence(0x20, 8);
    const Bytes plain = Sequence(0x30, 100);
    const Bytes expected = Stream(key, IetfNonce(0, iv), 0, plain);
    Context context;
    const Bytes evpIv = EvpIv(0, iv);
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_chacha20_nid_postfix(), nullptr, key.data(), evpIv.data()) == 1);
    Update(context.get(), true, plain, 0, 37);
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), nullptr, nullptr, nullptr, evpIv.data()) == 1);
    Require(Update(context.get(), true, plain, 0, plain.size()) == expected);
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), nullptr, nullptr, key.data(), nullptr) == 1);
    const Bytes afterKey = Update(context.get(), true, plain, 0, 64);
    const Bytes continued = Stream(key, IetfNonce(0, iv), 2, Bytes(plain.begin(), plain.begin() + 64));
    Require(afterKey == continued);
}

void CheckStreamKeyOnly() {
    const Bytes key = Sequence(0x50, 32);
    const Bytes zeros(64, 0);
    const Bytes expected = Stream(key, Bytes(12, 0), 0, zeros);
    Context context;
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_chacha20_nid_postfix(), nullptr, key.data(), nullptr) == 1);
    Require(Update(context.get(), true, zeros, 0, zeros.size()) == expected);
}

void CheckAeadVector() {
    const Bytes key = Sequence(0x80, 32);
    const Bytes nonce = {0x07, 0x00, 0x00, 0x00, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47};
    const Bytes aad = {0x50, 0x51, 0x52, 0x53, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7};
    const Bytes plain = Text();
    const Sealed expected = Seal(key, nonce, aad, plain);
    const Bytes rfcTag = {0x1a, 0xe1, 0x0b, 0x59, 0x4f, 0x09, 0xe2, 0x6a, 0x7e, 0x90, 0x2e, 0xcb, 0xd0, 0x60, 0x06, 0x91};
    Require(expected.tag == rfcTag);

    Context context;
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    int ivLength = 0;
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GetIvLength, 0, &ivLength) == 1 && ivLength == 12);
    Aad(context.get(), true, aad, 0, 5);
    Aad(context.get(), true, aad, 5, aad.size() - 5);
    Bytes text = Update(context.get(), true, plain, 0, 30);
    const Bytes rest = Update(context.get(), true, plain, 30, plain.size() - 30);
    text.insert(text.end(), rest.begin(), rest.end());
    Require(text == expected.text);
    int written = -1;
    unsigned char tail[1];
    Require(EVP_EncryptFinal_ex_nid_postfix(context.get(), tail, &written) == 1 && written == 0);
    Bytes tag(16);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GetTag, 16, tag.data()) == 1 && tag == expected.tag);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GetTag, 17, tag.data()) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), SetTag, 16, tag.data()) == 0);

    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), nullptr, nullptr, nullptr, nonce.data()) == 1);
    Aad(context.get(), true, aad, 0, aad.size());
    Require(Update(context.get(), true, plain, 0, plain.size()) == expected.text);
    Require(EVP_EncryptFinal_ex_nid_postfix(context.get(), tail, &written) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GetTag, 16, tag.data()) == 1 && tag == expected.tag);
}

bool Open(const Bytes& key, const Bytes& nonce, const Bytes& aad, const Sealed& sealed, int tagLength, Bytes* plain) {
    Context context;
    Require(EVP_DecryptInit_ex_nid_postfix(context.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    if (tagLength > 0) Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), SetTag, tagLength, const_cast<unsigned char*>(sealed.tag.data())) == 1);
    if (!aad.empty()) Aad(context.get(), false, aad, 0, aad.size());
    if (!sealed.text.empty()) *plain = Update(context.get(), false, sealed.text, 0, sealed.text.size());
    Bytes unused(16);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GetTag, 16, unused.data()) == 0);
    int written = -1;
    unsigned char tail[1];
    return EVP_DecryptFinal_ex_nid_postfix(context.get(), tail, &written) == 1;
}

void CheckAeadOpen() {
    const Bytes key = Sequence(0x80, 32);
    const Bytes nonce = Sequence(0x01, 12);
    const Bytes aad = Sequence(0xa0, 20);
    const Bytes plain = Sequence(0x05, 77);
    const Sealed sealed = Seal(key, nonce, aad, plain);
    Bytes opened;
    Require(Open(key, nonce, aad, sealed, 16, &opened) && opened == plain);
    Require(Open(key, nonce, aad, sealed, 8, &opened) && opened == plain);
    Sealed corrupted = sealed;
    corrupted.tag[3] ^= 1;
    Require(!Open(key, nonce, aad, corrupted, 16, &opened));
    corrupted = sealed;
    corrupted.text[10] ^= 0x80;
    Require(!Open(key, nonce, aad, corrupted, 16, &opened));
    Require(!Open(key, nonce, Sequence(0xa1, 20), sealed, 16, &opened));
    Require(!Open(key, nonce, aad, sealed, 0, &opened));
}

void CheckAeadAfterFailure() {
    const Bytes key = Sequence(0x80, 32);
    const Bytes nonce = Sequence(0x01, 12);
    const Bytes plain = Sequence(0x05, 40);
    Sealed sealed = Seal(key, nonce, Bytes(), plain);
    sealed.tag[0] ^= 1;
    Context context;
    Require(EVP_DecryptInit_ex_nid_postfix(context.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), SetTag, 16, sealed.tag.data()) == 1);
    Update(context.get(), false, sealed.text, 0, sealed.text.size());
    int written = -1;
    unsigned char tail[1];
    Require(EVP_DecryptFinal_ex_nid_postfix(context.get(), tail, &written) == 0);
    Bytes zeroTag(16, 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), SetTag, 16, zeroTag.data()) == 1);
    Require(EVP_DecryptFinal_ex_nid_postfix(context.get(), tail, &written) == 1);
}

void CheckAeadEmptyAndNegativeUpdates() {
    const Bytes key = Sequence(0x61, 32);
    const Bytes nonce = Sequence(0x71, 12);
    const Bytes aad = Sequence(0x81, 13);
    const Bytes plain = Sequence(0x91, 50);
    const Sealed expected = Seal(key, nonce, aad, plain);
    Context context;
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    int written = -1;
    unsigned char scratch[1];
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), nullptr, &written, aad.data(), -1) == 0);
    Aad(context.get(), true, aad, 0, 5);
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), scratch, &written, plain.data(), 0) == 1 && written == 0);
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), nullptr, &written, aad.data(), 0) == 1 && written == 0);
    Aad(context.get(), true, aad, 5, aad.size() - 5);
    Require(Update(context.get(), true, plain, 0, plain.size()) == expected.text);
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), scratch, &written, plain.data(), -5) == 0);
    unsigned char tail[1];
    Require(EVP_EncryptFinal_ex_nid_postfix(context.get(), tail, &written) == 1);
    Bytes tag(16);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GetTag, 16, tag.data()) == 1 && tag == expected.tag);
}

void CheckAeadControls() {
    const Bytes key = Sequence(0x12, 32);
    const Bytes nonce = Sequence(0x22, 12);
    const Bytes plain = Sequence(0x32, 20);
    const Sealed sealed = Seal(key, nonce, Bytes(), plain);
    Context encrypt;
    Require(EVP_EncryptInit_ex_nid_postfix(encrypt.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    Bytes tag(16);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(encrypt.get(), GetTag, 16, tag.data()) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(encrypt.get(), 0x7f, 0, nullptr) == 0);
    Context decrypt;
    Require(EVP_DecryptInit_ex_nid_postfix(decrypt.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    Bytes good = sealed.tag;
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(decrypt.get(), SetTag, 0, good.data()) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(decrypt.get(), SetTag, 17, good.data()) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(decrypt.get(), SetTag, 16, good.data()) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(decrypt.get(), SetTag, 16, nullptr) == 1);
    Require(Update(decrypt.get(), false, sealed.text, 0, sealed.text.size()) == plain);
    int written = -1;
    Require(EVP_DecryptUpdate_nid_postfix(decrypt.get(), nullptr, &written, sealed.text.data(), 4) == 0);
    unsigned char tail[1];
    Require(EVP_DecryptFinal_ex_nid_postfix(decrypt.get(), tail, &written) == 1);
}

void CheckAeadNonces() {
    const Bytes key = Sequence(0x33, 32);
    const Bytes aad = Sequence(0x44, 3);
    const Bytes plain = Sequence(0x55, 65);
    const Bytes shortIv = Sequence(0x66, 8);
    Bytes padded(4, 0);
    padded.insert(padded.end(), shortIv.begin(), shortIv.end());
    const Sealed expected = Seal(key, padded, aad, plain);

    Context context;
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, nullptr, nullptr) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), SetIvLength, 0, nullptr) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), SetIvLength, 13, nullptr) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), SetIvLength, 8, nullptr) == 1);
    int ivLength = 0;
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GetIvLength, 0, &ivLength) == 1 && ivLength == 8);
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), nullptr, nullptr, key.data(), shortIv.data()) == 1);
    Aad(context.get(), true, aad, 0, aad.size());
    Require(Update(context.get(), true, plain, 0, plain.size()) == expected.text);
    int written = -1;
    unsigned char tail[1];
    Require(EVP_EncryptFinal_ex_nid_postfix(context.get(), tail, &written) == 1);
    Bytes tag(16);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GetTag, 16, tag.data()) == 1 && tag == expected.tag);

    const Bytes fixed = Sequence(0x77, 12);
    const Sealed fixedExpected = Seal(key, fixed, aad, plain);
    Context fixedContext;
    Require(EVP_EncryptInit_ex_nid_postfix(fixedContext.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, nullptr, nullptr) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(fixedContext.get(), SetIvFixed, 11, const_cast<unsigned char*>(fixed.data())) == 0);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(fixedContext.get(), SetIvFixed, 12, const_cast<unsigned char*>(fixed.data())) == 1);
    Require(EVP_EncryptInit_ex_nid_postfix(fixedContext.get(), nullptr, nullptr, key.data(), nullptr) == 1);
    Aad(fixedContext.get(), true, aad, 0, aad.size());
    Require(Update(fixedContext.get(), true, plain, 0, plain.size()) == fixedExpected.text);
    Require(EVP_EncryptFinal_ex_nid_postfix(fixedContext.get(), tail, &written) == 1);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(fixedContext.get(), GetTag, 16, tag.data()) == 1 && tag == fixedExpected.tag);
}

void CheckAeadOrder() {
    const Bytes key = Sequence(0x21, 32);
    const Bytes nonce = Sequence(0x31, 12);
    const Bytes data = Sequence(0x41, 16);
    Context context;
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    Update(context.get(), true, data, 0, data.size());
    int written = -1;
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), nullptr, &written, data.data(), static_cast<int>(data.size())) == 0);

    const Sealed empty = Seal(key, nonce, Bytes(), Bytes());
    Context emptyContext;
    Require(EVP_EncryptInit_ex_nid_postfix(emptyContext.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    unsigned char tail[1];
    Require(EVP_EncryptFinal_ex_nid_postfix(emptyContext.get(), tail, &written) == 1 && written == 0);
    Bytes tag(16);
    Require(EVP_CIPHER_CTX_ctrl_nid_postfix(emptyContext.get(), GetTag, 16, tag.data()) == 1 && tag == empty.tag);
}

void CheckAeadRandom() {
    std::mt19937 random(12);
    for (int round = 0; round < 200; ++round) {
        const Bytes key = Random(random, 32);
        const Bytes nonce = Random(random, 12);
        const Bytes aad = Random(random, random() % 70);
        const Bytes plain = Random(random, random() % 300);
        const Sealed expected = Seal(key, nonce, aad, plain);
        Context context;
        Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
        for (std::size_t offset = 0; offset < aad.size();) {
            const std::size_t length = std::min<std::size_t>(aad.size() - offset, 1 + random() % 20);
            Aad(context.get(), true, aad, offset, length);
            offset += length;
        }
        Bytes text;
        for (std::size_t offset = 0; offset < plain.size();) {
            const std::size_t length = std::min<std::size_t>(plain.size() - offset, 1 + random() % 90);
            const Bytes part = Update(context.get(), true, plain, offset, length);
            text.insert(text.end(), part.begin(), part.end());
            offset += length;
        }
        Require(text == expected.text);
        int written = -1;
        unsigned char tail[1];
        Require(EVP_EncryptFinal_ex_nid_postfix(context.get(), tail, &written) == 1);
        Bytes tag(16);
        Require(EVP_CIPHER_CTX_ctrl_nid_postfix(context.get(), GetTag, 16, tag.data()) == 1 && tag == expected.tag);
    }
}

} // namespace

int main() {
    void* api[10] {reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
                   reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
                   reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    CheckObjects();
    CheckStreamVector();
    CheckStreamChunks();
    CheckCounterCarry();
    CheckStreamReinit();
    CheckStreamKeyOnly();
    CheckAeadVector();
    CheckAeadOpen();
    CheckAeadAfterFailure();
    CheckAeadEmptyAndNegativeUpdates();
    CheckAeadControls();
    CheckAeadNonces();
    CheckAeadOrder();
    CheckAeadRandom();
    std::puts("LibreSSL3 ChaCha: ok");
    return 0;
}
