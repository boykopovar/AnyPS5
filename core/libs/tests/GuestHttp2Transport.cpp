#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <csignal>
#endif

#include "SceTypes.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceHttp/include/Http.hpp"
#include "prx/libSceHttp2/include/Http2.hpp"
#include "prx/libSceHttp2/Transport.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libSceSsl/SslTypes.hpp"
#include "SslCertificateFixture.hpp"
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <nghttp2/nghttp2.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "HTTP2: %s\n", message); std::abort(); }
}

template<class TCall>
bool Throws(TCall call) {
    try { call(); } catch (const std::exception&) { return true; }
    return false;
}

enum class Mode { Normal, Large, NoLength, Head, NotFound, Redirect, Auth, Reset, Truncate, Gzip, GzipSplit, GzipTruncated, GzipChecksum, GzipConcat, GzipLarge, GzipEmpty, GzipEmptyMembers, GzipLargeMembers, GzipHeaders, GzipHeaderChecksum, GzipSecondHeaderChecksum, GzipSecondChecksum, GzipSecondSize, GzipTrailingJunk, StallHandshake, StallHeaders, StallBody, NoAlpn, Informational, Trailers, StallUpload, StallResponse, Cookie, CookieFlood, RedirectOnce, GzipRedirectEmpty, GzipRedirectBody, CloseHandshake, AlertHandshake, Expired, Http10, Http11, Http11Chunked, Http11Close, Http11Early, Http11Info, Http11Redirect, Http11StallHeaders, Http11StallBody, Http11BadLength, Http11Ambiguous, Http11Truncated, Http11BadChunk, Http11Gzip, Http1Plain, EarlyFinal, EarlyReset, EarlyEmpty, EarlyLarge, EarlyResetIncomplete };
const char* Protocols[] = {"h2", nullptr};
const char* Http1Protocols[] = {"http/1.1", nullptr};

std::string GzipPayload() {
    std::string payload(70000, '\0');
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>((i * 31 + 7) & 0xff);
    return payload;
}

std::uint32_t GzipCrc32(std::string_view payload) {
    std::uint32_t crc = 0xffffffffu;
    for (const unsigned char byte : payload) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

std::string StoredGzip(const std::string& payload) {
    static const unsigned char header[] = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};
    std::string body(reinterpret_cast<const char*>(header), sizeof(header));
    std::size_t offset = 0;
    do {
        const std::size_t count = std::min<std::size_t>(payload.size() - offset, 65535);
        body.push_back(static_cast<char>(offset + count == payload.size() ? 1 : 0));
        body.push_back(static_cast<char>(count & 0xff));
        body.push_back(static_cast<char>((count >> 8) & 0xff));
        body.push_back(static_cast<char>(~count & 0xff));
        body.push_back(static_cast<char>((~count >> 8) & 0xff));
        body.append(payload, offset, count);
        offset += count;
    } while (offset < payload.size());
    const auto crc = GzipCrc32(payload);
    const std::uint32_t size = static_cast<std::uint32_t>(payload.size());
    for (int shift = 0; shift < 32; shift += 8) body.push_back(static_cast<char>((crc >> shift) & 0xff));
    for (int shift = 0; shift < 32; shift += 8) body.push_back(static_cast<char>((size >> shift) & 0xff));
    return body;
}

std::string HeaderGzip(const std::string& payload, bool corrupt = false) {
    const auto member = StoredGzip(payload);
    auto header = member.substr(0, 10);
    header[3] = 0x1e;
    header.append("\x03\0abc", 5);
    header += "part.bin";
    header.push_back('\0');
    header += "loopback header";
    header.push_back('\0');
    const auto crc = GzipCrc32(header);
    header.push_back(static_cast<char>(crc & 0xff));
    header.push_back(static_cast<char>(((crc >> 8) & 0xff) ^ (corrupt ? 1 : 0)));
    return header + member.substr(10);
}

struct ResponseSpec {
    int status = 0;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::pair<std::string, std::string>> informationalHeaders;
};

class Server {
public:
    explicit Server(Mode mode, std::string location = "/another", int redirectStatus = 302, const char* bindHost = "127.0.0.1",
        std::vector<std::string> responseCookies = {}, bool clientMayClose = false)
        : mode(mode), location(std::move(location)), redirectStatus(redirectStatus), responseCookies(std::move(responseCookies)), clientMayClose(clientMayClose) {
        mbedtls_net_init(&listener);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&random);
        mbedtls_x509_crt_init(&certificate);
        mbedtls_pk_init(&key);
        mbedtls_ssl_config_init(&config);
        Require(mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy, nullptr, 0) == 0, "server RNG");
        const bool expired = mode == Mode::Expired;
        Require(mbedtls_x509_crt_parse(&certificate, reinterpret_cast<const unsigned char*>(expired ? ExpiredServerPem : ServerPem),
            expired ? sizeof(ExpiredServerPem) : sizeof(ServerPem)) == 0, "server cert");
        Require(mbedtls_pk_parse_key(&key, reinterpret_cast<const unsigned char*>(expired ? ExpiredServerKey : ServerKey),
            expired ? sizeof(ExpiredServerKey) : sizeof(ServerKey), nullptr, 0, mbedtls_ctr_drbg_random, &random) == 0, "server key");
        Require(mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0, "server TLS");
        mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &random);
        Require(mbedtls_ssl_conf_own_cert(&config, &certificate, &key) == 0, "server identity");
        mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_NONE);
        mbedtls_ssl_conf_read_timeout(&config, 2000);
        if (mode != Mode::NoAlpn) Require(mbedtls_ssl_conf_alpn_protocols(&config, IsHttp1() ? Http1Protocols : Protocols) == 0, "server ALPN");
        Require(mbedtls_net_bind(&listener, bindHost, "0", MBEDTLS_NET_PROTO_TCP) == 0, "server bind");
        sockaddr_storage address{};
#ifdef _WIN32
        int size = sizeof(address);
#else
        socklen_t size = sizeof(address);
#endif
        Require(getsockname(listener.fd, reinterpret_cast<sockaddr*>(&address), &size) == 0, "server port");
        port = address.ss_family == AF_INET6 ? ntohs(reinterpret_cast<sockaddr_in6*>(&address)->sin6_port)
                                            : ntohs(reinterpret_cast<sockaddr_in*>(&address)->sin_port);
        worker = std::thread([this] { Serve(); });
    }

    ~Server() {
        Release();
        if (worker.joinable()) worker.join();
        mbedtls_ssl_config_free(&config);
        mbedtls_pk_free(&key);
        mbedtls_x509_crt_free(&certificate);
        mbedtls_ctr_drbg_free(&random);
        mbedtls_entropy_free(&entropy);
        mbedtls_net_free(&listener);
    }

    std::string Url(const char* host = "127.0.0.1") const {
        return std::string(mode == Mode::Http1Plain ? "http://" : "https://") + host + ":" + std::to_string(port) + "/path?query=yes#fragment";
    }

    void WaitStage(int desired) {
        std::unique_lock lock(mutex);
        Require(changed.wait_for(lock, std::chrono::seconds(2), [&] { return stage >= desired; }), "server stage deadline");
    }

    void Release() {
        { std::lock_guard lock(mutex); released = true; }
        changed.notify_all();
    }

    void SetResponse(ResponseSpec response) {
        std::lock_guard lock(responseMutex);
        responseSpec = std::move(response);
    }
    void Join() { Release(); if (worker.joinable()) worker.join(); }
    const std::multimap<std::string, std::string>& Headers() const { return headers; }
    const std::string& Upload() const { return upload; }
    explicit Server(Mode mode, std::string location, int redirectStatus, std::vector<std::string> responseCookies)
        : Server(mode, std::move(location), redirectStatus, "127.0.0.1", std::move(responseCookies)) {}
    bool Negotiated() const { return negotiated; }
    bool IsEarly() const { return mode >= Mode::EarlyFinal; }
    bool IsHttp1() const { return mode >= Mode::Http10 && mode <= Mode::Http1Plain; }
private:
    const Mode mode;
    const std::string location;
    const int redirectStatus;
    const std::vector<std::string> responseCookies;
    const bool clientMayClose;
    std::mutex responseMutex;
    ResponseSpec responseSpec;
    unsigned accepted = 0;
    mbedtls_net_context listener;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context random;
    mbedtls_x509_crt certificate;
    mbedtls_pk_context key;
    mbedtls_ssl_config config;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable changed;
    bool released = false;
    int stage = 0;
    std::uint16_t port;
    bool negotiated = false;
    std::multimap<std::string, std::string> headers;
    std::string upload;
    std::string body;
    std::size_t offset = 0;
    bool responded = false;
    bool ended = false;
    bool splitSent = false;
    bool splitResumed = false;

    void Stage(int value) {
        { std::lock_guard lock(mutex); stage = value; }
        changed.notify_all();
    }
    void Stall() {
        std::unique_lock lock(mutex);
        Require(changed.wait_for(lock, std::chrono::seconds(2), [&] { return released; }), "server release deadline");
    }

    static int Header(nghttp2_session*, const nghttp2_frame*, const std::uint8_t* name, std::size_t nameSize,
        const std::uint8_t* value, std::size_t valueSize, std::uint8_t, void* data) {
        auto& server = *static_cast<Server*>(data);
        server.headers.emplace(std::string(reinterpret_cast<const char*>(name), nameSize), std::string(reinterpret_cast<const char*>(value), valueSize));
        return 0;
    }
    static int Data(nghttp2_session*, std::uint8_t, std::int32_t, const std::uint8_t* bytes, std::size_t size, void* data) {
        static_cast<Server*>(data)->upload.append(reinterpret_cast<const char*>(bytes), size);
        return 0;
    }
    static int Frame(nghttp2_session* session, const nghttp2_frame* frame, void* data) {
        auto& server = *static_cast<Server*>(data);
        if (server.IsEarly() && frame->hd.type == NGHTTP2_HEADERS && frame->hd.stream_id > 0) {
            server.Respond(session, frame->hd.stream_id);
            return 0;
        }
        if (server.mode == Mode::StallUpload && frame->hd.type == NGHTTP2_HEADERS) { server.Stage(2); server.Stall(); }
        if (frame->hd.stream_id > 0 && (frame->hd.type == NGHTTP2_HEADERS || frame->hd.type == NGHTTP2_DATA) &&
            (frame->hd.flags & NGHTTP2_FLAG_END_STREAM)) server.Respond(session, frame->hd.stream_id);
        return 0;
    }
    static int End(nghttp2_session*, std::int32_t, std::uint32_t, void* data) {
        static_cast<Server*>(data)->ended = true;
        return 0;
    }
    static nghttp2_ssize Body(nghttp2_session* session, std::int32_t streamId, std::uint8_t* bytes, std::size_t size,
        std::uint32_t* flags, nghttp2_data_source*, void* data) {
        auto& server = *static_cast<Server*>(data);
        if (server.mode == Mode::StallBody || server.mode == Mode::EarlyResetIncomplete) return NGHTTP2_ERR_DEFERRED;
        if (server.mode == Mode::GzipSplit && server.splitSent && !server.splitResumed) return NGHTTP2_ERR_DEFERRED;
        size = std::min(size, server.body.size() - server.offset);
        if (server.mode == Mode::GzipSplit && !server.splitSent) {
            size = std::min<std::size_t>(size, 8);
            server.splitSent = true;
        }
        if (size) std::memcpy(bytes, server.body.data() + server.offset, size);
        server.offset += size;
        if (server.offset == server.body.size()) {
            *flags |= NGHTTP2_DATA_FLAG_EOF;
            if (server.mode == Mode::Trailers) {
                *flags |= NGHTTP2_DATA_FLAG_NO_END_STREAM;
                nghttp2_nv trailers[] = {
                    {reinterpret_cast<std::uint8_t*>(const_cast<char*>("x-trailer")), reinterpret_cast<std::uint8_t*>(const_cast<char*>("yes")), 9, 3, 0},
                    {reinterpret_cast<std::uint8_t*>(const_cast<char*>("x-trailer-two")), reinterpret_cast<std::uint8_t*>(const_cast<char*>("2")), 13, 1, 0},
                };
                if (nghttp2_submit_trailer(session, streamId, trailers, 2) != 0) return NGHTTP2_ERR_CALLBACK_FAILURE;
            }
        }
        return static_cast<nghttp2_ssize>(size);
    }

    ResponseSpec GetResponse() {
        std::lock_guard lock(responseMutex);
        return responseSpec;
    }

    void Respond(nghttp2_session* session, std::int32_t stream) {
        const auto configured = GetResponse();
        Require(!responded, "one response per request");
        responded = true;
        if (mode == Mode::StallHeaders) { Stage(2); Stall(); return; }
        if (mode == Mode::StallResponse) { Stage(2); Stall(); }
        if (mode == Mode::Reset) { Require(nghttp2_submit_rst_stream(session, 0, stream, NGHTTP2_REFUSED_STREAM) == 0, "server reset"); return; }
        body = (mode == Mode::Large || mode == Mode::EarlyLarge) ? std::string(200000, 'x') : "response body";
        const bool redirect = mode == Mode::Redirect || mode == Mode::GzipRedirectEmpty || mode == Mode::GzipRedirectBody || (mode == Mode::RedirectOnce && accepted == 1);
        std::string status = IsEarly() ? "413" : mode == Mode::NotFound ? "404" : redirect ? std::to_string(redirectStatus) : mode == Mode::Auth ? "401" : "200";
        if (configured.status) status = std::to_string(configured.status);
        if (mode == Mode::EarlyEmpty) body.clear();
        auto length = std::to_string(body.size());
        const auto requestMethod = headers.find(":method");
        if (mode == Mode::Head || (requestMethod != headers.end() && requestMethod->second == "HEAD")) body.clear();
        if (mode == Mode::Truncate) body = "short";
        if (mode == Mode::GzipRedirectEmpty || mode == Mode::GzipRedirectBody || mode == Mode::Gzip || mode == Mode::GzipSplit || mode == Mode::GzipTruncated || mode == Mode::GzipChecksum || mode == Mode::GzipConcat) {
            static const unsigned char compressed[] = {
                0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x2b, 0x4a, 0x2d, 0x2e, 0xc8, 0xcf,
                0x2b, 0x4e, 0x55, 0x48, 0xca, 0x4f, 0xa9, 0x04, 0x00, 0x36, 0x76, 0x8b, 0x0c, 0x0d, 0x00, 0x00, 0x00,
            };
            body.assign(reinterpret_cast<const char*>(compressed), sizeof(compressed));
            if (mode == Mode::GzipTruncated) body.resize(body.size() - 4);
            if (mode == Mode::GzipChecksum) body[body.size() - 8] ^= 0xff;
            if (mode == Mode::GzipConcat) { const std::string member = body; body += member; }
            length = std::to_string(body.size());
        }
        if (mode == Mode::GzipRedirectEmpty) { body.clear(); length = "0"; }
        if (mode == Mode::GzipLarge) {
            body = StoredGzip(GzipPayload());
            length = std::to_string(body.size());
        }
        if (mode >= Mode::GzipEmpty && mode <= Mode::GzipTrailingJunk) {
            if (mode == Mode::GzipEmpty) body = StoredGzip("");
            if (mode == Mode::GzipEmptyMembers) body = StoredGzip("") + StoredGzip("first") + StoredGzip("") + StoredGzip("last") + StoredGzip("");
            if (mode == Mode::GzipLargeMembers) body = StoredGzip(GzipPayload()) + StoredGzip("tail");
            if (mode == Mode::GzipHeaders) body = HeaderGzip("first") + HeaderGzip("last");
            if (mode == Mode::GzipHeaderChecksum) body = HeaderGzip("first", true);
            if (mode == Mode::GzipSecondHeaderChecksum) body = HeaderGzip("first") + HeaderGzip("last", true);
            if (mode == Mode::GzipSecondChecksum) {
                body = StoredGzip("first") + StoredGzip("last");
                body[body.size() - 8] ^= 1;
            }
            if (mode == Mode::GzipSecondSize) {
                body = StoredGzip("first") + StoredGzip("last");
                body[body.size() - 4] ^= 1;
            }
            if (mode == Mode::GzipTrailingJunk) body = StoredGzip("first") + "trailing junk";
            length = std::to_string(body.size());
        }
        if (mode == Mode::Informational || !configured.informationalHeaders.empty()) {
            std::vector<std::pair<std::string, std::string>> earlyFields{{":status", "103"}};
            earlyFields.insert(earlyFields.end(), configured.informationalHeaders.begin(), configured.informationalHeaders.end());
            std::vector<nghttp2_nv> early;
            for (auto& [name, value] : earlyFields) early.push_back({reinterpret_cast<std::uint8_t*>(name.data()), reinterpret_cast<std::uint8_t*>(value.data()), name.size(), value.size(), 0});
            Require(nghttp2_submit_headers(session, 0, stream, nullptr, early.data(), early.size(), nullptr) == 0, "server informational response");
        }
        std::vector<std::pair<std::string, std::string>> fields{{":status", status}, {"x-loopback", "yes"}};
        if (mode != Mode::NoLength) fields.emplace_back("content-length", length);
        if (redirect) fields.emplace_back("location", location);
        if (mode == Mode::GzipRedirectEmpty || mode == Mode::GzipRedirectBody || mode == Mode::Gzip || mode == Mode::GzipSplit || mode == Mode::GzipTruncated || mode == Mode::GzipChecksum || mode == Mode::GzipConcat || mode == Mode::GzipLarge || (mode >= Mode::GzipEmpty && mode <= Mode::GzipTrailingJunk)) fields.emplace_back("content-encoding", "gzip");
        if (mode == Mode::Cookie) fields.emplace_back("set-cookie", "session=fresh; Path=/");
        if (mode == Mode::CookieFlood) {
            for (int i = 0; i < 1500; ++i) fields.emplace_back("set-cookie", "f" + std::to_string(i) + "=v; Path=/");
        }
        if (mode == Mode::Auth) fields.emplace_back("www-authenticate", "Basic realm=\"loopback\"");
        fields.insert(fields.end(), configured.headers.begin(), configured.headers.end());
        std::vector<nghttp2_nv> response;
        for (auto& [name, value] : fields) response.push_back({reinterpret_cast<std::uint8_t*>(name.data()), reinterpret_cast<std::uint8_t*>(value.data()), name.size(), value.size(), 0});
        nghttp2_data_provider2 provider{};
        provider.read_callback = Body;
        Require(nghttp2_submit_response2(session, stream, response.data(), response.size(), body.empty() ? nullptr : &provider) == 0, "server response");
    }

    void Serve() {
        const unsigned connections = mode == Mode::RedirectOnce ? 2 : 1;
        while (accepted < connections) {
            for (;;) {
                const int ready = mbedtls_net_poll(&listener, MBEDTLS_NET_POLL_READ, 20);
                Require(ready >= 0, "server accept poll");
                if (ready) break;
                std::lock_guard lock(mutex);
                if (released) return;
            }
            headers.clear();
            upload.clear();
            body.clear();
            offset = 0;
            responded = ended = false;
            ++accepted;
            ServeConnection();
        }
    }

    void ServeConnection() {
        mbedtls_net_context client;
        mbedtls_net_init(&client);
        mbedtls_ssl_context ssl;
        mbedtls_ssl_init(&ssl);
        Require(mbedtls_net_accept(&listener, &client, nullptr, 0, nullptr) == 0, "server accept");
        Stage(1);
        if (mode == Mode::Http1Plain) ExchangeHttp1(client, nullptr);
        else if (mode == Mode::StallHandshake) Stall();
        else if (mode == Mode::CloseHandshake || mode == Mode::AlertHandshake) RefuseHello(client);
        else {
            Require(mbedtls_ssl_setup(&ssl, &config) == 0, "server TLS setup");
            mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
            if (mbedtls_ssl_handshake(&ssl) == 0) {
                const char* selected = mbedtls_ssl_get_alpn_protocol(&ssl);
                negotiated = selected && std::string(selected) == (IsHttp1() ? "http/1.1" : "h2");
                if (IsHttp1()) {
                    ExchangeHttp1(client, &ssl);
                    mbedtls_ssl_close_notify(&ssl);
                } else Exchange(ssl);
            }
        }
        mbedtls_ssl_free(&ssl);
        mbedtls_net_free(&client);
    }

    void RefuseHello(mbedtls_net_context& client) {
        std::vector<unsigned char> hello(5);
        std::size_t received = 0;
        while (received < hello.size()) {
            const int count = mbedtls_net_recv(&client, hello.data() + received, hello.size() - received);
            if (count <= 0) return;
            received += static_cast<std::size_t>(count);
            if (received == 5) hello.resize(5 + ((static_cast<std::size_t>(hello[3]) << 8) | hello[4]));
        }
        static constexpr unsigned char Alert[] = {0x15, 0x03, 0x03, 0x00, 0x02, 0x02, 0x28};
        if (mode == Mode::AlertHandshake) Require(mbedtls_net_send(&client, Alert, sizeof(Alert)) == static_cast<int>(sizeof(Alert)), "server alert");
    }

    void ExchangeHttp1(mbedtls_net_context& client, mbedtls_ssl_context* ssl) {
        const auto configured = GetResponse();
        std::string input;
        std::array<unsigned char, 1024> buffer;
        const auto readMore = [&] {
            const int count = ssl ? mbedtls_ssl_read(ssl, buffer.data(), buffer.size()) : mbedtls_net_recv(&client, buffer.data(), buffer.size());
            Require(count > 0, "HTTP1 request read");
            input.append(reinterpret_cast<const char*>(buffer.data()), static_cast<std::size_t>(count));
        };
        const auto write = [&](const std::string& bytes) {
            for (std::size_t sent = 0; sent < bytes.size();) {
                const auto* data = reinterpret_cast<const unsigned char*>(bytes.data() + sent);
                const int count = ssl ? mbedtls_ssl_write(ssl, data, bytes.size() - sent) : mbedtls_net_send(&client, data, bytes.size() - sent);
                Require(count > 0 || clientMayClose, "HTTP1 response write");
                if (count <= 0) return;
                sent += static_cast<std::size_t>(count);
            }
        };
        std::size_t end;
        while ((end = input.find("\r\n\r\n")) == std::string::npos) {
            readMore();
            Require(input.size() < 65536, "HTTP1 request header limit");
        }
        const auto firstSpace = input.find(' '), secondSpace = input.find(' ', firstSpace + 1), lineEnd = input.find("\r\n");
        Require(firstSpace < secondSpace && secondSpace < lineEnd, "HTTP1 request line");
        headers.emplace(":method", input.substr(0, firstSpace));
        headers.emplace(":path", input.substr(firstSpace + 1, secondSpace - firstSpace - 1));
        headers.emplace(":version", input.substr(secondSpace + 1, lineEnd - secondSpace - 1));
        auto position = lineEnd + 2;
        while (position < end) {
            const auto next = input.find("\r\n", position), colon = input.find(':', position);
            Require(colon < next, "HTTP1 request header colon");
            std::string name = input.substr(position, colon - position);
            for (auto& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            auto value = colon + 1;
            while (value < next && (input[value] == ' ' || input[value] == '\t')) ++value;
            headers.emplace(std::move(name), input.substr(value, next - value));
            position = next + 2;
        }
        end += 4;
        if (mode == Mode::Http11Info || !configured.informationalHeaders.empty()) {
            std::string informational = "HTTP/1.1 100 Continue\r\n";
            for (const auto& [name, value] : configured.informationalHeaders) informational += name + ": " + value + "\r\n";
            write(informational + "\r\n");
        }
        if (mode == Mode::Http11Early) upload = input.substr(end);
        else if (headers.count("transfer-encoding")) {
            Require(headers.find("transfer-encoding")->second == "chunked", "HTTP1 request transfer coding");
            for (;;) {
                auto lineEnd = input.find("\r\n", end);
                while (lineEnd == std::string::npos) { readMore(); lineEnd = input.find("\r\n", end); }
                const auto length = std::stoull(input.substr(end, lineEnd - end), nullptr, 16);
                end = lineEnd + 2;
                while (input.size() < end + length + 2) readMore();
                Require(input.substr(end + length, 2) == "\r\n", "HTTP1 request chunk terminator");
                upload += input.substr(end, length);
                end += length + 2;
                if (!length) break;
            }
        } else {
            const auto field = headers.find("content-length");
            const std::size_t length = field == headers.end() ? 0 : std::stoull(field->second);
            while (input.size() - end < length) readMore();
            upload = input.substr(end, length);
        }
        if (mode == Mode::Http11StallHeaders) { Stage(2); Stall(); return; }
        const int code = configured.status ? configured.status : mode == Mode::Http11Early ? 413 : mode == Mode::Http11Redirect ? redirectStatus : 200;
        std::string response = mode == Mode::Http10 ? "HTTP/1.0 " : "HTTP/1.1 ";
        response += std::to_string(code) + " Fixture\r\n";
        if (mode == Mode::Http11Chunked || mode == Mode::Http11BadChunk) response += "Transfer-Encoding: chunked\r\n";
        else if (mode != Mode::Http11Close) response += "Content-Length: " + std::to_string(mode == Mode::Http11Gzip ? StoredGzip("response body").size() : 13) + "\r\n";
        if (mode == Mode::Http11Gzip) response += "Content-Encoding: gzip\r\n";
        if (mode == Mode::Http11BadLength) response += "Content-Length: 14\r\n";
        if (mode == Mode::Http11Ambiguous) response += "Transfer-Encoding: chunked\r\n";
        if (mode == Mode::Http11Redirect) response += "Location: " + location + "\r\n";
        for (const auto& cookie : responseCookies) response += "Set-Cookie: " + cookie + "\r\n";
        for (const auto& [name, value] : configured.headers) response += name + ": " + value + "\r\n";
        response += "X-Loopback: yes\r\nConnection: close\r\n\r\n";
        if (mode == Mode::Http11StallBody) { write(response); Stage(2); Stall(); return; }
        if (headers.find(":method")->second != "HEAD") {
            if (mode == Mode::Http11Chunked) response += "8;test=yes\r\nresponse\r\n5\r\n body\r\n0\r\nX-Trailer: done\r\n\r\n";
            else if (mode == Mode::Http11BadChunk) response += "z\r\n";
            else if (mode == Mode::Http11Gzip) response += StoredGzip("response body");
            else response += mode == Mode::Http11Truncated ? "short" : "response body";
        }
        write(response);
        if (mode == Mode::Http11Early) { Stage(2); Stall(); }
    }

    void Exchange(mbedtls_ssl_context& ssl) {
        nghttp2_session_callbacks* callbacks = nullptr;
        Require(nghttp2_session_callbacks_new(&callbacks) == 0, "server callbacks");
        nghttp2_session_callbacks_set_on_header_callback(callbacks, Header);
        nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, Frame);
        nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks, Data);
        nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, End);
        nghttp2_session* session = nullptr;
        nghttp2_option* options = nullptr;
        Require(nghttp2_option_new(&options) == 0, "server options");
        if (IsEarly()) nghttp2_option_set_no_auto_window_update(options, 1);
        Require(nghttp2_session_server_new2(&session, callbacks, this, options) == 0, "server session");
        nghttp2_option_del(options);
        nghttp2_session_callbacks_del(callbacks);
        Require(nghttp2_submit_settings(session, 0, nullptr, 0) == 0, "server settings");
        bool resetSent = false;
        for (;;) {
            const std::uint8_t* output = nullptr;
            nghttp2_ssize size = 0;
            bool sent = true;
            while ((size = nghttp2_session_mem_send2(session, &output)) > 0) {
                std::size_t position = 0;
                while (position < static_cast<std::size_t>(size)) {
                    const auto chunk = std::min<std::size_t>(1024, static_cast<std::size_t>(size) - position);
                    const int count = mbedtls_ssl_write(&ssl, output + position, chunk);
                    if (count <= 0) { sent = false; break; }
                    position += static_cast<std::size_t>(count);
                }
                if (!sent) break;
            }
            if (mode == Mode::GzipSplit && splitSent && !splitResumed) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                Require(nghttp2_session_resume_data(session, 1) == 0, "resume split gzip body");
                splitResumed = true;
                continue;
            }
            if (IsEarly() && responded && sent) {
                Stage(2);
                if (!resetSent && (mode == Mode::EarlyReset || mode == Mode::EarlyResetIncomplete)) {
                    Require(nghttp2_submit_rst_stream(session, 0, 1, NGHTTP2_NO_ERROR) == 0, "early response reset");
                    resetSent = true;
                    continue;
                }
                if (mode != Mode::EarlyLarge) { Stall(); break; }
            }
            if (mode == Mode::StallBody && responded) { Stage(2); Stall(); break; }
            if (ended && sent) {
                mbedtls_ssl_close_notify(&ssl);
                std::array<std::uint8_t, 1024> drain;
                while (mbedtls_ssl_read(&ssl, drain.data(), drain.size()) > 0) {}
                break;
            }
            if (!sent || (mode == Mode::StallHeaders && responded)) break;
            std::array<std::uint8_t, 8192> input;
            const int count = mbedtls_ssl_read(&ssl, input.data(), input.size());
            if (count <= 0 || nghttp2_session_mem_recv2(session, input.data(), static_cast<std::size_t>(count)) < 0) {
                std::fprintf(stderr, "server stopped count %d upload %zu response %zu/%zu ended %d\n", count, upload.size(), offset, body.size(), ended);
                break;
            }
        }
        nghttp2_session_del(session);
    }
};

struct Client {
    int context, tmpl, request = 0;
    explicit Client(int ssl, int maximum = 4, int autoProxy = 0, int version = 3) {
        context = sceHttp2Init(1, ssl, 1024 * 1024, maximum);
        tmpl = sceHttp2CreateTemplate(context, "loopback-agent", version, autoProxy);
        Require(context > 0 && tmpl > 0, "client handles");
        Require(sceHttp2SslDisableOption(tmpl, 0x4) == 0, "numeric loopback name policy");
    }
    void Create(const std::string& url, const char* method = "GET", std::size_t size = 0) {
        request = sceHttp2CreateRequestWithURL(tmpl, method, url.c_str(), size);
        Require(request > 0, "request handle");
    }
    ~Client() { if (context > 0) Require(sceHttp2Term(context) == 0, "client term"); }
};

void CookieBox(int ssl) {
    std::fprintf(stderr, "HTTP2 cookie box scenario\n");
    Server server(Mode::Cookie);
    Client client(ssl);
    const std::string url = server.Url("127.0.0.1");
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0 && sceHttp2AddCookie(box, url.c_str(), "stored=old", 10) == 0, "stored cookie");
    Require(sceHttp2SetCookieBox(client.tmpl, box) == 0, "bind cookie box");
    client.Create(url);
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "send with a cookie box");
    std::array<char, 64> buffer;
    while (sceHttp2ReadData(client.request, buffer.data(), buffer.size()) > 0) {}
    server.Join();
    const auto& headers = server.Headers();
    Require(headers.count("cookie") == 1 && headers.find("cookie")->second == "stored=old", "box cookie sent");
    std::array<char, 64> cookie{};
    std::size_t required = 0;
    Require(sceHttp2GetCookie(box, url.c_str(), cookie.data(), &required, cookie.size()) == 0, "response cookie lookup");
    Require(std::string_view(cookie.data()) == "session=fresh; stored=old", "response cookie stored in the box");
    Require(sceHttp2DeleteCookieBox(box) == 0, "delete cookie box");
}

void Http1CookieBox(int ssl, Mode mode) {
    std::fprintf(stderr, "HTTP1 cookie box scenario %d\n", static_cast<int>(mode));
    Server server(mode, "/another", 302, {"first=one; Path=/", "second=two; Path=/"});
    Client client(ssl, 1, 0, mode == Mode::Http10 ? 1 : 2);
    const auto url = server.Url();
    const auto secureUrl = mode == Mode::Http1Plain ? "https" + url.substr(4) : url;
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0, "HTTP1 cookie box");
    for (const std::string cookie : {"stored=old; Path=/", "secure=secret; Path=/; Secure", "hidden=path; Path=/other"})
        Require(sceHttp2AddCookie(box, secureUrl.c_str(), cookie.c_str(), cookie.size()) == 0, "HTTP1 seed cookie");
    Require(sceHttp2SetCookieBox(client.tmpl, box) == 0, "HTTP1 bind template cookie box");
    Require(sceHttp2AddRequestHeader(client.tmpl, "Cookie", "replaced=template", 0) == 0, "HTTP1 template cookie");
    client.Create(url);
    Require(sceHttp2AddRequestHeader(client.request, "Cookie", "manual=one", 0) == 0, "HTTP1 replace manual cookie");
    Require(sceHttp2AddRequestHeader(client.request, "Cookie", "extra=two", 1) == 0, "HTTP1 append manual cookie");
    Require(sceHttp2AddRequestHeader(client.request, "Host", "other.example", 0) == 0, "HTTP1 override host without changing cookie origin");
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "HTTP1 send with cookie box");
    std::array<char, 256> buffer{};
    while (sceHttp2ReadData(client.request, buffer.data(), buffer.size()) > 0) {}
    server.Join();
    const auto& headers = server.Headers();
    Require(headers.count("cookie") == 1, "HTTP1 exactly one cookie header");
    const auto& sent = headers.find("cookie")->second;
    Require(sent.starts_with("manual=one; extra=two; ") && sent.find("stored=old") != std::string::npos,
        "HTTP1 manual and matching box cookies sent together");
    Require((sent.find("secure=secret") != std::string::npos) == (mode != Mode::Http1Plain), "HTTP1 secure cookie follows URL scheme");
    Require(sent.find("hidden=") == std::string::npos && sent.find("replaced=") == std::string::npos,
        "HTTP1 path selection and manual replacement");
    std::size_t required = 0;
    Require(sceHttp2GetCookie(box, url.c_str(), buffer.data(), &required, buffer.size()) == 0, "HTTP1 response cookie lookup");
    const std::string_view stored(buffer.data());
    Require(stored.find("first=one") != std::string_view::npos && stored.find("second=two") != std::string_view::npos,
        "HTTP1 each response cookie stored");
    Require(sceHttp2DeleteCookieBox(box) == 0, "HTTP1 delete cookie box");
}

void Http1CookieRedirect(int ssl) {
    Server target(Mode::Http1Plain);
    Server redirect(Mode::Http11Redirect, target.Url(), 302, {"ordinary=next; Path=/", "secure=secret; Path=/; Secure"});
    Client client(ssl, 1, 0, 2);
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0 && sceHttp2SetCookieBox(client.tmpl, box) == 0, "HTTP1 redirect bind cookie box");
    client.Create(redirect.Url());
    Require(sceHttp2AddRequestHeader(client.request, "Cookie", "manual=origin", 0) == 0, "HTTP1 origin manual cookie");
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "HTTP1 cookie redirect");
    std::array<char, 64> buffer;
    while (sceHttp2ReadData(client.request, buffer.data(), buffer.size()) > 0) {}
    redirect.Join();
    target.Join();
    const auto& headers = target.Headers();
    Require(headers.count("cookie") == 1 && headers.find("cookie")->second == "ordinary=next",
        "HTTP1 redirect stores cookies and excludes secure and origin-only manual cookies on plain target");
    Require(sceHttp2DeleteCookieBox(box) == 0, "HTTP1 redirect delete cookie box");
}

void Http1EmptyCookie(int ssl, bool explicitCookie) {
    Server server(Mode::Http1Plain);
    Client client(ssl, 1, 0, 2);
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0 && sceHttp2SetCookieBox(client.tmpl, box) == 0, "HTTP1 empty bind cookie box");
    client.Create(server.Url());
    if (explicitCookie) Require(sceHttp2AddRequestHeader(client.request, "Cookie", "", 0) == 0, "HTTP1 explicit empty cookie");
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "HTTP1 empty box send");
    std::array<char, 64> buffer;
    while (sceHttp2ReadData(client.request, buffer.data(), buffer.size()) > 0) {}
    server.Join();
    const auto& headers = server.Headers();
    Require(headers.count("cookie") == (explicitCookie ? 1u : 0u), "HTTP1 empty box does not invent a cookie field");
    if (explicitCookie) Require(headers.find("cookie")->second.empty(), "HTTP1 explicit empty cookie preserved");
    Require(sceHttp2DeleteCookieBox(box) == 0, "HTTP1 empty delete cookie box");
}

void CookieBoxDelete(int ssl, Mode mode = Mode::CookieFlood) {
    std::fprintf(stderr, "cookie box delete scenario %d\n", static_cast<int>(mode));
    std::vector<std::string> responseCookies;
    if (mode != Mode::CookieFlood)
        for (int index = 0; index < 1500; ++index) responseCookies.push_back("item" + std::to_string(index) + "=value; Path=/");
    Server server(mode, "/another", 302, "127.0.0.1", std::move(responseCookies), true);
    Client client(ssl, 4, 0, mode == Mode::CookieFlood ? 3 : 2);
    const std::string url = server.Url("127.0.0.1");
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0 && sceHttp2SetCookieBox(client.tmpl, box) == 0, "bind cookie box");
    Require(sceHttp2SetCookieMaxNumPerDomain(box, 2000) == 0, "room for every response cookie");
    client.Create(url);
    auto pending = std::async(std::launch::async, [&] {
        try { static_cast<void>(sceHttp2SendRequest(client.request, nullptr, 0)); } catch (const std::exception&) {}
    });
    std::size_t required = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (sceHttp2GetCookie(box, url.c_str(), nullptr, &required, 0) != 0) {
        Require(std::chrono::steady_clock::now() < deadline, "first response cookie stored");
        std::this_thread::yield();
    }
    auto deleting = std::async(std::launch::async, [&] { return sceHttp2DeleteRequest(client.request); });
    Require(deleting.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "delete returns while the transport stores response cookies");
    Require(deleting.get() == 0, "delete a request whose transport stores response cookies");
    Require(pending.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "send ends after the delete");
    server.Join();
    Require(sceHttp2DeleteCookieBox(box) == 0, "delete cookie box");
}

struct RedirectObservation {
    int calls = 0;
    int request = 0;
    int status = 0;
    int result = 0;
    std::string method, location;
    const char* replacement = nullptr;
    bool nullMethod = false;
    bool throwException = false;
};

int APS5_VABI RedirectCallback(int request, int status, const char** method, const char* location, void* argument) {
    auto& observed = *static_cast<RedirectObservation*>(argument);
    ++observed.calls;
    observed.request = request;
    observed.status = status;
    observed.method = *method;
    observed.location = location;
    if (observed.replacement || observed.nullMethod) *method = observed.replacement;
    if (observed.throwException) throw std::runtime_error("redirect callback failure");
    return observed.result;
}

void Response(Client& client, int expectedStatus) {
    int status = 0;
    Require(sceHttp2GetStatusCode(client.request, &status) == 0 && status == expectedStatus, "redirect response status");
    std::string received;
    std::array<char, 64> buffer;
    for (;;) {
        const int count = sceHttp2ReadData(client.request, buffer.data(), buffer.size());
        Require(count >= 0, "redirect response read");
        if (!count) break;
        received.append(buffer.data(), static_cast<std::size_t>(count));
    }
    Require(received == "response body", "redirect preserves final response body");
}

void RedirectPolicies(int ssl) {
    for (int policy = 0; policy < 6; ++policy) {
        Server server(Mode::Redirect, policy == 4 ? "" : "/another");
        Client client(ssl);
        RedirectObservation original, changed;
        original.result = policy == 5 ? 7 : -1;
        if (policy == 0) Require(sceHttp2SetRedirectCallback(client.tmpl, RedirectCallback, &original) == 0, "template redirect callback");
        client.Create(server.Url());
        if (policy == 0) Require(sceHttp2SetRedirectCallback(client.tmpl, RedirectCallback, &changed) == 0, "template callback mutation");
        else Require(sceHttp2SetRedirectCallback(client.request, RedirectCallback, &original) == 0, "request redirect callback");
        if (policy == 2) Require(sceHttp2SetAutoRedirect(client.request, 0) == 0, "manual redirects omit callback");
        if (policy == 3) {
            Require(sceHttp2SetRedirectCallback(client.request, nullptr, &changed) == 0, "clear callback with argument");
            Require(sceHttp2SetAutoRedirect(client.request, 0) == 0, "manual cleared callback");
        }
        Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "redirect refusal response");
        Response(client, 302);
        server.Join();
        const int expectedCalls = policy < 2 || policy == 5 ? 1 : 0;
        Require(original.calls == expectedCalls && changed.calls == 0, "callback inheritance and policy");
        if (expectedCalls) Require(original.request == client.request && original.status == 302 && original.method == "GET" && original.location == "/another", "redirect callback arguments");
    }
}

void RedirectFollow(int ssl, int status, const char* replacement = nullptr, bool clearCallback = false, bool schemeRelative = false) {
    const bool head = replacement && std::string_view(replacement) == "HEAD";
    Server target(head ? Mode::Head : Mode::Normal);
    std::string destination = target.Url();
    if (schemeRelative) destination.erase(0, 6);
    Server redirect(Mode::Redirect, destination, status);
    Client client(ssl, 1);
    RedirectObservation observed;
    observed.replacement = replacement;
    Require(sceHttp2SetRedirectCallback(client.tmpl, RedirectCallback, &observed) == 0, "inherited follow callback");
    for (const auto* name : {"authorization", "proxy-authorization", "cookie", "origin", "referer"})
        Require(sceHttp2AddRequestHeader(client.tmpl, name, "origin-only", 0) == 0, "origin header");
    Require(sceHttp2AddRequestHeader(client.tmpl, "content-type", "application/octet-stream", 0) == 0, "body metadata");
    Require(sceHttp2AddRequestHeader(client.tmpl, "x-retained", "yes", 0) == 0, "retained header");
    const std::string body = "upload bytes";
    client.Create(redirect.Url(), "POST", body.size());
    if (clearCallback) Require(sceHttp2SetRedirectCallback(client.request, nullptr, &observed) == 0, "clear inherited callback");
    Require(sceHttp2SendRequest(client.request, body.data(), body.size()) == 0, "follow redirect");
    if (head) {
        int result = 0;
        Require(sceHttp2GetStatusCode(client.request, &result) == 0 && result == 200, "redirect HEAD status");
        char byte;
        Require(sceHttp2ReadData(client.request, &byte, 1) == 0, "redirect HEAD has no response body");
    } else Response(client, 200);
    redirect.Join();
    target.Join();
    const bool retrieve = status == 301 || status == 302 || status == 303;
    const std::string method = replacement ? replacement : retrieve ? "GET" : "POST";
    Require(target.Headers().find(":method")->second == method, "redirect request method");
    Require(target.Headers().find(":path")->second == "/path?query=yes", "redirect fragment removed");
    Require(target.Upload() == (method == "GET" || method == "HEAD" ? "" : body), "redirect upload policy");
    Require(target.Headers().count("content-type") == (method == "GET" || method == "HEAD" ? 0u : 1u), "redirect body headers");
    for (const auto* name : {"authorization", "proxy-authorization", "cookie", "origin", "referer"})
        Require(target.Headers().count(name) == 0, "origin headers excluded at a new origin");
    Require(target.Headers().count("x-retained") == 1, "ordinary header retained");
    Require(target.Headers().find("content-length")->second == std::to_string(target.Upload().size()), "redirect wire content length");
    Require(observed.calls == (clearCallback ? 0 : 1), "follow callback count");
    if (!clearCallback) Require(observed.request == client.request && observed.status == status && observed.method == (retrieve ? "GET" : "POST") && observed.location == destination, "follow callback arguments");
}

void RelativeRedirects(int ssl) {
    for (const auto& [reference, expected] : std::vector<std::pair<const char*, const char*>>{
        {"another", "/another"}, {"../another", "/another"}, {"/a/b/../c/.", "/a/c/"},
        {"?next=1", "/path?next=1"}, {"../a//b?x=/../z", "/a//b?x=/../z"},
        {"../../../../c", "/c"}, {".", "/"}, {"..", "/"}, {"./a/./b/../../c", "/c"},
        {"/a/%2e%2e/b", "/a/%2e%2e/b"}, {"g?y/./x", "/g?y/./x"}, {"#tag", "/path?query=yes"}}) {
        Server server(Mode::RedirectOnce, reference, 307);
        Client client(ssl, 1);
        Require(sceHttp2AddRequestHeader(client.tmpl, "authorization", "same-origin", 0) == 0, "same origin authorization");
        client.Create(server.Url());
        Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "relative redirect");
        Response(client, 200);
        server.Join();
        Require(server.Headers().find(":path")->second == expected, "resolved relative reference");
        Require(server.Headers().count("authorization") == 1, "same origin retains authorization");
    }
}

void RedirectRequestMethods(int ssl) {
    for (const auto& [method, status] : std::vector<std::pair<const char*, int>>{{"PUT", 301}, {"PUT", 303}, {"GET", 307}, {"HEAD", 303}}) {
        const bool head = std::string_view(method) == "HEAD";
        Server target(head ? Mode::Head : Mode::Normal);
        Server redirect(Mode::Redirect, target.Url(), status);
        Client client(ssl, 1);
        const std::string body = head ? "" : "retained data";
        client.Create(redirect.Url(), method, body.size());
        Require(sceHttp2SendRequest(client.request, body.data(), body.size()) == 0, "redirect method semantics");
        if (head) {
            int result = 0;
            char byte;
            Require(sceHttp2GetStatusCode(client.request, &result) == 0 && result == 200, "HEAD redirected status");
            Require(sceHttp2ReadData(client.request, &byte, 1) == 0, "HEAD redirected body");
        } else Response(client, 200);
        redirect.Join();
        target.Join();
        Require(target.Headers().find(":method")->second == (status == 303 && !head ? "GET" : method), "redirect preserves or rewrites method");
        Require(target.Upload() == (status == 303 && !head ? "" : body), "redirect preserves or drops data");
    }
}

struct SslObservation {
    int context = 0;
    int request = 0;
    int calls = 0;
    int decision = 0;
    unsigned int errors = 0;
    bool abort = false;
    bool remove = false;
    bool fail = false;
};

int APS5_VABI SslCallback(int context, unsigned int errors, void* const* certificates, int count, void* argument) {
    auto& observed = *static_cast<SslObservation*>(argument);
    Require(context == observed.context, "SSL callback receives SSL context ID");
    Require(certificates && count > 0 && certificates[0], "SSL callback receives real certificate handles");
    std::size_t size = 0;
    Require(sceSslGetSerialNumber(context, certificates[0], nullptr, &size) == 0 && size > 0 && size < 1024, "inspect peer certificate in callback");
    std::vector<unsigned char> serial(size);
    Require(sceSslGetSerialNumber(context, certificates[0], serial.data(), &size) == 0 && size == serial.size(), "read peer serial in callback");
    ++observed.calls;
    observed.errors |= errors;
    if (observed.abort) Require(sceHttp2AbortRequest(observed.request) == 0, "abort from SSL callback");
    if (observed.remove) Require(sceHttp2DeleteRequest(observed.request) == 0, "delete from SSL callback");
    if (observed.fail) throw std::runtime_error("SSL callback exception");
    return observed.decision;
}

int nullSslCalls = 0;
int APS5_VABI NullSslArgument(int context, unsigned int, void* const* certificates, int count, void* argument) {
    Require(context > 0 && certificates && count > 0 && argument == nullptr, "null SSL callback argument");
    ++nullSslCalls;
    return 0;
}

void SslCallbacks(int trusted) {
    const int untrusted = sceSslInit_nid_postfix(8 * 1024 * 1024);
    Require(untrusted > 0, "SSL untrusted test context");
    for (int mode = 0; mode < 12; ++mode) {
        const bool noTrust = mode == 1 || mode == 5 || mode == 10;
        const int context = noTrust ? untrusted : trusted;
        Server server(Mode::Normal);
        Client client(context);
        SslObservation observed, changed;
        observed.context = changed.context = context;
        observed.decision = mode == 2 ? -1 : mode == 3 ? 7 : 0;
        observed.abort = mode == 6;
        observed.remove = mode == 7;
        observed.fail = mode == 8;
        if (mode != 11) Require(sceHttp2SetSslCallback(client.tmpl, SslCallback, &observed) == 0, "template SSL callback");
        if (mode == 4 || mode == 5) Require(sceHttp2SetSslCallback(client.tmpl, nullptr, &observed) == 0, "clear SSL callback with argument");
        if (mode == 9) Require(sceHttp2SslEnableOption(client.tmpl, 0x4) == 0, "enable numeric-host verification");
        if (mode == 10) Require(sceHttp2SslDisableOption(client.tmpl, 0x3d) == 0, "disable named verification checks");
        client.Create(server.Url());
        observed.request = changed.request = client.request;
        if (mode == 11) Require(sceHttp2SetSslCallback(client.request, SslCallback, &observed) == 0, "request SSL callback override");
        else Require(sceHttp2SetSslCallback(client.tmpl, SslCallback, &changed) == 0, "template mutation after request creation");
        const bool failure = mode == 2 || mode == 3 || mode == 6 || mode == 7 || mode == 8;
        if (mode == 5) Require(sceHttp2SendRequest(client.request, nullptr, 0) == static_cast<int>(0x8095f00c), "unverified chain without a callback returns the handshake code");
        else if (failure) Require(Throws([&] { sceHttp2SendRequest(client.request, nullptr, 0); }), "SSL callback refusal or cancellation fails request");
        else {
            Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "SSL callback accepted real handshake");
            Response(client, 200);
        }
        server.Join();
        Require(changed.calls == 0 && observed.calls == (mode == 4 || mode == 5 ? 0 : 1), "SSL callback snapshot and invocation count");
        if (mode == 1) Require((observed.errors & 0x20) != 0, "untrusted chain flags reported");
        else if (mode == 9) Require((observed.errors & 0x4) != 0, "host-name error reported");
        else Require(observed.errors == 0, "successful or disabled checks report no error");
        if (observed.remove) Require(sceHttp2DeleteRequest(client.request) == static_cast<int>(0x817b1100), "SSL callback deletion persists");
    }
    {
        Server server(Mode::Normal);
        Client client(trusted);
        Require(sceHttp2SetSslCallback(client.tmpl, NullSslArgument, nullptr) == 0, "SSL null user argument registration");
        client.Create(server.Url());
        Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "SSL null user argument send");
        Response(client, 200);
        server.Join();
        Require(nullSslCalls == 1, "SSL null user argument delivery");
    }
    {
        Server target(Mode::Normal);
        Server redirect(Mode::Redirect, target.Url(), 307);
        Client client(trusted, 1);
        SslObservation observed;
        observed.context = trusted;
        Require(sceHttp2SetSslCallback(client.tmpl, SslCallback, &observed) == 0, "SSL callback for redirected requests");
        client.Create(redirect.Url());
        Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "redirect with SSL verification callback");
        Response(client, 200);
        redirect.Join();
        target.Join();
        Require(observed.calls == 2 && observed.errors == 0, "each TLS hop reports its chain");
    }
    Require(sceSslTerm_nid_postfix(untrusted) == 0, "untrusted SSL context cleanup");
}

void Http1Transport(int ssl) {
    const std::pair<Mode, int> cases[] = {{Mode::Http10, 1}, {Mode::Http11, 2}, {Mode::Http11Chunked, 2}, {Mode::Http11Close, 2}, {Mode::Http1Plain, 2}, {Mode::Http1Plain, 3}};
    for (const auto& [mode, version] : cases) {
        Server server(mode);
        Client client(ssl, 1, 0, version);
        const std::string upload = "upload bytes";
        Require(sceHttp2AddRequestHeader(client.tmpl, "X-Inherited", "retained", 0) == 0, "HTTP1 inherited header");
        Require(sceHttp2AddRequestHeader(client.tmpl, "Host", "template.example", 0) == 0, "HTTP1 inherited host");
        client.Create(server.Url(), "POST", upload.size());
        Require(sceHttp2AddRequestHeader(client.request, "host", "request.example", 0) == 0, "HTTP1 request host override");
        SslObservation observed;
        observed.context = ssl;
        observed.request = client.request;
        Require(sceHttp2SetSslCallback(client.request, SslCallback, &observed) == 0, "HTTP1 SSL callback");
        Require(sceHttp2SendRequest(client.request, upload.data(), upload.size()) == 0, "HTTP1 send over actual connection");
        int kind = -1;
        std::uint64_t length = 123;
        Require(sceHttp2GetResponseContentLength(client.request, &kind, &length) == 0, "HTTP1 content length getter");
        const int expectedKind = mode == Mode::Http11Chunked ? 2 : mode == Mode::Http11Close ? 1 : 0;
        Require(kind == expectedKind && length == (expectedKind == 0 ? 13u : 0u), "HTTP1 body framing reported");
        char* raw = nullptr;
        std::size_t size = 0;
        Require(sceHttp2GetAllResponseHeaders(client.request, &raw, &size) == 0 && raw && size > 0, "HTTP1 received headers");
        Require(std::string_view(raw, size).starts_with(version == 1 ? "HTTP/1.0 200" : "HTTP/1.1 200"), "HTTP1 status line retained");
        Response(client, 200);
        server.Join();
        Require(server.Upload() == upload, "HTTP1 complete upload");
        Require(server.Headers().find(":method")->second == "POST" && server.Headers().find(":path")->second == "/path?query=yes", "HTTP1 method and target");
        Require(server.Headers().find(":version")->second == (version == 1 ? "HTTP/1.0" : "HTTP/1.1"), "HTTP1 requested version");
        Require(server.Headers().find("x-inherited")->second == "retained", "HTTP1 template header inherited");
        Require(server.Headers().count("host") == 1 && server.Headers().find("host")->second == "request.example", "HTTP1 exactly one overridden host header");
        Require(observed.calls == (mode == Mode::Http1Plain ? 0 : 1), "HTTP1 TLS callback only for secure connections");
    }
}

void Http1Policies(int ssl) {
    for (const auto mode : {Mode::Http11, Mode::Http1Plain, Mode::Http10}) {
        Server server(mode);
        Client client(ssl, 1, 0, mode == Mode::Http10 ? 1 : 2);
        const std::string body = mode == Mode::Http10 ? "" : "chunked upload";
        client.Create(server.Url(), "POST", body.size());
        Require(sceHttp2SetRequestNoContentLength(client.request) == 0, "HTTP1 omit request content length");
        Require(sceHttp2SendRequest(client.request, body.data(), body.size()) == 0, "HTTP1 chunked or empty upload");
        Response(client, 200);
        server.Join();
        Require(server.Upload() == body && server.Headers().count("content-length") == 0, "HTTP1 omitted length on wire");
        Require(server.Headers().count("transfer-encoding") == (body.empty() ? 0u : 1u), "HTTP1 chunked upload framing");
    }
    {
        Server server(Mode::Http11);
        Client client(ssl, 1, 0, 2);
        client.Create(server.Url(), "HEAD");
        Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "HTTP1 HEAD send");
        char byte;
        Require(sceHttp2ReadData(client.request, &byte, 1) == 0, "HTTP1 HEAD empty body");
        server.Join();
    }
    for (const auto mode : {Mode::Http11Early, Mode::Http11Info}) {
        Server server(mode);
        Client client(ssl, 1, 0, 2);
        const std::string body(1024 * 1024, 'u');
        client.Create(server.Url(), "POST", body.size());
        Require(sceHttp2SendRequest(client.request, body.data(), body.size()) == 0, "HTTP1 early reply or continue");
        Response(client, mode == Mode::Http11Early ? 413 : 200);
        server.Join();
        Require(mode == Mode::Http11Early ? server.Upload().size() < body.size() : server.Upload() == body, "HTTP1 upload interleaving");
    }
    for (const auto targetMode : {Mode::Http11, Mode::Http1Plain}) {
        Server target(targetMode);
        Server redirect(Mode::Http11Redirect, target.Url());
        Client client(ssl, 1, 0, 2);
        SslObservation observed;
        observed.context = ssl;
        Require(sceHttp2SetSslCallback(client.tmpl, SslCallback, &observed) == 0, "HTTP1 redirected SSL callback");
        Require(sceHttp2AddRequestHeader(client.tmpl, "authorization", "origin-only", 0) == 0, "HTTP1 origin header");
        client.Create(redirect.Url(), "POST", 4);
        Require(sceHttp2SendRequest(client.request, "data", 4) == 0, "HTTP1 redirect send");
        Response(client, 200);
        redirect.Join();
        target.Join();
        Require(target.Headers().find(":method")->second == "GET" && target.Upload().empty(), "HTTP1 redirect rewrite");
        Require(target.Headers().count("authorization") == 0, "HTTP1 cross-origin header removed");
        Require(observed.calls == (targetMode == Mode::Http1Plain ? 1 : 2), "HTTP1 redirected TLS count");
    }
    for (const auto mode : {Mode::Http11BadLength, Mode::Http11Ambiguous, Mode::Http11Truncated, Mode::Http11BadChunk}) {
        Server server(mode);
        Client client(ssl, 1, 0, 2);
        client.Create(server.Url());
        Require(Throws([&] {
            sceHttp2SendRequest(client.request, nullptr, 0);
            std::array<char, 32> bytes;
            while (sceHttp2ReadData(client.request, bytes.data(), bytes.size()) > 0) {}
        }), "malformed HTTP1 response rejected");
        server.Join();
    }
}

void Http1BufferedDeadline(int ssl) {
    for (const auto mode : {Mode::Http11, Mode::Http11Chunked, Mode::Http11Close, Mode::Http1Plain}) {
        Server server(mode);
        Client client(ssl, 1, 0, 2);
        Require(sceHttp2SetTimeOut(client.tmpl, 500000) == 0, "HTTP1 overall timeout setup");
        client.Create(server.Url());
        Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "HTTP1 buffered deadline send");
        char byte = 0;
        Require(sceHttp2ReadData(client.request, &byte, 1) == 1 && byte == 'r', "HTTP1 initial body byte");
        server.Join();
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        byte = 'z';
        Require(Throws([&] { sceHttp2ReadData(client.request, &byte, 1); }) && byte == 'z', "HTTP1 buffered body respects overall deadline without changing output");
    }
}

void Http1Cancellation(int ssl) {
    for (const auto mode : {Mode::Http11StallHeaders, Mode::Http11StallBody}) {
        for (int teardown = 0; teardown < 4; ++teardown) {
            Server server(mode);
            Client client(ssl, 1, 0, 2);
            client.Create(server.Url());
            if (mode == Mode::Http11StallBody) Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "HTTP1 cancellation header send");
            auto pending = std::async(std::launch::async, [&] {
                try {
                    char byte = 'z';
                    const int result = mode == Mode::Http11StallBody ? sceHttp2ReadData(client.request, &byte, 1) : sceHttp2SendRequest(client.request, nullptr, 0);
                    if (result == static_cast<int>(0x817b1100)) {
                        Require(byte == 'z', "HTTP1 deleted-handle read leaves buffer untouched");
                        return teardown != 0;
                    }
                    return false;
                } catch (const std::exception&) { return true; }
            });
            server.WaitStage(2);
            if (teardown == 0) Require(sceHttp2AbortRequest(client.request) == 0, "HTTP1 abort");
            if (teardown == 1) Require(sceHttp2DeleteRequest(client.request) == 0, "HTTP1 delete request");
            if (teardown == 2) Require(sceHttp2DeleteTemplate(client.tmpl) == 0, "HTTP1 delete template");
            if (teardown == 3) { Require(sceHttp2Term(client.context) == 0, "HTTP1 terminate context"); client.context = 0; }
            Require(pending.wait_for(std::chrono::seconds(1)) == std::future_status::ready && pending.get(), "HTTP1 blocked IO cancels promptly");
            server.Join();
        }
        Server server(mode);
        Client client(ssl, 1, 0, 2);
        Require(sceHttp2SetRecvTimeOut(client.tmpl, 80000) == 0, "HTTP1 receive timeout");
        client.Create(server.Url());
        if (mode == Mode::Http11StallBody) Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "HTTP1 timeout header send");
        Require(Throws([&] {
            char byte;
            if (mode == Mode::Http11StallBody) sceHttp2ReadData(client.request, &byte, 1);
            else sceHttp2SendRequest(client.request, nullptr, 0);
        }), "HTTP1 blocked IO deadline");
        server.Join();
    }
}

void GzipRedirect(int ssl) {
    for (const auto mode : {Mode::Normal, Mode::Gzip}) {
        Server target(mode);
        Server redirect(mode == Mode::Normal ? Mode::GzipRedirectEmpty : Mode::GzipRedirectBody, target.Url());
        Client client(ssl, 1);
        Require(sceHttp2SetInflateGZIPEnabled(client.tmpl, 1) == 0, "gzip redirect option");
        client.Create(redirect.Url());
        Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "gzip redirect send");
        Response(client, 200);
        redirect.Join();
        target.Join();
    }
}

void Http1GzipPolicy(int ssl) {
    for (const int version : {1, 2}) {
        {
            Server server(Mode::Http11Gzip);
            Client client(ssl, 1, 0, version);
            Require(sceHttp2SetInflateGZIPEnabled(client.tmpl, 0) == 0, "HTTP1 gzip option");
            client.Create(server.Url());
            Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "HTTP1 disabled gzip send");
            const std::string expected = StoredGzip("response body");
            int kind = -1;
            std::uint64_t length = 0;
            Require(sceHttp2GetResponseContentLength(client.request, &kind, &length) == 0 && kind == 0 && length == expected.size(), "HTTP1 gzip wire length");
            std::string received;
            std::array<char, 64> buffer;
            for (;;) {
                const int count = sceHttp2ReadData(client.request, buffer.data(), buffer.size());
                Require(count >= 0, "HTTP1 gzip raw read");
                if (!count) break;
                received.append(buffer.data(), static_cast<std::size_t>(count));
            }
            Require(received == expected, "HTTP1 disabled gzip preserves encoded bytes");
            server.Join();
        }
        Server identity(Mode::Http11);
        Client plain(ssl, 1, 0, version);
        Require(sceHttp2SetInflateGZIPEnabled(plain.tmpl, 1) == 0, "HTTP1 identity option");
        plain.Create(identity.Url());
        Require(sceHttp2SendRequest(plain.request, nullptr, 0) == 0, "HTTP1 enabled identity send");
        Response(plain, 200);
        identity.Join();
        Server head(Mode::Http11Gzip);
        Client noBody(ssl, 1, 0, version);
        Require(sceHttp2SetInflateGZIPEnabled(noBody.tmpl, 1) == 0, "HTTP1 HEAD gzip option");
        noBody.Create(head.Url(), "HEAD");
        Require(sceHttp2SendRequest(noBody.request, nullptr, 0) == 0, "HTTP1 bodyless gzip metadata");
        char byte;
        Require(sceHttp2ReadData(noBody.request, &byte, 1) == 0, "HTTP1 HEAD has no encoded body");
        head.Join();
    }
}

void RedirectFailures(int ssl) {
    for (const int mode : {0, 1, 3}) {
        Server server(Mode::Redirect, "/another");
        Client client(ssl);
        RedirectObservation observed;
        observed.nullMethod = mode == 0;
        observed.replacement = mode == 1 ? "bad method" : nullptr;
        observed.throwException = mode == 3;
        client.Create(server.Url());
        Require(sceHttp2SetRedirectCallback(client.request, RedirectCallback, &observed) == 0, "failing redirect callback");
        Require(Throws([&] { sceHttp2SendRequest(client.request, nullptr, 0); }), "unsupported redirect remains explicit");
        server.Join();
        Require(observed.calls == 1, "failure callback delivered once");
    }
    std::vector<std::unique_ptr<Server>> chain;
    for (int index = 0; index < 7; ++index)
        chain.push_back(std::make_unique<Server>(Mode::Redirect, chain.empty() ? "/unused" : chain.back()->Url()));
    Client client(ssl, 1);
    client.Create(chain.back()->Url());
    Require(Throws([&] { sceHttp2SendRequest(client.request, nullptr, 0); }), "bounded redirects");
    for (auto& server : chain) server->Join();
}

void Success(int ssl, Mode mode, bool disableCa = false) {
    std::fprintf(stderr, "HTTP2 success scenario %d\n", static_cast<int>(mode));
    Server server(mode, "/another", 302, mode == Mode::Normal && !disableCa ? "localhost" : "127.0.0.1");
    Client client(ssl);
    if (disableCa) Require(sceHttp2SslDisableOption(client.tmpl, 0x20) == 0, "disable known CA check");
    if (mode == Mode::Normal && !disableCa) Require(sceHttp2SslEnableOption(client.tmpl, 0x4) == 0, "verify localhost certificate name");
    Require(sceHttp2AddRequestHeader(client.tmpl, "X-Inherited", "original", 0) == 0, "template header");
    Require(sceHttp2SetResolveRetry(client.tmpl, 2) == 0, "resolve retries");
    const std::string upload = mode == Mode::Large ? std::string(150000, 'u') : std::string{};
    client.Create(server.Url(mode == Mode::Normal && !disableCa ? "localhost" : "127.0.0.1"), mode == Mode::Head ? "HEAD" : upload.empty() ? "GET" : "POST", upload.size());
    Require(sceHttp2AddRequestHeader(client.tmpl, "X-Inherited", "changed", 0) == 0, "template mutation");
    Require(sceHttp2AddRequestHeader(client.request, "X-Duplicate", "first", 0) == 0, "overwrite header");
    Require(sceHttp2AddRequestHeader(client.request, "x-duplicate", "last", 0) == 0, "case insensitive overwrite");
    Require(sceHttp2AddRequestHeader(client.request, "x-duplicate", "appended", 1) == 0, "append header");
    Require(sceHttp2SetAutoRedirect(client.request, 0) == 0 && sceHttp2SetAuthEnabled(client.request, 0) == 0, "manual policies");
    if (mode == Mode::NoLength) Require(sceHttp2SetRequestNoContentLength(client.request) == 0, "omit request length");
    Require(sceHttp2SendRequest(client.request, upload.data(), upload.size()) == 0, "send real request");
    int status = -1;
    Require(sceHttp2GetStatusCode(client.request, &status) == 0, "status getter");
    const int expected = mode == Mode::NotFound ? 404 : mode == Mode::Redirect ? 302 : mode == Mode::Auth ? 401 : 200;
    Require(status == expected, "actual server status");
    int kind = -1;
    std::uint64_t length = 999;
    Require(sceHttp2GetResponseContentLength(client.request, &kind, &length) == 0, "length getter");
    Require(kind == (mode == Mode::NoLength ? 1 : 0), "length kind");
    if (kind == 0) Require(length == (mode == Mode::Large ? 200000 : 13), "actual content length");
    std::string body;
    std::array<char, 317> buffer;
    for (;;) {
        const int count = sceHttp2ReadData(client.request, buffer.data(), buffer.size());
        Require(count >= 0 && count <= static_cast<int>(buffer.size()), "stream read count");
        if (count == 0) break;
        body.append(buffer.data(), static_cast<std::size_t>(count));
    }
    Require(body == (mode == Mode::Head ? "" : mode == Mode::Large ? std::string(200000, 'x') : "response body"), "stream body");
    Require(sceHttp2ReadData(client.request, buffer.data(), buffer.size()) == 0, "repeat EOF");
    server.Join();
    Require(server.Negotiated(), "actual h2 ALPN");
    Require(server.Upload() == upload, "server received upload");
    const auto& headers = server.Headers();
    Require(headers.find(":path")->second == "/path?query=yes", "query retained and fragment omitted");
    Require(headers.find("x-inherited")->second == "original", "settings snapshot");
    Require(headers.find("user-agent")->second == "loopback-agent", "user agent");
    Require(headers.count("content-length") == (mode == Mode::NoLength ? 0u : 1u), "request length policy");
    Require(headers.count("x-duplicate") == 2, "duplicate count");
    Require(headers.find("x-duplicate")->second == "last", "duplicate overwrite");
    char* rawHeaders = nullptr;
    std::size_t rawSize = 0;
    Require(sceHttp2GetAllResponseHeaders(client.request, &rawHeaders, &rawSize) == 0, "response header serialization");
    const std::string blob(rawHeaders, rawSize);
    Require(blob.starts_with("HTTP/2.0 ") && blob.ends_with("\r\n\r\n") && blob.find("x-loopback: yes\r\n") != std::string::npos,
        "response header block");
    const std::size_t statusEnd = blob.find("\r\n");
    Require(statusEnd != std::string::npos, "response status line terminator");
    std::int32_t major = 0;
    std::int32_t minor = 0;
    std::int32_t code = 0;
    const char* phrase = nullptr;
    std::size_t phraseLength = 0;
    Require(sceHttpParseStatusLine(blob.data(), statusEnd + 2, &major, &minor, &code, &phrase, &phraseLength) ==
        static_cast<int>(statusEnd + 2), "response status line parses");
    Require(major == 2 && minor == 0 && code == expected, "response status line values");
    Require(phraseLength == std::strlen(" HTTP2 NO PHRASE") && std::strncmp(phrase, " HTTP2 NO PHRASE", phraseLength) == 0,
        "response reason phrase");
    const char* value = nullptr;
    std::size_t valueLength = 0;
    Require(sceHttpParseResponseHeader(blob.data(), blob.size(), "x-loopback", &value, &valueLength) > 0 &&
        std::string(value, valueLength) == "yes", "response header parses");
    if (mode == Mode::Trailers) {
        char* rawTrailers = nullptr;
        std::size_t rawTrailerSize = 0;
        Require(sceHttp2GetAllTrailingHeaders(client.request, &rawTrailers, &rawTrailerSize) == 0, "trailing header serialization");
        Require(std::string(rawTrailers, rawTrailerSize) == "x-trailer: yes\r\nx-trailer-two: 2\r\n\r\n", "trailing header block");
    }
    Require(Throws([&] { sceHttp2SendRequest(client.request, nullptr, 0); }), "unsupported reuse throws");
}

void Gzip(int ssl) {
    std::fprintf(stderr, "HTTP2 gzip scenario\n");
    for (const int enabled : {1, 0}) {
        Server server(Mode::Gzip);
        Client client(ssl);
        Require(sceHttp2SetInflateGZIPEnabled(client.tmpl, enabled) == 0, "gzip option");
        client.Create(server.Url());
        Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "gzip send");
        int kind = -1;
        std::uint64_t length = 0;
        Require(sceHttp2GetResponseContentLength(client.request, &kind, &length) == 0, "gzip length");
        std::array<char, 64> buffer{};
        const int count = sceHttp2ReadData(client.request, buffer.data(), buffer.size());
        if (enabled) {
            Require(kind == 0 && length == 13 && count == 13 && std::string(buffer.data(), 13) == "response body", "gzip inflated body");
        } else {
            Require(kind == 0 && length == 33 && count == 33 && static_cast<unsigned char>(buffer[0]) == 0x1f &&
                static_cast<unsigned char>(buffer[1]) == 0x8b, "gzip passthrough body");
        }
        server.Join();
    }
}

void GzipSplit(int ssl) {
    std::fprintf(stderr, "HTTP2 gzip split scenario\n");
    Server server(Mode::GzipSplit);
    Client client(ssl);
    Require(sceHttp2SetInflateGZIPEnabled(client.tmpl, 1) == 0, "gzip split option");
    client.Create(server.Url());
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "gzip split send");
    std::string body;
    std::array<char, 3> buffer;
    while (body.size() < 12) {
        const int count = sceHttp2ReadData(client.request, buffer.data(), buffer.size());
        Require(count > 0 && count <= static_cast<int>(buffer.size()), "gzip split read count");
        body.append(buffer.data(), static_cast<std::size_t>(count));
    }
    int kind = -1;
    std::uint64_t length = 0;
    Require(sceHttp2GetResponseContentLength(client.request, &kind, &length) == 0, "gzip split length");
    Require(kind == 0 && length == 13, "gzip split inflated length");
    for (;;) {
        const int count = sceHttp2ReadData(client.request, buffer.data(), buffer.size());
        Require(count >= 0 && count <= static_cast<int>(buffer.size()), "gzip split read count");
        if (!count) break;
        body.append(buffer.data(), static_cast<std::size_t>(count));
    }
    Require(body == "response body", "gzip split inflated body");
    server.Join();
}

void GzipLarge(int ssl) {
    std::fprintf(stderr, "HTTP2 gzip large scenario\n");
    Server server(Mode::GzipLarge);
    Client client(ssl);
    Require(sceHttp2SetInflateGZIPEnabled(client.tmpl, 1) == 0, "gzip large option");
    client.Create(server.Url());
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "gzip large send");
    int kind = -1;
    std::uint64_t length = 0;
    Require(sceHttp2GetResponseContentLength(client.request, &kind, &length) == 0, "gzip large length");
    Require(kind == 0 && length == 70000, "gzip large inflated length");
    std::string body;
    std::array<char, 317> buffer;
    for (;;) {
        const int count = sceHttp2ReadData(client.request, buffer.data(), buffer.size());
        Require(count >= 0 && count <= static_cast<int>(buffer.size()), "gzip large read count");
        if (!count) break;
        body.append(buffer.data(), static_cast<std::size_t>(count));
    }
    Require(body == GzipPayload(), "gzip large inflated body");
    server.Join();
}

void GzipMembers(int ssl) {
    for (const auto mode : {Mode::GzipConcat, Mode::GzipEmpty, Mode::GzipEmptyMembers, Mode::GzipLargeMembers, Mode::GzipHeaders}) {
        for (const int enabled : {0, 1}) {
            for (const bool lengthFirst : {false, true}) {
                std::fprintf(stderr, "HTTP2 gzip member scenario %d enabled %d length-first %d\n", static_cast<int>(mode), enabled, lengthFirst);
                Server server(mode);
                Client client(ssl);
                Require(sceHttp2SetInflateGZIPEnabled(client.tmpl, enabled) == 0, "gzip member option");
                client.Create(server.Url());
                Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "gzip member send");
                const std::string expected = mode == Mode::GzipConcat ? "response bodyresponse body" :
                    mode == Mode::GzipLargeMembers ? GzipPayload() + "tail" : mode == Mode::GzipEmpty ? "" : "firstlast";
                const std::string encoded = mode == Mode::GzipEmpty ? StoredGzip("") : mode == Mode::GzipEmptyMembers ?
                        StoredGzip("") + StoredGzip("first") + StoredGzip("") + StoredGzip("last") + StoredGzip("") :
                        mode == Mode::GzipLargeMembers ? StoredGzip(GzipPayload()) + StoredGzip("tail") :
                        mode == Mode::GzipHeaders ? HeaderGzip("first") + HeaderGzip("last") : std::string{};
                const auto checkLength = [&] (std::size_t bytes) {
                    int kind = -1;
                    std::uint64_t length = 0;
                    Require(sceHttp2GetResponseContentLength(client.request, &kind, &length) == 0 && kind == 0 && length == bytes, "gzip member length");
                };
                if (lengthFirst) checkLength(enabled ? expected.size() : mode == Mode::GzipConcat ? 66 : encoded.size());
                std::array<char, 317> buffer;
                std::string body;
                Require(sceHttp2ReadData(client.request, nullptr, 0) == 0, "gzip member zero read");
                for (;;) {
                    const int count = sceHttp2ReadData(client.request, buffer.data(), buffer.size());
                    Require(count >= 0 && count <= static_cast<int>(buffer.size()), "gzip member read count");
                    if (!count) break;
                    body.append(buffer.data(), static_cast<std::size_t>(count));
                }
                if (enabled) Require(body == expected, "gzip member decoded bytes");
                else {
                    if (mode == Mode::GzipConcat) Require(body.size() == 66 && static_cast<unsigned char>(body[0]) == 0x1f && body.substr(0, 33) == body.substr(33), "gzip concatenated wire bytes");
                    else Require(body == encoded, "gzip member wire bytes");
                }
                checkLength(enabled ? expected.size() : body.size());
                Require(sceHttp2ReadData(client.request, buffer.data(), 1) == 0, "gzip member repeated EOF");
                server.Join();
            }
        }
    }
}

void GzipFailure(int ssl, Mode mode, const char* message) {
    std::fprintf(stderr, "HTTP2 gzip failure scenario %d\n", static_cast<int>(mode));
    Server server(mode);
    Client client(ssl);
    Require(sceHttp2SetInflateGZIPEnabled(client.tmpl, 1) == 0, "gzip failure option");
    client.Create(server.Url());
    Require(Throws([&] {
        sceHttp2SendRequest(client.request, nullptr, 0);
        std::array<char, 64> buffer;
        sceHttp2ReadData(client.request, buffer.data(), buffer.size());
    }), message);
    server.Join();
}

void Async(int ssl) {
    std::fprintf(stderr, "HTTP2 async scenario\n");
    Server server(Mode::Normal, "/another", 302, "localhost");
    Client client(ssl);
    Require(sceHttp2SslEnableOption(client.tmpl, 0x4) == 0, "verify async host name");
    client.Create(server.Url("localhost"));
    KernelEqueue eq = 0;
    Require(sceKernelCreateEqueue(&eq, "http2-async") == 0, "async equeue");
    Require(sceKernelAddUserEventEdge(eq, client.request) == 0, "async user event");
    int tag = 0;
    Http2AsyncOption option{};
    option.equeue = eq;
    option.user_event_id = client.request;
    option.user_data = &tag;
    Require(sceHttp2SendRequestAsync(client.request, nullptr, 0, &option, nullptr) == 0, "async send submission");
    KernelEvent event{};
    int count = 0;
    const KernelUseconds timeout = 10000000;
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &timeout) == 0 && count == 1, "async send event");
    Require(event.ident == static_cast<std::uintptr_t>(client.request) && event.udata == &tag, "async send event payload");
    Http2AsyncResult result{};
    Require(sceHttp2WaitAsync(client.request, &result, nullptr, nullptr) == 0, "async send completion");
    Require(result.req_id == client.request && result.result == 0 && result.event_type == 0, "async send result");
    std::array<char, 64> buffer{};
    Http2AsyncOption readOption = option;
    Require(sceHttp2ReadDataAsync(client.request, buffer.data(), buffer.size(), &readOption, nullptr) == 0, "async read submission");
    count = 0;
    Require(sceKernelWaitEqueue(eq, &event, 1, &count, &timeout) == 0 && count == 1, "async read event");
    Require(event.ident == static_cast<std::uintptr_t>(client.request) && event.udata == &tag, "async read event payload");
    Http2AsyncResult readResult{};
    Require(sceHttp2WaitAsync(client.request, &readResult, nullptr, nullptr) == 0, "async read completion");
    Require(readResult.req_id == client.request && readResult.result == 13 && readResult.event_type == 1, "async read result");
    Require(std::string(buffer.data(), 13) == "response body", "async read body");
    server.Join();
    Require(server.Negotiated(), "async h2 ALPN");
    Require(sceKernelDeleteUserEvent(eq, client.request) == 0, "delete async user event");
    Require(sceKernelDeleteEqueue(eq) == 0, "delete async equeue");
}

void AsyncWithoutQueue(int ssl) {
    std::fprintf(stderr, "HTTP2 async without a kernel queue scenario\n");
    Server server(Mode::Normal, "/another", 302, "localhost");
    Client client(ssl);
    Require(sceHttp2SslEnableOption(client.tmpl, 0x4) == 0, "verify queueless async host name");
    client.Create(server.Url("localhost"));
    Require(sceHttp2SendRequestAsync(client.request, nullptr, 0, nullptr, nullptr) == 0, "queueless async send submission");
    Http2AsyncResult result{};
    Require(sceHttp2WaitAsync(client.request, &result, nullptr, nullptr) == 0, "queueless async send completion");
    Require(result.req_id == client.request && result.result == 0 && result.event_type == 0, "queueless async send result");
    std::array<char, 64> buffer{};
    Require(sceHttp2ReadDataAsync(client.request, buffer.data(), buffer.size(), nullptr, nullptr) == 0, "queueless async read submission");
    Http2AsyncResult readResult{};
    Require(sceHttp2WaitAsync(client.request, &readResult, nullptr, nullptr) == 0, "queueless async read completion");
    Require(readResult.req_id == client.request && readResult.result == 13 && readResult.event_type == 1, "queueless async read result");
    Require(std::string(buffer.data(), 13) == "response body", "queueless async read body");
    server.Join();
    Require(server.Negotiated(), "queueless async h2 ALPN");
}

struct AsyncQueue {
    KernelEqueue eq = 0;
    int tag = 0;
    Http2AsyncOption option{};
    explicit AsyncQueue(int request) {
        Require(sceKernelCreateEqueue(&eq, "http2-async") == 0, "async equeue");
        Require(sceKernelAddUserEventEdge(eq, request) == 0, "async user event");
        option.equeue = eq;
        option.user_event_id = request;
        option.user_data = &tag;
    }
    AsyncQueue(const AsyncQueue&) = delete;
    AsyncQueue& operator=(const AsyncQueue&) = delete;
    ~AsyncQueue() {
        Require(sceKernelDeleteUserEvent(eq, option.user_event_id) == 0, "delete async user event");
        Require(sceKernelDeleteEqueue(eq) == 0, "delete async equeue");
    }
    void ExpectEvent() {
        KernelEvent event{};
        int count = 0;
        const KernelUseconds timeout = 10000000;
        Require(sceKernelWaitEqueue(eq, &event, 1, &count, &timeout) == 0 && count == 1, "async event");
        Require(event.ident == static_cast<std::uintptr_t>(option.user_event_id) && event.udata == option.user_data, "async event payload");
    }
    void ExpectNoEvent() {
        KernelEvent event{};
        int count = 0;
        const KernelUseconds timeout = 200000;
        Require(sceKernelWaitEqueue(eq, &event, 1, &count, &timeout) != 0, "deleted request drops completion");
    }
};

void AsyncOptionalArguments(int ssl) {
    std::fprintf(stderr, "HTTP2 async optional arguments scenario\n");
    Server server(Mode::Normal, "/another", 302, "localhost");
    Client client(ssl);
    Require(sceHttp2SslEnableOption(client.tmpl, 0x4) == 0, "verify async optional host name");
    client.Create(server.Url("localhost"));
    AsyncQueue queue(client.request);
    int opaque = 0;
    Http2AsyncResult reserved{};
    reserved.reserved = &opaque;
    Require(sceHttp2WaitAsync(client.request, nullptr, nullptr, nullptr) == static_cast<int>(0x817B1225), "async null result");
    Require(sceHttp2WaitAsync(client.request, &reserved, nullptr, nullptr) == static_cast<int>(0x817B11FE), "async reserved result");
    Http2AsyncResult clean{};
    Require(sceHttp2WaitAsync(client.request, &clean, nullptr, &opaque) == static_cast<int>(0x817B11FE), "async reserved wait argument");
    uint32_t zeroTimeout = 0;
    Http2AsyncResult polled{};
    polled.req_id = -1;
    Require(sceHttp2WaitAsync(client.request, &polled, &zeroTimeout, nullptr) == static_cast<int>(0x817B1065), "unsent zero wait reports no completion");
    Require(zeroTimeout == 0, "unsent zero wait leaves the timeout at zero");
    Require(polled.req_id == -1, "unsent zero wait leaves the result untouched");
    uint32_t unsentTimeout = 5000000;
    const auto unsentStarted = std::chrono::steady_clock::now();
    Require(sceHttp2WaitAsync(client.request, &polled, &unsentTimeout, nullptr) == static_cast<int>(0x817B1065), "unsent wait reports no completion");
    Require(std::chrono::steady_clock::now() - unsentStarted < std::chrono::milliseconds(200), "unsent wait returned late");
    Require(unsentTimeout == 5000000, "unsent wait leaves the timeout unchanged");
    Require(polled.req_id == -1, "unsent wait leaves the result untouched");
    Require(sceHttp2WaitAsync(client.request, &polled, nullptr, nullptr) == static_cast<int>(0x817B1065), "unsent null wait reports no completion");
    Require(polled.req_id == -1, "unsent null wait leaves the result untouched");
    Require(sceHttp2SendRequestAsync(client.request, nullptr, 0, &queue.option, &opaque) == 0, "async opaque option submission");
    queue.ExpectEvent();
    Http2AsyncResult result{};
    uint32_t queuedTimeout = 5000000;
    Require(sceHttp2WaitAsync(client.request, &result, &queuedTimeout, nullptr) == 0, "async opaque option completion");
    Require(queuedTimeout == 5000000, "queued completion leaves the timeout unchanged");
    Require(result.req_id == client.request && result.result == 0 && result.event_type == 0, "async opaque option result");
    std::array<char, 64> buffer{};
    Require(sceHttp2ReadDataAsync(client.request, buffer.data(), buffer.size(), nullptr, &opaque) == 0, "async null kqueue read submission");
    queue.ExpectNoEvent();
    Http2AsyncResult readResult{};
    Require(sceHttp2WaitAsync(client.request, &readResult, nullptr, nullptr) == 0, "async null kqueue read completion");
    Require(readResult.req_id == client.request && readResult.result == 13 && readResult.event_type == 1, "async null kqueue read result");
    Require(std::string(buffer.data(), 13) == "response body", "async null kqueue read body");
    server.Join();
    Require(server.Negotiated(), "async optional h2 ALPN");
}

void AsyncBlockingWait(int ssl) {
    std::fprintf(stderr, "HTTP2 async blocking wait scenario\n");
    Server server(Mode::StallResponse, "/another", 302, "localhost");
    Client client(ssl);
    client.Create(server.Url("localhost"));
    AsyncQueue queue(client.request);
    Require(sceHttp2SendRequestAsync(client.request, nullptr, 0, &queue.option, nullptr) == 0, "async blocking submission");
    uint32_t zeroTimeout = 0;
    Http2AsyncResult polled{};
    polled.req_id = -1;
    Require(sceHttp2WaitAsync(client.request, &polled, &zeroTimeout, nullptr) == static_cast<int>(0x817B1079), "zero wait reports the pending timeout");
    Require(zeroTimeout == 0, "zero wait leaves the timeout at zero");
    Require(polled.req_id == -1, "zero wait leaves the result untouched");
    std::thread release([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        server.Release();
    });
    uint32_t shortTimeout = 1;
    Http2AsyncResult early{};
    const auto expired = std::chrono::steady_clock::now();
    Require(sceHttp2WaitAsync(client.request, &early, &shortTimeout, nullptr) == static_cast<int>(0x817B1079), "expired wait reports the timeout");
    Require(std::chrono::steady_clock::now() - expired < std::chrono::milliseconds(200), "expired wait returned late");
    Require(shortTimeout == 0, "expired wait clears the timeout");
    const auto started = std::chrono::steady_clock::now();
    Http2AsyncResult result{};
    Require(sceHttp2WaitAsync(client.request, &result, nullptr, nullptr) == 0, "null timeout waits for the completion");
    const auto elapsed = std::chrono::steady_clock::now() - started;
    release.join();
    Require(elapsed >= std::chrono::milliseconds(200), "null timeout returned before the completion");
    Require(result.req_id == client.request && result.result == 0 && result.event_type == 0, "async blocking result");
    zeroTimeout = 0;
    polled = {};
    polled.req_id = -1;
    Require(sceHttp2WaitAsync(client.request, &polled, &zeroTimeout, nullptr) == static_cast<int>(0x817B1065), "drained zero wait reports no completion");
    Require(zeroTimeout == 0, "drained zero wait leaves the timeout at zero");
    Require(polled.req_id == -1, "drained zero wait leaves the result untouched");
    queue.ExpectEvent();
    server.Join();
}

void AsyncDelete(int ssl, bool waiter) {
    std::fprintf(stderr, "HTTP2 async delete scenario waiter %d\n", waiter ? 1 : 0);
    Server server(Mode::StallHeaders);
    Client client(ssl);
    client.Create(server.Url());
    AsyncQueue queue(client.request);
    Require(sceHttp2SendRequestAsync(client.request, nullptr, 0, &queue.option, nullptr) == 0, "async delete submission");
    server.WaitStage(2);
    int waitResult = -1;
    std::atomic<bool> entered{false};
    std::thread waiting;
    if (waiter) {
        waiting = std::thread([&] {
            Http2AsyncResult result{};
            entered.store(true);
            waitResult = sceHttp2WaitAsync(client.request, &result, nullptr, nullptr);
        });
        while (!entered.load()) std::this_thread::yield();
    }
    Require(sceHttp2DeleteRequest(client.request) == 0, "delete in-flight async request");
    if (waiter) {
        waiting.join();
        Require(waitResult == static_cast<int>(0x817b1100), "deleted async wait returns invalid id");
    }
    server.Release();
    server.Join();
    queue.ExpectNoEvent();
    Http2AsyncResult result{};
    Require(sceHttp2WaitAsync(client.request, &result, nullptr, nullptr) == static_cast<int>(0x817b1100), "deleted async request stays invalid");
}

void AsyncTwoWaiters(int ssl) {
    std::fprintf(stderr, "HTTP2 async two-waiter scenario\n");
    Server server(Mode::StallResponse);
    Client client(ssl);
    client.Create(server.Url());
    AsyncQueue queue(client.request);
    Require(sceHttp2SendRequestAsync(client.request, nullptr, 0, &queue.option, nullptr) == 0, "two-waiter submission");
    server.WaitStage(2);
    std::atomic<int> successes{0};
    std::atomic<int> entered{0};
    int codes[2] = {-1, -1};
    const auto consume = [&](int index) {
        Http2AsyncResult result{};
        entered.fetch_add(1);
        codes[index] = sceHttp2WaitAsync(client.request, &result, nullptr, nullptr);
        if (codes[index] == 0) successes.fetch_add(1);
    };
    std::thread first(consume, 0);
    std::thread second(consume, 1);
    while (entered.load() != 2) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    server.Release();
    while (successes.load() == 0) std::this_thread::yield();
    Require(sceHttp2DeleteRequest(client.request) == 0, "delete after last completion");
    first.join();
    second.join();
    Require(successes.load() == 1, "one waiter consumes the completion");
    Require(codes[0] == static_cast<int>(0x817b1100) || codes[1] == static_cast<int>(0x817b1100), "the other waiter observes deletion");
    server.Join();
}

void AsyncTeardown(int ssl) {
    std::fprintf(stderr, "HTTP2 async teardown scenario\n");
    Server server(Mode::StallHeaders);
    Client client(ssl);
    client.Create(server.Url());
    AsyncQueue queue(client.request);
    Require(sceHttp2SendRequestAsync(client.request, nullptr, 0, &queue.option, nullptr) == 0, "async teardown submission");
    server.WaitStage(2);
    Require(sceHttp2Term(client.context) == 0, "term context with in-flight async request");
    client.context = 0;
    server.Release();
    server.Join();
    queue.ExpectNoEvent();
    Http2AsyncResult result{};
    Require(sceHttp2WaitAsync(client.request, &result, nullptr, nullptr) == static_cast<int>(0x817b1100), "torn-down async request stays invalid");
}

void AsyncFailure(int ssl) {
    std::fprintf(stderr, "HTTP2 async failure scenario\n");
    Client client(ssl);
    client.Create("https://localhost/", "POST", 0);
    AsyncQueue queue(client.request);
    const char body = 'x';
    Require(sceHttp2SendRequestAsync(client.request, &body, 1, &queue.option, nullptr) == 0, "async failure submission");
    queue.ExpectEvent();
    Http2AsyncResult result{};
    Require(Throws([&] { sceHttp2WaitAsync(client.request, &result, nullptr, nullptr); }), "async worker failure reaches waiter");
}

struct PublishGate {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
};

void AsyncDeletePublished(int ssl, bool parent) {
    std::fprintf(stderr, "HTTP2 async delete published scenario parent %d\n", parent ? 1 : 0);
    Client client(ssl);
    client.Create("https://localhost/", "POST", 0);
    const int request = client.request;
    const int tmpl = client.tmpl;
    AsyncQueue queue(request);
    PublishGate gate;
    GuestSceHttp2Test::SetAsyncPublishHookForTests_nid_no_patch([](void* data) {
        auto& gate = *static_cast<PublishGate*>(data);
        gate.entered.store(true, std::memory_order_release);
        while (!gate.release.load(std::memory_order_acquire)) std::this_thread::yield();
    }, &gate);
    const char body = 'x';
    Require(sceHttp2SendRequestAsync(request, &body, 1, &queue.option, nullptr) == 0, "async published submission");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!gate.entered.load(std::memory_order_acquire)) {
        Require(std::chrono::steady_clock::now() < deadline, "async publication entered");
        std::this_thread::yield();
    }
    std::atomic<bool> started{false};
    std::atomic<bool> deleted{false};
    int deleteResult = -1;
    std::thread deleter([&] {
        started.store(true, std::memory_order_release);
        deleteResult = parent ? sceHttp2DeleteTemplate(tmpl) : sceHttp2DeleteRequest(request);
        deleted.store(true, std::memory_order_release);
    });
    while (!started.load(std::memory_order_acquire)) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!deleted.load(std::memory_order_acquire), "deletion waits for event delivery");
    gate.release.store(true, std::memory_order_release);
    deleter.join();
    GuestSceHttp2Test::SetAsyncPublishHookForTests_nid_no_patch(nullptr, nullptr);
    Require(deleteResult == 0, "deletion after publication");
    queue.ExpectEvent();
    queue.ExpectNoEvent();
    Http2AsyncResult result{};
    Require(sceHttp2WaitAsync(request, &result, nullptr, nullptr) == static_cast<int>(0x817b1100), "published completion retired");
}

void Failure(int ssl, Mode mode) {
    std::fprintf(stderr, "HTTP2 failure scenario %d\n", static_cast<int>(mode));
    Server server(mode);
    Client client(ssl);
    client.Create(server.Url());
    Require(sceHttp2SetConnectTimeOut(client.request, 1000000) == 0 && sceHttp2SetRecvTimeOut(client.request, 1000000) == 0, "failure deadlines");
    Require(Throws([&] {
        sceHttp2SendRequest(client.request, nullptr, 0);
        std::array<char, 32> buffer;
        while (sceHttp2ReadData(client.request, buffer.data(), buffer.size()) > 0) {}
    }), "failure never reports complete success");
    int status = 1234;
    Require(Throws([&] { sceHttp2GetStatusCode(client.request, &status); }) && status == 1234, "failed outputs unchanged");
}

void AfterFailedSend(const Client& client, int expected) {
    int status = 1234;
    Require(sceHttp2GetStatusCode(client.request, &status) == 0 && status == 0, "status after a failed send");
    int kind = 5;
    std::uint64_t length = 999;
    Require(sceHttp2GetResponseContentLength(client.request, &kind, &length) == static_cast<int>(0x817b1065) && kind == 5 && length == 999,
        "no content length after a failed send");
    char byte = 'z';
    Require(sceHttp2ReadData(client.request, &byte, 1) == static_cast<int>(0x817b1065) && byte == 'z', "no response to read after a failed send");
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == expected, "another send repeats the failure");
}

void Handshake(int ssl, Mode mode, int expected, std::uint32_t enable = 0) {
    std::fprintf(stderr, "HTTP2 handshake failure scenario %d\n", static_cast<int>(mode));
    Server server(mode);
    Client client(ssl);
    if (enable) Require(sceHttp2SslEnableOption(client.tmpl, enable) == 0, "enable certificate check");
    client.Create(server.Url());
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == expected, "handshake failure result");
    AfterFailedSend(client, expected);
}

void Cancellation(int ssl, Mode mode, int teardown) {
    std::fprintf(stderr, "HTTP2 cancellation scenario %d teardown %d\n", static_cast<int>(mode), teardown);
    Server server(mode);
    Client client(ssl);
    const std::string upload = mode == Mode::StallUpload ? std::string(150000, 'u') : std::string{};
    client.Create(server.Url(), upload.empty() ? "GET" : "POST", upload.size());
    SceSsl::MemoryPoolStats before{};
    Require(sceSslGetMemoryPoolStats(ssl, &before) == 0, "TLS pool baseline");
    auto pending = std::async(std::launch::async, [&] {
        try {
            const int sent = sceHttp2SendRequest(client.request, upload.data(), upload.size());
            if (sent == static_cast<int>(0x817b1100)) return teardown != 0;
            Require(sent == 0, "pending send result");
            char byte = 'z';
            const int read = sceHttp2ReadData(client.request, &byte, 1);
            if (read == static_cast<int>(0x817b1100)) {
                Require(byte == 'z', "deleted-handle read leaves buffer untouched");
                return teardown != 0;
            }
            return false;
        } catch (const std::exception&) { return true; }
    });
    server.WaitStage(mode == Mode::StallHandshake ? 1 : 2);
    Require(pending.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout, "operation is pending");
    if (teardown == 0) Require(sceHttp2AbortRequest(client.request) == 0, "cancel request");
    if (teardown == 1) Require(sceHttp2DeleteRequest(client.request) == 0, "delete pending request");
    if (teardown == 2) Require(sceHttp2DeleteTemplate(client.tmpl) == 0, "delete pending template");
    if (teardown == 3) { Require(sceHttp2Term(client.context) == 0, "term pending context"); client.context = 0; }
    if (teardown) {
        SceSsl::MemoryPoolStats after{};
        Require(sceSslGetMemoryPoolStats(ssl, &after) == 0 && after.currentInuseSize == before.currentInuseSize,
            "teardown releases TLS allocations before returning");
    }
    Require(pending.wait_for(std::chrono::seconds(1)) == std::future_status::ready && pending.get(), "pending call observes cancellation");
    if (teardown) Require(sceHttp2DeleteRequest(client.request) == static_cast<int>(0x817b1100), "deleted request stays invalid");
}

void EarlyResponse(int ssl, Mode mode) {
    std::fprintf(stderr, "HTTP2 early response scenario %d\n", static_cast<int>(mode));
    Server server(mode);
    Client client(ssl, 1, 1);
    const std::string upload(150000, 'u');
    client.Create(server.Url(), "POST", upload.size());
    auto pending = std::async(std::launch::async, [&] {
        Require(sceHttp2SendRequest(client.request, upload.data(), upload.size()) == 0, "early final send succeeds");
        int status = -1;
        Require(sceHttp2GetStatusCode(client.request, &status) == 0 && status == 413, "early final status");
        int kind = -1;
        std::uint64_t length = 999;
        const std::string expected = mode == Mode::EarlyEmpty ? "" : mode == Mode::EarlyLarge ? std::string(200000, 'x') : "response body";
        Require(sceHttp2GetResponseContentLength(client.request, &kind, &length) == 0 && kind == 0 && length == expected.size(), "early final length");
        std::string body;
        std::array<char, 317> buffer;
        for (;;) {
            const int count = sceHttp2ReadData(client.request, buffer.data(), buffer.size());
            Require(count >= 0 && count <= static_cast<int>(buffer.size()), "early final read count");
            if (!count) break;
            body.append(buffer.data(), static_cast<std::size_t>(count));
        }
        Require(body == expected, "early final body");
        Require(sceHttp2ReadData(client.request, buffer.data(), buffer.size()) == 0, "early final repeat EOF");
    });
    server.WaitStage(2);
    Require(pending.wait_for(std::chrono::seconds(1)) == std::future_status::ready, "early response completes before upload release");
    pending.get();
    server.Join();
    Require(server.Upload().size() <= 65535 && server.Upload().size() < upload.size(), "early final stops upload within initial window");
    Require(server.Negotiated(), "early final h2 ALPN");
}

#ifdef _WIN32
void FirstNetworkRequest(int ssl) {
    WSADATA data{};
    Require(WSAStartup(MAKEWORD(2, 2), &data) == 0, "reserve refused endpoint startup");
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Require(listener != INVALID_SOCKET, "reserve refused endpoint socket");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    Require(bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "reserve refused endpoint bind");
    int size = sizeof(address);
    Require(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) == 0, "reserve refused endpoint port");
    Require(closesocket(listener) == 0 && WSACleanup() == 0, "return to uninitialized Winsock");
    addrinfo* addresses = nullptr;
    Require(getaddrinfo("localhost", nullptr, nullptr, &addresses) == WSANOTINITIALISED && !addresses, "DNS starts without Winsock");
    Client client(ssl, 1, 1);
    client.Create("https://localhost:" + std::to_string(ntohs(address.sin_port)) + "/");
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == static_cast<int>(0x8041013d), "first network request resolves and reaches refused endpoint");
}
#endif

void Refused(int ssl) {
    mbedtls_net_context listener;
    mbedtls_net_init(&listener);
    Require(mbedtls_net_bind(&listener, "127.0.0.1", "0", MBEDTLS_NET_PROTO_TCP) == 0, "refused bind");
    sockaddr_in address{};
#ifdef _WIN32
    int size = sizeof(address);
#else
    socklen_t size = sizeof(address);
#endif
    Require(getsockname(listener.fd, reinterpret_cast<sockaddr*>(&address), &size) == 0, "refused port");
    const auto url = "https://127.0.0.1:" + std::to_string(ntohs(address.sin_port)) + "/";
    mbedtls_net_free(&listener);
    Client client(ssl);
    client.Create(url);
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == static_cast<int>(0x8041013d), "raw connection refused error");
    AfterFailedSend(client, static_cast<int>(0x8041013d));
}

std::atomic<int> lookups{0};
std::atomic<int> failedLookups{0};

int CountedLookup(const char*) {
    return ++lookups <= failedLookups.load() ? -1 : 0;
}

void Retries(int ssl) {
    std::fprintf(stderr, "HTTP2 resolve retry scenarios\n");
    Http2SetLookupHook_nid_no_patch(CountedLookup);
    {
        Server server(Mode::Normal);
        Client client(ssl);
        Require(sceHttp2SetResolveRetry(client.tmpl, 2) == 0, "two resolve retries");
        client.Create(server.Url());
        lookups = 0;
        failedLookups = 2;
        Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "a lookup that fails twice succeeds on its second retry");
        Require(lookups == 3, "two failed lookups and the third one were made");
        std::array<char, 64> buffer;
        std::string body;
        for (int count; (count = sceHttp2ReadData(client.request, buffer.data(), buffer.size())) > 0;) body.append(buffer.data(), static_cast<std::size_t>(count));
        Require(body == "response body", "body after retried lookups");
        server.Join();
    }
    for (const int retry : {0, 1}) {
        Client client(ssl);
        Require(sceHttp2SetResolveRetry(client.tmpl, retry) == 0, "resolve retry count");
        client.Create("https://127.0.0.1:1/");
        lookups = 0;
        failedLookups = 100;
        Require(Throws([&] { sceHttp2SendRequest(client.request, nullptr, 0); }), "every lookup failing fails the request");
        Require(lookups == retry + 1, "a request makes its retry count plus one lookups before failing");
    }
    Http2SetLookupHook_nid_no_patch(nullptr);
}

void Capacity(int ssl) {
    Server server(Mode::StallBody);
    Client client(ssl, 1);
    client.Create(server.Url());
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "first capacity slot");
    const int second = sceHttp2CreateRequestWithURL(client.tmpl, "GET", server.Url().c_str(), 0);
    Require(Throws([&] { sceHttp2SendRequest(second, nullptr, 0); }), "capacity wait throws before another connection");
    Require(sceHttp2DeleteRequest(client.request) == 0, "release capacity slot");
    char unchanged = 'z';
    Require(sceHttp2ReadData(client.request, &unchanged, 1) == static_cast<int>(0x817b1100) && unchanged == 'z',
        "read after deletion leaves buffer untouched");
    Server independent(Mode::Normal);
    const int third = sceHttp2CreateRequestWithURL(client.tmpl, "GET", independent.Url().c_str(), 0);
    Require(sceHttp2SendRequest(third, nullptr, 0) == 0, "slot available after deletion");
    char buffer[32];
    while (sceHttp2ReadData(third, buffer, sizeof(buffer)) > 0) {}
}

void Deadline(int ssl, Mode mode, bool overall) {
    std::fprintf(stderr, "HTTP2 deadline scenario %d overall %d\n", static_cast<int>(mode), overall);
    Server server(mode);
    Client client(ssl);
    const std::string upload = mode == Mode::StallUpload ? std::string(150000, 'u') : std::string{};
    client.Create(server.Url(), upload.empty() ? "GET" : "POST", upload.size());
    const auto begin = std::chrono::steady_clock::now();
    Require((overall ? sceHttp2SetTimeOut(client.request, 80000) : mode == Mode::StallHandshake ?
        sceHttp2SetConnectTimeOut(client.request, 80000) : mode == Mode::StallUpload ? sceHttp2SetSendTimeOut(client.request, 80000) :
        sceHttp2SetRecvTimeOut(client.request, 80000)) == 0, "set deadline");
    Require(Throws([&] {
        sceHttp2SendRequest(client.request, upload.data(), upload.size());
        char byte;
        sceHttp2ReadData(client.request, &byte, 1);
    }), "deadline terminates blocked operation");
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    Require(elapsed >= std::chrono::milliseconds(60) && elapsed < std::chrono::seconds(1), "bounded deadline");
}

}

void HostAuthority(int ssl) {
    std::fprintf(stderr, "HTTP2 host header scenario\n");
    Server server(Mode::Normal);
    Client client(ssl);
    Require(sceHttp2AddRequestHeader(client.tmpl, "Host", "template.example", 0) == 0, "template host");
    client.Create(server.Url());
    Require(sceHttp2AddRequestHeader(client.request, "host", "request.example", 0) == 0, "request host");
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "send host request");
    std::array<char, 64> buffer;
    while (sceHttp2ReadData(client.request, buffer.data(), buffer.size()) > 0) {}
    server.Join();
    const auto& headers = server.Headers();
    Require(headers.count(":authority") == 1 && headers.find(":authority")->second == "request.example", "host header becomes the authority");
    Require(headers.count("host") == 0, "host header is not sent");
}

void AuthenticationStatus(int ssl, int version, ResponseSpec response, bool authEnabled) {
    std::fprintf(stderr, "HTTP2 authentication status scenario version=%d status=%d enabled=%d\n", version, response.status, authEnabled);
    const int expected = response.status;
    Server server(version == 1 ? Mode::Http10 : version == 2 ? Mode::Http11 : Mode::Normal);
    server.SetResponse(std::move(response));
    Client client(ssl, 4, 0, version);
    int enabled = -1;
    Require(sceHttp2GetAuthEnabled(client.tmpl, &enabled) == 0 && enabled == 1, "authentication remains enabled by default");
    if (!authEnabled) Require(sceHttp2SetAuthEnabled(client.tmpl, 0) == 0, "disable automatic authentication");
    client.Create(server.Url());
    Require(sceHttp2SendRequest(client.request, nullptr, 0) == 0, "receive response without negotiating authentication");
    int status = 0;
    Require(sceHttp2GetStatusCode(client.request, &status) == 0 && status == expected, "preserve authentication response status");
    int kind = -1;
    std::uint64_t length = 0;
    Require(sceHttp2GetResponseContentLength(client.request, &kind, &length) == 0 && kind == 0 && length == 13, "preserve authentication response length");
    std::string body;
    std::array<char, 32> buffer{};
    for (;;) {
        const int count = sceHttp2ReadData(client.request, buffer.data(), buffer.size());
        Require(count >= 0 && count <= static_cast<int>(buffer.size()), "read authentication response body");
        if (count == 0) break;
        body.append(buffer.data(), static_cast<std::size_t>(count));
    }
    Require(body == "response body", "preserve authentication response body");
    server.Join();
}

void AuthenticationStatuses(int ssl) {
    for (const int version : {1, 2, 3}) {
        const std::string origin = version == 3 ? "www-authenticate" : "WwW-AuThEnTiCaTe";
        const std::string proxy = version == 3 ? "proxy-authenticate" : "PrOxY-AuThEnTiCaTe";
        const std::string challenge = "Basic realm=\"loopback\"";
        for (const int status : {401, 407}) {
            const auto& matching = status == 401 ? origin : proxy;
            const auto& other = status == 401 ? proxy : origin;
            AuthenticationStatus(ssl, version, {status, {}, {}}, true);
            AuthenticationStatus(ssl, version, {status, {{other, challenge}}, {}}, true);
            AuthenticationStatus(ssl, version, {status, {{matching, challenge}}, {}}, false);
            if (version != 1) AuthenticationStatus(ssl, version, {status, {}, {{matching, challenge}}}, true);
        }
        AuthenticationStatus(ssl, version, {200, {{origin, challenge}, {proxy, challenge}}, {}}, true);
    }
}

int main(int argc, char** argv) {
#ifndef _WIN32
    std::signal(SIGPIPE, SIG_IGN);
#endif
    Require(sceNetInit_nid_postfix() == 0, "net init");
    const int ssl = sceSslInit_nid_postfix(8 * 1024 * 1024);
    Require(ssl > 0, "SSL context");
    SceSsl::Data root{reinterpret_cast<std::uint8_t*>(const_cast<char*>(RootPem)), sizeof(RootPem)};
    SceSsl::Data* authorities[] = {&root};
    Require(sceSslLoadCert(ssl, 1, authorities, nullptr, nullptr) == 0, "trust loopback CA");
    if (argc == 2 && std::string_view(argv[1]) == "--system-roots") {
        const auto list = std::filesystem::current_path() / sceKernelGetFsSandboxRandomWord() / "common" / "cert" / "CA_LIST.cer";
        std::filesystem::create_directories(list.parent_path());
        std::ofstream(list, std::ios::binary | std::ios::trunc) << RootPem;
        const int unloaded = sceSslInit_nid_postfix(8 * 1024 * 1024);
        Require(unloaded > 0, "SSL context without certificates");
        Success(unloaded, Mode::Normal);
        Require(sceSslTerm_nid_postfix(unloaded) == 0, "SSL context without certificates term");
    } else if (argc == 2 && std::string_view(argv[1]) == "--auth-status") {
        AuthenticationStatuses(ssl);
    } else if (argc == 2 && std::string_view(argv[1]) == "--gzip-members") {
        GzipMembers(ssl);
    } else if (argc == 2 && std::string_view(argv[1]) == "--gzip-headers") {
        GzipFailure(ssl, Mode::GzipHeaderChecksum, "gzip header checksum mismatch throws");
        GzipFailure(ssl, Mode::GzipSecondHeaderChecksum, "later gzip header checksum mismatch throws");
        GzipFailure(ssl, Mode::GzipSecondChecksum, "later gzip member checksum mismatch throws");
        GzipFailure(ssl, Mode::GzipSecondSize, "later gzip member size mismatch throws");
        GzipFailure(ssl, Mode::GzipTrailingJunk, "gzip trailing nonmember rejected");
    } else if (argc == 2 && std::string_view(argv[1]) == "--early") {
        for (const auto mode : {Mode::EarlyFinal, Mode::EarlyReset, Mode::EarlyEmpty, Mode::EarlyLarge}) EarlyResponse(ssl, mode);
        Failure(ssl, Mode::EarlyResetIncomplete);
#ifdef _WIN32
    } else if (argc == 2 && std::string_view(argv[1]) == "--dns-first") {
        FirstNetworkRequest(ssl);
#endif
    } else if (argc == 2 && std::string_view(argv[1]) == "--http1") {
        Http1GzipPolicy(ssl);
        Http1Transport(ssl);
        Http1Policies(ssl);
        Http1Cancellation(ssl);
        Http1BufferedDeadline(ssl);
    } else if (argc == 2 && std::string_view(argv[1]) == "--http1-cookies") {
        for (const auto mode : {Mode::Http10, Mode::Http11, Mode::Http11Chunked, Mode::Http11Close, Mode::Http1Plain}) Http1CookieBox(ssl, mode);
        Http1CookieRedirect(ssl);
        for (const bool explicitCookie : {false, true}) Http1EmptyCookie(ssl, explicitCookie);
        CookieBoxDelete(ssl, Mode::Http11);
        CookieBoxDelete(ssl, Mode::Http1Plain);
    } else if (argc == 2 && std::string_view(argv[1]) == "--ssl-callbacks") {
        SslCallbacks(ssl);
    } else if (argc == 2 && std::string_view(argv[1]) == "--redirects") {
        GzipRedirect(ssl);
        RedirectPolicies(ssl);
        for (const int status : {301, 302, 303, 307, 308}) RedirectFollow(ssl, status);
        RedirectFollow(ssl, 302, "PATCH");
        RedirectFollow(ssl, 303, "HEAD");
        RedirectFollow(ssl, 307, nullptr, true);
        RedirectFollow(ssl, 308, nullptr, false, true);
        RedirectRequestMethods(ssl);
        RelativeRedirects(ssl);
        RedirectFailures(ssl);
    } else {
        Require(argc == 1, "test arguments");
        for (const auto mode : {Mode::Normal, Mode::Large, Mode::NoLength, Mode::Head, Mode::NotFound, Mode::Redirect, Mode::Auth, Mode::Informational, Mode::Trailers}) Success(ssl, mode);
        Gzip(ssl);
        GzipSplit(ssl);
        GzipLarge(ssl);
        GzipFailure(ssl, Mode::GzipTruncated, "truncated gzip throws");
        GzipFailure(ssl, Mode::GzipChecksum, "gzip checksum mismatch throws");
        for (const auto mode : {Mode::NoAlpn, Mode::Reset, Mode::Truncate, Mode::Auth}) Failure(ssl, mode);
        CookieBox(ssl);
        CookieBoxDelete(ssl);
        Handshake(ssl, Mode::Normal, static_cast<int>(0x8095f00b), 0x4);
        Handshake(ssl, Mode::CloseHandshake, static_cast<int>(0x8095170d));
        Handshake(ssl, Mode::AlertHandshake, static_cast<int>(0x8095f010));
        Handshake(ssl, Mode::Expired, static_cast<int>(0x8095f00d), 0x8);
        for (const auto mode : {Mode::StallHandshake, Mode::StallHeaders, Mode::StallBody, Mode::StallUpload}) {
            Deadline(ssl, mode, false);
            Deadline(ssl, mode, true);
            for (int teardown = 0; teardown < 4; ++teardown) Cancellation(ssl, mode, teardown);
        }
        Refused(ssl);
        Capacity(ssl);
        Retries(ssl);
        Async(ssl);
        AsyncWithoutQueue(ssl);
        AsyncOptionalArguments(ssl);
        AsyncBlockingWait(ssl);
        AsyncDelete(ssl, false);
        AsyncDelete(ssl, true);
        AsyncTwoWaiters(ssl);
        AsyncTeardown(ssl);
        AsyncFailure(ssl);
        AsyncDeletePublished(ssl, false);
        AsyncDeletePublished(ssl, true);
        const int untrusted = sceSslInit_nid_postfix(8 * 1024 * 1024);
        Require(untrusted > 0, "untrusted SSL context");
        Handshake(untrusted, Mode::Normal, static_cast<int>(0x8095f00c));
        Success(untrusted, Mode::Normal, true);
        Require(sceSslTerm_nid_postfix(untrusted) == 0, "untrusted SSL term");
        HostAuthority(ssl);
    }
    Require(sceSslTerm_nid_postfix(ssl) == 0, "SSL term");
    std::puts("HTTP2 loopback transport passed");
}
