#include "prx/libc/include/General.hpp"
#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/LibTls.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "tests/SslCertificateFixture.hpp"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <type_traits>

using AcceptSocket = int (APS5_VABI*)(TlsContext*, TlsContext**, int);
using Handshake = int (APS5_VABI*)(TlsContext*);
using Read = std::int64_t (APS5_VABI*)(TlsContext*, void*, std::size_t);
using Write = std::int64_t (APS5_VABI*)(TlsContext*, const void*, std::size_t);
using Close = int (APS5_VABI*)(TlsContext*);
using Free = void (APS5_VABI*)(TlsContext*);

static_assert(std::is_same_v<decltype(&tls_accept_socket_nid_postfix), AcceptSocket>);
static_assert(std::is_same_v<decltype(&tls_handshake_nid_postfix), Handshake>);
static_assert(std::is_same_v<decltype(&tls_read_nid_postfix), Read>);
static_assert(std::is_same_v<decltype(&tls_write_nid_postfix), Write>);
static_assert(std::is_same_v<decltype(&tls_close_nid_postfix), Close>);
static_assert(std::is_same_v<decltype(&tls_free_nid_postfix), Free>);

namespace {

constexpr std::int64_t WantPollIn = -2;
constexpr std::int64_t WantPollOut = -3;
constexpr short PollIn = 1;
constexpr short PollOut = 4;

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "libtls server transport test failed at line %d\n", line);
    std::abort();
}

#define Check(condition) Require((condition), __LINE__)

void CheckCallerOwnsSocket(int descriptor, bool expected) {
    std::shared_ptr<void> owner;
    std::uintptr_t native = 0;
    Check(KernelLookupSocketNative_nid_no_patch(descriptor, &owner, &native) == expected);
    if (expected) Check(native != 0);
}

struct Peer {
    mbedtls_net_context listener;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context random;
    mbedtls_x509_crt authorities;
    mbedtls_ssl_config config;
    std::uint16_t port = 0;
    std::atomic<bool> handshakeOk{false};
    std::atomic<bool> exchangeOk{false};
    std::atomic<bool> sawCloseNotify{false};

    Peer() {
        mbedtls_net_init(&listener);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&random);
        mbedtls_x509_crt_init(&authorities);
        mbedtls_ssl_config_init(&config);
        Check(mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy, nullptr, 0) == 0);
        Check(mbedtls_x509_crt_parse(&authorities,
            reinterpret_cast<const unsigned char*>(RootPem), sizeof(RootPem)) == 0);
        Check(mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0);
        mbedtls_ssl_conf_read_timeout(&config, 5000);
        mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &random);
        mbedtls_ssl_conf_ca_chain(&config, &authorities, nullptr);
        Check(mbedtls_net_bind(&listener, "127.0.0.1", "0", MBEDTLS_NET_PROTO_TCP) == 0);
        sockaddr_in address{};
#ifdef _WIN32
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif
        Check(getsockname(listener.fd, reinterpret_cast<sockaddr*>(&address), &length) == 0);
        port = ntohs(address.sin_port);
    }

    ~Peer() {
        mbedtls_ssl_config_free(&config);
        mbedtls_x509_crt_free(&authorities);
        mbedtls_ctr_drbg_free(&random);
        mbedtls_entropy_free(&entropy);
        mbedtls_net_free(&listener);
    }

    std::thread Spawn() {
        handshakeOk = false;
        exchangeOk = false;
        sawCloseNotify = false;
        return std::thread([this] {
            mbedtls_net_context client;
            mbedtls_ssl_context ssl;
            mbedtls_net_init(&client);
            mbedtls_ssl_init(&ssl);
            if (mbedtls_net_poll(&listener, MBEDTLS_NET_POLL_READ, 5000) == 1 &&
                mbedtls_net_accept(&listener, &client, nullptr, 0, nullptr) == 0 &&
                mbedtls_ssl_setup(&ssl, &config) == 0) {
                mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, nullptr, mbedtls_net_recv_timeout);
                mbedtls_ssl_set_hostname(&ssl, "localhost");
                if (mbedtls_ssl_handshake(&ssl) == 0) {
                    handshakeOk = true;
                    std::array<unsigned char, 16> buffer{};
                    if (mbedtls_ssl_write(&ssl,
                            reinterpret_cast<const unsigned char*>("ping"), 4) == 4 &&
                        mbedtls_ssl_read(&ssl, buffer.data(), buffer.size()) == 4 &&
                        std::memcmp(buffer.data(), "pong", 4) == 0)
                        exchangeOk = true;
                }
                mbedtls_ssl_close_notify(&ssl);
                for (;;) {
                    std::array<unsigned char, 16> buffer{};
                    const int count = mbedtls_ssl_read(&ssl, buffer.data(), buffer.size());
                    if (count == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
                        sawCloseNotify = true;
                        break;
                    }
                    if (count != MBEDTLS_ERR_SSL_WANT_READ && count != MBEDTLS_ERR_SSL_WANT_WRITE) break;
                }
            }
            mbedtls_ssl_free(&ssl);
            mbedtls_net_free(&client);
        });
    }
};

int OpenGuestSocket(std::uint16_t port) {
    const int descriptor = socket_nid_postfix(2, 1, 6);
    Check(descriptor >= 0);
    std::array<std::uint8_t, 16> address{
        16, 2, static_cast<std::uint8_t>(port >> 8), static_cast<std::uint8_t>(port), 127, 0, 0, 1
    };
    Check(connect_nid_postfix(descriptor, address.data(), 16) == 0);
    return descriptor;
}

void WaitSocket(int descriptor, std::int64_t code) {
    GuestPollDescriptor pollfd{descriptor, code == WantPollIn ? PollIn : PollOut, 0};
    Check(poll_nid_postfix(&pollfd, 1, 5000) == 1);
}

TlsConfig* ServerConfig(bool cert, bool key, const char* certificate = ServerPem, const char* privateKey = ServerKey) {
    TlsConfig* config = tls_config_new_nid_postfix();
    Check(config != nullptr);
    if (cert) Check(tls_config_set_cert_mem_nid_postfix(config,
        reinterpret_cast<const std::uint8_t*>(certificate), std::strlen(certificate) + 1) == 0);
    if (key) Check(tls_config_set_key_mem_nid_postfix(config,
        reinterpret_cast<const std::uint8_t*>(privateKey), std::strlen(privateKey) + 1) == 0);
    return config;
}

void Exchange(Peer& peer, TlsContext* server) {
    auto thread = peer.Spawn();
    const int descriptor = OpenGuestSocket(peer.port);
    TlsContext* connection = nullptr;
    Check(tls_accept_socket_nid_postfix(server, &connection, descriptor) == 0);
    Check(connection != nullptr);
    Check(tls_error_nid_postfix(server) == nullptr);
    Check(tls_error_nid_postfix(connection) == nullptr);
    for (;;) {
        const int result = tls_handshake_nid_postfix(connection);
        if (result == 0) break;
        Check(result == WantPollIn || result == WantPollOut);
        WaitSocket(descriptor, result);
    }
    std::array<char, 16> buffer{};
    std::size_t received = 0;
    while (received < 4) {
        const auto result = tls_read_nid_postfix(connection, buffer.data() + received, 4 - received);
        if (result > 0) {
            received += static_cast<std::size_t>(result);
            continue;
        }
        Check(result == WantPollIn || result == WantPollOut);
        WaitSocket(descriptor, result);
    }
    Check(received == 4 && std::memcmp(buffer.data(), "ping", 4) == 0);
    std::size_t sent = 0;
    while (sent < 4) {
        const auto result = tls_write_nid_postfix(connection, "pong" + sent, 4 - sent);
        if (result > 0) {
            sent += static_cast<std::size_t>(result);
            continue;
        }
        Check(result == WantPollIn || result == WantPollOut);
        WaitSocket(descriptor, result);
    }
    for (;;) {
        const int result = tls_close_nid_postfix(connection);
        if (result == 0) break;
        Check(result == WantPollIn || result == WantPollOut);
        WaitSocket(descriptor, result);
    }
    Check(tls_error_nid_postfix(connection) == nullptr);
    tls_free_nid_postfix(connection);
    CheckCallerOwnsSocket(descriptor, true);
    Check(close_nid_postfix(descriptor) == 0);
    CheckCallerOwnsSocket(descriptor, false);
    thread.join();
    Check(peer.handshakeOk);
    Check(peer.exchangeOk);
    Check(peer.sawCloseNotify);
}

void TestAcceptAndExchange() {
    Peer peer;
    TlsConfig* config = ServerConfig(true, true);
    TlsContext* server = tls_server_nid_postfix();
    Check(server != nullptr);
    Check(tls_configure_nid_postfix(server, config) == 0);
    Check(tls_error_nid_postfix(server) == nullptr);
    Exchange(peer, server);
    tls_config_free_nid_postfix(config);
    Exchange(peer, server);
    tls_free_nid_postfix(server);
}

void TestPemWithoutNul() {
    Peer peer;
    TlsConfig* config = tls_config_new_nid_postfix();
    Check(config != nullptr);
    Check(tls_config_set_cert_mem_nid_postfix(config,
        reinterpret_cast<const std::uint8_t*>(ServerPem), std::strlen(ServerPem)) == 0);
    Check(tls_config_set_key_mem_nid_postfix(config,
        reinterpret_cast<const std::uint8_t*>(ServerKey), std::strlen(ServerKey)) == 0);
    TlsContext* server = tls_server_nid_postfix();
    Check(server != nullptr);
    Check(tls_configure_nid_postfix(server, config) == 0);
    Check(tls_error_nid_postfix(server) == nullptr);
    Exchange(peer, server);
    tls_config_free_nid_postfix(config);
    tls_free_nid_postfix(server);
}

void TestInvalidArguments() {
    Peer peer;
    const int descriptor = OpenGuestSocket(peer.port);
    TlsConfig* config = ServerConfig(true, true);
    TlsConfig* ca = tls_config_new_nid_postfix();
    Check(ca != nullptr);
    Check(tls_config_set_ca_mem_nid_postfix(ca,
        reinterpret_cast<const std::uint8_t*>(RootPem), sizeof(RootPem) - 1) == 0);
    TlsContext* server = tls_server_nid_postfix();
    TlsContext* client = tls_client_nid_postfix();
    Check(server != nullptr && client != nullptr);
    Check(tls_configure_nid_postfix(server, config) == 0);
    Check(tls_configure_nid_postfix(client, ca) == 0);
    TlsContext* output = server;
    Check(tls_accept_socket_nid_postfix(nullptr, &output, descriptor) == -1);
    Check(output == nullptr);
    Check(tls_accept_socket_nid_postfix(server, nullptr, descriptor) == -1);
    output = server;
    Check(tls_accept_socket_nid_postfix(server, &output, -1) == -1);
    Check(output == nullptr);
    Check(std::strcmp(tls_error_nid_postfix(server), "ssl file descriptor failure") == 0);
    output = server;
    Check(tls_accept_socket_nid_postfix(client, &output, descriptor) == -1);
    Check(output == nullptr);
    Check(std::strcmp(tls_error_nid_postfix(client), "not a server context") == 0);
    Check(tls_handshake_nid_postfix(server) == -1);
    Check(std::strcmp(tls_error_nid_postfix(server), "invalid operation for context") == 0);
    tls_config_free_nid_postfix(config);
    tls_config_free_nid_postfix(ca);
    tls_free_nid_postfix(server);
    tls_free_nid_postfix(client);
    CheckCallerOwnsSocket(descriptor, true);
    Check(close_nid_postfix(descriptor) == 0);
    CheckCallerOwnsSocket(descriptor, false);
}

void TestConfigurationValidation() {
    const unsigned char invalid[] = {'n', 'o', 't', ' ', 'P', 'E', 'M'};
    struct Case {
        bool cert;
        bool key;
        const char* certificate;
        const char* privateKey;
        const char* expected;
    };
    const Case cases[] = {
        {false, false, ServerPem, ServerKey, "private/public key mismatch"},
        {true, false, ServerPem, ServerKey, "private/public key mismatch"},
        {false, true, ServerPem, ServerKey, "private/public key mismatch"},
        {true, true, ServerPem, ClientKey, "private/public key mismatch"},
        {true, true, "not a certificate", ServerKey, "failed to load certificate"},
        {true, true, ServerPem, "not a key", "failed to read private key"},
    };
    for (const auto& item : cases) {
        TlsConfig* config = ServerConfig(item.cert, item.key, item.certificate, item.privateKey);
        TlsContext* server = tls_server_nid_postfix();
        Check(server != nullptr);
        Check(tls_configure_nid_postfix(server, config) == -1);
        const char* error = tls_error_nid_postfix(server);
        Check(error != nullptr && std::strcmp(error, item.expected) == 0);
        tls_config_free_nid_postfix(config);
        tls_free_nid_postfix(server);
    }
    TlsConfig* badCert = tls_config_new_nid_postfix();
    Check(badCert != nullptr);
    Check(tls_config_set_cert_mem_nid_postfix(badCert, invalid, sizeof(invalid)) == 0);
    TlsContext* server = tls_server_nid_postfix();
    Check(tls_configure_nid_postfix(server, badCert) == -1);
    Check(std::strcmp(tls_error_nid_postfix(server), "failed to load certificate") == 0);
    TlsConfig* badKey = tls_config_new_nid_postfix();
    Check(badKey != nullptr);
    Check(tls_config_set_key_mem_nid_postfix(badKey, invalid, sizeof(invalid)) == 0);
    Check(tls_configure_nid_postfix(server, badKey) == -1);
    Check(std::strcmp(tls_error_nid_postfix(server), "failed to read private key") == 0);
    tls_config_free_nid_postfix(badCert);
    tls_config_free_nid_postfix(badKey);
    tls_free_nid_postfix(server);
}

}

int main() {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    TestAcceptAndExchange();
    TestPemWithoutNul();
    TestInvalidArguments();
    TestConfigurationValidation();
    std::puts("libtls server transport tests passed");
    return 0;
}
