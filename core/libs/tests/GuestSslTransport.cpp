#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <csignal>
#endif

#include "prx/libSceSsl/SslTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
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
#include <string>
#include <thread>

extern "C" {
int APS5_VABI sceSslInit_nid_postfix(std::uint64_t);
int APS5_VABI sceSslTerm_nid_postfix(int);
int APS5_VABI sceSslGetMemoryPoolStats(int, SceSsl::MemoryPoolStats*);
int APS5_VABI sceSslLoadCert(int, int, SceSsl::Data**, SceSsl::Data*, SceSsl::Data*);
int APS5_VABI sceSslCreateConnection(int, int, const char*);
int APS5_VABI sceSslDeleteConnection(int);
int APS5_VABI sceSslSetVerifyCallback(int, int (APS5_VABI *)(int, unsigned int, void**, int, void*), void*);
int APS5_VABI sceSslConnect(int);
int APS5_VABI sceSslRead(int, void*, std::size_t, int);
int APS5_VABI sceSslWrite(int, void*, std::size_t, int);
int APS5_VABI socket_nid_postfix(int, int, int);
int APS5_VABI connect_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI close_nid_postfix(int);
int APS5_VABI sceNetInit_nid_postfix();
int APS5_VABI sceNetSocket(const char*, int, int, int);
int APS5_VABI sceNetConnect(int, const void*, std::uint32_t);
int APS5_VABI sceNetSocketClose(int);
}

namespace {

constexpr int ParallelConnections = 4;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::exit(1);
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

int APS5_VABI VerifyCallback(int, unsigned int, void**, int, void*) { return 0; }

struct Server {
    mbedtls_net_context listener;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt authority;
    mbedtls_pk_context key;
    mbedtls_ssl_config config;
    std::uint16_t port = 0;
    std::string failure;

    Server() {
        mbedtls_net_init(&listener);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        mbedtls_x509_crt_init(&certificate);
        mbedtls_x509_crt_init(&authority);
        mbedtls_pk_init(&key);
        mbedtls_ssl_config_init(&config);
        Require(mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, nullptr, 0) == 0, "seed server random generator");
        Require(mbedtls_x509_crt_parse(&certificate, reinterpret_cast<const unsigned char*>(ServerPem), sizeof(ServerPem)) == 0,
            "parse server certificate");
        Require(mbedtls_x509_crt_parse(&authority, reinterpret_cast<const unsigned char*>(RootPem), sizeof(RootPem)) == 0,
            "parse client authority");
        Require(mbedtls_pk_parse_key(&key, reinterpret_cast<const unsigned char*>(ServerKey), sizeof(ServerKey), nullptr, 0,
            mbedtls_ctr_drbg_random, &drbg) == 0, "parse server key");
        Require(mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0,
            "configure server");
        mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &drbg);
        Require(mbedtls_ssl_conf_own_cert(&config, &certificate, &key) == 0, "set server certificate");
        mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
        mbedtls_ssl_conf_ca_chain(&config, &authority, nullptr);
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
        mbedtls_x509_crt_free(&authority);
        mbedtls_x509_crt_free(&certificate);
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_net_free(&listener);
    }

    void Serve(bool handshakeSucceeds) {
        mbedtls_net_context client;
        mbedtls_net_init(&client);
        mbedtls_ssl_context ssl;
        mbedtls_ssl_init(&ssl);
        if (mbedtls_net_accept(&listener, &client, nullptr, 0, nullptr) != 0 || mbedtls_ssl_setup(&ssl, &config) != 0) {
            failure = "server accept or setup failed";
        } else {
            mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, mbedtls_net_recv, nullptr);
            const int handshake = mbedtls_ssl_handshake(&ssl);
            if ((handshake == 0) != handshakeSucceeds) failure = "server handshake result differs, error " + std::to_string(handshake);
            else if (handshake == 0) Exchange(ssl);
        }
        mbedtls_ssl_free(&ssl);
        mbedtls_net_free(&client);
    }

    void Exchange(mbedtls_ssl_context& ssl) {
        std::array<unsigned char, 4> request{};
        std::size_t received = 0;
        while (received < request.size()) {
            const int result = mbedtls_ssl_read(&ssl, request.data() + received, request.size() - received);
            if (result <= 0) {
                failure = "server read failed, error " + std::to_string(result);
                return;
            }
            received += static_cast<std::size_t>(result);
        }
        if (std::memcmp(request.data(), "ping", 4) != 0) {
            failure = "server received the wrong request";
            return;
        }
        if (mbedtls_ssl_write(&ssl, reinterpret_cast<const unsigned char*>("pong"), 4) != 4) {
            failure = "server write failed";
            return;
        }
        if (mbedtls_ssl_close_notify(&ssl) != 0) failure = "server close notify failed";
    }
};

struct Sockets {
    bool net;

    int Open(std::uint16_t port) const {
        const int descriptor = net ? sceNetSocket("ssl", 2, 1, 6) : socket_nid_postfix(2, 1, 6);
        Require(descriptor >= 0, "create guest socket");
        std::array<std::uint8_t, 16> address{16, 2, static_cast<std::uint8_t>(port >> 8), static_cast<std::uint8_t>(port), 127, 0, 0, 1};
        Require((net ? sceNetConnect(descriptor, address.data(), 16) : connect_nid_postfix(descriptor, address.data(), 16)) == 0,
            "connect guest socket");
        return descriptor;
    }

    int Close(int descriptor) const { return net ? sceNetSocketClose(descriptor) : close_nid_postfix(descriptor); }
};

std::size_t InUse(int context) {
    SceSsl::MemoryPoolStats stats{};
    Require(sceSslGetMemoryPoolStats(context, &stats) == 0, "read pool statistics");
    return stats.currentInuseSize;
}

}

int main(int argc, char** argv) {
    Require(argc == 2 && (std::strcmp(argv[1], "kernel") == 0 || std::strcmp(argv[1], "net") == 0), "usage: kernel|net");
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    const Sockets sockets{std::strcmp(argv[1], "net") == 0};
    if (sockets.net) Require(sceNetInit_nid_postfix() == 0, "initialize libSceNet");
    Server server;
    std::thread serving([&] {
        server.Serve(false);
        server.Serve(true);
        for (int index = 0; index < ParallelConnections; ++index) server.Serve(true);
    });

    const int empty = sceSslInit_nid_postfix(256 * 1024);
    const int withoutAuthority = sceSslCreateConnection(empty, -1, "localhost");
    Require(Throws([&] { sceSslConnect(withoutAuthority); }), "a handshake without a loaded CA was attempted");
    Require(sceSslDeleteConnection(withoutAuthority) == 0 && sceSslTerm_nid_postfix(empty) == 0, "release the context without CA");

    const int context = sceSslInit_nid_postfix(1024 * 1024);
    std::string root = RootPem;
    SceSsl::Data authority{reinterpret_cast<std::uint8_t*>(root.data()), root.size()};
    auto* authorities = &authority;
    std::string clientCertificate = ClientPem, clientKey = ClientKey;
    SceSsl::Data certificate{reinterpret_cast<std::uint8_t*>(clientCertificate.data()), clientCertificate.size()};
    SceSsl::Data key{reinterpret_cast<std::uint8_t*>(clientKey.data()), clientKey.size()};
    Require(sceSslLoadCert(context, 1, &authorities, &certificate, &key) == 0, "load the fixture root and client certificate");
    const auto loaded = InUse(context);

    const int callback = sceSslCreateConnection(context, -1, "localhost");
    Require(sceSslSetVerifyCallback(callback, VerifyCallback, nullptr) == 0, "set verify callback");
    Require(Throws([&] { sceSslConnect(callback); }), "a verify callback was accepted");
    Require(sceSslDeleteConnection(callback) == 0, "delete callback connection");

    const int closed = sceSslCreateConnection(context, 1000, "localhost");
    Require(Throws([&] { sceSslConnect(closed); }), "a handshake over a closed descriptor was attempted");
    Require(sceSslDeleteConnection(closed) == 0, "delete connection over a closed descriptor");

    const int mismatchSocket = sockets.Open(server.port);
    const int mismatch = sceSslCreateConnection(context, mismatchSocket, "example.com");
    Require(Throws([&] { sceSslConnect(mismatch); }), "a certificate for another host was accepted");
    Require(Throws([&] { sceSslRead(mismatch, root.data(), 1, 0); }), "a failed handshake left the connection usable");
    Require(sceSslDeleteConnection(mismatch) == 0 && sockets.Close(mismatchSocket) == 0, "release the mismatched connection");

    const int descriptor = sockets.Open(server.port);
    const int connection = sceSslCreateConnection(context, descriptor, "localhost");
    std::array<char, 16> buffer{};
    Require(Throws([&] { sceSslRead(connection, buffer.data(), buffer.size(), 0); }), "read before the handshake");
    Require(Throws([&] { sceSslWrite(connection, buffer.data(), buffer.size(), 0); }), "write before the handshake");
    Require(sceSslConnect(connection) == 0, "handshake with the loopback server");
    Require(InUse(context) > loaded, "the connection is not charged to the context pool");
    Require(Throws([&] { sceSslConnect(connection); }), "a second handshake was accepted");
    Require(Throws([&] { sceSslWrite(connection, buffer.data(), 1, 1); }), "unknown write flags were accepted");
    Require(Throws([&] { sceSslRead(connection, buffer.data(), 1, 3); }), "peek was accepted");
    char request[] = "ping";
    Require(sceSslWrite(connection, request, 4, 0) == 4, "write the request");
    Require(sceSslRead(connection, buffer.data(), buffer.size(), 0) == 4 && std::memcmp(buffer.data(), "pong", 4) == 0, "read the reply");
    Require(sceSslRead(connection, buffer.data(), buffer.size(), 1) == 0, "close notify did not end the stream");
    Require(sceSslDeleteConnection(connection) == 0, "delete the connection");
    Require(InUse(context) == loaded, "the connection's pool memory was not returned");
    Require(sockets.Close(descriptor) == 0, "the socket did not stay open after the connection was deleted");

    std::array<std::thread, ParallelConnections> clients;
    for (auto& client : clients) {
        client = std::thread([&] {
            const int parallelSocket = sockets.Open(server.port);
            const int parallel = sceSslCreateConnection(context, parallelSocket, "localhost");
            std::array<char, 16> reply{};
            char ping[] = "ping";
            Require(sceSslConnect(parallel) == 0 && sceSslWrite(parallel, ping, 4, 0) == 4, "parallel handshake or write");
            Require(sceSslRead(parallel, reply.data(), reply.size(), 0) == 4 && std::memcmp(reply.data(), "pong", 4) == 0, "parallel reply");
            Require(sceSslRead(parallel, reply.data(), reply.size(), 0) == 0, "parallel end of stream");
            Require(sceSslDeleteConnection(parallel) == 0 && sockets.Close(parallelSocket) == 0, "release parallel connection");
        });
    }
    for (auto& client : clients) client.join();
    Require(InUse(context) == loaded, "parallel connections did not return their pool memory");

    serving.join();
    Require(server.failure.empty(), server.failure.c_str());
    Require(sceSslTerm_nid_postfix(context) == 0, "terminate the context");
}
