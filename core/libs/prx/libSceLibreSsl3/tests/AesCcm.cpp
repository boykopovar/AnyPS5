#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Cipher.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <mbedtls/ccm.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <source_location>
#include <vector>

namespace {

using Bytes = std::vector<unsigned char>;

constexpr int ControlCopy = 0x8;
constexpr int SetIvLength = 0x9;
constexpr int GetTag = 0x10;
constexpr int SetTag = 0x11;
constexpr int SetL = 0x14;
constexpr int GetIvLength = 0x25;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 AES-CCM check failed at line %u\n", location.line());
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) {
    void* memory = std::malloc(size);
    if (memory) std::memset(memory, 0xFF, size);
    return memory;
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

Bytes Sequence(unsigned char first, std::size_t length) {
    Bytes bytes(length);
    for (std::size_t index = 0; index < length; ++index) bytes[index] = static_cast<unsigned char>(first + index);
    return bytes;
}

Bytes Hex(const char* text) {
    Bytes bytes;
    for (; text[0] && text[1]; text += 2) {
        auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        bytes.push_back(static_cast<unsigned char>(digit(text[0]) << 4 | digit(text[1])));
    }
    return bytes;
}

Bytes Random(std::mt19937& random, std::size_t length) {
    Bytes bytes(length);
    for (auto& byte : bytes) byte = static_cast<unsigned char>(random());
    return bytes;
}

struct Sealed {
    Bytes text;
    Bytes tag;
};

Sealed Seal(const Bytes& key, const Bytes& nonce, const Bytes& aad, const Bytes& plain, std::size_t tagLength) {
    mbedtls_ccm_context context;
    mbedtls_ccm_init(&context);
    Require(mbedtls_ccm_setkey(&context, MBEDTLS_CIPHER_ID_AES, key.data(), 128) == 0);
    Sealed sealed {Bytes(plain.size()), Bytes(tagLength)};
    Require(mbedtls_ccm_encrypt_and_tag(&context, plain.size(), nonce.data(), nonce.size(), aad.data(), aad.size(), plain.data(), sealed.text.data(),
                                        sealed.tag.data(), tagLength) == 0);
    mbedtls_ccm_free(&context);
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

int Control(LibreSslEvpCipherCtx* ctx, int type, int arg, void* pointer) {
    return EVP_CIPHER_CTX_ctrl_nid_postfix(ctx, type, arg, pointer);
}

int Update(LibreSslEvpCipherCtx* ctx, unsigned char* out, int* written, const unsigned char* in, int length) {
    return ctx->encrypt ? EVP_EncryptUpdate_nid_postfix(ctx, out, written, in, length) : EVP_DecryptUpdate_nid_postfix(ctx, out, written, in, length);
}

void SetLength(LibreSslEvpCipherCtx* ctx, std::size_t length) {
    int written = -1;
    Require(Update(ctx, nullptr, &written, nullptr, static_cast<int>(length)) == 1 && written == static_cast<int>(length));
}

void AddAad(LibreSslEvpCipherCtx* ctx, const Bytes& aad) {
    int written = -1;
    Require(Update(ctx, nullptr, &written, aad.data(), static_cast<int>(aad.size())) == 1 && written == static_cast<int>(aad.size()));
}

void Setup(LibreSslEvpCipherCtx* ctx, bool encrypt, const Bytes& key, const Bytes& nonce, std::size_t tagLength, const unsigned char* tag, bool useSetL) {
    const auto init = encrypt ? EVP_EncryptInit_ex_nid_postfix : EVP_DecryptInit_ex_nid_postfix;
    Require(init(ctx, EVP_aes_128_ccm_nid_postfix(), nullptr, nullptr, nullptr) == 1);
    const int nonceLength = static_cast<int>(nonce.size());
    Require(useSetL ? Control(ctx, SetL, 15 - nonceLength, nullptr) == 1 : Control(ctx, SetIvLength, nonceLength, nullptr) == 1);
    Require(Control(ctx, SetTag, static_cast<int>(tagLength), const_cast<unsigned char*>(tag)) == 1);
    Require(init(ctx, nullptr, nullptr, key.data(), nonce.data()) == 1);
}

Sealed EvpSeal(const Bytes& key, const Bytes& nonce, const Bytes& aad, const Bytes& plain, std::size_t tagLength, bool announce, bool useSetL) {
    Context context;
    Setup(context.get(), true, key, nonce, tagLength, nullptr, useSetL);
    if (announce || !aad.empty()) SetLength(context.get(), plain.size());
    if (!aad.empty()) AddAad(context.get(), aad);
    Sealed sealed {Bytes(plain.size()), Bytes(tagLength)};
    int written = -1;
    unsigned char empty[1];
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), plain.empty() ? empty : sealed.text.data(), &written, plain.empty() ? empty : plain.data(),
                                          static_cast<int>(plain.size())) == 1);
    Require(written == static_cast<int>(plain.size()));
    unsigned char tail[1];
    written = -1;
    Require(EVP_EncryptFinal_ex_nid_postfix(context.get(), tail, &written) == 1 && written == 0);
    Require(Control(context.get(), GetTag, static_cast<int>(tagLength), sealed.tag.data()) == 1);
    return sealed;
}

bool EvpOpen(const Bytes& key, const Bytes& nonce, const Bytes& aad, const Bytes& text, const Bytes& tag, Bytes& plain) {
    Context context;
    Setup(context.get(), false, key, nonce, tag.size(), tag.data(), false);
    SetLength(context.get(), text.size());
    if (!aad.empty()) AddAad(context.get(), aad);
    plain.assign(text.size(), 0xA5);
    int written = -1;
    unsigned char empty[1];
    const int ok = EVP_DecryptUpdate_nid_postfix(context.get(), plain.empty() ? empty : plain.data(), &written, text.empty() ? empty : text.data(),
                                                 static_cast<int>(text.size()));
    Require(written == (ok ? static_cast<int>(text.size()) : 0));
    unsigned char tail[1];
    written = -1;
    Require(EVP_DecryptFinal_ex_nid_postfix(context.get(), tail, &written) == 0 && written == 0);
    return ok == 1;
}

bool AllZero(const Bytes& bytes) {
    for (unsigned char byte : bytes)
        if (byte) return false;
    return true;
}

void CheckObject() {
    const LibreSslEvpCipher* cipher = EVP_aes_128_ccm_nid_postfix();
    Require(cipher == EVP_aes_128_ccm_nid_postfix());
    Require(cipher->nid == 896 && cipher->blockSize == 1 && cipher->keyLength == 16 && cipher->ivLength == 12);
    Require(cipher->flags == 0x501477 && cipher->contextSize == 336);
    Require(cipher->init && cipher->doCipher && !cipher->cleanup && cipher->control);
    Require(!cipher->setAsn1Parameters && !cipher->getAsn1Parameters && !cipher->applicationData);
    const auto* view = reinterpret_cast<const LibreSsl3EvpCipher*>(cipher);
    Require(EVP_CIPHER_key_length_nid_postfix(view) == 16 && EVP_CIPHER_iv_length_nid_postfix(view) == 12);
}

void CheckVectorKeyed(const Bytes& key, const Bytes& nonce, const Bytes& aad, const Bytes& plain, const char* expected) {
    const Bytes output = Hex(expected);
    const std::size_t tagLength = output.size() - plain.size();
    const Bytes text(output.begin(), output.begin() + static_cast<std::ptrdiff_t>(plain.size()));
    const Bytes tag(output.begin() + static_cast<std::ptrdiff_t>(plain.size()), output.end());
    const Sealed sealed = EvpSeal(key, nonce, aad, plain, tagLength, true, false);
    Require(sealed.text == text && sealed.tag == tag);
    Bytes opened;
    Require(EvpOpen(key, nonce, aad, text, tag, opened) && opened == plain);
}

void CheckVector(const Bytes& nonce, const Bytes& aad, const Bytes& plain, const char* expected) {
    CheckVectorKeyed(Sequence(0x40, 16), nonce, aad, plain, expected);
}

void CheckNistVectors() {
    CheckVector(Sequence(0x10, 7), Sequence(0x00, 8), Sequence(0x20, 4), "7162015b4dac255d");
    CheckVectorKeyed(Hex("c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"), Hex("00000003020100a0a1a2a3a4a5"), Hex("0001020304050607"),
        Hex("08090a0b0c0d0e0f101112131415161718191a1b1c1d1e"), "588c979a61c663d2f066d0c2c0f989806d5f6b61dac38417e8d12cfdf926e0");
    CheckVector(Sequence(0x10, 8), Sequence(0x00, 16), Sequence(0x20, 16), "d2a1f0e051ea5f62081a7792073d593d1fc64fbfaccd");
    CheckVector(Sequence(0x10, 12), Sequence(0x00, 20), Sequence(0x20, 24), "e3b201a9f5b71a7a9b1ceaeccd97e70b6176aad9a4428aa5484392fbc1b09951");
    Bytes longAad(65536);
    for (std::size_t index = 0; index < longAad.size(); ++index) longAad[index] = static_cast<unsigned char>(index);
    CheckVector(Sequence(0x10, 13), longAad, Sequence(0x20, 32),
                "69915dad1e84c6376a68c2967e4dab615ae0fd1faec44cc484828529463ccf72b4ac6bec93e8598e7f0dadbcea5b");
}

void CheckRandom() {
    std::mt19937 random(31);
    for (int round = 0; round < 400; ++round) {
        const Bytes key = Random(random, 16);
        const bool longest = round % 40 == 1;
        const std::size_t l = longest ? 2 : 2 + random() % 7;
        const Bytes nonce = Random(random, 15 - l);
        const std::size_t tagLength = 4 + 2 * (random() % 7);
        const std::size_t aadLength = round % 50 == 0 ? 0xFEFF - random() % 600 : random() % 300;
        const Bytes aad = Random(random, round % 7 == 0 ? 0 : aadLength);
        std::size_t messageLength = random() % 600;
        if (longest) messageLength = 0xFFFF;
        const Bytes plain = Random(random, round % 11 == 0 && !longest ? 0 : messageLength);
        const Sealed expected = Seal(key, nonce, aad, plain, tagLength);
        const Sealed sealed = EvpSeal(key, nonce, aad, plain, tagLength, round % 3 == 0, round % 2 == 0);
        Require(sealed.text == expected.text && sealed.tag == expected.tag);
        Bytes opened;
        Require(EvpOpen(key, nonce, aad, expected.text, expected.tag, opened) && opened == plain);
        Bytes badTag = expected.tag;
        badTag[random() % badTag.size()] ^= static_cast<unsigned char>(1 << (random() % 8));
        Require(!EvpOpen(key, nonce, aad, expected.text, badTag, opened) && AllZero(opened));
        if (!plain.empty()) {
            Bytes badText = expected.text;
            badText[random() % badText.size()] ^= 0x80;
            Require(!EvpOpen(key, nonce, aad, badText, expected.tag, opened) && AllZero(opened));
        }
        if (!aad.empty()) {
            Bytes badAad = aad;
            badAad[random() % badAad.size()] ^= 0x01;
            Require(!EvpOpen(key, nonce, badAad, expected.text, expected.tag, opened) && AllZero(opened));
        }
    }
}

void CheckDefaults() {
    std::mt19937 random(32);
    const Bytes key = Random(random, 16);
    const Bytes nonce = Random(random, 12);
    const Bytes plain = Random(random, 45);
    const Sealed expected = Seal(key, Bytes(nonce.begin(), nonce.begin() + 7), {}, plain, 12);
    Context context;
    Require(EVP_EncryptInit_ex_nid_postfix(context.get(), EVP_aes_128_ccm_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    int ivLength = -1;
    Require(Control(context.get(), GetIvLength, 0, &ivLength) == 1 && ivLength == 7);
    Bytes text(plain.size());
    int written = -1;
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), text.data(), &written, plain.data(), static_cast<int>(plain.size())) == 1);
    Require(written == static_cast<int>(plain.size()) && text == expected.text);
    Bytes tag(16);
    Require(Control(context.get(), GetTag, 16, tag.data()) == 0);
    Require(Control(context.get(), GetTag, 12, tag.data()) == 1);
    tag.resize(12);
    Require(tag == expected.tag);
    Require(Control(context.get(), GetTag, 12, tag.data()) == 0);
}

void CheckControls() {
    Context context;
    LibreSslEvpCipherCtx* ctx = context.get();
    Require(EVP_EncryptInit_ex_nid_postfix(ctx, EVP_aes_128_ccm_nid_postfix(), nullptr, nullptr, nullptr) == 1);
    int ivLength = -1;
    Require(Control(ctx, GetIvLength, 0, &ivLength) == 1 && ivLength == 7);
    Require(Control(ctx, SetIvLength, 6, nullptr) == 0 && Control(ctx, SetIvLength, 14, nullptr) == 0);
    Require(Control(ctx, SetIvLength, 13, nullptr) == 1 && Control(ctx, GetIvLength, 0, &ivLength) == 1 && ivLength == 13);
    Require(Control(ctx, SetIvLength, 7, nullptr) == 1 && Control(ctx, GetIvLength, 0, &ivLength) == 1 && ivLength == 7);
    Require(Control(ctx, SetIvLength, -2147483647 - 1, nullptr) == 0 && Control(ctx, SetIvLength, 2147483647, nullptr) == 0);
    Require(Control(ctx, SetL, 1, nullptr) == 0 && Control(ctx, SetL, 9, nullptr) == 0);
    Require(Control(ctx, SetL, 3, nullptr) == 1 && Control(ctx, GetIvLength, 0, &ivLength) == 1 && ivLength == 12);
    unsigned char tag[16] {};
    Require(Control(ctx, SetTag, 5, nullptr) == 0 && Control(ctx, SetTag, 2, nullptr) == 0 && Control(ctx, SetTag, 18, nullptr) == 0);
    Require(Control(ctx, SetTag, 16, tag) == 0);
    Require(Control(ctx, SetTag, 16, nullptr) == 1 && Control(ctx, SetTag, 4, nullptr) == 1);
    Require(Control(ctx, GetTag, 4, tag) == 0);
    Require(Control(ctx, 0x13, 0, nullptr) == 0 && Control(ctx, 0x16, 13, tag) == 0);
    Context decrypt;
    Require(EVP_DecryptInit_ex_nid_postfix(decrypt.get(), EVP_aes_128_ccm_nid_postfix(), nullptr, nullptr, nullptr) == 1);
    Require(Control(decrypt.get(), SetTag, 8, nullptr) == 0);
    Require(Control(decrypt.get(), SetTag, 8, tag) == 1);
    Require(Control(decrypt.get(), GetTag, 8, tag) == 0);
}

void CheckParametersFixedAtKey() {
    std::mt19937 random(33);
    const Bytes key = Random(random, 16);
    const Bytes nonce = Random(random, 12);
    const Bytes plain = Random(random, 20);
    Context context;
    LibreSslEvpCipherCtx* ctx = context.get();
    Require(EVP_EncryptInit_ex_nid_postfix(ctx, EVP_aes_128_ccm_nid_postfix(), nullptr, nullptr, nullptr) == 1);
    Require(Control(ctx, SetIvLength, 12, nullptr) == 1);
    Require(EVP_EncryptInit_ex_nid_postfix(ctx, nullptr, nullptr, key.data(), nonce.data()) == 1);
    Require(Control(ctx, SetTag, 16, nullptr) == 1);
    Bytes text(plain.size());
    int written = -1;
    Require(EVP_EncryptUpdate_nid_postfix(ctx, text.data(), &written, plain.data(), static_cast<int>(plain.size())) == 1);
    const Sealed expected = Seal(key, nonce, {}, plain, 12);
    Require(text == expected.text);
    Bytes tag(16);
    Require(Control(ctx, GetTag, 16, tag.data()) == 0);
    Require(Control(ctx, GetTag, 12, tag.data()) == 1);
    tag.resize(12);
    Require(tag == expected.tag);

    Require(EVP_EncryptInit_ex_nid_postfix(ctx, nullptr, nullptr, nullptr, nonce.data()) == 1);
    Require(Control(ctx, SetIvLength, 7, nullptr) == 1);
    written = -1;
    Require(EVP_EncryptUpdate_nid_postfix(ctx, text.data(), &written, plain.data(), static_cast<int>(plain.size())) == 0);
    Require(Control(ctx, SetIvLength, 12, nullptr) == 1);
    Require(EVP_EncryptUpdate_nid_postfix(ctx, text.data(), &written, plain.data(), static_cast<int>(plain.size())) == 1 && text == expected.text);
    Require(Control(ctx, GetTag, 12, tag.data()) == 1 && tag == expected.tag);

    Bytes longer = nonce;
    longer.push_back(0x5A);
    Require(Control(ctx, SetIvLength, 13, nullptr) == 1);
    Require(EVP_EncryptInit_ex_nid_postfix(ctx, nullptr, nullptr, nullptr, longer.data()) == 1);
    Require(EVP_EncryptUpdate_nid_postfix(ctx, text.data(), &written, plain.data(), static_cast<int>(plain.size())) == 1 && text == expected.text);
}

void CheckSequencing() {
    std::mt19937 random(34);
    const Bytes key = Random(random, 16);
    const Bytes nonce = Random(random, 7);
    const Bytes aad = Random(random, 30);
    const Bytes plain = Random(random, 40);
    const Sealed expected = Seal(key, nonce, aad, plain, 12);
    Context context;
    LibreSslEvpCipherCtx* ctx = context.get();
    int written = -1;
    Bytes text(plain.size());

    Require(EVP_EncryptInit_ex_nid_postfix(ctx, EVP_aes_128_ccm_nid_postfix(), nullptr, nullptr, nullptr) == 1);
    Require(EVP_EncryptUpdate_nid_postfix(ctx, nullptr, &written, nullptr, 10) == 0);
    Require(EVP_EncryptInit_ex_nid_postfix(ctx, nullptr, nullptr, nullptr, nonce.data()) == 1);
    Require(EVP_EncryptUpdate_nid_postfix(ctx, nullptr, &written, nullptr, 10) == 0);

    Require(EVP_EncryptInit_ex_nid_postfix(ctx, nullptr, nullptr, key.data(), nonce.data()) == 1);
    Require(EVP_EncryptUpdate_nid_postfix(ctx, nullptr, &written, aad.data(), static_cast<int>(aad.size())) == 0);
    written = -1;
    Require(EVP_EncryptUpdate_nid_postfix(ctx, nullptr, &written, aad.data(), 0) == 1 && written == 0);
    written = -1;
    Require(EVP_EncryptUpdate_nid_postfix(ctx, nullptr, &written, nullptr, -1) == 0);
    Require(EVP_EncryptUpdate_nid_postfix(ctx, text.data(), &written, plain.data(), -5) == 0);
    SetLength(ctx, plain.size());
    AddAad(ctx, aad);
    Require(EVP_EncryptUpdate_nid_postfix(ctx, text.data(), &written, plain.data(), -5) == 0);
    written = -1;
    Require(EVP_EncryptUpdate_nid_postfix(ctx, text.data(), &written, plain.data(), static_cast<int>(plain.size())) == 1);
    Require(written == static_cast<int>(plain.size()) && text == expected.text);
    Require(EVP_EncryptUpdate_nid_postfix(ctx, text.data(), &written, plain.data(), static_cast<int>(plain.size())) == 0);
    Bytes tag(12);
    Require(Control(ctx, GetTag, 12, tag.data()) == 0 && Control(ctx, GetTag, 2, tag.data()) == 1);

    Require(EVP_EncryptInit_ex_nid_postfix(ctx, nullptr, nullptr, nullptr, nonce.data()) == 1);
    SetLength(ctx, plain.size() + 1);
    Require(EVP_EncryptUpdate_nid_postfix(ctx, text.data(), &written, plain.data(), static_cast<int>(plain.size())) == 0);

    Require(EVP_EncryptInit_ex_nid_postfix(ctx, EVP_aes_128_ccm_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    SetLength(ctx, plain.size());
    AddAad(ctx, aad);
    Bytes inPlace = plain;
    Require(EVP_EncryptUpdate_nid_postfix(ctx, inPlace.data(), &written, inPlace.data(), static_cast<int>(inPlace.size())) == 1);
    Require(inPlace == expected.text);
    Require(Control(ctx, GetTag, 12, tag.data()) == 1 && tag == expected.tag);

    Context decrypt;
    Setup(decrypt.get(), false, key, nonce, 12, expected.tag.data(), false);
    SetLength(decrypt.get(), inPlace.size());
    AddAad(decrypt.get(), aad);
    Require(EVP_DecryptUpdate_nid_postfix(decrypt.get(), inPlace.data(), &written, inPlace.data(), static_cast<int>(inPlace.size())) == 1);
    Require(written == static_cast<int>(plain.size()) && inPlace == plain);
    Require(EVP_DecryptUpdate_nid_postfix(decrypt.get(), inPlace.data(), &written, inPlace.data(), static_cast<int>(inPlace.size())) == 0);

    Context missing;
    Require(EVP_DecryptInit_ex_nid_postfix(missing.get(), EVP_aes_128_ccm_nid_postfix(), nullptr, key.data(), nonce.data()) == 1);
    Require(EVP_DecryptUpdate_nid_postfix(missing.get(), nullptr, &written, nullptr, static_cast<int>(plain.size())) == 0 && written == 0);
}

void CheckEmptyMessage() {
    std::mt19937 random(35);
    const Bytes key = Random(random, 16);
    const Bytes nonce = Random(random, 10);
    const Bytes aad = Random(random, 21);
    const Sealed expected = Seal(key, nonce, aad, {}, 10);
    Context context;
    Setup(context.get(), true, key, nonce, 10, nullptr, false);
    SetLength(context.get(), 0);
    AddAad(context.get(), aad);
    int written = -1;
    unsigned char tail[1];
    Require(EVP_EncryptUpdate_nid_postfix(context.get(), tail, &written, tail, 0) == 1 && written == 0);
    Bytes tag(10);
    Require(Control(context.get(), GetTag, 10, tag.data()) == 1 && tag == expected.tag);
    Context before;
    Setup(before.get(), true, key, nonce, 10, nullptr, false);
    SetLength(before.get(), 0);
    AddAad(before.get(), aad);
    Require(EVP_EncryptFinal_ex_nid_postfix(before.get(), tail, &written) == 1 && written == 0);
    Require(Control(before.get(), GetTag, 10, tag.data()) == 0);
}

void CheckCopy() {
    std::mt19937 random(36);
    const Bytes key = Random(random, 16);
    const Bytes nonce = Random(random, 7);
    const Bytes plain = Random(random, 33);
    const Sealed expected = Seal(key, nonce, {}, plain, 12);
    auto* source = EVP_CIPHER_CTX_new_nid_postfix();
    Require(source != nullptr);
    Require(EVP_EncryptInit_ex_nid_postfix(source, EVP_aes_128_ccm_nid_postfix(), nullptr, nullptr, nullptr) == 1);
    Context empty;
    Require(Control(source, ControlCopy, 0, empty.get()) == 1);
    Require(EVP_EncryptInit_ex_nid_postfix(source, nullptr, nullptr, key.data(), nonce.data()) == 1);
    Context copy;
    LibreSslEvpCipherCtx* target = copy.get();
    std::memcpy(target, source, sizeof(LibreSslEvpCipherCtx));
    target->cipherData = malloc_nid_postfix(static_cast<std::size_t>(source->cipher->contextSize));
    Require(target->cipherData != nullptr);
    std::memcpy(target->cipherData, source->cipherData, static_cast<std::size_t>(source->cipher->contextSize));
    Require(Control(source, ControlCopy, 0, target) == 1);
    EVP_CIPHER_CTX_free_nid_postfix(source);
    Bytes text(plain.size());
    int written = -1;
    Require(EVP_EncryptUpdate_nid_postfix(target, text.data(), &written, plain.data(), static_cast<int>(plain.size())) == 1);
    Bytes tag(12);
    Require(text == expected.text && Control(target, GetTag, 12, tag.data()) == 1 && tag == expected.tag);
}

} // namespace

int main() {
    void* api[10] {reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
                   reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
                   reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    CheckObject();
    CheckNistVectors();
    CheckRandom();
    CheckDefaults();
    CheckControls();
    CheckParametersFixedAtKey();
    CheckSequencing();
    CheckEmptyMessage();
    CheckCopy();
    std::puts("LibreSSL3 AES-CCM: ok");
    return 0;
}
