#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "SceTypes.hpp"
#include "ProcessParametersFixture.hpp"
#include "prx/libSceHttp/src/Resolver.hpp"
#include "SslCertificateFixture.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <thread>
#include <vector>

extern "C" {
int APS5_VABI sceNetInit_nid_postfix(void);
int APS5_VABI sceSslInit_nid_postfix(std::uint64_t);
int APS5_VABI sceSslTerm_nid_postfix(int);
int APS5_VABI sceHttpInit_nid_postfix(int, int, std::uint64_t);
int APS5_VABI sceHttpTerm_nid_postfix(int);
int APS5_VABI sceHttpCreateTemplate(int, const char*, int, int);
int APS5_VABI sceHttpDeleteTemplate(int);
int APS5_VABI sceHttpCreateConnectionWithURL(int, const char*, int);
int APS5_VABI sceHttpDeleteConnection(int);
int APS5_VABI sceHttpCreateRequestWithURL(int, int, const char*, std::uint64_t);
int APS5_VABI sceHttpDeleteRequest(int);
int APS5_VABI sceHttpAddRequestHeader(int, const char*, const char*, std::uint32_t);
int APS5_VABI sceHttpSetConnectTimeOut(int, std::uint32_t);
int APS5_VABI sceHttpSetResolveTimeOut(int, std::uint32_t);
int APS5_VABI sceHttpsSetSslCallback(int, HttpsCallback, void*);
int APS5_VABI sceHttpSendRequest(int, const void*, std::size_t);
int APS5_VABI sceHttpGetStatusCode(int, int*);
int APS5_VABI sceHttpGetLastErrno(int, int*);
int APS5_VABI sceHttpGetResponseContentLength(int, int*, std::uint64_t*);
int APS5_VABI sceHttpGetAllResponseHeaders(int, char**, std::size_t*);
int APS5_VABI sceHttpReadData(int, void*, std::size_t);
}

#ifdef _WIN32
using NativeSocket = SOCKET;
static constexpr NativeSocket InvalidSocket = INVALID_SOCKET;
static void CloseSocket(const NativeSocket socket) { closesocket(socket); }
#else
using NativeSocket = int;
static constexpr NativeSocket InvalidSocket = -1;
static void CloseSocket(const NativeSocket socket) { close(socket); }
#endif

static void Require(const bool value) { if (!value) std::abort(); }

static int APS5_VABI AcceptTlsCertificate(int, unsigned int, void* const*, int, void* argument) {
    ++*static_cast<int*>(argument);
    return 0;
}

class HttpServer {
public:
    explicit HttpServer(std::vector<std::string> responses) : responses_(std::move(responses)) {
        listener_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Require(listener_ != InvalidSocket);
        const int reuse = 1;
        setsockopt(listener_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;
        Require(bind(listener_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0);
        Require(listen(listener_, static_cast<int>(responses_.size())) == 0);
#ifdef _WIN32
        int addressSize = sizeof(address);
#else
        socklen_t addressSize = sizeof(address);
#endif
        Require(getsockname(listener_, reinterpret_cast<sockaddr*>(&address), &addressSize) == 0);
        port_ = ntohs(address.sin_port);
        worker_ = std::thread([this] { Serve(); });
    }

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    ~HttpServer() {
        Wait();
        if (listener_ != InvalidSocket) CloseSocket(listener_);
    }

    void Wait() {
        if (worker_.joinable()) worker_.join();
    }

    void SetResponse(const std::size_t index, std::string response) {
        std::lock_guard lock(responseMutex_);
        responses_.at(index) = std::move(response);
    }

    std::uint16_t Port() const { return port_; }
    const std::vector<std::string>& Requests() const { return requests_; }

private:
    static bool SendAll(const NativeSocket socket, const std::string& data) {
        std::size_t sent = 0;
        while (sent < data.size()) {
#ifdef _WIN32
            const int result = send(socket, data.data() + sent, static_cast<int>(data.size() - sent), 0);
#else
            const auto result = send(socket, data.data() + sent, data.size() - sent, 0);
#endif
            if (result <= 0) return false;
            sent += static_cast<std::size_t>(result);
        }
        return true;
    }

    static std::size_t RequestLength(const std::string& request) {
        const auto start = request.find("\r\nContent-Length:");
        if (start == std::string::npos) return 0;
        auto value = start + sizeof("\r\nContent-Length:") - 1;
        while (value < request.size() && request[value] == ' ') ++value;
        std::size_t result = 0;
        while (value < request.size() && request[value] >= '0' && request[value] <= '9') {
            result = result * 10 + static_cast<std::size_t>(request[value] - '0');
            ++value;
        }
        return result;
    }

    void Serve() {
        for (std::size_t index = 0; index < responses_.size(); ++index) {
            const NativeSocket client = accept(listener_, nullptr, nullptr);
            if (client == InvalidSocket) return;
            std::string request;
            std::array<char, 4096> bytes{};
            std::size_t headerEnd = std::string::npos;
            std::size_t contentLength = 0;
            for (;;) {
#ifdef _WIN32
                const int count = recv(client, bytes.data(), static_cast<int>(bytes.size()), 0);
#else
                const auto count = recv(client, bytes.data(), bytes.size(), 0);
#endif
                if (count <= 0) break;
                request.append(bytes.data(), static_cast<std::size_t>(count));
                if (headerEnd == std::string::npos) {
                    headerEnd = request.find("\r\n\r\n");
                    if (headerEnd != std::string::npos) contentLength = RequestLength(request);
                }
                if (headerEnd != std::string::npos && request.size() >= headerEnd + 4 + contentLength) break;
            }
            requests_.push_back(std::move(request));
            std::string response;
            {
                std::lock_guard lock(responseMutex_);
                response = responses_[index];
            }
            SendAll(client, response);
#ifdef _WIN32
            shutdown(client, SD_BOTH);
#else
            shutdown(client, SHUT_RDWR);
#endif
            CloseSocket(client);
        }
    }

    NativeSocket listener_ = InvalidSocket;
    std::uint16_t port_ = 0;
    std::vector<std::string> responses_;
    std::mutex responseMutex_;
    std::vector<std::string> requests_;
    std::thread worker_;
};

class TlsHttpServer {
public:
    TlsHttpServer() {
        mbedtls_net_init(&listener_);
        mbedtls_entropy_init(&entropy_);
        mbedtls_ctr_drbg_init(&random_);
        mbedtls_ssl_config_init(&config_);
        mbedtls_x509_crt_init(&certificate_);
        mbedtls_pk_init(&key_);
        Require(mbedtls_ctr_drbg_seed(&random_, mbedtls_entropy_func, &entropy_,
            reinterpret_cast<const unsigned char*>("anyps5-http-test"), sizeof("anyps5-http-test") - 1) == 0);
        Require(mbedtls_x509_crt_parse(&certificate_, reinterpret_cast<const unsigned char*>(ServerPem), sizeof(ServerPem)) == 0);
        Require(mbedtls_pk_parse_key(&key_, reinterpret_cast<const unsigned char*>(ServerKey), sizeof(ServerKey), nullptr, 0,
            mbedtls_ctr_drbg_random, &random_) == 0);
        Require(mbedtls_ssl_config_defaults(&config_, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT) == 0);
        mbedtls_ssl_conf_rng(&config_, mbedtls_ctr_drbg_random, &random_);
        mbedtls_ssl_conf_authmode(&config_, MBEDTLS_SSL_VERIFY_NONE);
        Require(mbedtls_ssl_conf_own_cert(&config_, &certificate_, &key_) == 0);
        Require(mbedtls_net_bind(&listener_, "127.0.0.1", "0", MBEDTLS_NET_PROTO_TCP) == 0);
        sockaddr_in address{};
#ifdef _WIN32
        int addressSize = sizeof(address);
#else
        socklen_t addressSize = sizeof(address);
#endif
        Require(getsockname(listener_.fd, reinterpret_cast<sockaddr*>(&address), &addressSize) == 0);
        port_ = ntohs(address.sin_port);
        worker_ = std::thread([this] { Serve(); });
    }

    TlsHttpServer(const TlsHttpServer&) = delete;
    TlsHttpServer& operator=(const TlsHttpServer&) = delete;

    ~TlsHttpServer() {
        Wait();
        mbedtls_ssl_config_free(&config_);
        mbedtls_pk_free(&key_);
        mbedtls_x509_crt_free(&certificate_);
        mbedtls_ctr_drbg_free(&random_);
        mbedtls_entropy_free(&entropy_);
        mbedtls_net_free(&listener_);
    }

    void Wait() {
        if (worker_.joinable()) worker_.join();
    }

    std::uint16_t Port() const { return port_; }
    const std::string& Failure() const { return failure_; }

private:
    void Serve() {
        mbedtls_net_context client;
        mbedtls_net_init(&client);
        mbedtls_ssl_context ssl;
        mbedtls_ssl_init(&ssl);
        if (mbedtls_net_accept(&listener_, &client, nullptr, 0, nullptr) != 0 || mbedtls_ssl_setup(&ssl, &config_) != 0) {
            failure_ = "TLS server accept or setup failed";
        } else {
            mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, mbedtls_net_recv, nullptr);
            if (mbedtls_ssl_handshake(&ssl) != 0) {
                failure_ = "TLS server handshake failed";
            } else {
                std::string request;
                std::array<unsigned char, 2048> buffer{};
                while (request.find("\r\n\r\n") == std::string::npos && request.size() < 8192) {
                    const int count = mbedtls_ssl_read(&ssl, buffer.data(), buffer.size());
                    if (count <= 0) {
                        failure_ = "TLS server did not receive the HTTP request";
                        break;
                    }
                    request.append(reinterpret_cast<const char*>(buffer.data()), static_cast<std::size_t>(count));
                }
                if (failure_.empty() && request.find("GET /secure HTTP/1.1\r\n") != 0)
                    failure_ = "TLS server received an unexpected HTTP request";
                if (failure_.empty()) {
                    constexpr char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\ntls";
                    std::size_t sent = 0;
                    while (sent < sizeof(response) - 1) {
                        const int count = mbedtls_ssl_write(&ssl, reinterpret_cast<const unsigned char*>(response) + sent,
                            sizeof(response) - 1 - sent);
                        if (count <= 0) {
                            failure_ = "TLS server could not send the HTTP response";
                            break;
                        }
                        sent += static_cast<std::size_t>(count);
                    }
                    if (failure_.empty() && mbedtls_ssl_close_notify(&ssl) != 0) failure_ = "TLS server close notify failed";
                }
            }
        }
        mbedtls_ssl_free(&ssl);
        mbedtls_net_free(&client);
    }

    mbedtls_net_context listener_{};
    mbedtls_entropy_context entropy_{};
    mbedtls_ctr_drbg_context random_{};
    mbedtls_ssl_config config_{};
    mbedtls_x509_crt certificate_{};
    mbedtls_pk_context key_{};
    std::uint16_t port_ = 0;
    std::string failure_;
    std::thread worker_;
};

struct RequestObjects {
    int context;
    int httpTemplate;
    int connection;
    int request;
};

static RequestObjects CreateRequestWithUrl(const std::string& url, const int method,
    const std::uint32_t resolveTimeOutUsec = 0, const std::uint32_t connectTimeOutUsec = 0) {
    RequestObjects objects{};
    objects.context = sceHttpInit_nid_postfix(0, 0, 0x10000);
    Require(objects.context > 0);
    objects.httpTemplate = sceHttpCreateTemplate(objects.context, "anyps5-http-test", 2, 0);
    Require(objects.httpTemplate > 0);
    if (resolveTimeOutUsec) Require(sceHttpSetResolveTimeOut(objects.httpTemplate, resolveTimeOutUsec) == 0);
    if (connectTimeOutUsec) Require(sceHttpSetConnectTimeOut(objects.httpTemplate, connectTimeOutUsec) == 0);
    objects.connection = sceHttpCreateConnectionWithURL(objects.httpTemplate, url.c_str(), 0);
    Require(objects.connection > 0);
    objects.request = sceHttpCreateRequestWithURL(objects.connection, method, url.c_str(), 0);
    Require(objects.request > 0);
    return objects;
}

static RequestObjects CreateRequest(const std::uint16_t port, const int method, const char* path = "/") {
    const std::string url = "http://127.0.0.1:" + std::to_string(port) + path;
    return CreateRequestWithUrl(url, method);
}

static void DeleteRequest(const RequestObjects& objects) {
    Require(sceHttpDeleteRequest(objects.request) == 0);
    Require(sceHttpDeleteConnection(objects.connection) == 0);
    Require(sceHttpDeleteTemplate(objects.httpTemplate) == 0);
    Require(sceHttpTerm_nid_postfix(objects.context) == 0);
}

static std::string ReadBody(const int request) {
    std::string body;
    std::array<char, 3> buffer{};
    for (;;) {
        const int count = sceHttpReadData(request, buffer.data(), buffer.size());
        Require(count >= 0 && count <= static_cast<int>(buffer.size()));
        if (count == 0) break;
        body.append(buffer.data(), static_cast<std::size_t>(count));
    }
    return body;
}

static void CheckContentLengthResponse() {
    HttpServer server({"HTTP/1.1 200 OK\r\nContent-Length: 13\r\nX-Reply: works\r\nConnection: close\r\n\r\nhello, world!"});
    const auto objects = CreateRequest(server.Port(), 0, "/catalog?page=2");
    Require(sceHttpAddRequestHeader(objects.request, "X-Device-Id", "fixture", 0) == 0);
    Require(sceHttpSendRequest(objects.request, nullptr, 0) == 0);
    int status = 0;
    Require(sceHttpGetStatusCode(objects.request, &status) == 0 && status == 200);
    int lengthKnown = -1;
    std::uint64_t length = 0;
    Require(sceHttpGetResponseContentLength(objects.request, &lengthKnown, &length) == 0 && lengthKnown == 0 && length == 13);
    char* headers = nullptr;
    std::size_t headerSize = 0;
    Require(sceHttpGetAllResponseHeaders(objects.request, &headers, &headerSize) == 0 && headers && headerSize > 0);
    Require(std::string_view(headers, headerSize).find("X-Reply: works\r\n") != std::string_view::npos);
    Require(ReadBody(objects.request) == "hello, world!");
    server.Wait();
    Require(server.Requests().size() == 1);
    Require(server.Requests()[0].find("GET /catalog?page=2 HTTP/1.1\r\n") == 0);
    Require(server.Requests()[0].find("Host: 127.0.0.1:") != std::string::npos);
    Require(server.Requests()[0].find("X-Device-Id: fixture\r\n") != std::string::npos);
    DeleteRequest(objects);
}

static void CheckChunkedResponse() {
    HttpServer server({"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n3\r\nabc\r\n4\r\ndefg\r\n0\r\nX-Trailer: done\r\n\r\n"});
    const auto objects = CreateRequest(server.Port(), 0);
    Require(sceHttpSendRequest(objects.request, nullptr, 0) == 0);
    int lengthKnown = -1;
    std::uint64_t length = 1;
    Require(sceHttpGetResponseContentLength(objects.request, &lengthKnown, &length) == 0 && lengthKnown == 2 && length == 0);
    Require(ReadBody(objects.request) == "abcdefg");
    DeleteRequest(objects);
}

static void CheckTlsTransport() {
    TlsHttpServer server;
    Require(sceNetInit_nid_postfix() == 0);
    const int sslContext = sceSslInit_nid_postfix(1024 * 1024);
    Require(sslContext > 0);
    const int context = sceHttpInit_nid_postfix(0, sslContext, 0x10000);
    Require(context > 0);
    const int httpTemplate = sceHttpCreateTemplate(context, "anyps5-https-test", 2, 0);
    Require(httpTemplate > 0);
    int callbackCount = 0;
    Require(sceHttpsSetSslCallback(httpTemplate, AcceptTlsCertificate, &callbackCount) == 0);
    const auto url = "https://localhost:" + std::to_string(server.Port()) + "/secure";
    const int connection = sceHttpCreateConnectionWithURL(httpTemplate, url.c_str(), 0);
    Require(connection > 0);
    const int request = sceHttpCreateRequestWithURL(connection, 0, url.c_str(), 0);
    Require(request > 0);
    Require(sceHttpSendRequest(request, nullptr, 0) == 0);
    int status = 0;
    Require(sceHttpGetStatusCode(request, &status) == 0 && status == 200);
    Require(callbackCount > 0);
    std::array<char, 8> body{};
    Require(sceHttpReadData(request, body.data(), body.size()) == 3 && std::memcmp(body.data(), "tls", 3) == 0);
    Require(sceHttpReadData(request, body.data(), body.size()) == 0);
    server.Wait();
    Require(sceHttpDeleteRequest(request) == 0 && sceHttpDeleteConnection(connection) == 0 &&
        sceHttpDeleteTemplate(httpTemplate) == 0 && sceHttpTerm_nid_postfix(context) == 0);
    Require(server.Failure().empty());
    Require(sceSslTerm_nid_postfix(sslContext) == 0);
}

static void CheckConnectTimeout() {
    const auto objects = CreateRequestWithUrl("http://192.0.2.1:9/", 0, 0, 100000);
    const auto start = std::chrono::steady_clock::now();
    const int result = sceHttpSendRequest(objects.request, nullptr, 0);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(result == static_cast<int>(0x80431063u));
    Require(elapsed < std::chrono::seconds(2));
    DeleteRequest(objects);
}

static void CheckResolveTimeout() {
    const auto objects = CreateRequestWithUrl("http://127.0.0.1:9/", 0, 50000);
    GuestHttpTest::SetResolverDelayForTests(250000);
    const auto start = std::chrono::steady_clock::now();
    const int result = sceHttpSendRequest(objects.request, nullptr, 0);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    GuestHttpTest::SetResolverDelayForTests(0);
    Require(result == static_cast<int>(0x80431063u));
    Require(elapsed < std::chrono::seconds(2));
    int lastError = 0;
    Require(sceHttpGetLastErrno(objects.request, &lastError) == 0 && lastError == static_cast<int>(0x804101e1u));
    DeleteRequest(objects);
}

static void CheckRedirect() {
    HttpServer server({"", "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\ndone"});
    const std::string redirect = "HTTP/1.1 302 Found\r\nLocation: http://localhost:" + std::to_string(server.Port()) +
        "/final\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    server.SetResponse(0, redirect);
    const auto objects = CreateRequest(server.Port(), 0, "/start");
    Require(sceHttpAddRequestHeader(objects.request, "Authorization", "Bearer secret", 0) == 0);
    Require(sceHttpAddRequestHeader(objects.request, "Cookie", "session=secret", 0) == 0);
    Require(sceHttpAddRequestHeader(objects.request, "Host", "origin.example", 0) == 0);
    Require(sceHttpSendRequest(objects.request, nullptr, 0) == 0);
    int status = 0;
    Require(sceHttpGetStatusCode(objects.request, &status) == 0 && status == 200);
    Require(ReadBody(objects.request) == "done");
    server.Wait();
    Require(server.Requests().size() == 2);
    Require(server.Requests()[0].find("GET /start HTTP/1.1\r\n") == 0);
    Require(server.Requests()[0].find("Authorization: Bearer secret\r\n") != std::string::npos);
    Require(server.Requests()[0].find("Cookie: session=secret\r\n") != std::string::npos);
    Require(server.Requests()[0].find("Host: origin.example\r\n") != std::string::npos);
    Require(server.Requests()[1].find("GET /final HTTP/1.1\r\n") == 0);
    Require(server.Requests()[1].find("Authorization:") == std::string::npos);
    Require(server.Requests()[1].find("Cookie:") == std::string::npos);
    Require(server.Requests()[1].find("Host: localhost:") != std::string::npos);
    DeleteRequest(objects);
}

int main() {
#ifdef _WIN32
    WSADATA winsock{};
    Require(WSAStartup(MAKEWORD(2, 2), &winsock) == 0);
#endif
    alignas(16) std::array<std::byte, 0x40> processParameters{};
    const std::uint64_t processSize = processParameters.size();
    const std::uint32_t processMagic = 0x4942524f;
    const std::uint32_t sdkVersion = 0x1000000;
    std::memcpy(processParameters.data(), &processSize, sizeof(processSize));
    std::memcpy(processParameters.data() + 8, &processMagic, sizeof(processMagic));
    std::memcpy(processParameters.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(ProcessParametersFixture::Install(processParameters.data(), processParameters.size()));
    Require(sceNetInit_nid_postfix() == 0);
    CheckContentLengthResponse();
    CheckChunkedResponse();
    CheckRedirect();
    CheckTlsTransport();
    CheckConnectTimeout();
    CheckResolveTimeout();
#ifdef _WIN32
    WSACleanup();
#endif
}
