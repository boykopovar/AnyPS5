#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

constexpr std::uint64_t BadDecryptError = 0x06fff000 | 100;
constexpr std::uint64_t WrongFinalBlockLengthError = 0x06fff000 | 109;
constexpr std::uint64_t BadBlockLengthError = 0x06fff000 | 136;
constexpr std::uint64_t NotMultipleError = 0x06fff000 | 138;
constexpr unsigned char XorByte = 0x5a;

struct Record {
    int calls = 0;
    const unsigned char* in = nullptr;
    std::size_t length = 0;
    int result = 1;
};

Record record;

int APS5_VABI BlockCipher(LibreSslEvpCipherCtx*, unsigned char* out, const unsigned char* in, std::size_t length) {
    ++record.calls;
    record.in = in;
    record.length = length;
    if (record.result <= 0) return record.result;
    for (std::size_t index = 0; index < length; ++index) out[index] = static_cast<unsigned char>(in[index] ^ XorByte);
    return 1;
}

int APS5_VABI CustomCipher(LibreSslEvpCipherCtx*, unsigned char*, const unsigned char* in, std::size_t length) {
    ++record.calls;
    record.in = in;
    record.length = length;
    return record.result < 0 ? record.result : 7;
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_*Final_ex check failed at line %u\n", location.line());
    std::abort();
}

void Reset(int result = 1) {
    record = {};
    record.result = result;
    ERR_clear_error_nid_postfix();
}

void RequireQueue(std::uint64_t code) {
    Require(ERR_get_error_nid_postfix() == code && ERR_get_error_nid_postfix() == 0);
}

LibreSslEvpCipher Cipher(int blockSize, std::uint64_t flags, decltype(LibreSslEvpCipher::doCipher) doCipher) {
    LibreSslEvpCipher cipher{};
    cipher.blockSize = blockSize;
    cipher.flags = flags;
    cipher.doCipher = doCipher;
    return cipher;
}

LibreSslEvpCipherCtx Context(const LibreSslEvpCipher& cipher, int encrypt) {
    LibreSslEvpCipherCtx context{};
    context.cipher = &cipher;
    context.encrypt = encrypt;
    return context;
}

void CheckCustomCipher() {
    const auto custom = Cipher(1, CipherFlagCustomCipher, CustomCipher);
    auto context = Context(custom, 1);
    unsigned char out[8]{};
    int written = 77;
    Reset();
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&context, out, &written) == 1 && written == 7 && record.calls == 1 && !record.in && record.length == 0);
    Reset(-1);
    written = 77;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&context, out, &written) == 0 && written == 0 && record.calls == 1);
}

void CheckEncrypt() {
    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto context = Context(block, 1);
    std::array<unsigned char, 16> out{};
    int written = 77;
    for (int index = 0; index < 5; ++index) context.buf[index] = static_cast<unsigned char>(index + 1);
    context.bufLen = 5;
    Reset();
    Require(EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&context, out.data(), &written) == 1 && written == 16 && record.calls == 1 && record.length == 16);
    for (int index = 0; index < 16; ++index) {
        const unsigned char plain = index < 5 ? static_cast<unsigned char>(index + 1) : 11;
        Require(out[static_cast<std::size_t>(index)] == static_cast<unsigned char>(plain ^ XorByte));
    }

    auto empty = Context(block, 1);
    Reset();
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&empty, out.data(), &written) == 1 && written == 16 && empty.buf[0] == 16 && empty.buf[15] == 16);

    Reset(0);
    written = 77;
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&empty, out.data(), &written) == 0 && written == 0 && record.calls == 1);

    auto unpadded = Context(block, 1);
    unpadded.flags = CipherFlagNoPadding;
    unpadded.bufLen = 3;
    Reset();
    written = 77;
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&unpadded, out.data(), &written) == 0 && written == 0 && record.calls == 0);
    RequireQueue(NotMultipleError);
    unpadded.bufLen = 0;
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&unpadded, out.data(), &written) == 1 && written == 0 && record.calls == 0);

    auto corrupt = Context(block, 1);
    corrupt.bufLen = 16;
    Reset();
    written = 77;
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&corrupt, out.data(), &written) == 0 && written == 0 && record.calls == 0);
    RequireQueue(BadBlockLengthError);

    const auto stream = Cipher(1, 0, BlockCipher);
    auto streamContext = Context(stream, 1);
    Reset();
    written = 77;
    Require(EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&streamContext, out.data(), &written) == 1 && written == 0 && record.calls == 0);
}

void Hold(LibreSslEvpCipherCtx& context, std::size_t plain, unsigned char pad) {
    for (std::size_t index = 0; index < 16; ++index) context.final[index] = index < plain ? static_cast<unsigned char>(0xa0 + index) : pad;
    context.finalUsed = 1;
    context.bufLen = 0;
}

void CheckDecrypt() {
    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    std::array<unsigned char, 16> out{};
    int written = 77;

    auto context = Context(block, 0);
    Hold(context, 10, 6);
    Reset();
    Require(EVP_CipherFinal_ex_nid_disambig1_nid_postfix(&context, out.data(), &written) == 1 && written == 10 && record.calls == 0);
    for (std::size_t index = 0; index < 10; ++index) Require(out[index] == static_cast<unsigned char>(0xa0 + index));

    Hold(context, 0, 16);
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&context, nullptr, &written) == 1 && written == 0);

    Hold(context, 15, 0);
    Reset();
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&context, out.data(), &written) == 0 && written == 0);
    RequireQueue(BadDecryptError);
    Hold(context, 0, 17);
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&context, out.data(), &written) == 0);
    RequireQueue(BadDecryptError);
    Hold(context, 12, 4);
    context.final[13] = 5;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&context, out.data(), &written) == 0);
    RequireQueue(BadDecryptError);

    auto missing = Context(block, 0);
    written = 77;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&missing, out.data(), &written) == 0 && written == 0);
    RequireQueue(WrongFinalBlockLengthError);
    Hold(missing, 10, 6);
    missing.bufLen = 3;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&missing, out.data(), &written) == 0);
    RequireQueue(WrongFinalBlockLengthError);

    auto unpadded = Context(block, 0);
    unpadded.flags = CipherFlagNoPadding;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&unpadded, out.data(), &written) == 1 && written == 0);
    unpadded.bufLen = 2;
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&unpadded, out.data(), &written) == 0);
    RequireQueue(NotMultipleError);

    const auto stream = Cipher(1, 0, BlockCipher);
    auto streamContext = Context(stream, 0);
    Require(EVP_DecryptFinal_ex_nid_disambig1_nid_postfix(&streamContext, out.data(), &written) == 1 && written == 0);
    Require(ERR_get_error_nid_postfix() == 0 && record.calls == 0);
}

void CheckInvalidArguments() {
    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto context = Context(block, 1);
    unsigned char out[16]{};
    int written = 0;
    bool threw = false;
    try {
        EVP_CipherFinal_ex_nid_disambig1_nid_postfix(nullptr, out, &written);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    try {
        EVP_EncryptFinal_ex_nid_disambig1_nid_postfix(&context, out, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    CheckCustomCipher();
    CheckEncrypt();
    CheckDecrypt();
    CheckInvalidArguments();
    std::puts("LibreSSL3 EVP_*Final_ex: ok");
}
