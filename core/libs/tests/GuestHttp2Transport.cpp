#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <csignal>
#endif

#include "SceTypes.hpp"
#include "prx/libSceHttp2/include/Http2.hpp"
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
#include <future>
#include <map>
#include <mutex>
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

enum class Mode { Normal, Large, NoLength, Head, NotFound, Redirect, Auth, Reset, Truncate, StallHandshake, StallHeaders, StallBody, NoAlpn, Informational, StallUpload, EarlyFinal, EarlyReset, EarlyEmpty, EarlyLarge, EarlyResetIncomplete };
const char* Protocols[] = {"h2", nullptr};

class Server {
public:
    explicit Server(Mode mode) : mode(mode) {
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
        if (mode != Mode::NoAlpn) Require(mbedtls_ssl_conf_alpn_protocols(&config, Protocols) == 0, "server ALPN");
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
        return std::string("https://") + host + ":" + std::to_string(port) + "/path?query=yes#fragment";
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
private:
    const Mode mode;
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
        size = std::min(size, server.body.size() - server.offset);
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
        std::string status = IsEarly() ? "413" : mode == Mode::NotFound ? "404" : mode == Mode::Redirect ? "302" : mode == Mode::Auth ? "401" : "200";
        if (mode == Mode::EarlyEmpty) body.clear();
        auto length = std::to_string(body.size());
        if (mode == Mode::Head) body.clear();
        if (mode == Mode::Truncate) body = "short";
        if (mode == Mode::Informational) {
            nghttp2_nv early{reinterpret_cast<std::uint8_t*>(const_cast<char*>(":status")), reinterpret_cast<std::uint8_t*>(const_cast<char*>("103")), 7, 3, 0};
            Require(nghttp2_submit_headers(session, 0, stream, nullptr, &early, 1, nullptr) == 0, "server informational response");
        }
        std::vector<std::pair<std::string, std::string>> fields{{":status", status}, {"x-loopback", "yes"}};
        if (mode != Mode::NoLength) fields.emplace_back("content-length", length);
        if (mode == Mode::Redirect) fields.emplace_back("location", "/another");
        std::vector<nghttp2_nv> response;
        for (auto& [name, value] : fields) response.push_back({reinterpret_cast<std::uint8_t*>(name.data()), reinterpret_cast<std::uint8_t*>(value.data()), name.size(), value.size(), 0});
        nghttp2_data_provider2 provider{};
        provider.read_callback = Body;
        Require(nghttp2_submit_response2(session, stream, response.data(), response.size(), body.empty() ? nullptr : &provider) == 0, "server response");
    }

    void Serve() {
        mbedtls_net_context client;
        mbedtls_net_init(&client);
        mbedtls_ssl_context ssl;
        mbedtls_ssl_init(&ssl);
        Require(mbedtls_net_accept(&listener, &client, nullptr, 0, nullptr) == 0, "server accept");
        Stage(1);
        if (mode == Mode::StallHandshake) Stall();
        else {
            Require(mbedtls_ssl_setup(&ssl, &config) == 0, "server TLS setup");
            mbedtls_ssl_set_bio(&ssl, &client, mbedtls_net_send, mbedtls_net_recv, mbedtls_net_recv_timeout);
            if (mbedtls_ssl_handshake(&ssl) == 0) {
                const char* selected = mbedtls_ssl_get_alpn_protocol(&ssl);
                negotiated = selected && std::string(selected) == "h2";
                Exchange(ssl);
            }
        }
        mbedtls_ssl_free(&ssl);
        mbedtls_net_free(&client);
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
    explicit Client(int ssl, int maximum = 4, int autoProxy = 0) {
        context = sceHttp2Init(1, ssl, 1024 * 1024, maximum);
        tmpl = sceHttp2CreateTemplate(context, "loopback-agent", 3, autoProxy);
        Require(context > 0 && tmpl > 0, "client handles");
        Require(sceHttp2SslDisableOption(tmpl, 0x4) == 0, "numeric loopback name policy");
    }
    void Create(const std::string& url, const char* method = "GET", std::size_t size = 0) {
        request = sceHttp2CreateRequestWithURL(tmpl, method, url.c_str(), size);
        Require(request > 0, "request handle");
    }
    ~Client() { if (context > 0) Require(sceHttp2Term(context) == 0, "client term"); }
};

void Success(int ssl, Mode mode, bool disableCa = false) {
    std::fprintf(stderr, "HTTP2 success scenario %d\n", static_cast<int>(mode));
    Server server(mode);
    Client client(ssl);
    if (disableCa) Require(sceHttp2SslDisableOption(client.tmpl, 0x20) == 0, "disable known CA check");
    if (mode == Mode::Normal && !disableCa) Require(sceHttp2SslEnableOption(client.tmpl, 0x4) == 0, "verify localhost certificate name");
    Require(sceHttp2AddRequestHeader(client.tmpl, "X-Inherited", "original", 0) == 0, "template header");
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
    if (argc == 2 && std::string_view(argv[1]) == "--early") {
        for (const auto mode : {Mode::EarlyFinal, Mode::EarlyReset, Mode::EarlyEmpty, Mode::EarlyLarge}) EarlyResponse(ssl, mode);
        Failure(ssl, Mode::EarlyResetIncomplete);
#ifdef _WIN32
    } else if (argc == 2 && std::string_view(argv[1]) == "--dns-first") {
        FirstNetworkRequest(ssl);
#endif
    } else {
        Require(argc == 1, "test arguments");
        for (const auto mode : {Mode::Normal, Mode::Large, Mode::NoLength, Mode::Head, Mode::NotFound, Mode::Redirect, Mode::Auth, Mode::Informational}) Success(ssl, mode);
        for (const auto mode : {Mode::NoAlpn, Mode::Reset, Mode::Truncate, Mode::Redirect, Mode::Auth}) Failure(ssl, mode);
        Failure(ssl, Mode::Normal, true);
        for (const auto mode : {Mode::StallHandshake, Mode::StallHeaders, Mode::StallBody, Mode::StallUpload}) {
            Deadline(ssl, mode, false);
            Deadline(ssl, mode, true);
            for (int teardown = 0; teardown < 4; ++teardown) Cancellation(ssl, mode, teardown);
        }
        Refused(ssl);
        Capacity(ssl);
        const int untrusted = sceSslInit_nid_postfix(8 * 1024 * 1024);
        Require(untrusted > 0, "untrusted SSL context");
        Failure(untrusted, Mode::Normal);
        Success(untrusted, Mode::Normal, true);
        Require(sceSslTerm_nid_postfix(untrusted) == 0, "untrusted SSL term");
    }
    Require(sceSslTerm_nid_postfix(ssl) == 0, "SSL term");
    std::puts("HTTP2 loopback transport passed");
}
