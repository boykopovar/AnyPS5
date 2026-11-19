#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
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

template<typename TInit, typename TUpdate, typename TFinal>
void VerifySha512DigestCleanup(
    TInit init,
    TUpdate update,
    TFinal finalFn,
    std::size_t expectedLength,
    const char* input,
    const char* expectedHex,
    const char* label) {
    Sha512Context ctx;
    std::memset(&ctx, 0xa5, sizeof(ctx));
    Require(init(&ctx) == 1, label);
    Require(ctx.num == 0 && ctx.mdLength == expectedLength, label);
    Require(update(&ctx, input, std::strlen(input)) == 1, label);
    unsigned char digest[64];
    std::memset(digest, 0xee, sizeof(digest));
    Require(finalFn(digest, &ctx) == 1, label);
    Require(Hex(digest, expectedLength) == expectedHex, label);
    for (std::size_t i = expectedLength; i < sizeof(digest); ++i) {
        Require(digest[i] == 0xee, label);
    }
    Require(ctx.num == 0, label);
    const auto* dataBytes = reinterpret_cast<const unsigned char*>(ctx.data);
    for (std::size_t i = 0; i < sizeof(ctx.data); ++i) {
        Require(dataBytes[i] == 0, label);
    }
}

void VerifySha512FamilyFinalCleanup() {
    static constexpr const char* ShortInput = "abc";
    static constexpr const char* TwoBlockInput =
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";

    VerifySha512DigestCleanup(
        SHA384_Init_nid_postfix,
        SHA384_Update_nid_postfix,
        SHA384_Final_nid_postfix,
        48,
        ShortInput,
        "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7",
        "SHA384 short input digest, num reset, and buffer wipe");
    VerifySha512DigestCleanup(
        SHA384_Init_nid_postfix,
        SHA384_Update_nid_postfix,
        SHA384_Final_nid_postfix,
        48,
        TwoBlockInput,
        "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712fcc7c71a557e2db966c3e9fa91746039",
        "SHA384 two-block input digest, num reset, and buffer wipe");
    VerifySha512DigestCleanup(
        SHA512_Init_nid_postfix,
        SHA512_Update_nid_postfix,
        SHA512_Final_nid_postfix,
        64,
        ShortInput,
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
        "SHA512 short input digest, num reset, and buffer wipe");
    VerifySha512DigestCleanup(
        SHA512_Init_nid_postfix,
        SHA512_Update_nid_postfix,
        SHA512_Final_nid_postfix,
        64,
        TwoBlockInput,
        "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909",
        "SHA512 two-block input digest, num reset, and buffer wipe");

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

    VerifySha512FamilyFinalCleanup();
    return 0;
}
