#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/LibTls.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "tests/SslCertificateFixture.hpp"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <type_traits>

using ConnectSocket = int (APS5_VABI*)(TlsContext*, int, const char*);
using Handshake = int (APS5_VABI*)(TlsContext*);
using Read = std::int64_t (APS5_VABI*)(TlsContext*, void*, std::size_t);
using Write = std::int64_t (APS5_VABI*)(TlsContext*, const void*, std::size_t);

static_assert(std::is_same_v<decltype(&tls_connect_socket_nid_postfix), ConnectSocket>);
static_assert(std::is_same_v<decltype(&tls_handshake_nid_postfix), Handshake>);
static_assert(std::is_same_v<decltype(&tls_read_nid_postfix), Read>);
static_assert(std::is_same_v<decltype(&tls_write_nid_postfix), Write>);

namespace {

constexpr std::int64_t WantPollIn = -2;
constexpr std::int64_t WantPollOut = -3;
constexpr int GuestSocketLevel = 0xffff;
constexpr int GuestNonBlocking = 0x1200;
constexpr short PollIn = 1;
constexpr short PollOut = 4;

enum class ServerMode { Exchange, DelayedExchange, HandshakeOnly };

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "libtls transport test failed at line %d\n", line);
    std::abort();
}

#define Check(condition) Require((condition), __LINE__)

struct Server {
    mbedtls_net_context listener;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_x509_crt certificate;
    mbedtls_pk_context key;
    mbedtls_ssl_config config;
    std::uint16_t port = 0;

    Server() {
        mbedtls_net_init(&listener);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        mbedtls_x509_crt_init(&certificate);
        mbedtls_pk_init(&key);
        mbedtls_ssl_config_init(&config);
        Check(mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, nullptr, 0) == 0);
        Check(mbedtls_x509_crt_parse(&certificate,
            reinterpret_cast<const unsigned char*>(ServerPem), sizeof(ServerPem)) == 0);
        Check(mbedtls_pk_parse_key(&key, reinterpret_cast<const unsigned char*>(ServerKey),
            sizeof(ServerKey), nullptr, 0, mbedtls_ctr_drbg_random, &drbg) == 0);
        Check(mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_SERVER,
            MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0);
        mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &drbg);
        Check(mbedtls_ssl_conf_own_cert(&config, &certificate, &key) == 0);
        Check(mbedtls_net_bind(&listener, "127.0.0.1", "0", MBEDTLS_NET_PROTO_TCP) == 0);
        sockaddr_in address{};
#ifdef _WIN32
        int size = sizeof(address);
#else
        socklen_t size = sizeof(address);
#endif
        Check(getsockname(listener.fd, reinterpret_cast<sockaddr*>(&address), &size) == 0);
        port = ntohs(address.sin_port);
    }

    ~Server() {
        mbedtls_ssl_config_free(&config);
        mbedtls_pk_free(&key);
        mbedtls_x509_crt_free(&certificate);
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_net_free(&listener);
    }

    std::thread Spawn(ServerMode mode) {
        return std::thread([this, mode]() {
            mbedtls_net_context client;
            mbedtls_ssl_context ssl;
            mbedtls_net_init(&client);
            mbedtls_ssl_init(&ssl);
            if (mbedtls_net_accept(&listener, &client, nullptr, 0, nullptr) == 0 &&
                mbedtls_ssl_setup(&ssl, &config) == 0) {
                mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, mbedtls_net_recv, nullptr);
                if (mode == ServerMode::DelayedExchange) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                }
                if (mbedtls_ssl_handshake(&ssl) == 0 && mode != ServerMode::HandshakeOnly) {
                    std::array<unsigned char, 4> request{};
                    std::size_t received = 0;
                    while (received < request.size()) {
                        const int count = mbedtls_ssl_read(&ssl, request.data() + received,
                            request.size() - received);
                        if (count <= 0) break;
                        received += static_cast<std::size_t>(count);
                    }
                    if (received == request.size() && std::memcmp(request.data(), "ping", 4) == 0) {
                        if (mode == ServerMode::DelayedExchange) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(30));
                        }
                        mbedtls_ssl_write(&ssl, reinterpret_cast<const unsigned char*>("pong"), 4);
                        mbedtls_ssl_close_notify(&ssl);
                    }
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
    GuestPollDescriptor pollfd{
        descriptor,
        code == WantPollIn ? PollIn : PollOut,
        0
    };
    Check(poll_nid_postfix(&pollfd, 1, 5000) == 1);
}

TlsConfig* MakeConfig(const char* ca, std::size_t caLength) {
    TlsConfig* config = tls_config_new_nid_postfix();
    Check(config != nullptr);
    Check(tls_config_set_ca_mem_nid_postfix(config,
        reinterpret_cast<const std::uint8_t*>(ca), caLength) == 0);
    return config;
}

void TestHandshakeAndExchange(Server& server) {
    auto thread = server.Spawn(ServerMode::Exchange);
    const int sock = OpenGuestSocket(server.port);
    TlsConfig* config = MakeConfig(RootPem, sizeof(RootPem) - 1);
    TlsContext* client = tls_client_nid_postfix();
    Check(client != nullptr);
    Check(tls_configure_nid_postfix(client, config) == 0);
    Check(tls_connect_socket_nid_postfix(client, sock, "localhost") == 0);
    Check(tls_handshake_nid_postfix(client) == 0);
    Check(tls_error_nid_postfix(client) == nullptr);

    Check(tls_write_nid_postfix(client, nullptr, 0) == 0);
    Check(tls_read_nid_postfix(client, nullptr, 0) == 0);
    Check(tls_write_nid_postfix(client, "ping", 4) == 4);

    std::array<char, 8> reply{};
    std::size_t total = 0;
    while (total < 4) {
        const auto count = tls_read_nid_postfix(client, reply.data() + total, reply.size() - total);
        Check(count > 0);
        total += static_cast<std::size_t>(count);
    }
    Check(total == 4 && std::memcmp(reply.data(), "pong", 4) == 0);
    Check(tls_read_nid_postfix(client, reply.data(), reply.size()) == 0);
    Check(tls_close_nid_postfix(client) == 0);
    Check(tls_read_nid_postfix(client, reply.data(), reply.size()) == -1);
    Check(tls_error_nid_postfix(client) != nullptr);

    tls_free_nid_postfix(client);
    tls_config_free_nid_postfix(config);
    Check(close_nid_postfix(sock) == 0);
    thread.join();
}

void TestImplicitHandshakeAndNoVerifyName(Server& server, const char* servername) {
    auto thread = server.Spawn(ServerMode::Exchange);
    const int sock = OpenGuestSocket(server.port);
    TlsConfig* config = MakeConfig(RootPem, sizeof(RootPem) - 1);
    tls_config_insecure_noverifyname_nid_postfix(config);
    TlsContext* client = tls_client_nid_postfix();
    Check(client != nullptr);
    Check(tls_configure_nid_postfix(client, config) == 0);
    Check(tls_connect_socket_nid_postfix(client, sock, servername) == 0);
    Check(tls_write_nid_postfix(client, "ping", 4) == 4);

    std::array<char, 8> reply{};
    std::size_t total = 0;
    while (total < 4) {
        const auto count = tls_read_nid_postfix(client, reply.data() + total, reply.size() - total);
        Check(count > 0);
        total += static_cast<std::size_t>(count);
    }
    Check(total == 4 && std::memcmp(reply.data(), "pong", 4) == 0);
    Check(tls_close_nid_postfix(client) == 0);

    tls_free_nid_postfix(client);
    tls_config_free_nid_postfix(config);
    Check(close_nid_postfix(sock) == 0);
    thread.join();
}

void TestNonBlockingPoll(Server& server) {
    auto thread = server.Spawn(ServerMode::DelayedExchange);
    const int sock = OpenGuestSocket(server.port);
    const int nonBlocking = 1;
    Check(setsockopt_nid_postfix(sock, GuestSocketLevel, GuestNonBlocking,
        &nonBlocking, sizeof(nonBlocking)) == 0);

    TlsConfig* config = MakeConfig(RootPem, sizeof(RootPem) - 1);
    TlsContext* client = tls_client_nid_postfix();
    Check(client != nullptr);
    Check(tls_configure_nid_postfix(client, config) == 0);
    Check(tls_connect_socket_nid_postfix(client, sock, "localhost") == 0);

    bool sawPollIn = false;
    for (;;) {
        const int result = tls_handshake_nid_postfix(client);
        if (result == 0) break;
        Check(result == WantPollIn || result == WantPollOut);
        if (result == WantPollIn) sawPollIn = true;
        WaitSocket(sock, result);
    }
    Check(sawPollIn);

    const char* request = "ping";
    std::size_t sent = 0;
    while (sent < 4) {
        const auto result = tls_write_nid_postfix(client, request + sent, 4 - sent);
        if (result > 0) {
            sent += static_cast<std::size_t>(result);
            continue;
        }
        Check(result == WantPollIn || result == WantPollOut);
        WaitSocket(sock, result);
    }

    std::array<char, 8> reply{};
    std::size_t received = 0;
    bool sawReadPollIn = false;
    while (received < 4) {
        const auto result = tls_read_nid_postfix(client, reply.data() + received,
            reply.size() - received);
        if (result > 0) {
            received += static_cast<std::size_t>(result);
            continue;
        }
        Check(result == WantPollIn || result == WantPollOut);
        if (result == WantPollIn) sawReadPollIn = true;
        WaitSocket(sock, result);
    }
    Check(sawReadPollIn);
    Check(received == 4 && std::memcmp(reply.data(), "pong", 4) == 0);

    for (;;) {
        const int result = tls_close_nid_postfix(client);
        if (result == 0) break;
        Check(result == WantPollIn || result == WantPollOut);
        WaitSocket(sock, result);
    }

    tls_free_nid_postfix(client);
    tls_config_free_nid_postfix(config);
    Check(close_nid_postfix(sock) == 0);
    thread.join();
}

void TestErrors(Server& server) {
    {
        auto thread = server.Spawn(ServerMode::HandshakeOnly);
        const int sock = OpenGuestSocket(server.port);
        TlsConfig* config = MakeConfig(RootPem, sizeof(RootPem) - 1);
        TlsContext* client = tls_client_nid_postfix();
        Check(client != nullptr);
        Check(tls_configure_nid_postfix(client, config) == 0);
        Check(tls_connect_socket_nid_postfix(client, sock, nullptr) == -1);
        Check(tls_error_nid_postfix(client) != nullptr);
        Check(tls_connect_socket_nid_postfix(client, sock, "wrong.example") == 0);
        Check(tls_handshake_nid_postfix(client) == -1);
        Check(tls_error_nid_postfix(client) != nullptr);
        tls_free_nid_postfix(client);
        tls_config_free_nid_postfix(config);
        Check(close_nid_postfix(sock) == 0);
        thread.join();
    }
    {
        auto thread = server.Spawn(ServerMode::HandshakeOnly);
        const int sock = OpenGuestSocket(server.port);
        TlsConfig* config = MakeConfig(ClientPem, sizeof(ClientPem) - 1);
        TlsContext* client = tls_client_nid_postfix();
        Check(client != nullptr);
        Check(tls_configure_nid_postfix(client, config) == 0);
        Check(tls_connect_socket_nid_postfix(client, sock, "localhost") == 0);
        Check(tls_handshake_nid_postfix(client) == -1);
        Check(tls_error_nid_postfix(client) != nullptr);
        tls_free_nid_postfix(client);
        tls_config_free_nid_postfix(config);
        Check(close_nid_postfix(sock) == 0);
        thread.join();
    }
}

}

int main() {
    Server server;
    TestHandshakeAndExchange(server);
    TestImplicitHandshakeAndNoVerifyName(server, "mismatch.example");
    TestImplicitHandshakeAndNoVerifyName(server, nullptr);
    TestNonBlockingPoll(server);
    TestErrors(server);
    std::puts("libtls transport tests passed");
    return 0;
}
