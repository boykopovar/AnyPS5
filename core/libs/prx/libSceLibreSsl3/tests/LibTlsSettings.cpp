#include "prx/libSceLibreSsl3/include/LibTls.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

using SetMemory = int (APS5_VABI*)(TlsConfig*, const std::uint8_t*, std::size_t);
using SetProtocols = int (APS5_VABI*)(TlsConfig*, std::uint32_t);
using SetFlag = void (APS5_VABI*)(TlsConfig*);

static_assert(std::is_same_v<decltype(&tls_config_set_ca_mem_nid_postfix), SetMemory>);
static_assert(std::is_same_v<decltype(&tls_config_set_protocols_nid_postfix), SetProtocols>);
static_assert(std::is_same_v<decltype(&tls_config_verify_client_nid_postfix), SetFlag>);
static_assert(std::is_same_v<decltype(&tls_config_insecure_noverifyname_nid_postfix), SetFlag>);

namespace {

void Require(bool valid, int line) {
    if (valid) return;
    std::fprintf(stderr, "libtls settings test failed at line %d\n", line);
    std::abort();
}

#define Check(value) Require((value), __LINE__)

void TestConfiguration() {
    Check(tls_config_set_ca_mem_nid_postfix(nullptr, nullptr, 0) == -1);
    Check(tls_config_set_protocols_nid_postfix(nullptr, 24) == -1);
    tls_config_verify_client_nid_postfix(nullptr);
    tls_config_insecure_noverifyname_nid_postfix(nullptr);

    for (int iteration = 0; iteration != 8192; ++iteration) {
        TlsConfig* config = tls_config_new_nid_postfix();
        Check(config != nullptr);
        std::array<std::uint8_t, 257> ca{};
        for (std::size_t index = 0; index < ca.size(); ++index)
            ca[index] = static_cast<std::uint8_t>(index * 31 + iteration);

        Check(tls_config_set_ca_mem_nid_postfix(config, ca.data(), ca.size()) == 0);
        ca.fill(0xAA);
        Check(tls_config_set_protocols_nid_postfix(config, static_cast<std::uint32_t>(iteration)) == 0);
        tls_config_verify_client_nid_postfix(config);
        tls_config_insecure_noverifyname_nid_postfix(config);

        TlsContext* client = tls_client_nid_postfix();
        Check(client != nullptr);
        Check(tls_configure_nid_postfix(client, config) == 0);
        Check(tls_config_set_ca_mem_nid_postfix(config, nullptr, ca.size()) == 0);
        Check(tls_config_set_ca_mem_nid_postfix(config, ca.data(), ca.size()) == 0);
        Check(tls_config_set_ca_mem_nid_postfix(config, nullptr, 0) == 0);

        tls_config_free_nid_postfix(config);
        Check(tls_close_nid_postfix(client) == 0);
        tls_free_nid_postfix(client);
    }
}

}

int main() {
    TestConfiguration();
    std::puts("libtls CA and verification settings tests passed");
    return 0;
}
