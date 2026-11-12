#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceSsl/include/Ssl.hpp"
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <csignal>
#endif

#include "prx/libSceSsl/SslTypes.hpp"
#include "SslCertificateFixture.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

extern "C" int SslTrustSystemRoots_nid_no_patch(int sslConnectionId);

namespace {

constexpr int ErrorInvalidId = static_cast<int>(0x8095f006);
constexpr int ErrorUnknownCa = static_cast<int>(0x8095f00c);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

std::string ThrownMessage(int connection) {
    try {
        SslTrustSystemRoots_nid_no_patch(connection);
    } catch (const std::exception& error) {
        return error.what();
    }
    return {};
}

void ServeSystemList(const std::string& text) {
    const auto path = std::filesystem::current_path() / sceKernelGetFsSandboxRandomWord() / "common" / "cert" / "CA_LIST.cer";
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    Require(static_cast<bool>(file.flush()), "write the system CA list");
}

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
        Require(mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, nullptr, 0) == 0, "seed server random generator");
        Require(mbedtls_x509_crt_parse(&certificate, reinterpret_cast<const unsigned char*>(ServerPem), sizeof(ServerPem)) == 0,
            "parse server certificate");
        Require(mbedtls_pk_parse_key(&key, reinterpret_cast<const unsigned char*>(ServerKey), sizeof(ServerKey), nullptr, 0,
            mbedtls_ctr_drbg_random, &drbg) == 0, "parse server key");
        Require(mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0,
            "configure server");
        mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &drbg);
        Require(mbedtls_ssl_conf_own_cert(&config, &certificate, &key) == 0, "set server certificate");
        mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_NONE);
        Require(mbedtls_net_bind(&listener, "127.0.0.1", "0", MBEDTLS_NET_PROTO_TCP) == 0, "bind loopback listener");
        sockaddr_in address{};
#ifdef _WIN32
        int size = sizeof(address);
#else
        socklen_t size = sizeof(address);
#endif
        Require(getsockname(listener.fd, reinterpret_cast<sockaddr*>(&address), &size) == 0, "read listener port");
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

    void Serve() {
        mbedtls_net_context client;
        mbedtls_net_init(&client);
        mbedtls_ssl_context ssl;
        mbedtls_ssl_init(&ssl);
        if (mbedtls_net_accept(&listener, &client, nullptr, 0, nullptr) == 0 && mbedtls_ssl_setup(&ssl, &config) == 0) {
            mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, mbedtls_net_recv, nullptr);
            if (mbedtls_ssl_handshake(&ssl) == 0) mbedtls_ssl_close_notify(&ssl);
        }
        mbedtls_ssl_free(&ssl);
        mbedtls_net_free(&client);
    }
};

int LoadAuthority(const char* pem) {
    const int context = sceSslInit_nid_postfix(256 * 1024);
    Require(context > 0, "create an SSL context");
    std::string text = pem;
    SceSsl::Data authority{reinterpret_cast<std::uint8_t*>(text.data()), text.size()};
    auto* authorities = &authority;
    Require(sceSslLoadCert(context, 1, &authorities, nullptr, nullptr) == 0, "load the context authority");
    return context;
}

int Connect(int context, std::uint16_t port, bool systemRoots) {
    const int descriptor = socket_nid_postfix(2, 1, 6);
    Require(descriptor >= 0, "create guest socket");
    std::array<std::uint8_t, 16> address{16, 2, static_cast<std::uint8_t>(port >> 8), static_cast<std::uint8_t>(port), 127, 0, 0, 1};
    Require(connect_nid_postfix(descriptor, address.data(), 16) == 0, "connect guest socket");
    const int connection = sceSslCreateConnection(context, descriptor, "localhost");
    Require(connection > 0, "create an SSL connection");
    if (systemRoots) Require(SslTrustSystemRoots_nid_no_patch(connection) == 0, "trust the system CA list");
    const int result = sceSslConnect(connection);
    if (result == 0) {
        Require(!ThrownMessage(connection).empty(), "the system CA list was added to a started connection");
        std::array<char, 4> scratch{};
        Require(sceSslRead(connection, scratch.data(), scratch.size(), 0) == 0, "close notify did not end the stream");
    }
    Require(sceSslDeleteConnection(connection) == 0 && close_nid_postfix(descriptor) == 0, "release the connection");
    return result;
}

void CheckRejectedList(const std::string& text, const char* message) {
    ServeSystemList(text);
    const int context = sceSslInit_nid_postfix(256 * 1024);
    Require(context > 0, "create an SSL context");
    const int connection = sceSslCreateConnection(context, 1000, "localhost");
    Require(connection > 0, "create an SSL connection");
    const auto path = std::string("/") + sceKernelGetFsSandboxRandomWord() + "/common/cert/CA_LIST.cer";
    Require(ThrownMessage(connection).find(path) != std::string::npos, message);
    Require(ThrownMessage(connection).find(path) != std::string::npos, "a rejected system CA list was cached");
    Require(sceSslDeleteConnection(connection) == 0 && sceSslTerm_nid_postfix(context) == 0, "release the context");
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "usage: system|context|empty|blank");
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    const std::string mode = argv[1];
    if (mode == "empty") {
        CheckRejectedList("", "an empty system CA list did not throw with its path");
        return 0;
    }
    if (mode == "blank") {
        CheckRejectedList("no certificate here\n", "a system CA list without a certificate did not throw with its path");
        return 0;
    }
    Require(mode == "system" || mode == "context", "usage: system|context|empty|blank");
    ServeSystemList(mode == "system" ? RootPem : ClientPem);
    Server server;
    std::thread serving([&] {
        for (int index = 0; index < 4; ++index) server.Serve();
    });
    const int empty = sceSslInit_nid_postfix(256 * 1024);
    Require(empty > 0, "create an SSL context without authorities");
    Require(SslTrustSystemRoots_nid_no_patch(9999) == ErrorInvalidId, "an unknown connection id was accepted");
    if (mode == "system") {
        Require(Connect(empty, server.port, false) == ErrorUnknownCa, "a connection without the system CA list trusted the fixture root");
        Require(Connect(empty, server.port, true) == 0, "the system CA list did not trust the fixture root");
        const int unrelated = LoadAuthority(ClientPem);
        Require(Connect(unrelated, server.port, false) == ErrorUnknownCa, "an unrelated context authority trusted the server");
        Require(Connect(unrelated, server.port, true) == 0, "the system CA list was not trusted after the context's authorities");
        Require(sceSslTerm_nid_postfix(unrelated) == 0, "release the context with an unrelated authority");
    } else {
        Require(Connect(empty, server.port, true) == ErrorUnknownCa, "a system CA list without the fixture root trusted the server");
        const int fixture = LoadAuthority(RootPem);
        Require(Connect(fixture, server.port, false) == 0, "the context's fixture root was not trusted");
        Require(Connect(fixture, server.port, true) == 0, "the context's fixture root was not trusted alongside the system CA list");
        Require(Connect(empty, server.port, false) == ErrorUnknownCa, "a connection without authorities trusted the server");
        Require(sceSslTerm_nid_postfix(fixture) == 0, "release the context with the fixture root");
    }
    serving.join();
    Require(sceSslTerm_nid_postfix(empty) == 0, "release the context without authorities");
    std::puts("SSL system roots passed");
    return 0;
}
