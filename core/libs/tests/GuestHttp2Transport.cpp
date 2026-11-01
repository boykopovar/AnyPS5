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

enum class Mode { Normal, Large, NoLength, Head, NotFound, Redirect, Auth, Reset, Truncate, Gzip, GzipSplit, GzipTruncated, GzipChecksum, GzipConcat, GzipLarge, StallHandshake, StallHeaders, StallBody, NoAlpn, Informational, StallUpload, RedirectOnce, GzipRedirectEmpty, GzipRedirectBody, Http10, Http11, Http11Chunked, Http11Close, Http11Early, Http11Info, Http11Redirect, Http11StallHeaders, Http11StallBody, Http11BadLength, Http11Ambiguous, Http11Truncated, Http11BadChunk, Http11Gzip, Http1Plain, EarlyFinal, EarlyReset, EarlyEmpty, EarlyLarge, EarlyResetIncomplete };
const char* Protocols[] = {"h2", nullptr};
const char* Http1Protocols[] = {"http/1.1", nullptr};

std::string GzipPayload() {
    std::string payload(70000, '\0');
    for (std::size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>((i * 31 + 7) & 0xff);
    return payload;
}

std::string StoredGzip(const std::string& payload) {
    static const unsigned char header[] = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff};
    std::string body(reinterpret_cast<const char*>(header), sizeof(header));
    std::size_t offset = 0;
    while (offset < payload.size()) {
        const std::size_t count = std::min<std::size_t>(payload.size() - offset, 65535);
        body.push_back(static_cast<char>(offset + count == payload.size() ? 1 : 0));
        body.push_back(static_cast<char>(count & 0xff));
        body.push_back(static_cast<char>((count >> 8) & 0xff));
        body.push_back(static_cast<char>(~count & 0xff));
        body.push_back(static_cast<char>((~count >> 8) & 0xff));
        body.append(payload, offset, count);
        offset += count;
    }
    std::uint32_t crc = 0xffffffffu;
    for (const unsigned char byte : payload) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    crc = ~crc;
    const std::uint32_t size = static_cast<std::uint32_t>(payload.size());
    for (int shift = 0; shift < 32; shift += 8) body.push_back(static_cast<char>((crc >> shift) & 0xff));
    for (int shift = 0; shift < 32; shift += 8) body.push_back(static_cast<char>((size >> shift) & 0xff));
    return body;
}

class Server {
public:
    explicit Server(Mode mode, std::string location = "/another", int redirectStatus = 302)
        : mode(mode), location(std::move(location)), redirectStatus(redirectStatus) {
        mbedtls_net_init(&listener);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&random);
        mbedtls_x509_crt_init(&certificate);
        mbedtls_pk_init(&key);
        mbedtls_ssl_config_init(&config);
        Require(mbedtls_ctr_drbg_seed(&random, mbedtls_entropy_func, &entropy, nullptr, 0) == 0, "server RNG");
        Require(mbedtls_x509_crt_parse(&certificate, reinterpret_cast<const unsigned char*>(ServerPem), sizeof(ServerPem)) == 0, "server cert");
        Require(mbedtls_pk_parse_key(&key, reinterpret_cast<const unsigned char*>(ServerKey), sizeof(ServerKey), nullptr, 0,
            mbedtls_ctr_drbg_random, &random) == 0, "server key");
        Require(mbedtls_ssl_config_defaults(&config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0, "server TLS");
        mbedtls_ssl_conf_rng(&config, mbedtls_ctr_drbg_random, &random);
        Require(mbedtls_ssl_conf_own_cert(&config, &certificate, &key) == 0, "server identity");
        mbedtls_ssl_conf_authmode(&config, MBEDTLS_SSL_VERIFY_NONE);
        mbedtls_ssl_conf_read_timeout(&config, 2000);
        if (mode != Mode::NoAlpn) Require(mbedtls_ssl_conf_alpn_protocols(&config, IsHttp1() ? Http1Protocols : Protocols) == 0, "server ALPN");
        Require(mbedtls_net_bind(&listener, "127.0.0.1", "0", MBEDTLS_NET_PROTO_TCP) == 0, "server bind");
        sockaddr_in address{};
#ifdef _WIN32
        int size = sizeof(address);
#else
        socklen_t size = sizeof(address);
#endif
        Require(getsockname(listener.fd, reinterpret_cast<sockaddr*>(&address), &size) == 0, "server port");
        port = ntohs(address.sin_port);
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

    void Join() { Release(); if (worker.joinable()) worker.join(); }
    const std::multimap<std::string, std::string>& Headers() const { return headers; }
    const std::string& Upload() const { return upload; }
    bool Negotiated() const { return negotiated; }
    bool IsEarly() const { return mode >= Mode::EarlyFinal; }
    bool IsHttp1() const { return mode >= Mode::Http10 && mode <= Mode::Http1Plain; }
private:
    const Mode mode;
    const std::string location;
    const int redirectStatus;
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
    static nghttp2_ssize Body(nghttp2_session*, std::int32_t, std::uint8_t* bytes, std::size_t size,
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
        if (server.offset == server.body.size()) *flags |= NGHTTP2_DATA_FLAG_EOF;
        return static_cast<nghttp2_ssize>(size);
    }

    void Respond(nghttp2_session* session, std::int32_t stream) {
        Require(!responded, "one response per request");
        responded = true;
        if (mode == Mode::StallHeaders) { Stage(2); Stall(); return; }
        if (mode == Mode::Reset) { Require(nghttp2_submit_rst_stream(session, 0, stream, NGHTTP2_REFUSED_STREAM) == 0, "server reset"); return; }
        body = (mode == Mode::Large || mode == Mode::EarlyLarge) ? std::string(200000, 'x') : "response body";
        const bool redirect = mode == Mode::Redirect || mode == Mode::GzipRedirectEmpty || mode == Mode::GzipRedirectBody || (mode == Mode::RedirectOnce && accepted == 1);
        std::string status = IsEarly() ? "413" : mode == Mode::NotFound ? "404" : redirect ? std::to_string(redirectStatus) : mode == Mode::Auth ? "401" : "200";
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
        if (mode == Mode::Informational) {
            nghttp2_nv early{reinterpret_cast<std::uint8_t*>(const_cast<char*>(":status")), reinterpret_cast<std::uint8_t*>(const_cast<char*>("103")), 7, 3, 0};
            Require(nghttp2_submit_headers(session, 0, stream, nullptr, &early, 1, nullptr) == 0, "server informational response");
        }
        std::vector<std::pair<std::string, std::string>> fields{{":status", status}, {"x-loopback", "yes"}};
        if (mode != Mode::NoLength) fields.emplace_back("content-length", length);
        if (redirect) fields.emplace_back("location", location);
        if (mode == Mode::GzipRedirectEmpty || mode == Mode::GzipRedirectBody || mode == Mode::Gzip || mode == Mode::GzipSplit || mode == Mode::GzipTruncated || mode == Mode::GzipChecksum || mode == Mode::GzipConcat || mode == Mode::GzipLarge) fields.emplace_back("content-encoding", "gzip");
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

    void ExchangeHttp1(mbedtls_net_context& client, mbedtls_ssl_context* ssl) {
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
                Require(count > 0, "HTTP1 response write");
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
        if (mode == Mode::Http11Info) write("HTTP/1.1 100 Continue\r\n\r\n");
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
        const int code = mode == Mode::Http11Early ? 413 : mode == Mode::Http11Redirect ? redirectStatus : 200;
        std::string response = mode == Mode::Http10 ? "HTTP/1.0 " : "HTTP/1.1 ";
        response += std::to_string(code) + " Fixture\r\n";
        if (mode == Mode::Http11Chunked || mode == Mode::Http11BadChunk) response += "Transfer-Encoding: chunked\r\n";
        else if (mode != Mode::Http11Close) response += "Content-Length: " + std::to_string(mode == Mode::Http11Gzip ? StoredGzip("response body").size() : 13) + "\r\n";
        if (mode == Mode::Http11Gzip) response += "Content-Encoding: gzip\r\n";
        if (mode == Mode::Http11BadLength) response += "Content-Length: 14\r\n";
        if (mode == Mode::Http11Ambiguous) response += "Transfer-Encoding: chunked\r\n";
        if (mode == Mode::Http11Redirect) response += "Location: " + location + "\r\n";
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
        const bool failure = mode == 2 || mode == 3 || mode == 5 || mode == 6 || mode == 7 || mode == 8;
        if (failure) Require(Throws([&] { sceHttp2SendRequest(client.request, nullptr, 0); }), "SSL callback refusal or cancellation fails request");
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
    for (const auto mode : {Mode::Http10, Mode::Http11, Mode::Http11Chunked, Mode::Http11Close, Mode::Http1Plain}) {
        Server server(mode);
        const int version = mode == Mode::Http10 ? 1 : 2;
        Client client(ssl, 1, 0, version);
        const std::string upload = "upload bytes";
        Require(sceHttp2AddRequestHeader(client.tmpl, "X-Inherited", "retained", 0) == 0, "HTTP1 inherited header");
        client.Create(server.Url(), "POST", upload.size());
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
    Server server(mode);
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
    char* rawHeaders = reinterpret_cast<char*>(std::uintptr_t{1});
    std::size_t rawSize = 987;
    Require(Throws([&] { sceHttp2GetAllResponseHeaders(client.request, &rawHeaders, &rawSize); }) &&
        rawHeaders == reinterpret_cast<char*>(std::uintptr_t{1}) && rawSize == 987, "unknown header serialization leaves outputs untouched");
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

void Failure(int ssl, Mode mode, bool verifyName = false) {
    std::fprintf(stderr, "HTTP2 failure scenario %d\n", static_cast<int>(mode));
    Server server(mode);
    Client client(ssl);
    if (verifyName) Require(sceHttp2SslEnableOption(client.tmpl, 0x4) == 0, "enable numeric host name rejection");
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
    int status = 123;
    Require(sceHttp2GetStatusCode(client.request, &status) == static_cast<int>(0x8041013d) && status == 123, "failed status untouched");
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
        for (const auto mode : {Mode::Normal, Mode::Large, Mode::NoLength, Mode::Head, Mode::NotFound, Mode::Redirect, Mode::Auth, Mode::Informational}) Success(ssl, mode);
        Gzip(ssl);
        GzipSplit(ssl);
        GzipLarge(ssl);
        GzipFailure(ssl, Mode::GzipTruncated, "truncated gzip throws");
        GzipFailure(ssl, Mode::GzipChecksum, "gzip checksum mismatch throws");
        GzipFailure(ssl, Mode::GzipConcat, "concatenated gzip members throw");
        for (const auto mode : {Mode::NoAlpn, Mode::Reset, Mode::Truncate, Mode::Auth}) Failure(ssl, mode);
        Failure(ssl, Mode::Normal, true);
        for (const auto mode : {Mode::StallHandshake, Mode::StallHeaders, Mode::StallBody, Mode::StallUpload}) {
            Deadline(ssl, mode, false);
            Deadline(ssl, mode, true);
            for (int teardown = 0; teardown < 4; ++teardown) Cancellation(ssl, mode, teardown);
        }
        Refused(ssl);
        Capacity(ssl);
        Retries(ssl);
        const int untrusted = sceSslInit_nid_postfix(8 * 1024 * 1024);
        Require(untrusted > 0, "untrusted SSL context");
        Failure(untrusted, Mode::Normal);
        Success(untrusted, Mode::Normal, true);
        Require(sceSslTerm_nid_postfix(untrusted) == 0, "untrusted SSL term");
    }
    Require(sceSslTerm_nid_postfix(ssl) == 0, "SSL term");
    std::puts("HTTP2 loopback transport passed");
}
