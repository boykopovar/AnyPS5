#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Evp.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::uint64_t CipherModeCcm = 0x7;
constexpr std::uint64_t BadBlockLengthError = 0x06fff000 | 136;
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
    return record.result < 0 ? record.result : static_cast<int>(length);
}

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 EVP_*Update check failed at line %u\n", location.line());
    std::abort();
}

void Reset(int result = 1) {
    record = {};
    record.result = result;
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

std::vector<unsigned char> Plain(std::size_t size) {
    std::vector<unsigned char> data(size);
    for (std::size_t index = 0; index < size; ++index) data[index] = static_cast<unsigned char>(index * 7 + 3);
    return data;
}

void CheckCustomCipher() {
    const auto custom = Cipher(1, CipherFlagCustomCipher, CustomCipher);
    auto context = Context(custom, 1);
    unsigned char out[4]{};
    int written = 77;
    Reset();
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out, &written, nullptr, 0) == 1 && written == 0 && record.calls == 0);
    written = 77;
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, nullptr, &written, nullptr, 0) == 1 && written == 0 && record.calls == 0);
    written = 77;
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out, &written, out, -1) == 0 && written == 0 && record.calls == 0);
    written = 77;
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, out, &written, out, -1) == 0 && written == 0 && record.calls == 0);
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out, &written, out, 3) == 1 && written == 3 && record.calls == 1 && record.in == out && record.length == 3);
    Reset(-1);
    written = 77;
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, out, &written, out, 3) == 0 && written == 0 && record.calls == 1);

    const auto ccm = Cipher(1, CipherFlagCustomCipher | CipherModeCcm, CustomCipher);
    auto ccmContext = Context(ccm, 0);
    Reset();
    written = 77;
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&ccmContext, nullptr, &written, nullptr, 0) == 1 && written == 0 && record.calls == 1 && !record.in && record.length == 0);
    written = 77;
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&ccmContext, nullptr, &written, nullptr, 0) == 1 && written == 0 && record.calls == 2 && !record.in && record.length == 0);
}

void CheckEncryptBuffering() {
    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto context = Context(block, 1);
    const auto plain = Plain(37);
    std::array<unsigned char, 64> out{};
    int written = 77;
    Reset();
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data(), 0) == 1 && written == 0 && context.bufLen == 0);
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data(), 5) == 1 && written == 0 && context.bufLen == 5 && record.calls == 0);
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data() + 5, 3) == 1 && written == 0 && context.bufLen == 8 && record.calls == 0);
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data() + 8, 24) == 1 && written == 32 && context.bufLen == 0 && record.calls == 2);
    Require(EVP_CipherUpdate_nid_disambig1_nid_postfix(&context, out.data() + 32, &written, plain.data() + 32, 5) == 1 && written == 0 && context.bufLen == 5);
    for (std::size_t index = 0; index < 32; ++index) Require(out[index] == static_cast<unsigned char>(plain[index] ^ XorByte));

    Reset(0);
    written = 77;
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data(), 16) == 0 && written == 0 && record.calls == 1 && context.bufLen == 5);
}

void CheckDecryptHeldBlock() {
    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto context = Context(block, 0);
    const auto plain = Plain(48);
    std::array<unsigned char, 64> out{};
    int written = 77;
    Reset();
    Require(EVP_CipherUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data(), 32) == 1 && written == 16 && context.finalUsed == 1);
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, out.data() + 16, &written, plain.data() + 32, 16) == 1 && written == 16 && context.finalUsed == 1);
    for (std::size_t index = 0; index < 32; ++index) Require(out[index] == static_cast<unsigned char>(plain[index] ^ XorByte));
    for (std::size_t index = 0; index < 16; ++index) Require(context.final[index] == static_cast<unsigned char>(plain[32 + index] ^ XorByte));

    Reset(0);
    written = 77;
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), &written, plain.data(), 16) == 0 && written == 0 && context.finalUsed == 0);

    auto unpadded = Context(block, 0);
    unpadded.flags = CipherFlagNoPadding;
    Reset();
    Require(EVP_DecryptUpdate_nid_disambig1_nid_postfix(&unpadded, out.data(), &written, plain.data(), 32) == 1 && written == 32 && unpadded.finalUsed == 0);
}

void CheckInvalidState() {
    const auto odd = Cipher(12, CipherModeCbc, BlockCipher);
    auto oddContext = Context(odd, 1);
    const auto plain = Plain(16);
    std::array<unsigned char, 32> out{};
    int written = 77;
    Reset();
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&oddContext, out.data(), &written, plain.data(), 12) == 0 && written == 0 && record.calls == 0);

    const auto block = Cipher(16, CipherModeCbc, BlockCipher);
    auto corrupt = Context(block, 1);
    corrupt.bufLen = 16;
    ERR_clear_error_nid_postfix();
    written = 77;
    Require(EVP_EncryptUpdate_nid_disambig1_nid_postfix(&corrupt, out.data(), &written, plain.data(), 3) == 0 && written == 0 && record.calls == 0);
    Require(ERR_get_error_nid_postfix() == BadBlockLengthError && ERR_get_error_nid_postfix() == 0);

    bool threw = false;
    try {
        EVP_EncryptUpdate_nid_disambig1_nid_postfix(nullptr, out.data(), &written, plain.data(), 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    threw = false;
    auto context = Context(block, 1);
    try {
        EVP_DecryptUpdate_nid_disambig1_nid_postfix(&context, out.data(), nullptr, plain.data(), 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    CheckCustomCipher();
    CheckEncryptBuffering();
    CheckDecryptHeldBlock();
    CheckInvalidState();
    std::puts("LibreSSL3 EVP_*Update: ok");
}
