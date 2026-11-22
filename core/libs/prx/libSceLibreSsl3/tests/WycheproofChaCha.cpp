#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Cipher.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include "WycheproofChaChaVectors.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <source_location>

namespace {

using Bytes = std::vector<unsigned char>;

constexpr int SetIvLength = 0x9;
constexpr int GetTag = 0x10;
constexpr int SetTag = 0x11;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 Wycheproof ChaCha check failed at line %u\n", location.line());
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

Bytes FromHex(const char* hex) {
    Bytes bytes;
    for (; hex[0] != 0 && hex[1] != 0; hex += 2) bytes.push_back(static_cast<unsigned char>(std::stoul(std::string(hex, 2), nullptr, 16)));
    return bytes;
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

void CheckWycheproofVector(const LibreSsl3WycheproofChaCha::Vector& vector) {
    const Bytes key = FromHex(vector.keyHex);
    const Bytes iv = FromHex(vector.ivHex);
    const Bytes aad = FromHex(vector.aadHex);
    const Bytes msg = FromHex(vector.msgHex);
    const Bytes expectedCt = FromHex(vector.ctHex);
    const Bytes expectedTag = FromHex(vector.tagHex);
    const bool expectedValid = vector.expectedValid == 1;

    Context encryptCtx;
    Require(EVP_EncryptInit_ex_nid_postfix(encryptCtx.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), iv.data()) == 1);

    int ivLength = 0;
    const int setIvLenEncrypt = EVP_CIPHER_CTX_ctrl_nid_postfix(encryptCtx.get(), SetIvLength, static_cast<int>(iv.size()), &ivLength);
    if (setIvLenEncrypt != 1) {
        Require(!expectedValid);
        return;
    }

    int aadOk = 1;
    if (!aad.empty()) {
        int written = -1;
        aadOk = EVP_EncryptUpdate_nid_postfix(encryptCtx.get(), nullptr, &written, aad.data(), static_cast<int>(aad.size()));
    }

    int updateOk = 1;
    Bytes ciphertext;
    if (!msg.empty()) {
        ciphertext = Update(encryptCtx.get(), true, msg, 0, msg.size());
    }

    int written = -1;
    unsigned char tail[1];
    const int finalOk = EVP_EncryptFinal_ex_nid_postfix(encryptCtx.get(), tail, &written);

    int getTagOk = 1;
    Bytes tag(16);
    getTagOk = EVP_CIPHER_CTX_ctrl_nid_postfix(encryptCtx.get(), GetTag, 16, tag.data());

    if (expectedValid) {
        Require(aadOk == 1);
        Require(updateOk == 1);
        Require(finalOk == 1 && written == 0);
        Require(getTagOk == 1);
        Require(ciphertext == expectedCt);
        Require(Bytes(tag.data(), tag.data() + 16) == expectedTag);
    } else {
        Require(aadOk != 1 || updateOk != 1 || finalOk != 1 || getTagOk != 1 || ciphertext != expectedCt || Bytes(tag.data(), tag.data() + 16) != expectedTag);
    }

    Context decryptCtx;
    Require(EVP_DecryptInit_ex_nid_postfix(decryptCtx.get(), EVP_chacha20_poly1305_nid_postfix(), nullptr, key.data(), iv.data()) == 1);

    ivLength = 0;
    const int setIvLenDecrypt = EVP_CIPHER_CTX_ctrl_nid_postfix(decryptCtx.get(), SetIvLength, static_cast<int>(iv.size()), &ivLength);
    if (setIvLenDecrypt != 1) {
        Require(!expectedValid);
        return;
    }

    int setTagOk = 1;
    if (!expectedTag.empty()) {
        setTagOk = EVP_CIPHER_CTX_ctrl_nid_postfix(decryptCtx.get(), SetTag, static_cast<int>(expectedTag.size()), const_cast<unsigned char*>(expectedTag.data()));
    }

    int aadDecOk = 1;
    if (!aad.empty()) {
        int writtenDec = -1;
        aadDecOk = EVP_DecryptUpdate_nid_postfix(decryptCtx.get(), nullptr, &writtenDec, aad.data(), static_cast<int>(aad.size()));
    }

    int updateDecOk = 1;
    Bytes plaintext;
    if (!expectedCt.empty()) {
        plaintext = Update(decryptCtx.get(), false, expectedCt, 0, expectedCt.size());
    }

    unsigned char tailDec[1];
    const int decryptFinal = EVP_DecryptFinal_ex_nid_postfix(decryptCtx.get(), tailDec, &written);

    if (expectedValid) {
        Require(setTagOk == 1);
        Require(aadDecOk == 1);
        Require(updateDecOk == 1);
        Require(decryptFinal == 1);
        Require(plaintext == msg);
    } else {
        Require(setTagOk != 1 || aadDecOk != 1 || updateDecOk != 1 || decryptFinal != 1);
    }
}

}

int main() {
    void* api[10] {reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
                   reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
                   reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);

    for (const auto& vector : LibreSsl3WycheproofChaCha::Vectors) {
        CheckWycheproofVector(vector);
    }

    std::puts("LibreSSL3 Wycheproof ChaCha: ok");
    return 0;
}
