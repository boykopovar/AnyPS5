#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#endif

#include "Transport.hpp"
#include "Cookies.hpp"
#include "prx/libSceHttp/include/Http.hpp"

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
constexpr int ResponseUnavailable = static_cast<int>(0x817b1065);
constexpr int ConnectionEnded = static_cast<int>(0x817b1064);
constexpr int AlreadySent = static_cast<int>(0x817b1066);
constexpr int TimedOut = static_cast<int>(0x817b1068);
constexpr int Aborted = static_cast<int>(0x817b1080);
constexpr int ConnectTimedOut = static_cast<int>(0x817b1221);
constexpr int ReceiveTimedOut = static_cast<int>(0x817b1222);
constexpr int SendTimedOut = static_cast<int>(0x817b1223);
constexpr int ConnectionReset = static_cast<int>(0x80410136);
constexpr int GoawayReceived = static_cast<int>(0x817b120a);
constexpr int StreamReset = static_cast<int>(0x817b122a);
constexpr std::uint32_t MeasuredStreamErrors = 13;
constexpr std::array<int, 7> HandshakeFailures{
    static_cast<int>(0x8095f00b), static_cast<int>(0x8095f00c), static_cast<int>(0x8095f00d), static_cast<int>(0x8095f010),
    static_cast<int>(0x8095170d), static_cast<int>(0x80951d78), static_cast<int>(0x80410136)};
constexpr std::size_t MaxGzipBytes = 64 * 1024 * 1024;

[[noreturn]] void Unknown(const char* message) {
    NotImplemented_nid_no_patch(message);
    std::terminate();
}

void Require(bool condition, const char* message) {
    if (!condition) Unknown(message);
}

struct RequestFailure {
    int code;
};

struct Limit {
    Clock::time_point at;
    int code;
};

Limit Earlier(Limit first, Limit second) {
    return second.at < first.at ? second : first;
}

std::string Lower(std::string text) {
    for (auto& c : text) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return text;
}

std::uint64_t Number(std::string_view text, int base = 10) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, base);
    Require(!text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size(),
        "sceHttp2: invalid number; console error mapping is unknown");
    return value;
}

std::uint64_t Http1ContentLength(std::string_view text) {
    std::uint64_t length = 0;
    bool first = true;
    for (;;) {
        const auto comma = text.find(',');
        auto digits = text.substr(0, comma);
        while (!digits.empty() && (digits.front() == ' ' || digits.front() == '\t')) digits.remove_prefix(1);
        while (!digits.empty() && (digits.back() == ' ' || digits.back() == '\t')) digits.remove_suffix(1);
        const auto value = Number(digits);
        Require(first || value == length, "sceHttp2: inconsistent HTTP1 content lengths");
        length = value;
        first = false;
        if (comma == std::string_view::npos) return length;
        text.remove_prefix(comma + 1);
    }
}

struct Url {
    std::string host, authority, path;
    std::uint16_t port = 443;
    bool secure = true;
};

Url Parse(std::string_view text) {
    const bool secure = text.size() >= 8 && Lower(std::string(text.substr(0, 8))) == "https://";
    Require(secure || (text.size() >= 7 && Lower(std::string(text.substr(0, 7))) == "http://"), "sceHttp2: unsupported URL scheme");
    for (const unsigned char c : text) Require(c > 0x20 && c != 0x7f, "sceHttp2: invalid URL character");
    text.remove_prefix(secure ? 8 : 7);
    const auto end = text.find_first_of("/?#");
    const auto authority = text.substr(0, end);
    Require(!authority.empty() && authority.find('@') == std::string_view::npos, "sceHttp2: empty authority or URL credentials");
    Url url;
    url.secure = secure;
    url.port = secure ? 443 : 80;
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

std::string NormalizePath(std::string_view path) {
    std::string result;
    while (!path.empty()) {
        if (path.starts_with("../")) path.remove_prefix(3);
        else if (path.starts_with("./")) path.remove_prefix(2);
        else if (path.starts_with("/./")) path.remove_prefix(2);
        else if (path == "/.") path = "/";
        else if (path.starts_with("/../") || path == "/..") {
            path = path == "/.." ? "/" : path.substr(3);
            const auto slash = result.rfind('/');
            result.erase(slash == std::string::npos ? 0 : slash);
        } else if (path == "." || path == "..") path = {};
        else {
            const auto slash = path.find('/', path.front() == '/' ? 1 : 0);
            result.append(path.substr(0, slash));
            path = slash == std::string_view::npos ? std::string_view{} : path.substr(slash);
        }
    }
    return result;
}

Url RedirectTarget(const Url& base, std::string_view reference) {
    reference = reference.substr(0, reference.find('#'));
    const auto colon = reference.find(':');
    const auto delimiter = reference.find_first_of("/?");
    Url result;
    if (colon != std::string_view::npos && (delimiter == std::string_view::npos || colon < delimiter)) result = Parse(reference);
    else if (reference.starts_with("//")) result = Parse(std::string(base.secure ? "https:" : "http:") + std::string(reference));
    else {
        std::string path;
        if (reference.empty()) path = base.path;
        else if (reference.starts_with('?')) path = base.path.substr(0, base.path.find('?')) + std::string(reference);
        else if (reference.starts_with('/')) path = reference;
        else {
            path = base.path.substr(0, base.path.find('?'));
            path.erase(path.rfind('/') + 1);
            path += reference;
        }
        result = Parse(std::string(base.secure ? "https://" : "http://") + base.authority + path);
    }
    const auto query = result.path.find('?');
    result.path = NormalizePath(std::string_view(result.path).substr(0, query)) +
        (query == std::string::npos ? std::string{} : result.path.substr(query));
    return result;
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
    if (key == "host") Require(value[0] != '\0' && std::string_view(value).find_first_of(" \t") == std::string_view::npos,
        "sceHttp2AddRequestHeader: invalid host header value");
    Require(key != "connection" && key != "proxy-connection" && key != "keep-alive" && key != "upgrade" &&
        key != "transfer-encoding" && key != "content-length" &&
        (key != "te" || std::string_view(value) == "trailers"), "sceHttp2AddRequestHeader: unsupported HTTP/2 control header");
    auto updated = settings.headers;
    if (mode == 0) std::erase_if(updated, [&](const auto& header) { return header.first == key; });
    updated.emplace_back(std::move(key), value);
    settings.headers = std::move(updated);
    return 0;
}

struct Request::State {
    const int id;
    Url url;
    const int version;
    int sslContext;
    const std::shared_ptr<Capacity> capacity;
    bool acquired = false;
    Settings settings;
    std::string method;
    std::mutex mutex;
    std::atomic<bool> cancelled{false};
    std::atomic<bool> stopped{false};
    std::atomic<bool> deleted{false};
    int streamFailure = 0;
    bool started = false;
    bool callbackActive = false;
    std::unique_lock<std::mutex>* callbackLock = nullptr;
    bool ready = false;
    bool ended = false;
    int status = 0;
    std::string contentEncoding;
    bool inflatedGzip = false;
    std::vector<std::pair<std::string, std::string>> blockHeaders;
    std::string allHeaders;
    std::string trailerHeaders;
    bool trailerBlock = false;
    int blockStatus = 0;
    bool hasAuthChallenge = false;
    bool hasProxyAuthChallenge = false;
    std::string location;
    bool hasLocation = false;
    int networkResult = 0;
    int readResult = 0;
    int socket = -1;
    int ssl = -1;
    std::uintptr_t native = 0;
    std::shared_ptr<void> socketOwner;
    nghttp2_session* session = nullptr;
    std::int32_t stream = -1;
    std::uint64_t contentLength = 0;
    bool hasLength = false;
    bool chunked = false;
    bool chunkTerminator = false;
    std::uint64_t remaining = 0, chunkRemaining = 0;
    std::vector<std::uint8_t> pending;
    std::size_t pendingOffset = 0;
    std::string rawHeaders;
    unsigned http1HeaderBlocks = 0;
    std::size_t headerBytes = 0;
    std::vector<std::uint8_t> body;
    std::size_t bodyOffset = 0;
    const std::uint8_t* upload = nullptr;
    std::size_t uploadSize = 0;
    std::size_t uploadOffset = 0;
    std::exception_ptr failure;
    std::exception_ptr callbackFailure;
    Clock::time_point totalDeadline;

    State(int id, int version, int context, std::shared_ptr<Capacity> capacity, Settings options, std::string verb, std::string target)
        : id(id), url(Parse(target)), version(version == 3 && !url.secure ? 2 : version), sslContext(context), capacity(std::move(capacity)), settings(std::move(options)), method(std::move(verb)) {
        Require(!method.empty() && nghttp2_check_method(reinterpret_cast<const std::uint8_t*>(method.data()), method.size()),
            "sceHttp2CreateRequestWithURL: invalid method");
        Require(version >= 1 && version <= 3, "sceHttp2: unsupported version");
        Require(method != "CONNECT", "sceHttp2: CONNECT is not implemented");
        Require(nghttp2_check_header_value_rfc9113(reinterpret_cast<const std::uint8_t*>(settings.userAgent.data()), settings.userAgent.size()),
            "sceHttp2: invalid user agent");
    }

    ~State() { Close(); }

    std::string Address() const { return std::string(url.secure ? "https://" : "http://") + url.authority + url.path; }

    void Close() {
        callbackLock = nullptr;
        if (session) nghttp2_session_del(std::exchange(session, nullptr));
        if (ssl >= 0) sceSslDeleteConnection(std::exchange(ssl, -1));
        if (socket >= 0) sceNetSocketClose(std::exchange(socket, -1));
        socketOwner.reset();
        if (std::exchange(acquired, false)) capacity->active.fetch_sub(1);
    }

    Limit Deadline(std::uint32_t duration, int code) const {
        Require(duration != 0, "sceHttp2: zero timeout semantics are unknown");
        const auto phase = Clock::now() + std::chrono::microseconds(duration);
        return phase < totalDeadline ? Limit{phase, code} : Total();
    }

    Limit Total() const { return {totalDeadline, TimedOut}; }

    [[noreturn]] void Raise(int code, const char* message) const {
        Require(code != 0, message);
        throw RequestFailure{code};
    }

    template<class TCallback>
    int InvokeCallback(std::unique_lock<std::mutex>& lock, TCallback callback) {
        callbackActive = true;
        lock.unlock();
        try {
            const int result = callback();
            lock.lock();
            callbackActive = false;
            Require(!deleted.load(), "sceHttp2: request deleted from its own callback; console error mapping is unknown");
            Check(Total());
            return result;
        } catch (...) {
            if (!lock.owns_lock()) lock.lock();
            callbackActive = false;
            throw;
        }
    }

    static int APS5_VABI Verify_nid_no_patch(int context, unsigned int errors, void** certificates, int count, void* argument) {
        auto& state = *static_cast<State*>(argument);
        Require(state.callbackLock && state.settings.sslCallback, "sceHttp2: unexpected SSL callback phase");
        const int decision = state.InvokeCallback(*state.callbackLock, [&] {
            return state.settings.sslCallback(context, errors, certificates, count, state.settings.sslArgument);
        });
        return decision == 0 ? 0 : -1;
    }

    void Check(Limit deadline) const {
        Require(!stopped.load(), "sceHttp2: context terminated during an operation; console error mapping is unknown");
        if (cancelled.load()) Raise(Aborted, "sceHttp2: request cancelled; console error mapping is unknown");
        if (Clock::now() >= deadline.at) Raise(deadline.code, "sceHttp2: request timed out; console error mapping is unknown");
    }

    void Wait(bool write, Limit deadline, bool both = false) {
        for (;;) {
            Check(deadline);
            int result;
            if (const auto poll = PollHookSlot().load(std::memory_order_acquire)) result = poll(native, write, both);
            else {
#ifdef _WIN32
                WSAPOLLFD entry{static_cast<SOCKET>(native), static_cast<short>(both ? POLLWRNORM | POLLRDNORM : write ? POLLWRNORM : POLLRDNORM), 0};
                result = WSAPoll(&entry, 1, 5);
#else
                pollfd entry{static_cast<int>(native), static_cast<short>(both ? POLLOUT | POLLIN : write ? POLLOUT : POLLIN), 0};
                result = ::poll(&entry, 1, 5);
#endif
            }
            Require(result >= 0, "sceHttp2: host poll failed");
            if (result > 0) { Check(deadline); return; }
        }
    }

    bool Inflating() const { return settings.inflateGzip && contentEncoding == "gzip"; }

    void InflateGzip() {
        if (inflatedGzip || !Inflating() || body.empty()) return;
        inflatedGzip = true;
        Require(body.size() <= MaxGzipBytes, "sceHttp2: encoded gzip response exceeds the host limit");
        const auto* data = body.data();
        const auto size = body.size();
        z_stream inflater{};
        Require(inflateInit2(&inflater, -15) == Z_OK, "sceHttp2: gzip inflate initialization failed");
        struct End {
            z_stream* stream;
            ~End() { inflateEnd(stream); }
        } end{&inflater};
        std::vector<std::uint8_t> output;
        std::size_t offset = 0;
        const auto readLe32 = [&](std::size_t position) {
            return static_cast<std::uint32_t>(data[position]) | (static_cast<std::uint32_t>(data[position + 1]) << 8) |
                (static_cast<std::uint32_t>(data[position + 2]) << 16) | (static_cast<std::uint32_t>(data[position + 3]) << 24);
        };
        while (offset < size) {
            const auto header = offset;
            Require(size - offset >= 18 && data[offset] == 0x1f && data[offset + 1] == 0x8b && data[offset + 2] == 8,
                "sceHttp2: malformed gzip header");
            const auto flags = data[offset + 3];
            Require((flags & 0xe0) == 0, "sceHttp2: unknown gzip header flags");
            offset += 10;
            const auto skip = [&](std::size_t count) {
                Require(offset <= size - 8 && count <= size - 8 - offset, "sceHttp2: truncated gzip header");
                offset += count;
            };
            if (flags & 4) {
                Require(offset + 2 <= size - 8, "sceHttp2: truncated gzip extra field");
                const auto extra = static_cast<std::size_t>(data[offset]) | (static_cast<std::size_t>(data[offset + 1]) << 8);
                skip(2 + extra);
            }
            if (flags & 8) while (skip(1), data[offset - 1] != 0) {}
            if (flags & 16) while (skip(1), data[offset - 1] != 0) {}
            if (flags & 2) {
                Require(offset + 2 <= size - 8, "sceHttp2: truncated gzip header checksum");
                const auto expected = static_cast<std::uint32_t>(data[offset]) | (static_cast<std::uint32_t>(data[offset + 1]) << 8);
                Require((crc32(0, data + header, static_cast<uInt>(offset - header)) & 0xffffu) == expected,
                    "sceHttp2: gzip header checksum mismatch");
                offset += 2;
            }
            inflater.next_in = const_cast<std::uint8_t*>(data + offset);
            inflater.avail_in = static_cast<uInt>(size - offset);
            const auto begin = output.size();
            std::uint8_t chunk[16384];
            int result = Z_OK;
            while (result != Z_STREAM_END) {
                inflater.next_out = chunk;
                inflater.avail_out = sizeof(chunk);
                result = inflate(&inflater, Z_NO_FLUSH);
                Require(result == Z_OK || result == Z_STREAM_END, "sceHttp2: gzip inflate failed");
                const auto count = sizeof(chunk) - inflater.avail_out;
                Require(count <= MaxGzipBytes - output.size(), "sceHttp2: inflated response exceeds the host limit");
                output.insert(output.end(), chunk, chunk + count);
                Require(result == Z_STREAM_END || inflater.avail_in != 0 || inflater.avail_out == 0, "sceHttp2: truncated gzip stream");
            }
            offset = size - inflater.avail_in;
            Require(size - offset >= 8, "sceHttp2: truncated gzip trailer");
            const auto length = output.size() - begin;
            Require(crc32(0, length ? output.data() + begin : nullptr, static_cast<uInt>(length)) == readLe32(offset),
                "sceHttp2: gzip checksum mismatch");
            Require(static_cast<std::uint32_t>(length) == readLe32(offset + 4), "sceHttp2: gzip size mismatch");
            offset += 8;
            if (offset < size) Require(inflateReset(&inflater) == Z_OK, "sceHttp2: gzip member reset failed");
        }
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
            const auto deadline = Deadline(settings.resolveTimeout, 0);
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
        const auto connectDeadline = Deadline(settings.connectTimeout, ConnectTimedOut);
        int result = 0;
        for (const auto& address : job->addresses) {
            Check(connectDeadline);
            socket = sceNetSocket("http2", address.bytes[1], 1, 0);
            Require(socket >= 0, "sceHttp2: socket creation failed; console error mapping is unknown");
            int enabled = 1;
            Require(sceNetSetsockopt(socket, 0xffff, 0x1200, &enabled, sizeof(enabled)) == 0,
                "sceHttp2: cannot set nonblocking socket");
            Require(KernelLookupSocketNative_nid_no_patch(socket, &socketOwner, &native), "sceHttp2: native socket lookup failed");
            const auto connect = ConnectHookSlot().load(std::memory_order_acquire);
            result = connect ? connect(socket, address.bytes.data(), address.size) : sceNetConnect(socket, address.bytes.data(), address.size);
            Check(connectDeadline);
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
        if (!url.secure) return 0;
        ssl = sceSslCreateConnection(sslContext, socket, url.host.c_str());
        Require(ssl > 0, "sceHttp2: SSL context or connection unavailable");
        Require(SslTrustSystemRoots_nid_no_patch(ssl) == 0, "sceHttp2: system CA list unavailable");
        Require(sceSslDisableVerifyOption(ssl, 0x3d) == 0 && sceSslEnableVerifyOption(ssl, settings.verifyFlags) == 0,
            "sceHttp2: SSL verification options rejected");
        Require(sceSslSetAlpn(ssl, version == 3 ? "h2" : "http/1.1") == 0, "sceHttp2: ALPN setup failed");
        if (settings.sslCallback) Require(sceSslSetVerifyCallback(ssl, Verify_nid_no_patch, this) == 0, "sceHttp2: SSL callback setup failed");
        const auto handshakeDeadline = Deadline(settings.recvTimeout, ReceiveTimedOut);
        for (;;) {
            Check(handshakeDeadline);
            result = sceSslConnect(ssl);
            if (result == WantRead || result == WantWrite) Wait(result == WantWrite, handshakeDeadline);
            else break;
        }
        if (settings.sslCallback) Require(sceSslSetVerifyCallback(ssl, nullptr, nullptr) == 0, "sceHttp2: SSL callback cleanup failed");
        if (std::find(HandshakeFailures.begin(), HandshakeFailures.end(), result) != HandshakeFailures.end()) return result;
        Require(result == 0, "sceHttp2: TLS failed; console error mapping is unknown");
        const char* selected = sceSslGetAlpnSelected(ssl);
        Require(version == 3 ? selected && std::string_view(selected) == "h2" : !selected || std::string_view(selected) == "http/1.1",
            "sceHttp2: server negotiated an unsupported protocol");
        return 0;
    }

    int TryRead(void* data, std::size_t size) {
        const auto count = url.secure ? SslRead_nid_no_patch(ssl, data, size, 0) : sceNetRecv(socket, data, size, 0);
        if (!url.secure && count < 0 && *sceNetErrnoLoc() == 35) return WantRead;
        if (count == ConnectionReset) Raise(!url.secure ? ConnectionReset : ready ? 0 : ConnectionEnded, "sceHttp2: TLS reset after the response headers; console error mapping is unknown");
        Require((count >= 0 && count <= std::numeric_limits<int>::max()) || count == WantRead || count == WantWrite,
            "sceHttp2: read failed; console error mapping is unknown");
        return static_cast<int>(count);
    }

    int TryWrite(const void* data, std::size_t size) {
        size = std::min<std::size_t>(size, 16384);
        const auto write = WriteHookSlot().load(std::memory_order_acquire);
        const auto count = url.secure ? sceSslWrite(ssl, const_cast<void*>(data), size, 0) :
            write ? write(socket, data, size, 0) : sceNetSend(socket, data, size, 0);
        if (!url.secure && count < 0 && *sceNetErrnoLoc() == 35) return WantWrite;
        if (!url.secure && count == ConnectionReset) Raise(ConnectionReset, "sceHttp2: connection reset during write");
        Require((count > 0 && static_cast<std::uint64_t>(count) <= size) || count == WantRead || count == WantWrite,
            "sceHttp2: write failed; console error mapping is unknown");
        return static_cast<int>(count);
    }

    int RawRead(void* data, std::size_t size, Limit deadline) {
        for (;;) {
            Check(deadline);
            const int count = TryRead(data, size);
            if (count == WantRead || count == WantWrite) { Wait(count == WantWrite, deadline); continue; }
            return count;
        }
    }

    void RawWrite(const void* data, std::size_t size, Limit deadline) {
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        while (size) {
            Check(deadline);
            const int count = TryWrite(bytes, size);
            if (count == WantRead || count == WantWrite) { Wait(count == WantWrite, deadline); continue; }
            bytes += count;
            size -= static_cast<std::size_t>(count);
        }
    }

    std::string Line(Limit deadline, int endCode = 0) {
        for (;;) {
            Check(deadline);
            for (std::size_t index = pendingOffset; index + 1 < pending.size(); ++index) {
                if (pending[index] == '\r' && pending[index + 1] == '\n') {
                    Require(index - pendingOffset <= 65536, "sceHttp2: HTTP1 line exceeds host limit");
                    std::string result(reinterpret_cast<const char*>(pending.data() + pendingOffset), index - pendingOffset);
                    pendingOffset = index + 2;
                    return result;
                }
            }
            Require(pending.size() - pendingOffset <= 65536, "sceHttp2: HTTP1 line exceeds host limit");
            if (pendingOffset) { pending.erase(pending.begin(), pending.begin() + pendingOffset); pendingOffset = 0; }
            std::array<std::uint8_t, 16384> buffer;
            const int count = RawRead(buffer.data(), buffer.size(), deadline);
            if (count == 0) Raise(endCode, "sceHttp2: truncated HTTP1 headers or chunk framing");
            pending.insert(pending.end(), buffer.begin(), buffer.begin() + count);
        }
    }

    int PendingRead(void* output, std::size_t size, Limit deadline) {
        Check(deadline);
        if (pendingOffset == pending.size()) return RawRead(output, size, deadline);
        const auto count = std::min(size, pending.size() - pendingOffset);
        std::memcpy(output, pending.data() + pendingOffset, count);
        pendingOffset += count;
        return static_cast<int>(count);
    }

    static std::pair<std::string, std::string_view> Field(std::string_view line) {
        const auto colon = line.find(':');
        Require(colon != std::string_view::npos, "sceHttp2: malformed HTTP1 field");
        auto name = Lower(std::string(line.substr(0, colon)));
        Require(!name.empty() && nghttp2_check_header_name(reinterpret_cast<const std::uint8_t*>(name.data()), name.size()), "sceHttp2: invalid HTTP1 field name");
        auto value = line.substr(colon + 1);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
        Require(nghttp2_check_header_value_rfc9113(reinterpret_cast<const std::uint8_t*>(value.data()), value.size()), "sceHttp2: invalid HTTP1 field value");
        return {std::move(name), value};
    }

    void Http1Headers(Limit deadline) {
        {
            Require(++http1HeaderBlocks <= 8, "sceHttp2: too many informational responses");
            rawHeaders = Line(deadline, ConnectionEnded) + "\r\n";
            std::int32_t major = 0, minor = 0, code = 0;
            const char* reason = nullptr;
            std::size_t reasonSize = 0;
            Require(sceHttpParseStatusLine(rawHeaders.data(), rawHeaders.size(), &major, &minor, &code, &reason, &reasonSize) >= 0 && major == 1 && minor >= 0 && minor <= 1,
                "sceHttp2: malformed or unsupported HTTP1 status line");
            status = code;
            hasLength = chunked = hasLocation = hasAuthChallenge = hasProxyAuthChallenge = false;
            contentEncoding.clear();
            contentLength = 0;
            location.clear();
            for (;;) {
                const auto line = Line(deadline, ConnectionEnded);
                Require(rawHeaders.size() + line.size() + 2 <= 65536, "sceHttp2: HTTP1 headers exceed host limit");
                rawHeaders += line + "\r\n";
                if (line.empty()) break;
                const auto [name, value] = Field(line);
                if (name == "content-length") {
                    const auto length = Http1ContentLength(value);
                    Require(!hasLength || length == contentLength, "sceHttp2: inconsistent HTTP1 content lengths");
                    hasLength = true;
                    contentLength = length;
                } else if (name == "transfer-encoding") {
                    Require(!chunked && Lower(std::string(value)) == "chunked" && minor == 1, "sceHttp2: unsupported HTTP1 transfer coding");
                    chunked = true;
                } else if (name == "content-encoding") {
                    contentEncoding = Lower(std::string(value));
                } else if (name == "location") {
                    Require(!hasLocation, "sceHttp2: duplicate HTTP1 Location");
                    hasLocation = true;
                    location = value;
                } else if (name == "set-cookie" && settings.cookieBox != 0) {
                    Http2StoreResponseCookie(settings.cookieBox, Address(), value);
                } else if (name == "www-authenticate") {
                    hasAuthChallenge = true;
                } else if (name == "proxy-authenticate") {
                    hasProxyAuthChallenge = true;
                }
            }
            if (status >= 100 && status < 200) {
                Require(status != 101, "sceHttp2: HTTP1 upgrades are not implemented");
                return;
            }
            Require(status >= 200 && status <= 599 && !(hasLength && chunked), "sceHttp2: invalid or ambiguous HTTP1 framing");
            const bool authenticationChallenge = (status == 401 && hasAuthChallenge) || (status == 407 && hasProxyAuthChallenge);
            Require(!settings.auth || !authenticationChallenge, "sceHttp2: authentication is not implemented");
            remaining = contentLength;
            ready = true;
            ended = method == "HEAD" || status == 204 || status == 304 || (hasLength && remaining == 0);
            Require(!Inflating() || ended, "sceHttp2: HTTP1 gzip inflation is not implemented");
            return;
        }
    }

    void SendHttp1(const void* data, std::size_t size) {
        std::string header = method + " " + url.path + (version == 1 ? " HTTP/1.0\r\n" : " HTTP/1.1\r\n");
        std::string authority = url.authority;
        for (const auto& [name, value] : settings.headers) if (name == "host") authority = value;
        header += "Host: " + authority + "\r\nConnection: close\r\n";
        if (!settings.userAgent.empty() && std::none_of(settings.headers.begin(), settings.headers.end(), [](const auto& field) { return field.first == "user-agent"; }))
            header += "User-Agent: " + settings.userAgent + "\r\n";
        std::string cookies;
        bool hasCookie = false;
        for (const auto& [name, value] : settings.headers) {
            if (name == "cookie") {
                if (!cookies.empty() && !value.empty()) cookies += "; ";
                cookies += value;
                hasCookie = true;
            } else if (name != "host") header += name + ": " + value + "\r\n";
        }
        if (settings.cookieBox != 0) {
            if (const auto stored = Http2CookieHeader(settings.cookieBox, Address())) {
                if (!cookies.empty()) cookies += "; ";
                cookies += *stored;
                hasCookie = true;
            }
        }
        if (hasCookie) header += "cookie: " + cookies + "\r\n";
        const bool chunkUpload = settings.noContentLength && size;
        if (chunkUpload) header += "Transfer-Encoding: chunked\r\n";
        else if (!settings.noContentLength) header += "Content-Length: " + std::to_string(size) + "\r\n";
        header += "\r\n";
        std::string prefix;
        if (chunkUpload) {
            std::array<char, 32> digits;
            const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), size, 16);
            Require(result.ec == std::errc{}, "sceHttp2: chunk size encoding failed");
            prefix = std::string(digits.data(), result.ptr) + "\r\n";
        }
        const std::array<std::pair<const void*, std::size_t>, 4> blocks{{
            {header.data(), header.size()}, {prefix.data(), prefix.size()}, {data, size}, {"\r\n0\r\n\r\n", chunkUpload ? 7u : 0u}}};
        const auto deadline = Deadline(settings.sendTimeout, SendTimedOut);
        std::size_t block = 0, offset = 0;
        while (!ready) {
            while (block < blocks.size() && offset == blocks[block].second) { ++block; offset = 0; }
            if (block == blocks.size()) break;
            Check(deadline);
            std::array<std::uint8_t, 16384> input;
            const int read = TryRead(input.data(), input.size());
            if (read >= 0) {
                if (read == 0) Raise(ConnectionEnded, "sceHttp2: connection ended before HTTP1 response");
                pending.insert(pending.end(), input.begin(), input.begin() + read);
                for (;;) {
                    const auto first = pending.begin() + pendingOffset;
                    static constexpr std::array<std::uint8_t, 4> separator{'\r', '\n', '\r', '\n'};
                    if (std::search(first, pending.end(), separator.begin(), separator.end()) == pending.end()) break;
                    Http1Headers(deadline);
                    if (ready) break;
                }
                Require(pending.size() - pendingOffset <= 81920, "sceHttp2: HTTP1 headers exceed host limit");
                if (ready) break;
            }
            const auto* output = static_cast<const std::uint8_t*>(blocks[block].first) + offset;
            const int written = TryWrite(output, blocks[block].second - offset);
            if (written > 0) offset += static_cast<std::size_t>(written);
            else if (read < 0) Wait(read == WantWrite && written == WantWrite, deadline, read != written);
        }
        while (!ready) Http1Headers(Deadline(settings.recvTimeout, ReceiveTimedOut));

    }

    int ReadHttp1(void* output, std::size_t size) {
        const auto deadline = Deadline(settings.recvTimeout, ReceiveTimedOut);
        size = std::min(size, static_cast<std::size_t>(std::numeric_limits<int>::max()));
        if (chunked) {
            if (chunkTerminator) { Require(Line(deadline).empty(), "sceHttp2: invalid chunk terminator"); chunkTerminator = false; }
            if (!chunkRemaining) {
                const auto line = Line(deadline, ConnectionEnded);
                const auto extension = line.find(';');
                auto digits = std::string_view(line).substr(0, extension);
                if (extension != std::string::npos) {
                    while (!digits.empty() && (digits.back() == ' ' || digits.back() == '\t')) digits.remove_suffix(1);
                }
                chunkRemaining = Number(digits, 16);
                if (!chunkRemaining) {
                    std::size_t bytes = 0;
                    std::string trailers;
                    for (;;) {
                        const auto trailer = Line(deadline);
                        bytes += trailer.size() + 2;
                        Require(bytes <= 65536, "sceHttp2: trailers exceed host limit");
                        if (trailer.empty()) break;
                        Field(trailer);
                        trailers += trailer + "\r\n";
                    }
                    if (!trailers.empty()) trailers += "\r\n";
                    trailerHeaders = std::move(trailers);
                    ended = true;
                    return 0;
                }
            }
            size = static_cast<std::size_t>(std::min<std::uint64_t>(size, chunkRemaining));
        } else if (hasLength) size = static_cast<std::size_t>(std::min<std::uint64_t>(size, remaining));
        const int count = PendingRead(output, size, deadline);
        if (count == 0 && chunked) Raise(ConnectionEnded, "sceHttp2: truncated chunk");
        if (count == 0 && hasLength) {
            ended = true;
            return 0;
        }
        Require(count > 0 || (!hasLength && !chunked), "sceHttp2: truncated HTTP1 body");
        if (chunked) { chunkRemaining -= count; chunkTerminator = chunkRemaining == 0; }
        else if (hasLength) { remaining -= count; ended = remaining == 0; }
        else if (!count) ended = true;
        return count;
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
                if (state.ready) {
                    state.trailerBlock = true;
                    state.blockHeaders.clear();
                    return;
                }
                state.blockStatus = 0;
                state.blockHeaders.clear();
                state.hasAuthChallenge = state.hasProxyAuthChallenge = false;
                state.hasLength = false;
                state.contentLength = 0;
                state.location.clear();
                state.hasLocation = false;
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
            if (state.trailerBlock) {
                state.blockHeaders.emplace_back(std::string(key), std::string(text));
                return;
            }
            if (key == ":status") state.blockStatus = static_cast<int>(Number(text));
            if (key == "www-authenticate") state.hasAuthChallenge = true;
            if (key == "proxy-authenticate") state.hasProxyAuthChallenge = true;
            if (key == "location") {
                Require(!state.hasLocation, "sceHttp2: duplicate redirect location; console error mapping is unknown");
                state.location = text;
                state.hasLocation = true;
            }
            if (key == "content-length") {
                state.contentLength = Number(text);
                state.hasLength = true;
            }
            if (key == "set-cookie" && state.settings.cookieBox != 0) Http2StoreResponseCookie(state.settings.cookieBox, state.Address(), text);
            if (key == "content-encoding") state.contentEncoding = Lower(std::string(text));
            if (key != ":status") state.blockHeaders.emplace_back(std::string(key), std::string(text));
        });
    }

    static int Frame(nghttp2_session*, const nghttp2_frame* frame, void* data) {
        auto& state = *static_cast<State*>(data);
        return Callback(state, [&] {
            if (frame->hd.type == NGHTTP2_GOAWAY) {
                const auto error = frame->goaway.error_code;
                const bool graceful = error == NGHTTP2_NO_ERROR && frame->goaway.last_stream_id >= state.stream;
                if (!state.ready && error <= MeasuredStreamErrors && (graceful || frame->goaway.last_stream_id < state.stream))
                    state.Raise(GoawayReceived + static_cast<int>(error), "sceHttp2: GOAWAY");
                Require(graceful, "sceHttp2: GOAWAY rejected request");
            }
            if (frame->hd.stream_id != state.stream) return;
            if (frame->hd.type == NGHTTP2_RST_STREAM && !state.ended) {
                const auto error = frame->rst_stream.error_code;
                Require(error <= MeasuredStreamErrors && (!state.ready || error == NGHTTP2_CANCEL),
                    "sceHttp2: RST_STREAM; console error mapping is unknown");
                if (!state.ready) state.Raise(StreamReset + static_cast<int>(error), "sceHttp2: RST_STREAM");
                state.streamFailure = StreamReset + static_cast<int>(error);
                return;
            }
            if (frame->hd.type == NGHTTP2_HEADERS && state.trailerBlock) {
                state.trailerHeaders.clear();
                for (const auto& [key, value] : state.blockHeaders) state.trailerHeaders += key + ": " + value + "\r\n";
                state.trailerHeaders += "\r\n";
                state.trailerBlock = false;
            } else if (frame->hd.type == NGHTTP2_HEADERS && state.blockStatus >= 200) {
                state.status = state.blockStatus;
                const bool authenticationChallenge = (state.status == 401 && state.hasAuthChallenge) || (state.status == 407 && state.hasProxyAuthChallenge);
                Require(!state.settings.auth || !authenticationChallenge, "sceHttp2: authentication is not implemented");
                state.ready = true;
                state.allHeaders = "HTTP/2.0 " + std::to_string(state.status) + " HTTP2 NO PHRASE\r\n";
                for (const auto& [key, value] : state.blockHeaders) state.allHeaders += key + ": " + value + "\r\n";
                state.allHeaders += "\r\n";
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
            if (streamId != state.stream || state.streamFailure) return;
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
        std::string authority = url.authority;
        for (const auto& [name, value] : settings.headers) if (name == "host") authority = value;
        std::vector<std::pair<std::string, std::string>> fields{
            {":method", method}, {":scheme", "https"}, {":authority", authority}, {":path", url.path}};
        if (!settings.userAgent.empty() && std::none_of(settings.headers.begin(), settings.headers.end(), [](const auto& header) { return header.first == "user-agent"; }))
            fields.emplace_back("user-agent", settings.userAgent);
        for (const auto& header : settings.headers) if (header.first != "host") fields.push_back(header);
        if (settings.cookieBox != 0) {
            if (auto cookie = Http2CookieHeader(settings.cookieBox, Address())) fields.emplace_back("cookie", std::move(*cookie));
        }
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

    void Flush(Limit deadline) {
        for (;;) {
            const std::uint8_t* bytes = nullptr;
            const auto size = nghttp2_session_mem_send2(session, &bytes);
            CheckNg(static_cast<int>(size));
            if (!size) return;
            RawWrite(bytes, static_cast<std::size_t>(size), deadline);
        }
    }

    void Pump(Limit deadline) {
        Flush(Earlier(deadline, Deadline(settings.sendTimeout, SendTimedOut)));
        Receive(deadline);
    }

    void Receive(Limit deadline) {
        std::array<std::uint8_t, 16384> buffer;
        {
            const int size = RawRead(buffer.data(), buffer.size(), deadline);
            if (size == 0) Raise(ready ? ConnectionReset : ConnectionEnded, "sceHttp2: HTTP/2 connection ended");
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
        Require(!callbackActive, "sceHttp2: operation during request callback is not implemented");
        if (failure) std::rethrow_exception(failure);
        return lock;
    }

    void Fail() {
        failure = std::current_exception();
        upload = nullptr;
        Close();
    }
};

Request::Request(int id, int version, int context, std::shared_ptr<Capacity> capacity, Settings settings, std::string method, std::string url)
    : state(std::make_unique<State>(id, version, context, std::move(capacity), std::move(settings), std::move(method), std::move(url))) {}
Request::~Request() = default;

int Request::Send(const void* data, std::size_t size) {
    auto lock = state->Lock();
    if (state->started && state->networkResult) return state->networkResult;
    if (state->started && state->ended) return AlreadySent;
    Require(!state->started, "sceHttp2SendRequest: request reuse is not implemented");
    Require(data || size == 0, "sceHttp2SendRequest: null body");
    Require(state->settings.noContentLength || size == state->settings.contentLength, "sceHttp2SendRequest: body length mismatch");
    Require(state->version != 1 || !state->settings.noContentLength || size == 0, "sceHttp2: HTTP1.0 upload without a length is unsupported");
    state->started = true;
    state->totalDeadline = Clock::now() + std::chrono::microseconds(state->settings.timeout);
    try {
        const void* payload = data;
        std::size_t payloadSize = size;
        for (unsigned redirects = 0;; ++redirects) {
            const int active = state->capacity->active.fetch_add(1);
            state->acquired = true;
            Require(active < state->capacity->maximum, "sceHttp2: connection slot waiting is not implemented");
            state->callbackLock = &lock;
            state->networkResult = state->Connect();
            state->callbackLock = nullptr;
            if (state->networkResult < 0) { state->Close(); return state->networkResult; }
            if (state->version < 3) state->SendHttp1(payload, payloadSize);
            else {
                state->Setup(payload, payloadSize);
                const auto sendDeadline = Earlier(state->Deadline(state->settings.sendTimeout, SendTimedOut),
                    state->Deadline(state->settings.recvTimeout, ReceiveTimedOut));
                auto receiveDeadline = sendDeadline;
                bool uploaded = false;
                for (;;) {
                    state->Flush(uploaded ? state->Deadline(state->settings.sendTimeout, SendTimedOut) : sendDeadline);
                    if (!uploaded && state->uploadOffset == payloadSize) {
                        uploaded = true;
                        receiveDeadline = state->Deadline(state->settings.recvTimeout, ReceiveTimedOut);
                    }
                    if (state->ready) break;
                    state->Receive(uploaded ? receiveDeadline : sendDeadline);
                    if (state->ready) break;
                }
            }
            const bool redirectStatus = state->status == 301 || state->status == 302 || state->status == 303 || state->status == 307 || state->status == 308;
            if (!state->settings.redirect || !redirectStatus || state->location.empty()) break;
            const bool retrieve = (state->status == 303 && state->method != "HEAD") ||
                ((state->status == 301 || state->status == 302) && state->method == "POST");
            std::string method = retrieve ? "GET" : state->method;
            if (const auto callback = state->settings.redirectCallback) {
                const char* selected = method.c_str();
                const int result = state->InvokeCallback(lock, [&] {
                    return callback(state->id, state->status, &selected, state->location.c_str(), state->settings.redirectArgument);
                });
                if (result != 0) break;
                Require(selected, "sceHttp2: redirect callback returned a null method");
                method = selected;
            }
            Require(redirects < 6, "sceHttp2: redirect limit exceeded; console error mapping is unknown");
            Require(!method.empty() && nghttp2_check_method(reinterpret_cast<const std::uint8_t*>(method.data()), method.size()) && method != "CONNECT",
                "sceHttp2: redirect callback returned an unsupported method");
            auto target = RedirectTarget(state->url, state->location);
            Require(state->version != 3 || target.secure, "sceHttp2: cleartext HTTP2 redirect is unsupported");
            const bool differentOrigin = target.secure != state->url.secure || Lower(target.host) != Lower(state->url.host) || target.port != state->url.port;
            const bool dropBody = (retrieve || method != state->method) && (method == "GET" || method == "HEAD");
            std::erase_if(state->settings.headers, [&](const auto& header) {
                const auto& key = header.first;
                return (differentOrigin && (key == "authorization" || key == "proxy-authorization" || key == "cookie" || key == "origin" || key == "referer")) ||
                    (dropBody && (key.starts_with("content-") || key == "digest" || key == "last-modified"));
            });
            if (dropBody) { payload = nullptr; payloadSize = 0; }
            state->settings.contentLength = payloadSize;
            state->Close();
            state->method = std::move(method);
            state->url = std::move(target);
            state->ready = state->ended = false;
            state->status = state->blockStatus = 0;
            state->location.clear();
            state->hasLocation = state->hasLength = false;
            state->contentLength = 0;
            state->headerBytes = 0;
            state->body.clear();
            state->bodyOffset = 0;
            state->contentEncoding.clear();
            state->inflatedGzip = false;
            state->upload = nullptr;
            state->uploadSize = state->uploadOffset = 0;
            state->callbackFailure = nullptr;
            state->streamFailure = 0;
            state->stream = -1;
            state->pending.clear();
            state->pendingOffset = 0;
            state->chunked = state->chunkTerminator = false;
            state->chunkRemaining = state->remaining = 0;
            state->rawHeaders.clear();
            state->http1HeaderBlocks = 0;
        }
        state->upload = nullptr;
        if (state->ended && state->body.empty()) state->Close();
        return 0;
    } catch (const RequestFailure& failure) {
        state->networkResult = failure.code;
        state->upload = nullptr;
        state->Close();
        return failure.code;
    } catch (...) { state->Fail(); throw; }
}

int Request::Read(void* data, std::size_t size) {
    auto lock = state->Lock();
    if (state->networkResult) return ResponseUnavailable;
    if (state->readResult) return state->readResult;
    Require(state->ready, "sceHttp2ReadData: response unavailable");
    Require(data || size == 0, "sceHttp2ReadData: null buffer");
    if (!size) return 0;
    if (state->ended && state->body.empty()) return 0;
    try {
        if (state->version < 3) {
            const int count = state->ReadHttp1(data, size);
            if (state->ended) state->Close();
            return count;
        }
        const auto deadline = state->Deadline(state->settings.recvTimeout, ReceiveTimedOut);
        state->Check(deadline);
        while (!state->ended && !state->streamFailure && (state->bodyOffset == state->body.size() || state->Inflating())) state->Pump(deadline);
        if (state->streamFailure) state->Raise(state->streamFailure, "sceHttp2: RST_STREAM");
        const auto count = std::min({size, state->body.size() - state->bodyOffset, static_cast<std::size_t>(std::numeric_limits<int>::max())});
        state->Check(deadline);
        if (count) std::memcpy(data, state->body.data() + state->bodyOffset, count);
        state->bodyOffset += count;
        if (state->bodyOffset == state->body.size()) { state->body.clear(); state->bodyOffset = 0; }
        if (count && !state->ended) CheckNg(nghttp2_session_consume(state->session, state->stream, count));
        if (state->ended && state->body.empty()) state->Close();
        return static_cast<int>(count);
    } catch (const RequestFailure& failure) {
        state->readResult = failure.code;
        state->Close();
        return failure.code;
    } catch (...) { state->Fail(); throw; }
}

int Request::Status(int* status) {
    auto lock = state->Lock();
    if (state->networkResult) {
        Require(status, "sceHttp2GetStatusCode: null output");
        *status = 0;
        return 0;
    }
    Require(status && state->ready, "sceHttp2GetStatusCode: null output or response unavailable");
    *status = state->status;
    return 0;
}

int Request::ContentLength(int* kind, std::uint64_t* length) {
    auto lock = state->Lock();
    if (state->networkResult) return ResponseUnavailable;
    Require(kind && length && state->ready, "sceHttp2GetResponseContentLength: null output or response unavailable");
    if (state->Inflating()) {
        Require(!state->readResult || state->ended, "sceHttp2GetResponseContentLength: inflated length after a failed read");
        try {
            const auto deadline = state->Deadline(state->settings.recvTimeout, ReceiveTimedOut);
            while (!state->ended) state->Pump(deadline);
        } catch (const RequestFailure&) {
            try { Unknown("sceHttp2GetResponseContentLength: failure while inflating; console error mapping is unknown"); }
            catch (...) { state->Fail(); throw; }
        } catch (...) { state->Fail(); throw; }
    }
    *kind = state->chunked ? 2 : state->hasLength ? 0 : 1;
    *length = state->hasLength ? state->contentLength : 0;
    return 0;
}

int Request::Headers(char** header, std::size_t* size) {
    auto lock = state->Lock();
    if (state->networkResult) return state->networkResult;
    Require(header && size && state->ready, "sceHttp2GetAllResponseHeaders: null output or response unavailable");
    if (state->version < 3) {
        *header = state->rawHeaders.data();
        *size = state->rawHeaders.size();
    } else {
        *header = state->allHeaders.data();
        *size = state->allHeaders.size();
    }
    return 0;
}

int Request::TrailingHeaders(char** header, std::size_t* size) {
    auto lock = state->Lock();
    if (state->networkResult) return state->networkResult;
    Require(header && size && state->ready && state->ended, "sceHttp2GetAllTrailingHeaders: null output or response unavailable");
    Require(!state->trailerHeaders.empty(), "sceHttp2GetAllTrailingHeaders: response has no trailers");
    *header = state->trailerHeaders.data();
    *size = state->trailerHeaders.size();
    return 0;
}

void Request::Cancel() { state->cancelled.store(true); }

void Request::Stop(bool terminated) {
    state->stopped.store(terminated);
    state->deleted.store(true);
    Cancel();
    std::lock_guard lock(state->mutex);
    state->Close();
}

std::shared_ptr<Capacity> CreateCapacity_nid_no_patch(int maximum) {
    return std::make_shared<Capacity>(maximum);
}

std::shared_ptr<Request> CreateRequest_nid_no_patch(int id, int version, int sslContext, std::shared_ptr<Capacity> capacity, Settings settings, std::string method, std::string url) {
    return std::make_shared<Request>(id, version, sslContext, std::move(capacity), std::move(settings), std::move(method), std::move(url));
}

}
