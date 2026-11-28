#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireInvalidArgument(TAction action, const char* message) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    } catch (const std::exception&) {
    }
    Require(false, message);
}

std::string Hex(const unsigned char* data, std::size_t size) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    text.reserve(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        text += digits[data[i] >> 4];
        text += digits[data[i] & 15];
    }
    return text;
}

std::vector<unsigned char> FinalBlock(const char* input) {
    const std::size_t length = std::strlen(input);
    std::vector<unsigned char> block(128, 0);
    if (length < 112) {
        std::memcpy(block.data(), input, length);
        block[length] = 0x80;
    }
    const std::uint64_t bits = static_cast<std::uint64_t>(length) * 8;
    for (int index = 0; index < 8; ++index) block[127 - index] = static_cast<unsigned char>(bits >> (8 * index));
    return block;
}

template<typename TInit, typename TUpdate, typename TFinal>
void VerifySha512ContextAfterFinal(
    TInit init,
    TUpdate update,
    TFinal finalFn,
    std::size_t expectedLength,
    const char* input,
    const char* expectedHex,
    const char* secondHex,
    const char* appendedHex,
    const char* label) {
    const std::size_t inputLength = std::strlen(input);
    Sha512Context ctx;
    std::memset(&ctx, 0xa5, sizeof(ctx));
    Require(init(&ctx) == 1, label);
    Require(ctx.num == 0 && ctx.mdLength == expectedLength, label);
    Require(update(&ctx, input, inputLength) == 1, label);
    unsigned char digest[64];
    std::memset(digest, 0xee, sizeof(digest));
    Require(finalFn(digest, &ctx) == 1, label);
    Require(Hex(digest, expectedLength) == expectedHex, label);
    for (std::size_t i = expectedLength; i < sizeof(digest); ++i) {
        Require(digest[i] == 0xee, label);
    }
    Require(ctx.num == inputLength, label);
    const std::vector<unsigned char> block = FinalBlock(input);
    Require(std::memcmp(ctx.data, block.data(), block.size()) == 0, label);
    Require(finalFn(digest, &ctx) == 1, label);
    Require(Hex(digest, expectedLength) == secondHex, label);
    Require(ctx.num == inputLength, label);

    Require(init(&ctx) == 1, label);
    Require(update(&ctx, input, inputLength) == 1, label);
    Require(finalFn(digest, &ctx) == 1, label);
    Require(update(&ctx, "def", 3) == 1, label);
    Require(ctx.num == inputLength + 3, label);
    Require(finalFn(digest, &ctx) == 1, label);
    Require(Hex(digest, expectedLength) == appendedHex, label);
}

void VerifySha512FamilyFinal() {
    static constexpr const char* ShortInput = "abc";
    static constexpr const char* TwoBlockInput =
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";

    VerifySha512ContextAfterFinal(
        SHA384_Init_nid_postfix,
        SHA384_Update_nid_postfix,
        SHA384_Final_nid_postfix,
        48,
        ShortInput,
        "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7",
        "2a1caddc118b987c8aa64bd63140e4bcc0ae29e9090c4803cc4e648c0654a2f051285ad03bc67f8b6f38c19ee02c8104",
        "3ba684ed7dc733d5cc04a3268f1e310908946884d078b5aede6ee8b7ba34356ce6e3b61df12918473049da1003b110a0",
        "SHA384 short input: digest, context kept by Final, second Final and Update after Final");
    VerifySha512ContextAfterFinal(
        SHA384_Init_nid_postfix,
        SHA384_Update_nid_postfix,
        SHA384_Final_nid_postfix,
        48,
        TwoBlockInput,
        "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712fcc7c71a557e2db966c3e9fa91746039",
        "494bc66e2253abff275e2fbcca77d9f92cae77b6508a8c064e5967fdf7e08d4b1b8d13ed23422bde684ab3ac6d13c27f",
        "7db8d0f627fc78e94d724795577b25986e48955934308dfe4b29ad98fed9b4cfb807dab93f163b27cec6970d08387ab3",
        "SHA384 two-block input: digest, context kept by Final, second Final and Update after Final");
    VerifySha512ContextAfterFinal(
        SHA512_Init_nid_postfix,
        SHA512_Update_nid_postfix,
        SHA512_Final_nid_postfix,
        64,
        ShortInput,
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
        "64d920ef5e0e9f5f8c8668b636bc270ffa4560516a42b0c823c9280ab4d7d7e2dd527d496b4bbbc8f177357d07f9fa65ff4c3e0b131f6897cd505e97b8ecb081",
        "b59b4418c3de564e4c055d5300e88143d4e7055f916879cdae9b323f0a38d444899ab3f3bd23415901ee2ba453537b4d0c47e950fde7f782ec3ed3de79455389",
        "SHA512 short input: digest, context kept by Final, second Final and Update after Final");
    VerifySha512ContextAfterFinal(
        SHA512_Init_nid_postfix,
        SHA512_Update_nid_postfix,
        SHA512_Final_nid_postfix,
        64,
        TwoBlockInput,
        "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909",
        "d5091e5830649b67a95da634cbff6bbfe3d0ebb1c1000f1d5975187f2dd048623343d0257cd24d64ca4bcab56d10b2b85ef9f2578499205c9eb00df99e0561d1",
        "e362c2d116ac2b078dd05930e102c245fda226676c36f761dd141d9fdd3d9f7633408c964e0010fe9e14d92432a8893c3bdfe9a65970976de1f0e889e9fcf0cb",
        "SHA512 two-block input: digest, context kept by Final, second Final and Update after Final");

    Sha512Context ctx;
    std::memset(&ctx, 0xa5, sizeof(ctx));
    Require(SHA384_Init_nid_postfix(&ctx) == 1, "SHA384 init before null digest check");
    Require(SHA384_Update_nid_postfix(&ctx, ShortInput, std::strlen(ShortInput)) == 1, "SHA384 update before null digest check");
    Require(SHA384_Final_nid_postfix(nullptr, &ctx) == 0, "SHA384_Final rejects null digest");

    std::memset(&ctx, 0xa5, sizeof(ctx));
    Require(SHA512_Init_nid_postfix(&ctx) == 1, "SHA512 init before null digest check");
    Require(SHA512_Update_nid_postfix(&ctx, ShortInput, std::strlen(ShortInput)) == 1, "SHA512 update before null digest check");
    Require(SHA512_Final_nid_postfix(nullptr, &ctx) == 0, "SHA512_Final rejects null digest");

    unsigned char digest[64];
    std::memset(&ctx, 0xa5, sizeof(ctx));
    Require(SHA512_Init_nid_postfix(&ctx) == 1, "SHA512 init before invalid mdLength check");
    ctx.mdLength = 32;
    Require(SHA512_Final_nid_postfix(digest, &ctx) == 0, "SHA512_Final rejects invalid mdLength");

    std::memset(&ctx, 0xa5, sizeof(ctx));
    Require(SHA384_Init_nid_postfix(&ctx) == 1, "SHA384 init before zero mdLength check");
    ctx.mdLength = 0;
    Require(SHA384_Final_nid_postfix(digest, &ctx) == 0, "SHA384_Final rejects zero mdLength");
}

void VerifySslGetExData() {
    LibreSslSslInternal internal{};
    LibreSslSsl ssl{};
    ssl.internal = &internal;

    Require(SSL_get_ex_data_nid_postfix(&ssl, 0) == nullptr, "an SSL without stored ex-data returns null");

    void* first = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x11));
    void* second = reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x22));
    char* elements[2] = {static_cast<char*>(first), static_cast<char*>(second)};
    LibreSslStack stack{};
    stack.num = 2;
    stack.data = elements;
    internal.exData.stack = &stack;

    Require(SSL_get_ex_data_nid_postfix(&ssl, 0) == first, "SSL ex-data returns the element at the index");
    Require(SSL_get_ex_data_nid_postfix(&ssl, 1) == second, "SSL ex-data returns the last stored element");
    Require(SSL_get_ex_data_nid_postfix(&ssl, 2) == nullptr, "an index at the end of the stack returns null");
    Require(SSL_get_ex_data_nid_postfix(&ssl, -1) == nullptr, "a negative index returns null");

    LibreSslSsl noInternal{};
    RequireInvalidArgument([&] { SSL_get_ex_data_nid_postfix(&noInternal, 0); }, "a null internal state throws");
    RequireInvalidArgument([&] { SSL_get_ex_data_nid_postfix(nullptr, 0); }, "a null connection throws");
}

int APS5_VABI NewCallback(void*, void*, LibreSslExData*, int, std::int64_t, void*) {
    std::abort();
}

void APS5_VABI FreeCallback(void*, void*, LibreSslExData*, int, std::int64_t, void*) {
    std::abort();
}

int APS5_VABI DupCallback(LibreSslExData*, LibreSslExData*, void*, int, std::int64_t, void*) {
    std::abort();
}

int NewSslIndex() {
    return SSL_get_ex_new_index_nid_postfix(7, nullptr, nullptr, nullptr, nullptr);
}

int NewContextIndex() {
    int marker = 0;
    return SSL_CTX_get_ex_new_index_nid_postfix(-1, &marker, nullptr, nullptr, nullptr);
}

}

int main() {
    Require(NewSslIndex() == 0 && NewSslIndex() == 1 && NewSslIndex() == 2, "SSL indices count from 0");
    Require(NewContextIndex() == 0 && NewContextIndex() == 1, "SSL_CTX indices are counted apart from SSL ones");
    Require(SSL_get_ex_data_X509_STORE_CTX_idx_nid_postfix() == 0, "the verify-callback index is the first X509_STORE_CTX index");
    Require(SSL_get_ex_data_X509_STORE_CTX_idx_nid_postfix() == 0, "the verify-callback index is allocated once");
    Require(NewSslIndex() == 3, "the verify-callback index does not use an SSL index");

    CRYPTO_cleanup_all_ex_data_nid_postfix();
    Require(NewSslIndex() == 0 && NewContextIndex() == 0, "cleanup releases every class");
    Require(SSL_get_ex_data_X509_STORE_CTX_idx_nid_postfix() == 0, "cleanup keeps the cached verify-callback index");
    CRYPTO_cleanup_all_ex_data_nid_postfix();
    CRYPTO_cleanup_all_ex_data_nid_postfix();
    Require(NewContextIndex() == 0, "cleanup can be repeated");

    CRYPTO_cleanup_all_ex_data_nid_postfix();
    int argument = 0;
    Require(SSL_get_ex_new_index_nid_postfix(-5, &argument, NewCallback, DupCallback, FreeCallback) == 0, "typed SSL callbacks are accepted and stored");
    Require(SSL_CTX_get_ex_new_index_nid_postfix(9, &argument, NewCallback, DupCallback, FreeCallback) == 0, "typed SSL_CTX callbacks are accepted and stored");
    Require(NewSslIndex() == 1 && NewContextIndex() == 1, "indices with callbacks count like any other");
    CRYPTO_cleanup_all_ex_data_nid_postfix();
    std::vector<std::thread> threads;
    std::vector<int> indices(64, -1);
    for (int thread = 0; thread < 8; ++thread)
        threads.emplace_back([&indices, thread] {
            for (int round = 0; round < 8; ++round) indices[thread * 8 + round] = NewSslIndex();
        });
    for (auto& thread : threads) thread.join();
    std::vector<bool> seen(indices.size(), false);
    for (const int index : indices) {
        Require(index >= 0 && index < static_cast<int>(indices.size()) && !seen[index], "concurrent callers get distinct indices");
        seen[index] = true;
    }

    VerifySha512FamilyFinal();
    VerifySslGetExData();
    return 0;
}
