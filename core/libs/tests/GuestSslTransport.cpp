#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libSceNet/include/Net.hpp"
#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
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
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <thread>

namespace {

constexpr int ParallelConnections = 4;
constexpr int ErrorInvalidId = static_cast<int>(0x8095f006);
constexpr int ErrorInvalidValue = static_cast<int>(0x8095f007);
constexpr int ErrorCnCheck = static_cast<int>(0x8095f00b);
constexpr int ErrorUnknownCa = static_cast<int>(0x8095f00c);
constexpr int ErrorNotAfterCheck = static_cast<int>(0x8095f00d);
constexpr int ErrorNotBeforeCheck = static_cast<int>(0x8095f00e);
constexpr int ErrorFatalAlert = static_cast<int>(0x8095f010);
constexpr int ErrorWantPollIn = static_cast<int>(0x8095f013);
constexpr int ErrorWantPollOut = static_cast<int>(0x8095f014);
constexpr int ErrorCallbackCheck = static_cast<int>(0x8095f0fd);
constexpr int ErrorSocketClosed = static_cast<int>(0x8095170d);
constexpr int ErrorNetBadDescriptor = static_cast<int>(0x80410109);
constexpr int ErrorNetPipe = static_cast<int>(0x80410120);
constexpr int ErrorNetReset = static_cast<int>(0x80410136);
constexpr int ErrorBadLength = static_cast<int>(0x80951773);
constexpr std::uint32_t ServerVerify = 0x1, CnCheck = 0x4, NotAfterCheck = 0x8, NotBeforeCheck = 0x10, KnownCaCheck = 0x20;
constexpr int VersionTls12 = 5;
constexpr int GuestSocketLevel = 0xffff;
constexpr int GuestNonBlocking = 0x1200;
constexpr int GuestReceiveTimeout = 0x1006;
constexpr int WaitRounds = 2000;
const char* ServerProtocols[] = {"http/1.1", nullptr};

enum class Serve { Close, Exchange, Drop, Reset, Alert };
enum class Leaf { Valid, Expired, Future };

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

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

void Settle() { std::this_thread::sleep_for(std::chrono::milliseconds(5)); }

struct CallbackRecord {
    int context;
    int result;
    int id = 0;
    unsigned int error = 0;
    int count = 0;
    int calls = 0;
    void* leaf = nullptr;
    std::string commonName;
};

int APS5_VABI RecordingCallback(int id, unsigned int error, void** certificates, int count, void* argument) {
    auto& record = *static_cast<CallbackRecord*>(argument);
    record.id = id;
    record.error = error;
    record.count = count;
    ++record.calls;
    if (count > 0 && certificates && certificates[0]) {
        record.leaf = certificates[0];
        void* name = sceSslGetSubjectName(record.context, record.leaf);
        Require(name != nullptr, "the peer certificate handle has no subject name");
        const int entries = sceSslGetNameEntryCount(record.context, name);
        Require(entries > 0, "the peer subject has no entries");
        char oid[16]{};
        char value[64]{};
        std::size_t valueLength = 0;
        Require(sceSslGetNameEntryInfo(record.context, name, entries - 1, oid, sizeof(oid), value, sizeof(value), &valueLength) == 0,
            "read the peer subject's last entry");
        record.commonName = std::string(oid) + "=" + value;
        Require(sceSslFreeSslCertName(record.context, name) == 0, "free the peer subject name");
    }
    return record.result;
}

struct Server {
    mbedtls_net_context listener;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt expiredCertificate;
    mbedtls_x509_crt futureCertificate;
    mbedtls_x509_crt authority;
    mbedtls_pk_context key;
    mbedtls_pk_context expiredKey;
    mbedtls_pk_context futureKey;
    mbedtls_ssl_config configs[3];
    std::uint16_t port = 0;
    std::string failure;

    Server() {
        mbedtls_net_init(&listener);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&drbg);
        mbedtls_x509_crt_init(&certificate);
        mbedtls_x509_crt_init(&expiredCertificate);
        mbedtls_x509_crt_init(&futureCertificate);
        mbedtls_x509_crt_init(&authority);
        mbedtls_pk_init(&key);
        mbedtls_pk_init(&expiredKey);
        mbedtls_pk_init(&futureKey);
        for (auto& config : configs) mbedtls_ssl_config_init(&config);
        Require(mbedtls_ctr_drbg_seed(&drbg, mbedtls_entropy_func, &entropy, nullptr, 0) == 0, "seed server random generator");
        Require(mbedtls_x509_crt_parse(&certificate, reinterpret_cast<const unsigned char*>(ServerPem), sizeof(ServerPem)) == 0,
            "parse server certificate");
        Require(mbedtls_x509_crt_parse(&authority, reinterpret_cast<const unsigned char*>(RootPem), sizeof(RootPem)) == 0,
            "parse client authority");
        Require(mbedtls_pk_parse_key(&key, reinterpret_cast<const unsigned char*>(ServerKey), sizeof(ServerKey), nullptr, 0,
            mbedtls_ctr_drbg_random, &drbg) == 0, "parse server key");
        Require(mbedtls_x509_crt_parse(&expiredCertificate, reinterpret_cast<const unsigned char*>(ExpiredServerPem), sizeof(ExpiredServerPem)) == 0,
            "parse expired server certificate");
        Require(mbedtls_pk_parse_key(&expiredKey, reinterpret_cast<const unsigned char*>(ExpiredServerKey), sizeof(ExpiredServerKey), nullptr, 0,
            mbedtls_ctr_drbg_random, &drbg) == 0, "parse expired server key");
        Require(mbedtls_x509_crt_parse(&futureCertificate, reinterpret_cast<const unsigned char*>(FutureServerPem), sizeof(FutureServerPem)) == 0,
            "parse future server certificate");
        Require(mbedtls_pk_parse_key(&futureKey, reinterpret_cast<const unsigned char*>(FutureServerKey), sizeof(FutureServerKey), nullptr, 0,
            mbedtls_ctr_drbg_random, &drbg) == 0, "parse future server key");
        mbedtls_x509_crt* leaves[] = {&certificate, &expiredCertificate, &futureCertificate};
        mbedtls_pk_context* keys[] = {&key, &expiredKey, &futureKey};
        for (int index = 0; index < 3; ++index) {
            auto& config = configs[index];
            Require(mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0,
                "configure server");
            mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &drbg);
            Require(mbedtls_ssl_conf_own_cert(&config, leaves[index], keys[index]) == 0, "set server certificate");
            mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_REQUIRED);
            mbedtls_ssl_conf_ca_chain(&config, &authority, nullptr);
            Require(mbedtls_ssl_conf_alpn_protocols(&config, ServerProtocols) == 0, "set server protocols");
        }
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
        for (auto& config : configs) mbedtls_ssl_config_free(&config);
        mbedtls_pk_free(&futureKey);
        mbedtls_pk_free(&expiredKey);
        mbedtls_pk_free(&key);
        mbedtls_x509_crt_free(&authority);
        mbedtls_x509_crt_free(&futureCertificate);
        mbedtls_x509_crt_free(&expiredCertificate);
        mbedtls_x509_crt_free(&certificate);
        mbedtls_ctr_drbg_free(&drbg);
        mbedtls_entropy_free(&entropy);
        mbedtls_net_free(&listener);
    }

    void Serve(Serve mode, bool requireClientCertificate = true, Leaf leaf = Leaf::Valid) {
        auto& config = configs[static_cast<int>(leaf)];
        mbedtls_ssl_conf_authmode(&config, requireClientCertificate ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE);
        mbedtls_net_context client;
        mbedtls_net_init(&client);
        mbedtls_ssl_context ssl;
        mbedtls_ssl_init(&ssl);
        if (mbedtls_net_accept(&listener, &client, nullptr, 0, nullptr) != 0 || mbedtls_ssl_setup(&ssl, &config) != 0) {
            failure = "server accept or setup failed";
        } else if (mode == Serve::Alert) {
            Alert(client);
        } else {
            mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, mbedtls_net_recv, nullptr);
            const int handshake = mbedtls_ssl_handshake(&ssl);
            if (handshake != 0) failure = "server handshake failed, error " + std::to_string(handshake);
            else if (mode == Serve::Exchange) Exchange(ssl);
            else if (mode == Serve::Close) mbedtls_ssl_close_notify(&ssl);
            else if (mode == Serve::Reset) Reset(ssl, client);
        }
        mbedtls_ssl_free(&ssl);
        mbedtls_net_free(&client);
    }

    void Alert(mbedtls_net_context& client) {
        std::array<unsigned char, 512> hello{};
        if (mbedtls_net_recv(&client, hello.data(), hello.size()) <= 0) {
            failure = "server did not receive the client hello before the alert";
            return;
        }
        const unsigned char alert[] = {0x15, 0x03, 0x03, 0x00, 0x02, 0x02, 0x28};
        if (mbedtls_net_send(&client, alert, sizeof(alert)) != sizeof(alert)) failure = "server could not send the alert";
    }

    void Reset(mbedtls_ssl_context& ssl, mbedtls_net_context& client) {
        unsigned char signal = 0;
        if (mbedtls_ssl_read(&ssl, &signal, 1) != 1) {
            failure = "server did not receive the byte before the reset";
            return;
        }
        linger immediate{1, 0};
        setsockopt(client.fd, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&immediate), sizeof(immediate));
#ifdef _WIN32
        closesocket(client.fd);
#else
        close(client.fd);
#endif
        client.fd = -1;
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

    int Set(int descriptor, int option, const void* value, std::uint32_t length) const {
        return net ? sceNetSetsockopt(descriptor, GuestSocketLevel, option, value, length)
                   : setsockopt_nid_postfix(descriptor, GuestSocketLevel, option, value, length);

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
        server.Serve(Serve::Close, false);
        server.Serve(Serve::Close, false);
        server.Serve(Serve::Close, false);
        server.Serve(Serve::Close);
        server.Serve(Serve::Close);
        server.Serve(Serve::Close);
        server.Serve(Serve::Close);
        server.Serve(Serve::Exchange);
        server.Serve(Serve::Exchange);
        server.Serve(Serve::Drop);
        server.Serve(Serve::Reset);
        server.Serve(Serve::Alert);
        server.Serve(Serve::Close, true, Leaf::Expired);
        server.Serve(Serve::Close, true, Leaf::Expired);
        server.Serve(Serve::Close, true, Leaf::Future);
        server.Serve(Serve::Close, true, Leaf::Future);
        if (sockets.net) server.Serve(Serve::Exchange);
        for (int index = 0; index < ParallelConnections; ++index) server.Serve(Serve::Exchange);
    });

    Require(sceSslInit_nid_postfix(0) == static_cast<int>(0x80020016), "an empty pool was accepted");
    const int empty = sceSslInit_nid_postfix(256 * 1024);
    const int untrustedSocket = sockets.Open(server.port);
    const int untrusted = sceSslCreateConnection(empty, untrustedSocket, "localhost");
    Require(sceSslConnect(untrusted) == ErrorUnknownCa, "a server certificate without a loaded CA passed the initial verify options");
    Require(sceSslConnect(untrusted) == ErrorUnknownCa, "a second connect did not repeat the verification result");
    std::array<char, 16> scratch{};
    Require(Throws([&] { sceSslRead(untrusted, scratch.data(), 1, 0); }), "a connection that failed verification was readable");
    Require(sceSslClose(untrusted) == 0, "close a connection that failed verification");
    Require(sceSslDeleteConnection(untrusted) == 0 && sockets.Close(untrustedSocket) == 0, "release the untrusted connection");
    Require(sceSslDeleteConnection(untrusted) == ErrorInvalidId && sceSslClose(untrusted) == ErrorInvalidId
        && sceSslConnect(untrusted) == ErrorInvalidId && sceSslRead(untrusted, scratch.data(), 1, 0) == ErrorInvalidId
        && sceSslWrite(untrusted, scratch.data(), 1, 0) == ErrorInvalidId && sceSslSetAlpn(untrusted, "h2") == ErrorInvalidId
        && sceSslEnableVerifyOption(untrusted, CnCheck) == ErrorInvalidId && sceSslDisableVerifyOption(untrusted, CnCheck) == ErrorInvalidId
        && sceSslSetMinSslVersion(untrusted, VersionTls12) == ErrorInvalidId && sceSslSetVerifyCallback(untrusted, nullptr, nullptr) == ErrorInvalidId,
        "a deleted connection did not report an invalid id");

    const int unverifiedSocket = sockets.Open(server.port);
    const int unverified = sceSslCreateConnection(empty, unverifiedSocket, "localhost");
    Require(sceSslDisableVerifyOption(unverified, ServerVerify) == 0, "disable server verification");
    Require(sceSslConnect(unverified) == 0, "with server verification disabled no loaded CA is no failure");
    Require(sceSslGetAlpnSelected(unverified) == nullptr, "a protocol was selected without an ALPN list");
    Require(sceSslRead(unverified, scratch.data(), scratch.size(), 0) == 0, "close notify did not end the unverified stream");
    Require(sceSslDeleteConnection(unverified) == 0 && sockets.Close(unverifiedSocket) == 0, "release the unverified connection");

    CallbackRecord accepting{empty, 0};
    const int acceptedSocket = sockets.Open(server.port);
    const int accepted = sceSslCreateConnection(empty, acceptedSocket, "localhost");
    Require(sceSslSetVerifyCallback(accepted, RecordingCallback, &accepting) == 0, "set the accepting verify callback");
    Require(sceSslConnect(accepted) == 0, "a verify callback that returns 0 did not accept the connection");
    Require(accepting.id == empty && accepting.error == KnownCaCheck, "the verify callback did not receive the context id and the failed option");
    Require(accepting.count == 1 && accepting.commonName == "commonName=localhost", "the verify callback did not receive the peer chain");
    Require(sceSslConnect(accepted) == 0 && accepting.calls == 1, "a second connect after an accepted callback ran the callback again");
    Require(sceSslRead(accepted, scratch.data(), scratch.size(), 0) == 0, "an accepted connection was not readable");
    Require(sceSslDeleteConnection(accepted) == 0 && sockets.Close(acceptedSocket) == 0, "release the accepted connection");
    Require(Throws([&] { sceSslGetSubjectName(empty, accepting.leaf); }), "a peer certificate handle outlived its connection");
    Require(sceSslTerm_nid_postfix(empty) == 0, "release the context without CA");
    Require(sceSslTerm_nid_postfix(empty) == ErrorInvalidId, "a second context termination did not report an invalid id");

    const int context = sceSslInit_nid_postfix(1024 * 1024);
    std::string root = RootPem;
    SceSsl::Data authority{reinterpret_cast<std::uint8_t*>(root.data()), root.size()};
    auto* authorities = &authority;
    std::string clientCertificate = ClientPem, clientKey = ClientKey;
    SceSsl::Data certificate{reinterpret_cast<std::uint8_t*>(clientCertificate.data()), clientCertificate.size()};
    SceSsl::Data key{reinterpret_cast<std::uint8_t*>(clientKey.data()), clientKey.size()};
    Require(sceSslLoadCert(context, 1, &authorities, &certificate, &key) == 0, "load the fixture root and client certificate");
    const auto loaded = InUse(context);

    Require(sceSslCreateConnection(context, 1000, nullptr) == ErrorInvalidValue, "a null host was accepted");
    const int closed = sceSslCreateConnection(context, 1000, "localhost");
    Require(sceSslConnect(closed) == ErrorNetBadDescriptor, "a handshake over a closed descriptor did not report the bad descriptor");
    Require(sceSslClose(closed) == 0, "close a connection that never connected");
    Require(sceSslDeleteConnection(closed) == 0, "delete connection over a closed descriptor");

    const int mismatchSocket = sockets.Open(server.port);
    const int mismatch = sceSslCreateConnection(context, mismatchSocket, "example.com");
    Require(sceSslConnect(mismatch) == ErrorCnCheck, "a certificate for another host passed the host name check");
    Require(sceSslConnect(mismatch) == ErrorCnCheck, "a second connect did not repeat the host name result");
    Require(Throws([&] { sceSslRead(mismatch, root.data(), 1, 0); }), "a failed verification left the connection usable");
    Require(sceSslDeleteConnection(mismatch) == 0 && sockets.Close(mismatchSocket) == 0, "release the mismatched connection");

    const int lenientSocket = sockets.Open(server.port);
    const int lenient = sceSslCreateConnection(context, lenientSocket, "example.com");
    Require(sceSslDisableVerifyOption(lenient, CnCheck) == 0, "disable the host name check");
    Require(sceSslConnect(lenient) == 0, "a disabled host name check still failed the handshake");
    Require(sceSslDeleteConnection(lenient) == 0 && sockets.Close(lenientSocket) == 0, "release the lenient connection");

    CallbackRecord rejecting{context, -1};
    const int rejectedSocket = sockets.Open(server.port);
    const int rejected = sceSslCreateConnection(context, rejectedSocket, "localhost");
    Require(sceSslSetVerifyCallback(rejected, RecordingCallback, &rejecting) == 0, "set the rejecting verify callback");
    Require(sceSslConnect(rejected) == ErrorCallbackCheck, "a negative verify callback result did not fail the connect");
    Require(rejecting.error == 0 && rejecting.count == 1 && rejecting.id == context, "the verify callback did not see a passed chain");
    Require(sceSslConnect(rejected) == ErrorCallbackCheck && rejecting.calls == 2, "a second connect did not run the callback again");
    Require(sceSslRead(rejected, scratch.data(), 1, 0) == ErrorCallbackCheck && rejecting.calls == 3, "a read after the rejection did not run the callback again");
    Require(sceSslWrite(rejected, scratch.data(), 1, 0) == ErrorCallbackCheck && rejecting.calls == 4, "a write after the rejection did not run the callback again");
    Require(sceSslClose(rejected) == 0, "close the rejected connection");
    Require(sceSslDeleteConnection(rejected) == 0 && sockets.Close(rejectedSocket) == 0, "release the rejected connection");

    const int earlySocket = sockets.Open(server.port);
    const int early = sceSslCreateConnection(context, earlySocket, "localhost");
    Require(sceSslClose(early) == 0, "close before the handshake");
    Require(sceSslRead(early, scratch.data(), scratch.size(), 0) == 0, "a read before the handshake did not run it and reach the close notify");
    Require(sceSslConnect(early) == 0, "a connect after the read-driven handshake did not return 0");
    Require(sceSslDeleteConnection(early) == 0 && sockets.Close(earlySocket) == 0, "release the early connection");

    const int descriptor = sockets.Open(server.port);
    const int connection = sceSslCreateConnection(context, descriptor, "localhost");
    std::array<char, 16> buffer{};
    Require(sceSslSetMinSslVersion(connection, 0) == ErrorInvalidValue && sceSslSetMinSslVersion(connection, 6) == ErrorInvalidValue
        && sceSslSetMinSslVersion(connection, 2) == 0, "the accepted minimum versions changed");
    Require(sceSslEnableVerifyOption(connection, 0x40) == ErrorInvalidValue && sceSslDisableVerifyOption(connection, 0x1000) == ErrorInvalidValue
        && sceSslEnableVerifyOption(connection, 0x3f) == 0, "the verify option mask changed");
    Require(sceSslSetAlpn(connection, "") == ErrorBadLength, "an empty ALPN list was accepted");
    Require(sceSslSetMinSslVersion(connection, VersionTls12) == 0 && sceSslSetAlpn(connection, "h2,http/1.1") == 0, "set version and ALPN");
    Require(sceSslGetAlpnSelected(connection) == nullptr, "a protocol was selected before the handshake");
    Require(sceSslConnect(connection) == 0, "handshake with the loopback server");
    Require(InUse(context) > loaded, "the connection is not charged to the context pool");
    Require(sceSslConnect(connection) == 0, "a second connect after success did not return the same result");
    const char* selected = sceSslGetAlpnSelected(connection);
    Require(selected && std::strcmp(selected, "http/1.1") == 0, "the negotiated ALPN protocol is not reported");
    Require(sceSslWrite(connection, buffer.data(), 1, 1) == ErrorInvalidValue, "unknown write flags were accepted");
    char request[] = "ping";
    Require(sceSslSend(connection, request, 4) == 4, "send the request");
    Require(sceSslRead(connection, buffer.data(), 2, 1) == 2 && std::memcmp(buffer.data(), "po", 2) == 0, "a partial read did not return the available bytes");
    Require(sceSslRecv(connection, buffer.data(), 2) == 2 && std::memcmp(buffer.data(), "ng", 2) == 0, "receive did not fill the remaining reply");
    Require(sceSslRecv(connection, buffer.data(), buffer.size()) == 0, "receive did not stop at the end of the stream");
    Require(sceSslRead(connection, buffer.data(), buffer.size(), 1) == 0, "close notify did not end the stream");
    Require(sceSslClose(connection) == 0, "close the connection");
    Require(sceSslRead(connection, buffer.data(), buffer.size(), 0) == 0, "a read after the close did not return 0");
    Require(sceSslConnect(connection) == 0, "a connect after the close did not return 0");
    Require(selected && std::strcmp(sceSslGetAlpnSelected(connection), "http/1.1") == 0, "the ALPN protocol was forgotten after the close");
    Require(sceSslClose(connection) == 0, "a second close failed");
    Require(sceSslDeleteConnection(connection) == 0, "delete the connection");
    Require(InUse(context) == loaded, "the connection's pool memory was not returned");
    Require(sockets.Close(descriptor) == 0, "the socket did not stay open after the connection was deleted");

    const int pollingSocket = sockets.Open(server.port);
    const int enabled = 1;
    Require(sockets.Set(pollingSocket, GuestNonBlocking, &enabled, sizeof(enabled)) == 0, "make the guest socket non-blocking");
    const int polling = sceSslCreateConnection(context, pollingSocket, "localhost");
    int rounds = 0;
    for (int result; (result = sceSslConnect(polling)) != 0; Settle()) {
        Require(result == ErrorWantPollIn || result == ErrorWantPollOut, "a non-blocking handshake failed instead of asking to poll");
        Require(++rounds < WaitRounds, "a non-blocking handshake did not finish");
    }
    Require(sceSslRead(polling, buffer.data(), buffer.size(), 0) == ErrorWantPollIn, "a non-blocking read with nothing to read did not ask to poll");
    Require(sceSslRead(polling, buffer.data(), buffer.size(), 1) == ErrorWantPollIn, "a non-blocking partial read with nothing to read did not ask to poll");
    Require(sceSslWrite(polling, request, 4, 0) == 4, "write the non-blocking request");
    int received;
    for (rounds = 0; (received = sceSslRead(polling, buffer.data(), buffer.size(), 1)) == ErrorWantPollIn; Settle())
        Require(++rounds < WaitRounds, "the non-blocking reply did not arrive");
    Require(received == 4 && std::memcmp(buffer.data(), "pong", 4) == 0, "read the non-blocking reply");
    for (rounds = 0; (received = sceSslRead(polling, buffer.data(), buffer.size(), 0)) == ErrorWantPollIn; Settle())
        Require(++rounds < WaitRounds, "the non-blocking close notify did not arrive");
    Require(received == 0, "close notify did not end the non-blocking stream");
    Require(sceSslClose(polling) == 0 && sceSslDeleteConnection(polling) == 0 && sockets.Close(pollingSocket) == 0,
        "release the non-blocking connection");

    const int droppedSocket = sockets.Open(server.port);
    const int dropped = sceSslCreateConnection(context, droppedSocket, "localhost");
    Require(sceSslConnect(dropped) == 0, "handshake before the server drops the connection");
    Require(sceSslRead(dropped, buffer.data(), buffer.size(), 0) == 0, "a stream that ended without close notify did not end the read");
    Require(sceSslRead(dropped, buffer.data(), buffer.size(), 1) == 0, "a second read after the drop did not return 0");
    Require(sceSslClose(dropped) == 0, "close after the drop");
    Require(sceSslDeleteConnection(dropped) == 0 && sockets.Close(droppedSocket) == 0, "release the dropped connection");

    const int resetSocket = sockets.Open(server.port);
    const int reset = sceSslCreateConnection(context, resetSocket, "localhost");
    Require(sceSslConnect(reset) == 0, "handshake before the server resets the connection");
    Require(sceSslWrite(reset, request, 1, 0) == 1, "write the byte the server resets after");
    Require(sceSslRead(reset, buffer.data(), buffer.size(), 0) == ErrorNetReset, "a reset did not report the libSceNet reset code");
    Require(sceSslRead(reset, buffer.data(), buffer.size(), 1) == ErrorNetReset, "a second read after the reset changed the code");
    Require(sceSslWrite(reset, request, 4, 0) == ErrorNetPipe, "a write after the reset did not report the broken pipe");
    Require(sceSslConnect(reset) == 0, "a connect after the reset did not return 0");
    Require(sceSslClose(reset) == ErrorNetReset && sceSslClose(reset) == ErrorNetReset, "a close after the reset did not report the reset twice");
    Require(sceSslDeleteConnection(reset) == 0 && sockets.Close(resetSocket) == 0, "release the reset connection");

    const int alertedSocket = sockets.Open(server.port);
    const int alerted = sceSslCreateConnection(context, alertedSocket, "localhost");
    Require(sceSslConnect(alerted) == ErrorFatalAlert, "a fatal alert during the handshake did not report it");
    Require(sceSslConnect(alerted) == 0, "a connect after the fatal alert did not return 0");
    Require(sceSslRead(alerted, buffer.data(), buffer.size(), 0) == 0, "a read after the fatal alert did not return 0");
    Require(sceSslWrite(alerted, request, 4, 0) == ErrorSocketClosed, "a write after the fatal alert did not report the closed socket");
    Require(sceSslClose(alerted) == 0 && sceSslClose(alerted) == 0, "close after the fatal alert");
    Require(sceSslDeleteConnection(alerted) == 0 && sockets.Close(alertedSocket) == 0, "release the alerted connection");

    const int expiredSocket = sockets.Open(server.port);
    const int expired = sceSslCreateConnection(context, expiredSocket, "localhost");
    Require(sceSslConnect(expired) == 0, "an expired server certificate failed with the initial verify options");
    Require(sceSslRead(expired, buffer.data(), buffer.size(), 0) == 0, "close notify did not end the expired stream");
    Require(sceSslDeleteConnection(expired) == 0 && sockets.Close(expiredSocket) == 0, "release the expired connection");
    const int expiredCheckedSocket = sockets.Open(server.port);
    const int expiredChecked = sceSslCreateConnection(context, expiredCheckedSocket, "localhost");
    Require(sceSslEnableVerifyOption(expiredChecked, NotAfterCheck) == 0, "enable the expiry check");
    Require(sceSslConnect(expiredChecked) == ErrorNotAfterCheck, "an expired server certificate passed the enabled expiry check");
    Require(sceSslDeleteConnection(expiredChecked) == 0 && sockets.Close(expiredCheckedSocket) == 0, "release the checked expired connection");
    const int futureSocket = sockets.Open(server.port);
    const int future = sceSslCreateConnection(context, futureSocket, "localhost");
    Require(sceSslEnableVerifyOption(future, NotAfterCheck) == 0, "enable the expiry check on the future connection");
    Require(sceSslConnect(future) == 0, "a not yet valid server certificate failed without the start check");
    Require(sceSslDeleteConnection(future) == 0 && sockets.Close(futureSocket) == 0, "release the future connection");
    const int futureCheckedSocket = sockets.Open(server.port);
    const int futureChecked = sceSslCreateConnection(context, futureCheckedSocket, "localhost");
    Require(sceSslEnableVerifyOption(futureChecked, NotBeforeCheck) == 0, "enable the start check");
    Require(sceSslConnect(futureChecked) == ErrorNotBeforeCheck, "a not yet valid server certificate passed the enabled start check");
    Require(sceSslDeleteConnection(futureChecked) == 0 && sockets.Close(futureCheckedSocket) == 0, "release the checked future connection");

    if (sockets.net) {
        const int timedSocket = sockets.Open(server.port);
        const GuestTimeval timeout{0, 200000};
        Require(sockets.Set(timedSocket, GuestReceiveTimeout, &timeout, sizeof(timeout)) == 0, "set the receive timeout");
        const int timed = sceSslCreateConnection(context, timedSocket, "localhost");
        Require(sceSslConnect(timed) == 0, "handshake over the socket with a receive timeout");
        Require(sceSslRead(timed, buffer.data(), buffer.size(), 0) == ErrorWantPollIn, "an expired receive timeout did not ask to poll");
        Require(sceSslWrite(timed, request, 4, 0) == 4, "write the request after a timeout");
        Require(sceSslRead(timed, buffer.data(), buffer.size(), 0) == 4 && std::memcmp(buffer.data(), "pong", 4) == 0, "read the reply after a timeout");
        Require(sceSslRead(timed, buffer.data(), buffer.size(), 0) == 0, "close notify did not end the timed stream");
        Require(sceSslDeleteConnection(timed) == 0 && sockets.Close(timedSocket) == 0, "release the timed connection");
    }

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
            Require(sceSslClose(parallel) == 0 && sceSslDeleteConnection(parallel) == 0 && sockets.Close(parallelSocket) == 0,
                "release parallel connection");
        });
    }
    for (auto& client : clients) client.join();
    Require(InUse(context) == loaded, "parallel connections did not return their pool memory");

    serving.join();
    Require(server.failure.empty(), server.failure.c_str());
    Require(sceSslTerm_nid_postfix(context) == 0, "terminate the context");
}
