#include "prx/libSceLibreSsl3/include/LibTls.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

using SetMemory = int (APS5_VABI*)(TlsConfig*, const std::uint8_t*, std::size_t);
using SetProtocols = int (APS5_VABI*)(TlsConfig*, std::uint32_t);
using ParseProtocols = int (APS5_VABI*)(std::uint32_t*, const char*);
using SetFlag = void (APS5_VABI*)(TlsConfig*);

static_assert(std::is_same_v<decltype(&tls_config_set_ca_mem_nid_postfix), SetMemory>);
static_assert(std::is_same_v<decltype(&tls_config_set_cert_mem_nid_postfix), SetMemory>);
static_assert(std::is_same_v<decltype(&tls_config_set_protocols_nid_postfix), SetProtocols>);
static_assert(std::is_same_v<decltype(&tls_config_parse_protocols_nid_postfix), ParseProtocols>);
static_assert(std::is_same_v<decltype(&tls_config_verify_client_nid_postfix), SetFlag>);
static_assert(std::is_same_v<decltype(&tls_config_insecure_noverifyname_nid_postfix), SetFlag>);

namespace {

void Require(bool okay, int line) {
    if (okay) return;
    std::fprintf(stderr, "libtls settings check failed at line %d\n", line);
    std::abort();
}

#define Check(value) Require((value), __LINE__)

void CheckParse(const char* source, std::uint32_t expected) {
    std::uint32_t protocols = 0xABCD9876U;
    Check(tls_config_parse_protocols_nid_postfix(&protocols, source) == 0);
    Check(protocols == expected);
}

void CheckInvalid(const char* source) {
    std::uint32_t protocols = 0xABCD9876U;
    Check(tls_config_parse_protocols_nid_postfix(&protocols, source) == -1);
    Check(protocols == 0xABCD9876U);
}

void TestProtocolParser() {
    CheckParse(nullptr, 24);
    CheckParse("default", 24);
    CheckParse("SECURE", 24);
    CheckParse("legacy", 24);
    CheckParse("All", 24);
    CheckParse("tlsv1", 24);
    CheckParse("tlsv1.0", 2);
    CheckParse("TLSv1.1", 4);
    CheckParse("TLSv1.2", 8);
    CheckParse("TLSv1.3", 16);
    CheckParse("tlsv1.0:tlsv1.1,tlsv1.2", 14);
    CheckParse("\tsecure,!tlsv1.2", 16);
    CheckParse("!tlsv1.3", 8);
    CheckParse("!tlsv1.3, tlsv1.0", 10);
    CheckParse("tlsv1.3,!tlsv1.3,tlsv1.2", 8);
    CheckParse("tlsv1.2,!tlsv1.2", 0);
    CheckParse("tlsv1.0,!tlsv1.0", 0);
    CheckParse("default,!default", 0);

    for (const char* invalid : {"", " ", "tlsv1.4", "tlsv1.2 ", "tlsv1.2,", ",all", "tlsv1.2::tlsv1.3", "!", "!unknown", "TLSv2"})
        CheckInvalid(invalid);
    Check(tls_config_parse_protocols_nid_postfix(nullptr, nullptr) == -1);
}

void TestConfiguration() {
    Check(tls_config_set_ca_mem_nid_postfix(nullptr, nullptr, 0) == -1);
    Check(tls_config_set_cert_mem_nid_postfix(nullptr, nullptr, 0) == -1);
    Check(tls_config_set_protocols_nid_postfix(nullptr, 24) == -1);
    tls_config_verify_client_nid_postfix(nullptr);
    tls_config_insecure_noverifyname_nid_postfix(nullptr);

    for (int iteration = 0; iteration != 4096; ++iteration) {
        TlsConfig* config = tls_config_new_nid_postfix();
        Check(config != nullptr);
        std::array<std::uint8_t, 257> ca{};
        std::array<std::uint8_t, 313> cert{};
        for (std::size_t i = 0; i < ca.size(); ++i)
            ca[i] = static_cast<std::uint8_t>(i * 31 + iteration);
        for (std::size_t i = 0; i < cert.size(); ++i)
            cert[i] = static_cast<std::uint8_t>(i * 17 + iteration);

        Check(tls_config_set_ca_mem_nid_postfix(config, ca.data(), ca.size()) == 0);
        Check(tls_config_set_cert_mem_nid_postfix(config, cert.data(), cert.size()) == 0);
        std::memset(ca.data(), 0xFA, ca.size());
        std::memset(cert.data(), 0xCD, cert.size());

        Check(tls_config_set_protocols_nid_postfix(config, static_cast<std::uint32_t>(iteration)) == 0);
        tls_config_verify_client_nid_postfix(config);
        tls_config_insecure_noverifyname_nid_postfix(config);

        TlsContext* server = tls_server_nid_postfix();
        Check(server != nullptr);
        Check(tls_configure_nid_postfix(server, config) == -1);
        Check(std::strcmp(tls_error_nid_postfix(server), "failed to load certificate") == 0);
        TlsContext* client = tls_client_nid_postfix();
        Check(client != nullptr);
        Check(tls_configure_nid_postfix(client, config) == 0);

        Check(tls_config_set_ca_mem_nid_postfix(config, nullptr, ca.size()) == -1);
        Check(tls_config_set_cert_mem_nid_postfix(config, nullptr, cert.size()) == -1);
        Check(tls_config_set_ca_mem_nid_postfix(config, nullptr, 0) == 0);
        Check(tls_config_set_cert_mem_nid_postfix(config, nullptr, 0) == 0);
        tls_config_free_nid_postfix(config);
        Check(tls_close_nid_postfix(client) == 0);
        tls_free_nid_postfix(client);
        tls_free_nid_postfix(server);
    }
}

}

int main() {
    TestProtocolParser();
    TestConfiguration();
    std::puts("libtls memory and protocol settings tests passed");
    return 0;
}
