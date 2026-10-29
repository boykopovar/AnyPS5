#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
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
    return 0;
}
