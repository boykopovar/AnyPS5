#include "prx/libSceLibreSsl3/include/LibTls.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

using ConfigNew = TlsConfig* (APS5_VABI*)();
using ConfigFree = void (APS5_VABI*)(TlsConfig*);
using KeyMemory = int (APS5_VABI*)(TlsConfig*, const std::uint8_t*, std::size_t);
using Configure = int (APS5_VABI*)(TlsContext*, TlsConfig*);
using ContextFree = void (APS5_VABI*)(TlsContext*);
using Close = int (APS5_VABI*)(TlsContext*);

static_assert(std::is_same_v<decltype(&tls_config_new_nid_postfix), ConfigNew>);
static_assert(std::is_same_v<decltype(&tls_config_free_nid_postfix), ConfigFree>);
static_assert(std::is_same_v<decltype(&tls_config_set_key_mem_nid_postfix), KeyMemory>);
static_assert(std::is_same_v<decltype(&tls_configure_nid_postfix), Configure>);
static_assert(std::is_same_v<decltype(&tls_free_nid_postfix), ContextFree>);
static_assert(std::is_same_v<decltype(&tls_close_nid_postfix), Close>);

namespace {

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "libtls lifecycle test failed at line %d\n", line);
    std::abort();
}

#define Check(condition) Require((condition), __LINE__)

void TestDefaults() {
    Check(tls_config_error_nid_postfix(nullptr) == nullptr);
    Check(tls_error_nid_postfix(nullptr) == nullptr);
    Check(tls_close_nid_postfix(nullptr) == -1);
    Check(tls_config_set_key_mem_nid_postfix(nullptr, nullptr, 0) == -1);
    tls_config_free_nid_postfix(nullptr);
    tls_free_nid_postfix(nullptr);

    TlsConfig* config = tls_config_new_nid_postfix();
    Check(config != nullptr);
    Check(tls_config_error_nid_postfix(config) == nullptr);

    TlsContext* client = tls_client_nid_postfix();
    TlsContext* server = tls_server_nid_postfix();
    Check(client != nullptr && server != nullptr && client != server);
    Check(tls_error_nid_postfix(client) == nullptr);
    Check(tls_error_nid_postfix(server) == nullptr);
    Check(tls_close_nid_postfix(client) == 0);
    Check(tls_close_nid_postfix(client) == 0);
    Check(tls_close_nid_postfix(server) == -1);
    Check(std::strcmp(tls_error_nid_postfix(server), "invalid operation for context") == 0);

    Check(tls_configure_nid_postfix(client, config) == 0);
    Check(tls_configure_nid_postfix(client, config) == 0);
    Check(tls_configure_nid_postfix(client, nullptr) == 0);
    Check(tls_configure_nid_postfix(server, config) == -1);
    Check(std::strcmp(tls_error_nid_postfix(server), "private/public key mismatch") == 0);
    Check(tls_close_nid_postfix(server) == -1);
    Check(std::strcmp(tls_error_nid_postfix(server), "invalid operation for context") == 0);
    tls_config_free_nid_postfix(config);
    tls_free_nid_postfix(client);
    tls_free_nid_postfix(server);
}

void TestMemoryAndOwnership() {
    for (int iteration = 0; iteration != 8192; ++iteration) {
        TlsConfig* config = tls_config_new_nid_postfix();
        TlsConfig* other = tls_config_new_nid_postfix();
        Check(config && other && config != other);
        std::array<std::uint8_t, 320> key{};
        for (std::size_t i = 0; i < key.size(); ++i)
            key[i] = static_cast<std::uint8_t>(i * 47 + iteration);

        Check(tls_config_set_key_mem_nid_postfix(config, key.data(), key.size()) == 0);
        Check(tls_config_error_nid_postfix(config) == nullptr);
        TlsContext* first = tls_client_nid_postfix();
        TlsContext* second = tls_client_nid_postfix();
        Check(first && second);
        Check(tls_configure_nid_postfix(first, config) == 0);
        Check(tls_configure_nid_postfix(second, config) == 0);

        std::memset(key.data(), 0xCD, key.size());
        Check(tls_config_set_key_mem_nid_postfix(config, nullptr, 0) == 0);
        Check(tls_config_set_key_mem_nid_postfix(config, key.data(), key.size()) == 0);
        Check(tls_config_set_key_mem_nid_postfix(config, nullptr, key.size()) == 0);
        Check(tls_config_error_nid_postfix(config) == nullptr);
        Check(tls_config_set_key_mem_nid_postfix(config, key.data(), key.size()) == 0);

        tls_config_free_nid_postfix(config);
        Check(tls_close_nid_postfix(first) == 0);
        Check(tls_configure_nid_postfix(second, other) == 0);
        tls_config_free_nid_postfix(other);
        Check(tls_close_nid_postfix(second) == 0);
        tls_free_nid_postfix(first);
        tls_free_nid_postfix(second);
    }
}

}

int main() {
    TestDefaults();
    TestMemoryAndOwnership();
    std::puts("libtls lifecycle and key-memory tests passed");
    return 0;
}
