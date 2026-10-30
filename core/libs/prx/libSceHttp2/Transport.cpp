#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#endif

#include "Transport.hpp"

#include "zlib.h"
#include "prx/libSceNet/include/Net.hpp"
#include "prx/libSceSsl/include/Ssl.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include <nghttp2/nghttp2.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <exception>
#include <limits>
#include <mutex>
#include <string_view>
#include <thread>

extern "C" int SslRead_nid_no_patch(int, void*, std::size_t, int);
extern "C" int SslTrustSystemRoots_nid_no_patch(int);

namespace Http2 {
namespace {

using Clock = std::chrono::steady_clock;
constexpr int WantRead = static_cast<int>(0x8095f013);
constexpr int WantWrite = static_cast<int>(0x8095f014);
constexpr int InvalidValue = static_cast<int>(0x817B11FE);
constexpr std::size_t MaxGzipBytes = 64 * 1024 * 1024;

[[noreturn]] void Unknown(const char* message) {
    NotImplemented_nid_no_patch(message);
    std::terminate();
}

void Require(bool condition, const char* message) {
    if (!condition) Unknown(message);
}

std::string Lower(std::string text) {
    for (auto& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return text;
}

std::uint64_t Number(std::string_view text) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    Require(!text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size(),
        "sceHttp2: invalid number; console error mapping is unknown");
    return value;
}

struct Url {
    std::string host, authority, path;
    std::uint16_t port = 443;
};

Url Parse(std::string_view text) {
    Require(text.size() >= 8 && Lower(std::string(text.substr(0, 8))) == "https://",
        "sceHttp2: only HTTPS HTTP/2 transport is implemented");
    for (const unsigned char c : text) Require(c > 0x20 && c != 0x7f, "sceHttp2: invalid URL character");
    text.remove_prefix(8);
    const auto end = text.find_first_of("/?#");
    const auto authority = text.substr(0, end);
    Require(!authority.empty() && authority.find('@') == std::string_view::npos, "sceHttp2: empty authority or URL credentials");
    Url url;
    url.authority = authority;
    auto host = authority;
    std::string_view port;
    if (host.starts_with('[')) {
        const auto closing = host.find(']');
        Require(closing != std::string_view::npos, "sceHttp2: invalid IPv6 authority");
        if (closing + 1 != host.size()) {
            Require(host[closing + 1] == ':', "sceHttp2: invalid authority suffix");
            port = host.substr(closing + 2);
            Require(!port.empty(), "sceHttp2: empty URL port");
        }
        host = host.substr(1, closing - 1);
    } else if (const auto colon = host.find(':'); colon != std::string_view::npos) {
        port = host.substr(colon + 1);
        Require(!port.empty() && port.find(':') == std::string_view::npos, "sceHttp2: invalid URL port");
        host = host.substr(0, colon);
    }
    Require(!host.empty(), "sceHttp2: empty host");
    url.host = host;
    if (!port.empty()) {
        const auto value = Number(port);
        Require(value > 0 && value <= 65535, "sceHttp2: invalid URL port");
        url.port = static_cast<std::uint16_t>(value);
    }
    text = end == std::string_view::npos ? std::string_view{} : text.substr(end);
    text = text.substr(0, text.find('#'));
    url.path = text.starts_with('/') ? std::string(text) : "/" + std::string(text);
    return url;
}

struct Address {
    std::array<std::uint8_t, 28> bytes{};
    std::uint32_t size = 0;
};

struct Lookup {
    std::string host;
    std::uint16_t port;
    std::mutex mutex;
    std::condition_variable changed;
    bool complete = false;
    std::vector<Address> addresses;
    std::exception_ptr failure;
};

class Resolver {
public:
    Resolver() {
        try {
            for (auto& thread : workers) thread = std::thread([this] { Work(); });
        } catch (...) { Stop(); throw; }
    }
    ~Resolver() { Stop(); }
    std::shared_ptr<Lookup> Submit(const Url& url) {
        auto job = std::make_shared<Lookup>();
        job->host = url.host;
        job->port = url.port;
        {
            std::lock_guard lock(mutex);
            Require(queue.size() < 8, "sceHttp2: host DNS worker queue exhausted");
            queue.push_back(job);
        }
        changed.notify_one();
        return job;
    }
private:
    void Stop() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_all();
        for (auto& thread : workers) if (thread.joinable()) thread.join();
    }
    void Work() {
        for (;;) {
            std::shared_ptr<Lookup> job;
            {
                std::unique_lock lock(mutex);
                changed.wait(lock, [&] { return stopping || !queue.empty(); });
                if (stopping) return;
                job = std::move(queue.front());
                queue.pop_front();
            }
            try {
                addrinfo hints{};
                hints.ai_socktype = SOCK_STREAM;
                hints.ai_family = AF_UNSPEC;
                addrinfo* raw = nullptr;
                const auto service = std::to_string(job->port);
                if (const auto hook = LookupHookSlot().load(std::memory_order_acquire)) Require(hook(job->host.c_str()) == 0, "sceHttp2: DNS failed; console error mapping is unknown");
                const int result = getaddrinfo(job->host.c_str(), service.c_str(), &hints, &raw);
                std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(raw, freeaddrinfo);
                Require(result == 0, "sceHttp2: DNS failed; console error mapping is unknown");
                for (auto* entry = raw; entry; entry = entry->ai_next) {
                    Address address;
                    if (entry->ai_family == AF_INET) {
                        address.size = 16;
                        address.bytes[1] = 2;
                        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(entry->ai_addr);
                        std::memcpy(address.bytes.data() + 2, &ipv4->sin_port, 2);
                        std::memcpy(address.bytes.data() + 4, &ipv4->sin_addr, 4);
                    } else if (entry->ai_family == AF_INET6) {
                        address.size = 28;
                        address.bytes[1] = 28;
                        const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(entry->ai_addr);
                        std::memcpy(address.bytes.data() + 2, &ipv6->sin6_port, 2);
                        std::memcpy(address.bytes.data() + 4, &ipv6->sin6_flowinfo, 4);
                        std::memcpy(address.bytes.data() + 8, &ipv6->sin6_addr, 16);
                        std::memcpy(address.bytes.data() + 24, &ipv6->sin6_scope_id, 4);
                    } else continue;
                    address.bytes[0] = static_cast<std::uint8_t>(address.size);
                    job->addresses.push_back(address);
                }
                Require(!job->addresses.empty(), "sceHttp2: DNS returned no supported address");
            } catch (...) {
                job->failure = std::current_exception();
            }
            {
                std::lock_guard lock(job->mutex);
                job->complete = true;
            }
            job->changed.notify_all();
        }
    }
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::shared_ptr<Lookup>> queue;
    bool stopping = false;
    std::array<std::thread, 2> workers;
};

Resolver& Dns() {
    static Resolver resolver;
    return resolver;
}

void CheckNg(int result) {
    if (result == NGHTTP2_ERR_NOMEM) throw std::bad_alloc();
    Require(result >= 0, "sceHttp2: HTTP/2 protocol failure; console error mapping is unknown");
}

}

int AddHeader(Settings& settings, const char* name, const char* value, std::uint32_t mode) {
    if (!name || !value) {
        if (name && mode == 0) {
            const auto key = Lower(name);
            std::erase_if(settings.headers, [&](const auto& header) { return header.first == key; });
        }
        return InvalidValue;
    }
    Require(mode <= 1, "sceHttp2AddRequestHeader: unsupported arguments");
    auto key = Lower(name);
    Require(!key.empty() && nghttp2_check_header_name(reinterpret_cast<const std::uint8_t*>(key.data()), key.size()),
        "sceHttp2AddRequestHeader: invalid header name");
    Require(nghttp2_check_header_value_rfc9113(reinterpret_cast<const std::uint8_t*>(value), std::strlen(value)),
        "sceHttp2AddRequestHeader: invalid header value");
    Require(key != "connection" && key != "proxy-connection" && key != "keep-alive" && key != "upgrade" &&
        key != "transfer-encoding" && key != "host" && key != "content-length" &&
        (key != "te" || std::string_view(value) == "trailers"), "sceHttp2AddRequestHeader: unsupported HTTP/2 control header");
    auto updated = settings.headers;
    if (mode == 0) std::erase_if(updated, [&](const auto& header) { return header.first == key; });
    updated.emplace_back(std::move(key), value);
    settings.headers = std::move(updated);
    return 0;
}

struct Request::State {
    int sslContext;
    const std::shared_ptr<Capacity> capacity;
    bool acquired = false;
    const Settings settings;
    const std::string method;
    const Url url;
    std::mutex mutex;
    std::atomic<bool> cancelled{false};
    bool started = false;
    bool ready = false;
    bool ended = false;
    int status = 0;
    std::string contentEncoding;
    bool inflatedGzip = false;
    int blockStatus = 0;
    int networkResult = 0;
    int socket = -1;
    int ssl = -1;
    std::uintptr_t native = 0;
    std::shared_ptr<void> socketOwner;
    nghttp2_session* session = nullptr;
    std::int32_t stream = -1;
    std::uint64_t contentLength = 0;
    bool hasLength = false;
    std::size_t headerBytes = 0;
    std::vector<std::uint8_t> body;
    std::size_t bodyOffset = 0;
    const std::uint8_t* upload = nullptr;
    std::size_t uploadSize = 0;
    std::size_t uploadOffset = 0;
    std::exception_ptr failure;
    std::exception_ptr callbackFailure;
    Clock::time_point totalDeadline;

    State(int context, std::shared_ptr<Capacity> capacity, Settings options, std::string verb, std::string target)
        : sslContext(context), capacity(std::move(capacity)), settings(std::move(options)), method(std::move(verb)), url(Parse(target)) {
        Require(!method.empty() && nghttp2_check_method(reinterpret_cast<const std::uint8_t*>(method.data()), method.size()),
            "sceHttp2CreateRequestWithURL: invalid method");
        Require(method != "CONNECT", "sceHttp2: CONNECT is not implemented");
        Require(nghttp2_check_header_value_rfc9113(reinterpret_cast<const std::uint8_t*>(settings.userAgent.data()), settings.userAgent.size()),
            "sceHttp2: invalid user agent");
    }

    ~State() { Close(); }

    void Close() {
        if (session) nghttp2_session_del(std::exchange(session, nullptr));
        if (ssl >= 0) sceSslDeleteConnection(std::exchange(ssl, -1));
        if (socket >= 0) sceNetSocketClose(std::exchange(socket, -1));
        socketOwner.reset();
        if (std::exchange(acquired, false)) capacity->active.fetch_sub(1);
    }

    Clock::time_point Deadline(std::uint32_t duration) const {
        Require(duration != 0, "sceHttp2: zero timeout semantics are unknown");
        return std::min(totalDeadline, Clock::now() + std::chrono::microseconds(duration));
    }

    void Check(Clock::time_point deadline) const {
        Require(!cancelled.load(), "sceHttp2: request cancelled; console error mapping is unknown");
        Require(Clock::now() < deadline, "sceHttp2: request timed out; console error mapping is unknown");
    }

    void Wait(bool write, Clock::time_point deadline) {
        for (;;) {
            Check(deadline);
#ifdef _WIN32
            WSAPOLLFD entry{static_cast<SOCKET>(native), static_cast<short>(write ? POLLWRNORM : POLLRDNORM), 0};
            const auto result = WSAPoll(&entry, 1, 5);
#else
            pollfd entry{static_cast<int>(native), static_cast<short>(write ? POLLOUT : POLLIN), 0};
            const auto result = poll(&entry, 1, 5);
#endif
            Require(result >= 0, "sceHttp2: host poll failed");
            if (result > 0) { Check(deadline); return; }
        }
    }

    bool Inflating() const { return settings.inflateGzip && contentEncoding == "gzip"; }

    void InflateGzip() {
        if (inflatedGzip || !Inflating() || body.empty()) return;
        inflatedGzip = true;
        const std::uint8_t* data = body.data();
        const std::size_t size = body.size();
        Require(size >= 18 && data[0] == 0x1f && data[1] == 0x8b && data[2] == 8, "sceHttp2: malformed gzip header");
        const std::uint8_t flags = data[3];
        Require((flags & 0xe0) == 0, "sceHttp2: unknown gzip header flags");
        std::size_t offset = 10;
        const auto skip = [&](std::size_t count) { Require(offset + count <= size - 8, "sceHttp2: truncated gzip header"); offset += count; };
        if (flags & 4) {
            Require(offset + 2 <= size - 8, "sceHttp2: truncated gzip extra field");
            const std::size_t extra = static_cast<std::size_t>(data[offset]) | (static_cast<std::size_t>(data[offset + 1]) << 8);
            skip(2 + extra);
        }
        if (flags & 8) while (skip(1), data[offset - 1] != 0) {}
        if (flags & 16) while (skip(1), data[offset - 1] != 0) {}
        if (flags & 2) skip(2);
        z_stream inflater{};
        Require(inflateInit2(&inflater, -15) == Z_OK, "sceHttp2: gzip inflate initialization failed");
        struct End {
            z_stream* stream;
            ~End() { inflateEnd(stream); }
        } end{&inflater};
        inflater.next_in = const_cast<std::uint8_t*>(data + offset);
        inflater.avail_in = static_cast<uInt>(size - offset - 8);
        std::vector<std::uint8_t> output;
        std::uint8_t chunk[16384];
        int result = Z_OK;
        while (result != Z_STREAM_END) {
            inflater.next_out = chunk;
            inflater.avail_out = sizeof(chunk);
            result = inflate(&inflater, Z_NO_FLUSH);
            Require(result == Z_OK || result == Z_STREAM_END, "sceHttp2: gzip inflate failed");
            output.insert(output.end(), chunk, chunk + (sizeof(chunk) - inflater.avail_out));
            Require(result == Z_STREAM_END || inflater.avail_in != 0 || inflater.avail_out == 0, "sceHttp2: truncated gzip stream");
            Require(output.size() <= MaxGzipBytes, "sceHttp2: inflated response exceeds the host limit");
        }
        Require(inflater.avail_in == 0, "sceHttp2: concatenated gzip members are not implemented");
        const auto readLe32 = [&](std::size_t position) {
            return static_cast<std::uint32_t>(data[position]) | (static_cast<std::uint32_t>(data[position + 1]) << 8) |
                (static_cast<std::uint32_t>(data[position + 2]) << 16) | (static_cast<std::uint32_t>(data[position + 3]) << 24);
        };
        Require(crc32(0, output.data(), static_cast<uInt>(output.size())) == readLe32(size - 8), "sceHttp2: gzip checksum mismatch");
        Require(static_cast<std::uint32_t>(output.size()) == readLe32(size - 4), "sceHttp2: gzip size mismatch");
        body = std::move(output);
        bodyOffset = 0;
        contentLength = body.size();
        hasLength = true;
        contentEncoding.clear();
    }

    int Connect() {
#ifdef _WIN32
        const int startup = sceNetSocket("http2-resolver", 2, 1, 0);
        Require(startup >= 0, "sceHttp2: network initialization failed; console error mapping is unknown");
        Require(sceNetSocketClose(startup) == 0, "sceHttp2: network initialization socket close failed");
#endif
        std::shared_ptr<Lookup> job;
        for (std::int32_t attempt = 0;; ++attempt) {
            const auto deadline = Deadline(settings.resolveTimeout);
            job = Dns().Submit(url);
            std::unique_lock lock(job->mutex);
            while (!job->complete) {
                Check(deadline);
                job->changed.wait_for(lock, std::chrono::milliseconds(5));
            }
            Check(deadline);
            if (!job->failure) break;
            if (attempt >= settings.resolveRetry) std::rethrow_exception(job->failure);
        }
        const auto connectDeadline = Deadline(settings.connectTimeout);
        int result = 0;
        for (const auto& address : job->addresses) {
            Check(connectDeadline);
            socket = sceNetSocket("http2", address.bytes[1], 1, 0);
            Require(socket >= 0, "sceHttp2: socket creation failed; console error mapping is unknown");
            int enabled = 1;
            Require(sceNetSetsockopt(socket, 0xffff, 0x1200, &enabled, sizeof(enabled)) == 0,
                "sceHttp2: cannot set nonblocking socket");
            Require(KernelLookupSocketNative_nid_no_patch(socket, &socketOwner, &native), "sceHttp2: native socket lookup failed");
            result = sceNetConnect(socket, address.bytes.data(), address.size);
            if (result < 0 && *sceNetErrnoLoc() == 36) {
                Wait(true, connectDeadline);
                int error = 0;
                std::uint32_t size = sizeof(error);
                result = sceNetGetsockopt(socket, 0xffff, 0x1007, &error, &size);
                Require(result == 0, "sceHttp2: connect status unavailable; console error mapping is unknown");
                if (result == 0 && error != 0) result = static_cast<int>(0x80410100u | static_cast<unsigned>(error));
            }
            if (result == 0) break;
            sceNetSocketClose(std::exchange(socket, -1));
            socketOwner.reset();
        }
        if (result < 0) return result;
        ssl = sceSslCreateConnection(sslContext, socket, url.host.c_str());
        Require(ssl > 0, "sceHttp2: SSL context or connection unavailable");
        Require(SslTrustSystemRoots_nid_no_patch(ssl) == 0, "sceHttp2: system CA list unavailable");
        Require(sceSslDisableVerifyOption(ssl, 0x3d) == 0 && sceSslEnableVerifyOption(ssl, settings.verifyFlags) == 0,
            "sceHttp2: SSL verification options rejected");
        Require(sceSslSetAlpn(ssl, "h2") == 0, "sceHttp2: ALPN setup failed");
        for (;;) {
            Check(connectDeadline);
            result = sceSslConnect(ssl);
            if (result == WantRead || result == WantWrite) Wait(result == WantWrite, connectDeadline);
            else break;
        }
        Require(result == 0, "sceHttp2: TLS failed; console error mapping is unknown");
        const char* selected = sceSslGetAlpnSelected(ssl);
        Require(selected && std::string_view(selected) == "h2", "sceHttp2: server did not negotiate h2; fallback contract is unknown");
        return 0;
    }

    template<class TCallback>
    static int Callback(State& state, TCallback callback) noexcept {
        try { callback(); return 0; }
        catch (...) { state.callbackFailure = std::current_exception(); return NGHTTP2_ERR_CALLBACK_FAILURE; }
    }

    static int BeginHeaders(nghttp2_session*, const nghttp2_frame* frame, void* data) {
        auto& state = *static_cast<State*>(data);
        return Callback(state, [&] {
            if (frame->hd.stream_id == state.stream) {
                Require(!state.ready, "sceHttp2: response trailers are not implemented");
                state.blockStatus = 0;
                state.hasLength = false;
                state.contentLength = 0;
            }
        });
    }

    static int Header(nghttp2_session*, const nghttp2_frame* frame, const std::uint8_t* name, std::size_t nameSize,
        const std::uint8_t* value, std::size_t valueSize, std::uint8_t, void* data) {
        auto& state = *static_cast<State*>(data);
        return Callback(state, [&] {
            if (frame->hd.stream_id != state.stream) return;
            Require(nameSize <= 65536 && valueSize <= 65536 && nameSize + valueSize <= 65536 - state.headerBytes,
                "sceHttp2: response headers exceed host limit");
            state.headerBytes += nameSize + valueSize;
            const std::string_view key(reinterpret_cast<const char*>(name), nameSize);
            const std::string_view text(reinterpret_cast<const char*>(value), valueSize);
            if (key == ":status") state.blockStatus = static_cast<int>(Number(text));
            if (key == "content-length") {
                state.contentLength = Number(text);
                state.hasLength = true;
            }
            if (key == "content-encoding") state.contentEncoding = Lower(std::string(text));
        });
    }

    static int Frame(nghttp2_session*, const nghttp2_frame* frame, void* data) {
        auto& state = *static_cast<State*>(data);
        return Callback(state, [&] {
            if (frame->hd.type == NGHTTP2_GOAWAY)
                Require(frame->goaway.error_code == NGHTTP2_NO_ERROR && frame->goaway.last_stream_id >= state.stream,
                    "sceHttp2: GOAWAY rejected request");
            if (frame->hd.stream_id != state.stream) return;
            if (frame->hd.type == NGHTTP2_HEADERS && state.blockStatus >= 200) {
                state.status = state.blockStatus;
                Require(!state.settings.redirect || (state.status != 301 && state.status != 302 && state.status != 303 && state.status != 307 && state.status != 308),
                    "sceHttp2: automatic redirects are not implemented");
                Require(!state.settings.auth || (state.status != 401 && state.status != 407), "sceHttp2: authentication is not implemented");
                state.ready = true;
            }
            if ((frame->hd.type == NGHTTP2_HEADERS || frame->hd.type == NGHTTP2_DATA) &&
                (frame->hd.flags & NGHTTP2_FLAG_END_STREAM)) {
                Require(state.ready, "sceHttp2: response ended without final headers");
                state.ended = true;
                state.InflateGzip();
            }
        });
    }

    static int Data(nghttp2_session* session, std::uint8_t, std::int32_t streamId, const std::uint8_t* bytes, std::size_t size, void* data) {
        auto& state = *static_cast<State*>(data);
        return Callback(state, [&] {
            if (streamId != state.stream) return;
            if (state.Inflating()) {
                Require(state.body.size() + size <= MaxGzipBytes, "sceHttp2: encoded gzip response exceeds the host limit");
                CheckNg(nghttp2_session_consume(session, streamId, size));
            } else {
                Require(state.body.size() + size <= 65535, "sceHttp2: receive flow-control limit exceeded");
            }
            state.body.insert(state.body.end(), bytes, bytes + size);
        });
    }

    static int End(nghttp2_session*, std::int32_t streamId, std::uint32_t error, void* data) {
        auto& state = *static_cast<State*>(data);
        return Callback(state, [&] {
            if (streamId != state.stream) return;
            Require(error == NGHTTP2_NO_ERROR && state.ended, "sceHttp2: stream failed or response is incomplete");
            state.ended = true;
        });
    }

    static nghttp2_ssize Upload(nghttp2_session*, std::int32_t, std::uint8_t* buffer, std::size_t size,
        std::uint32_t* flags, nghttp2_data_source*, void* data) {
        auto& state = *static_cast<State*>(data);
        if (state.ready) return NGHTTP2_ERR_DEFERRED;
        const auto count = std::min(size, state.uploadSize - state.uploadOffset);
        if (count) std::memcpy(buffer, state.upload + state.uploadOffset, count);
        state.uploadOffset += count;
        if (state.uploadOffset == state.uploadSize) *flags |= NGHTTP2_DATA_FLAG_EOF;
        return static_cast<nghttp2_ssize>(count);
    }

    void Setup(const void* data, std::size_t size) {
        nghttp2_session_callbacks* raw = nullptr;
        CheckNg(nghttp2_session_callbacks_new(&raw));
        std::unique_ptr<nghttp2_session_callbacks, decltype(&nghttp2_session_callbacks_del)> callbacks(raw, nghttp2_session_callbacks_del);
        nghttp2_session_callbacks_set_on_begin_headers_callback(raw, BeginHeaders);
        nghttp2_session_callbacks_set_on_header_callback(raw, Header);
        nghttp2_session_callbacks_set_on_frame_recv_callback(raw, Frame);
        nghttp2_session_callbacks_set_on_data_chunk_recv_callback(raw, Data);
        nghttp2_session_callbacks_set_on_stream_close_callback(raw, End);
        nghttp2_option* optionsRaw = nullptr;
        CheckNg(nghttp2_option_new(&optionsRaw));
        std::unique_ptr<nghttp2_option, decltype(&nghttp2_option_del)> options(optionsRaw, nghttp2_option_del);
        nghttp2_option_set_no_auto_window_update(optionsRaw, 1);
        CheckNg(nghttp2_session_client_new2(&session, raw, this, optionsRaw));
        const nghttp2_settings_entry settingsEntry{NGHTTP2_SETTINGS_ENABLE_PUSH, 0};
        CheckNg(nghttp2_submit_settings(session, NGHTTP2_FLAG_NONE, &settingsEntry, 1));
        std::vector<std::pair<std::string, std::string>> fields{
            {":method", method}, {":scheme", "https"}, {":authority", url.authority}, {":path", url.path}};
        if (!settings.userAgent.empty() && std::none_of(settings.headers.begin(), settings.headers.end(), [](const auto& header) { return header.first == "user-agent"; }))
            fields.emplace_back("user-agent", settings.userAgent);
        fields.insert(fields.end(), settings.headers.begin(), settings.headers.end());
        if (!settings.noContentLength) fields.emplace_back("content-length", std::to_string(settings.contentLength));
        std::vector<nghttp2_nv> headers;
        for (auto& [name, value] : fields) headers.push_back({reinterpret_cast<std::uint8_t*>(name.data()),
            reinterpret_cast<std::uint8_t*>(value.data()), name.size(), value.size(), NGHTTP2_NV_FLAG_NONE});
        upload = static_cast<const std::uint8_t*>(data);
        uploadSize = size;
        nghttp2_data_provider2 provider{};
        provider.read_callback = Upload;
        stream = nghttp2_submit_request2(session, nullptr, headers.data(), headers.size(), size ? &provider : nullptr, nullptr);
        CheckNg(stream);
    }

    void Flush(Clock::time_point deadline) {
        for (;;) {
            const std::uint8_t* bytes = nullptr;
            const auto size = nghttp2_session_mem_send2(session, &bytes);
            CheckNg(static_cast<int>(size));
            if (!size) return;
            std::size_t offset = 0;
            while (offset < static_cast<std::size_t>(size)) {
                Check(deadline);
                const int result = sceSslWrite(ssl, const_cast<std::uint8_t*>(bytes + offset), static_cast<std::size_t>(size) - offset, 0);
                if (result == WantRead || result == WantWrite) { Wait(result == WantWrite, deadline); continue; }
                Require(result > 0, "sceHttp2: TLS write failed; console error mapping is unknown");
                offset += static_cast<std::size_t>(result);
            }
        }
    }

    void Pump(Clock::time_point deadline) {
        Flush(std::min(deadline, Deadline(settings.sendTimeout)));
        Receive(deadline);
    }

    void Receive(Clock::time_point deadline) {
        std::array<std::uint8_t, 16384> buffer;
        for (;;) {
            Check(deadline);
            const int size = SslRead_nid_no_patch(ssl, buffer.data(), buffer.size(), 0);
            if (size == WantRead || size == WantWrite) { Wait(size == WantWrite, deadline); continue; }
            Require(size > 0, "sceHttp2: TLS read failed or response truncated; console error mapping is unknown");
            const auto consumed = nghttp2_session_mem_recv2(session, buffer.data(), static_cast<std::size_t>(size));
            if (callbackFailure) std::rethrow_exception(callbackFailure);
            CheckNg(static_cast<int>(consumed));
            Require(consumed == size, "sceHttp2: incomplete HTTP/2 parser consumption");
            return;
        }
    }

    std::unique_lock<std::mutex> Lock() {
        std::unique_lock lock(mutex, std::try_to_lock);
        Require(lock.owns_lock(), "sceHttp2: concurrent operation on one request is not implemented");
        if (failure) std::rethrow_exception(failure);
        Require(!cancelled.load(), "sceHttp2: request cancelled; console error mapping is unknown");
        return lock;
    }

    void Fail() {
        failure = std::current_exception();
        upload = nullptr;
        Close();
    }
};

Request::Request(int context, std::shared_ptr<Capacity> capacity, Settings settings, std::string method, std::string url)
    : state(std::make_unique<State>(context, std::move(capacity), std::move(settings), std::move(method), std::move(url))) {}
Request::~Request() = default;

int Request::Send(const void* data, std::size_t size) {
    auto lock = state->Lock();
    Require(!state->started, "sceHttp2SendRequest: request reuse is not implemented");
    Require(data || size == 0, "sceHttp2SendRequest: null body");
    Require(state->settings.noContentLength || size == state->settings.contentLength, "sceHttp2SendRequest: body length mismatch");
    state->started = true;
    state->totalDeadline = Clock::now() + std::chrono::microseconds(state->settings.timeout);
    try {
        const int active = state->capacity->active.fetch_add(1);
        state->acquired = true;
        Require(active < state->capacity->maximum, "sceHttp2: connection slot waiting is not implemented");
        state->networkResult = state->Connect();
        if (state->networkResult < 0) { state->Close(); return state->networkResult; }
        state->Setup(data, size);
        const auto sendDeadline = state->Deadline(state->settings.sendTimeout);
        auto receiveDeadline = sendDeadline;
        bool uploaded = false;
        for (;;) {
            state->Flush(uploaded ? state->Deadline(state->settings.sendTimeout) : sendDeadline);
            if (!uploaded && state->uploadOffset == size) {
                uploaded = true;
                receiveDeadline = state->Deadline(state->settings.recvTimeout);
            }
            if (state->ready) break;
            state->Receive(uploaded ? receiveDeadline : sendDeadline);
            if (state->ready) break;
        }
        state->upload = nullptr;
        if (state->ended && state->body.empty()) state->Close();
        return 0;
    } catch (...) { state->Fail(); throw; }
}

int Request::Read(void* data, std::size_t size) {
    auto lock = state->Lock();
    if (state->networkResult) return state->networkResult;
    Require(state->ready, "sceHttp2ReadData: response unavailable");
    Require(data || size == 0, "sceHttp2ReadData: null buffer");
    if (!size) return 0;
    if (state->ended && state->body.empty()) return 0;
    try {
        const auto deadline = state->Deadline(state->settings.recvTimeout);
        state->Check(deadline);
        while (!state->ended && (state->bodyOffset == state->body.size() || state->Inflating())) state->Pump(deadline);
        const auto count = std::min({size, state->body.size() - state->bodyOffset, static_cast<std::size_t>(std::numeric_limits<int>::max())});
        state->Check(deadline);
        if (count) std::memcpy(data, state->body.data() + state->bodyOffset, count);
        state->bodyOffset += count;
        if (state->bodyOffset == state->body.size()) { state->body.clear(); state->bodyOffset = 0; }
        if (count && !state->ended) CheckNg(nghttp2_session_consume(state->session, state->stream, count));
        if (state->ended && state->body.empty()) state->Close();
        return static_cast<int>(count);
    } catch (...) { state->Fail(); throw; }
}

int Request::Status(int* status) {
    auto lock = state->Lock();
    if (state->networkResult) return state->networkResult;
    Require(status && state->ready, "sceHttp2GetStatusCode: null output or response unavailable");
    *status = state->status;
    return 0;
}

int Request::ContentLength(int* kind, std::uint64_t* length) {
    auto lock = state->Lock();
    if (state->networkResult) return state->networkResult;
    Require(kind && length && state->ready, "sceHttp2GetResponseContentLength: null output or response unavailable");
    if (state->Inflating()) {
        try {
            const auto deadline = state->Deadline(state->settings.recvTimeout);
            while (!state->ended) state->Pump(deadline);
        } catch (...) { state->Fail(); throw; }
    }
    *kind = state->hasLength ? 0 : 1;
    *length = state->hasLength ? state->contentLength : 0;
    return 0;
}

void Request::Cancel() { state->cancelled.store(true); }

void Request::Stop() {
    Cancel();
    std::lock_guard lock(state->mutex);
    state->Close();
}

std::shared_ptr<Capacity> CreateCapacity_nid_no_patch(int maximum) {
    return std::make_shared<Capacity>(maximum);
}

std::shared_ptr<Request> CreateRequest_nid_no_patch(int sslContext, std::shared_ptr<Capacity> capacity, Settings settings, std::string method, std::string url) {
    return std::make_shared<Request>(sslContext, std::move(capacity), std::move(settings), std::move(method), std::move(url));
}

}
