#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "SceTypes.hpp"
#include "prx/libSceHttp/src/HttpErrors.hpp"
#include "prx/libSceHttp/src/Options.hpp"
#include "prx/libSceHttp/src/Request.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" {
int APS5_VABI sceHttpUriParse(SceHttpUriElement*, const char*, void*, std::size_t*, std::size_t);
int APS5_VABI sceHttpUriMerge(char*, const char*, const char*, std::size_t*, std::size_t, std::uint32_t);
int APS5_VABI sceNetSocket(const char*, int, int, int);
int APS5_VABI sceNetSocketClose(int);
int APS5_VABI sceNetConnect(int, const void*, std::uint32_t);
std::int64_t APS5_VABI sceNetSend(int, const void*, std::size_t, int);
std::int64_t APS5_VABI sceNetRecv(int, void*, std::size_t, int);
int APS5_VABI sceNetSetsockopt(int, int, int, const void*, std::uint32_t);
int APS5_VABI sceNetGetsockopt(int, int, int, void*, std::uint32_t*);
int APS5_VABI sceNetEpollCreate(const char*, int);
int APS5_VABI sceNetEpollControl(int, int, int, const NetEpollEvent*);
int APS5_VABI sceNetEpollWait(int, NetEpollEvent*, int, int);
int APS5_VABI sceNetEpollDestroy(int);
int APS5_VABI sceSslCreateConnection(int, int, const char*);
int APS5_VABI sceSslDeleteConnection(int);
int APS5_VABI sceSslConnect(int);
int APS5_VABI sceSslRead(int, void*, std::size_t, int);
int APS5_VABI sceSslWrite(int, void*, std::size_t, int);
int APS5_VABI sceSslSetVerifyCallback(int, int (APS5_VABI *)(int, unsigned int, void**, int, void*), void*);
int APS5_VABI sceHttpParseStatusLine(const char*, std::size_t, std::int32_t*, std::int32_t*, std::int32_t*, const char**, std::size_t*);
}

namespace {

constexpr int kSocketLevel = 0xffff;
constexpr int kSocketSendTimeout = 0x1005;
constexpr int kSocketReceiveTimeout = 0x1006;
constexpr int kSocketError = 0x1007;
constexpr int kSocketNonBlocking = 0x1200;
constexpr int kNetErrorInProgress = static_cast<int>(0x80410124u);
constexpr std::uint32_t kEpollWrite = 2;
constexpr int kAddressFamilyIpv4 = 2;
constexpr int kAddressFamilyIpv6 = 28;
constexpr int kSocketStream = 1;
constexpr int kProtocolTcp = 6;
constexpr std::uint32_t kHeaderOverwrite = 0;
constexpr std::uint32_t kHeaderAppend = 1;
constexpr std::size_t kIoBufferSize = 8192;
constexpr std::size_t kMaximumChunkLine = 8192;

struct Url {
    std::string scheme;
    std::string host;
    std::string target;
    std::uint16_t port = 0;
    bool secure = false;
};

struct ContextData {
    int memId;
    int sslContextId;
};

struct TransportSettings {
    std::string userAgent;
    int httpVersion = 2;
    std::uint64_t responseHeaderMaxSize = 65536;
    std::uint32_t resolveTimeOutUsec = 0;
    std::uint32_t connectTimeOutUsec = 0;
    std::uint32_t sendTimeOutUsec = 0;
    std::uint32_t receiveTimeOutUsec = 0;
    HttpsCallback sslCallback = nullptr;
    void* sslCallbackArgument = nullptr;
};

struct TemplateData {
    int id;
    std::shared_ptr<ContextData> context;
    TransportSettings settings;
};

struct ConnectionData {
    int id;
    std::shared_ptr<TemplateData> httpTemplate;
    Url url;
    bool keepAlive;
    TransportSettings settings;
};

struct Header {
    std::string name;
    std::string value;
};

class NetworkStream {
public:
    NetworkStream(const int socket, const int ssl, const bool secure) : socket_(socket), ssl_(ssl), secure_(secure) {}
    NetworkStream(const NetworkStream&) = delete;
    NetworkStream& operator=(const NetworkStream&) = delete;

    ~NetworkStream() { Close(); }

    int Read(void* buffer, std::size_t size) {
        if (closed_) return 0;
        const auto limit = std::min<std::size_t>(size, static_cast<std::size_t>(std::numeric_limits<int>::max()));
        if (secure_) return sceSslRead(ssl_, buffer, limit, 0);
        const auto result = sceNetRecv(socket_, buffer, limit, 0);
        return result < 0 ? ERROR_NETWORK : static_cast<int>(result);
    }

    int Write(const void* buffer, std::size_t size) {
        const auto* bytes = static_cast<const unsigned char*>(buffer);
        std::size_t sent = 0;
        while (sent < size) {
            const auto count = std::min<std::size_t>(size - sent, static_cast<std::size_t>(std::numeric_limits<int>::max()));
            const int result = secure_ ? sceSslWrite(ssl_, const_cast<unsigned char*>(bytes + sent), count, 0)
                                       : static_cast<int>(sceNetSend(socket_, bytes + sent, count, 0));
            if (result <= 0) return ERROR_NETWORK;
            sent += static_cast<std::size_t>(result);
        }
        return 0;
    }

    void Close() {
        if (closed_) return;
        closed_ = true;
        if (secure_ && ssl_ >= 0) sceSslDeleteConnection(ssl_);
        if (socket_ >= 0) sceNetSocketClose(socket_);
        ssl_ = -1;
        socket_ = -1;
    }

private:
    int socket_;
    int ssl_;
    bool secure_;
    bool closed_ = false;
};

struct RequestData {
    RequestData(int requestId, int requestOptionsId, std::shared_ptr<ConnectionData> parent, Url requestUrl,
        std::string requestMethod, const std::uint64_t requestLength, TransportSettings transportSettings)
        : id(requestId), optionsId(requestOptionsId), connection(std::move(parent)), url(std::move(requestUrl)),
          method(std::move(requestMethod)), contentLength(requestLength), settings(std::move(transportSettings)) {}

    int id;
    int optionsId;
    std::shared_ptr<ConnectionData> connection;
    Url url;
    std::string method;
    std::uint64_t contentLength;
    TransportSettings settings;
    std::vector<Header> requestHeaders;
    std::mutex mutex;
    bool sent = false;
    int lastError = 0;
    int statusCode = 0;
    std::string rawResponseHeaders;
    std::vector<Header> responseHeaders;
    std::unique_ptr<NetworkStream> stream;
    std::vector<unsigned char> pending;
    std::size_t pendingOffset = 0;
    std::uint64_t contentRemaining = 0;
    std::uint64_t responseContentLength = 0;
    bool contentLengthKnown = false;
    bool chunked = false;
    bool closeDelimited = false;
    bool bodyDone = false;
    std::uint64_t chunkRemaining = 0;
    bool needChunkTerminator = false;
};

std::mutex g_objectsMutex;
std::unordered_map<int, std::shared_ptr<ContextData>> g_contexts;
std::unordered_map<int, std::shared_ptr<TemplateData>> g_templates;
std::unordered_map<int, std::shared_ptr<ConnectionData>> g_connections;
std::unordered_map<int, std::shared_ptr<RequestData>> g_requests;

std::string LowerAscii(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char c : text) result.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c);
    return result;
}

bool ValidRequestTarget(const std::string_view value) {
    return !value.empty() && value.find_first_of("\r\n \t") == std::string_view::npos;
}

std::string_view Trim(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
    return value;
}

bool IsToken(const char c) {
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return true;
    switch (c) {
    case '!': case '#': case '$': case '%': case '&': case '\'': case '*': case '+': case '-': case '.':
    case '^': case '_': case '`': case '|': case '~': return true;
    default: return false;
    }
}

bool ValidHeaderName(const std::string_view name) {
    return !name.empty() && std::all_of(name.begin(), name.end(), IsToken);
}

bool ValidHeaderValue(const std::string_view value) {
    return value.find('\r') == std::string_view::npos && value.find('\n') == std::string_view::npos;
}

int ParseUrl(const char* text, Url& output) {
    if (!text) return ERROR_INVALID_URL;
    std::size_t required = 0;
    if (const int result = sceHttpUriParse(nullptr, text, nullptr, &required, 0); result != 0) return result;
    std::vector<char> pool(required);
    SceHttpUriElement element{};
    if (const int result = sceHttpUriParse(&element, text, pool.data(), nullptr, pool.size()); result != 0) return result;
    if (!element.scheme || !element.hostname || element.hostname[0] == '\0') return ERROR_INVALID_URL;
    output.scheme = LowerAscii(element.scheme);
    output.secure = output.scheme == "https";
    if (!output.secure && output.scheme != "http") return ERROR_INVALID_URL;
    output.host = element.hostname;
    output.port = element.port;
    output.target = element.path && element.path[0] ? element.path : "/";
    if (element.query && element.query[0]) output.target += element.query;
    return 0;
}

int MakeUrl(const char* host, const char* scheme, const std::uint16_t port, Url& output) {
    if (!host || !scheme || host[0] == '\0' || !ValidHeaderValue(host) || std::string_view(host).find_first_of(" \t") != std::string_view::npos) return ERROR_INVALID_VALUE;
    output.scheme = LowerAscii(scheme);
    output.secure = output.scheme == "https";
    if (!output.secure && output.scheme != "http") return ERROR_INVALID_URL;
    output.host = host;
    output.port = port ? port : output.secure ? 443 : 80;
    output.target = "/";
    return 0;
}

std::shared_ptr<RequestData> FindRequest(const int id) {
    std::lock_guard lock(g_objectsMutex);
    const auto found = g_requests.find(id);
    return found == g_requests.end() ? nullptr : found->second;
}

int ReadLine(NetworkStream& stream, std::vector<unsigned char>& pending, std::size_t& offset, std::string& line,
    const std::size_t limit) {
    line.clear();
    for (;;) {
        const auto start = pending.begin() + static_cast<std::ptrdiff_t>(offset);
        const auto newline = std::find(start, pending.end(), static_cast<unsigned char>('\n'));
        if (newline != pending.end()) {
            const auto count = static_cast<std::size_t>(newline - start);
            if (count > limit) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
            line.assign(reinterpret_cast<const char*>(pending.data() + offset), count);
            offset += count + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return 0;
        }
        if (pending.size() - offset > limit) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
        std::array<unsigned char, kIoBufferSize> buffer{};
        const int received = stream.Read(buffer.data(), buffer.size());
        if (received <= 0) return ERROR_NETWORK;
        pending.insert(pending.end(), buffer.begin(), buffer.begin() + received);
    }
}

int ReadPending(NetworkStream& stream, std::vector<unsigned char>& pending, std::size_t& offset, void* output,
    const std::size_t size) {
    if (size == 0) return 0;
    if (offset < pending.size()) {
        const auto count = std::min(size, pending.size() - offset);
        std::memcpy(output, pending.data() + offset, count);
        offset += count;
        return static_cast<int>(count);
    }
    return stream.Read(output, size);
}

bool ParseUnsigned(const std::string_view text, std::uint64_t& result) {
    if (text.empty()) return false;
    const auto converted = std::from_chars(text.data(), text.data() + text.size(), result);
    return converted.ec == std::errc{} && converted.ptr == text.data() + text.size();
}

int ParseChunkSize(const std::string_view line, std::uint64_t& size) {
    const auto separator = line.find(';');
    auto number = Trim(line.substr(0, separator));
    if (number.empty()) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
    size = 0;
    for (const char c : number) {
        unsigned digit = 0;
        if (c >= '0' && c <= '9') digit = static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f') digit = static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') digit = static_cast<unsigned>(c - 'A' + 10);
        else return ERROR_PARSE_HTTP_INVALID_RESPONSE;
        if (size > (std::numeric_limits<std::uint64_t>::max() - digit) / 16) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
        size = size * 16 + digit;
    }
    return 0;
}

int ConnectSocket(const int socket, const void* address, const std::uint32_t addressLength, const std::uint32_t timeoutUsec) {
    const int nonblocking = 1;
    if (sceNetSetsockopt(socket, kSocketLevel, kSocketNonBlocking, &nonblocking, sizeof(nonblocking)) != 0)
        return ERROR_NETWORK;
    const int connectResult = sceNetConnect(socket, address, addressLength);
    if (connectResult != 0 && connectResult != kNetErrorInProgress) return ERROR_NETWORK;
    if (connectResult == kNetErrorInProgress) {
        const int epoll = sceNetEpollCreate("libSceHttp-connect", 0);
        if (epoll < 0) return ERROR_NETWORK;
        NetEpollEvent event{};
        event.events = kEpollWrite;
        event.ident = static_cast<std::uint64_t>(socket);
        event.data.fd = socket;
        int timeout = 0;
        if (timeoutUsec > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) timeout = std::numeric_limits<int>::max();
        else timeout = static_cast<int>(timeoutUsec);
        int result = sceNetEpollControl(epoll, 1, socket, &event);
        if (result == 0) {
            NetEpollEvent ready{};
            result = sceNetEpollWait(epoll, &ready, 1, timeout);
            if (result > 0) {
                int socketError = 0;
                std::uint32_t optionSize = sizeof(socketError);
                result = sceNetGetsockopt(socket, kSocketLevel, kSocketError, &socketError, &optionSize);
                if (result == 0 && socketError != 0) result = ERROR_NETWORK;
            } else {
                result = ERROR_NETWORK;
            }
        }
        sceNetEpollDestroy(epoll);
        if (result != 0) return ERROR_NETWORK;
    }
    const int blocking = 0;
    if (sceNetSetsockopt(socket, kSocketLevel, kSocketNonBlocking, &blocking, sizeof(blocking)) != 0)
        return ERROR_NETWORK;
    return 0;
}

bool HasHeader(const std::vector<Header>& headers, const std::string_view name) {
    const auto lower = LowerAscii(name);
    return std::any_of(headers.begin(), headers.end(), [&](const Header& header) { return LowerAscii(header.name) == lower; });
}

int AppendHeader(std::vector<Header>& headers, const char* name, const char* value, const std::uint32_t mode) {
    if (!name || !value || !ValidHeaderName(name) || !ValidHeaderValue(value)) return ERROR_INVALID_VALUE;
    if (mode != kHeaderOverwrite && mode != kHeaderAppend) return ERROR_INVALID_VALUE;
    const auto lower = LowerAscii(name);
    if (mode == kHeaderOverwrite) {
        headers.erase(std::remove_if(headers.begin(), headers.end(), [&](const Header& header) {
            return LowerAscii(header.name) == lower;
        }), headers.end());
    }
    headers.push_back({name, value});
    return 0;
}

int ParseResponseHeaders(RequestData& request, NetworkStream& stream, const std::size_t maximum) {
    for (int interim = 0; interim < 8; ++interim) {
        std::string statusLine;
        if (const int result = ReadLine(stream, request.pending, request.pendingOffset, statusLine, maximum); result != 0) return result;
        std::string lineWithEnding = statusLine + "\r\n";
        std::int32_t major = 0, minor = 0, status = 0;
        const char* reason = nullptr;
        std::size_t reasonLength = 0;
        if (sceHttpParseStatusLine(lineWithEnding.c_str(), lineWithEnding.size(), &major, &minor, &status, &reason, &reasonLength) < 0)
            return ERROR_PARSE_HTTP_INVALID_RESPONSE;
        std::vector<Header> headers;
        std::string raw = lineWithEnding;
        std::size_t total = lineWithEnding.size();
        for (;;) {
            std::string headerLine;
            if (const int result = ReadLine(stream, request.pending, request.pendingOffset, headerLine, maximum); result != 0) return result;
            total += headerLine.size() + 2;
            if (total > maximum) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
            if (headerLine.empty()) {
                raw += "\r\n";
                break;
            }
            raw += headerLine;
            raw += "\r\n";
            const auto colon = headerLine.find(':');
            if (colon == std::string::npos || !ValidHeaderName(std::string_view(headerLine).substr(0, colon)))
                return ERROR_PARSE_HTTP_INVALID_RESPONSE;
            headers.push_back({headerLine.substr(0, colon), std::string(Trim(std::string_view(headerLine).substr(colon + 1))) });
        }
        if (status >= 100 && status < 200 && status != 101) continue;
        request.statusCode = status;
        request.rawResponseHeaders = std::move(raw);
        request.responseHeaders = std::move(headers);
        break;
    }
    if (request.statusCode == 0) return ERROR_PARSE_HTTP_INVALID_RESPONSE;

    std::optional<std::uint64_t> contentLength;
    bool chunked = false;
    for (const auto& header : request.responseHeaders) {
        const auto name = LowerAscii(header.name);
        if (name == "content-length") {
            std::uint64_t length = 0;
            if (!ParseUnsigned(Trim(header.value), length)) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
            if (contentLength && *contentLength != length) return ERROR_PARSE_HTTP_INVALID_RESPONSE;
            contentLength = length;
        } else if (name == "transfer-encoding") {
            std::string lower = LowerAscii(header.value);
            std::size_t start = 0;
            while (start <= lower.size()) {
                const auto end = lower.find(',', start);
                if (Trim(std::string_view(lower).substr(start, end == std::string::npos ? lower.size() - start : end - start)) == "chunked") chunked = true;
                if (end == std::string::npos) break;
                start = end + 1;
            }
        }
    }
    request.chunked = chunked;
    request.contentLengthKnown = contentLength.has_value() && !chunked;
    request.contentRemaining = request.contentLengthKnown ? *contentLength : 0;
    request.responseContentLength = request.contentLengthKnown ? *contentLength : 0;
    request.closeDelimited = !request.contentLengthKnown && !chunked;
    request.bodyDone = request.method == "HEAD" || request.statusCode == 204 || request.statusCode == 304 ||
        (request.statusCode >= 100 && request.statusCode < 200) || (request.contentLengthKnown && *contentLength == 0);
    return 0;
}

int SendBytes(NetworkStream& stream, const std::string& bytes) {
    return stream.Write(bytes.data(), bytes.size());
}

int BuildRequest(const RequestData& request, const TransportSettings& settings, const void* body, const std::size_t bodySize,
    std::string& output) {
    if (bodySize && !body) return ERROR_INVALID_VALUE;
    if (request.contentLength != bodySize && (request.contentLength != 0 || bodySize != 0)) return ERROR_INVALID_VALUE;
    if (!ValidRequestTarget(request.url.target) || !ValidHeaderValue(settings.userAgent)) return ERROR_INVALID_VALUE;
    const bool http10 = settings.httpVersion == 1;
    output = request.method + " " + request.url.target + (http10 ? " HTTP/1.0\r\n" : " HTTP/1.1\r\n");
    if (!HasHeader(request.requestHeaders, "Host")) {
        const bool ipv6 = request.url.host.find(':') != std::string::npos;
        output += "Host: ";
        if (ipv6) output += "[" + request.url.host + "]";
        else output += request.url.host;
        if (request.url.port != (request.url.secure ? 443 : 80)) output += ":" + std::to_string(request.url.port);
        output += "\r\n";
    }
    if (!settings.userAgent.empty() && !HasHeader(request.requestHeaders, "User-Agent")) output += "User-Agent: " + settings.userAgent + "\r\n";
    if (!HasHeader(request.requestHeaders, "Connection")) output += "Connection: close\r\n";
    if (HasHeader(request.requestHeaders, "Transfer-Encoding")) return ERROR_INVALID_VALUE;
    if (HasHeader(request.requestHeaders, "Content-Length")) {
        for (const auto& header : request.requestHeaders) {
            if (LowerAscii(header.name) != "content-length") continue;
            std::uint64_t declaredLength = 0;
            if (!ParseUnsigned(Trim(header.value), declaredLength) || declaredLength != bodySize) return ERROR_INVALID_VALUE;
        }
    } else {
        output += "Content-Length: " + std::to_string(bodySize) + "\r\n";
    }
    for (const auto& header : request.requestHeaders) output += header.name + ": " + header.value + "\r\n";
    output += "\r\n";
    return 0;
}

int CreateSocketStream(const std::shared_ptr<RequestData>& request, const Url& url, const TransportSettings& settings,
    std::unique_ptr<NetworkStream>& stream) {
    const auto context = request->connection->httpTemplate->context;
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    const auto service = std::to_string(url.port);
    const int socketProbe = sceNetSocket("libSceHttp", kAddressFamilyIpv4, kSocketStream, kProtocolTcp);
    if (socketProbe < 0) {
        request->lastError = ERROR_NETWORK;
        return ERROR_NETWORK;
    }
    sceNetSocketClose(socketProbe);
    addrinfo* addresses = nullptr;
    if (getaddrinfo(url.host.c_str(), service.c_str(), &hints, &addresses) != 0) {
        request->lastError = static_cast<int>(0x804101e1u);
        return ERROR_NETWORK;
    }
    int socket = -1;
    for (auto* address = addresses; address; address = address->ai_next) {
        int family = 0;
        std::array<unsigned char, 28> guestAddress{};
        std::uint32_t addressLength = 0;
        if (address->ai_family == AF_INET && address->ai_addrlen >= sizeof(sockaddr_in)) {
            family = kAddressFamilyIpv4;
            guestAddress[0] = 16;
            guestAddress[1] = static_cast<unsigned char>(family);
            const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(address->ai_addr);
            std::memcpy(guestAddress.data() + 2, &ipv4->sin_port, sizeof(ipv4->sin_port));
            std::memcpy(guestAddress.data() + 4, &ipv4->sin_addr, sizeof(ipv4->sin_addr));
            addressLength = 16;
        } else if (address->ai_family == AF_INET6 && address->ai_addrlen >= sizeof(sockaddr_in6)) {
            family = kAddressFamilyIpv6;
            guestAddress[0] = 28;
            guestAddress[1] = static_cast<unsigned char>(family);
            const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(address->ai_addr);
            std::memcpy(guestAddress.data() + 2, &ipv6->sin6_port, sizeof(ipv6->sin6_port));
            std::memcpy(guestAddress.data() + 4, &ipv6->sin6_flowinfo, sizeof(ipv6->sin6_flowinfo));
            std::memcpy(guestAddress.data() + 8, &ipv6->sin6_addr, sizeof(ipv6->sin6_addr));
            std::memcpy(guestAddress.data() + 24, &ipv6->sin6_scope_id, sizeof(ipv6->sin6_scope_id));
            addressLength = 28;
        } else {
            continue;
        }
        socket = sceNetSocket("libSceHttp", family, kSocketStream, kProtocolTcp);
        if (socket < 0) continue;
        const auto setTimeout = [socket](const int option, const std::uint32_t usec) {
            if (!usec) return 0;
            const KernelTimeval timeout{static_cast<std::int64_t>(usec / 1000000), static_cast<std::int64_t>(usec % 1000000)};
            return sceNetSetsockopt(socket, kSocketLevel, option, &timeout, sizeof(timeout));
        };
        if (setTimeout(kSocketSendTimeout, settings.sendTimeOutUsec) != 0 ||
            setTimeout(kSocketReceiveTimeout, settings.receiveTimeOutUsec) != 0 ||
            ConnectSocket(socket, guestAddress.data(), addressLength, settings.connectTimeOutUsec) != 0) {
            request->lastError = ERROR_NETWORK;
            sceNetSocketClose(socket);
            socket = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(addresses);
    if (socket < 0) return ERROR_NETWORK;

    int ssl = -1;
    if (url.secure) {
        ssl = sceSslCreateConnection(context->sslContextId, socket, url.host.c_str());
        if (ssl < 0) {
            request->lastError = ssl;
            sceNetSocketClose(socket);
            return ERROR_NETWORK;
        }
        if (settings.sslCallback) {
            auto callback = reinterpret_cast<int (APS5_VABI *)(int, unsigned int, void**, int, void*)>(settings.sslCallback);
            const int result = sceSslSetVerifyCallback(ssl, callback, settings.sslCallbackArgument);
            if (result != 0) {
                request->lastError = result;
                sceSslDeleteConnection(ssl);
                sceNetSocketClose(socket);
                return ERROR_NETWORK;
            }
        }
        if (const int result = sceSslConnect(ssl); result != 0) {
            request->lastError = result;
            sceSslDeleteConnection(ssl);
            sceNetSocketClose(socket);
            return ERROR_NETWORK;
        }
    }
    stream = std::make_unique<NetworkStream>(socket, ssl, url.secure);
    return 0;
}

int ReadRequestHeaders(RequestData& request, const std::shared_ptr<RequestData>& shared, const Url& url,
    const TransportSettings& settings, const void* body, const std::size_t bodySize) {
    if (const int result = CreateSocketStream(shared, url, settings, request.stream); result != 0) return result;
    std::string requestBytes;
    if (const int result = BuildRequest(request, settings, body, bodySize, requestBytes); result != 0) return result;
    if (const int result = SendBytes(*request.stream, requestBytes); result != 0) {
        request.lastError = result;
        return ERROR_NETWORK;
    }
    if (bodySize) {
        if (const int result = request.stream->Write(body, bodySize); result != 0) {
            request.lastError = result;
            return ERROR_NETWORK;
        }
    }
    const auto maxHeader = static_cast<std::size_t>(std::min<std::uint64_t>(settings.responseHeaderMaxSize,
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())));
    if (maxHeader == 0) return ERROR_INVALID_VALUE;
    const int result = ParseResponseHeaders(request, *request.stream, maxHeader);
    if (result != 0) {
        request.lastError = result;
        return result == ERROR_PARSE_HTTP_INVALID_RESPONSE ? result : ERROR_NETWORK;
    }
    return 0;
}

bool SameOrigin(const Url& left, const Url& right) {
    return left.secure == right.secure && left.port == right.port && LowerAscii(left.host) == LowerAscii(right.host);
}

int ParseRedirectUrl(const Url& current, const std::string& location, Url& result) {
    const std::string base = current.scheme + "://" + (current.host.find(':') != std::string::npos ? "[" + current.host + "]" : current.host) +
        (current.port != (current.secure ? 443 : 80) ? ":" + std::to_string(current.port) : "") + current.target;
    std::size_t required = 0;
    if (const int code = sceHttpUriMerge(nullptr, base.c_str(), location.c_str(), &required, 0, 0); code != 0) return code;
    std::vector<char> merged(required);
    if (const int code = sceHttpUriMerge(merged.data(), base.c_str(), location.c_str(), nullptr, merged.size(), 0); code != 0) return code;
    return ParseUrl(merged.data(), result);
}

int PerformSend(const std::shared_ptr<RequestData>& request, const HttpOptions& options, const void* body,
    const std::size_t bodySize) {
    if (options.nonblock) return ERROR_INVALID_VALUE;
    if (bodySize && !body) return ERROR_INVALID_VALUE;
    if (request->contentLength != bodySize && (request->contentLength != 0 || bodySize != 0)) return ERROR_INVALID_VALUE;
    Url current = request->url;
    std::string method = request->method;
    const void* currentBody = body;
    std::size_t currentBodySize = bodySize;
    for (int redirects = 0; redirects <= 10; ++redirects) {
        request->url = current;
        request->method = method;
        const int result = ReadRequestHeaders(*request, request, current, request->settings, currentBody, currentBodySize);
        if (result != 0) {
            request->stream.reset();
            return result;
        }
        std::string location;
        for (const auto& header : request->responseHeaders) {
            if (LowerAscii(header.name) == "location") {
                location = header.value;
                break;
            }
        }
        if (!options.autoRedirect || location.empty() ||
            (request->statusCode != 301 && request->statusCode != 302 && request->statusCode != 303 &&
             request->statusCode != 307 && request->statusCode != 308)) return 0;
        if (redirects == 10) {
            request->lastError = ERROR_INVALID_VALUE;
            request->stream.reset();
            return ERROR_INVALID_VALUE;
        }
        Url next;
        if (const int code = ParseRedirectUrl(current, location, next); code != 0) {
            request->lastError = code;
            request->stream.reset();
            return code;
        }
        if (!SameOrigin(current, next)) {
            request->requestHeaders.erase(std::remove_if(request->requestHeaders.begin(), request->requestHeaders.end(),
                [](const Header& header) {
                    const auto name = LowerAscii(header.name);
                    return name == "authorization" || name == "proxy-authorization" || name == "cookie" || name == "host";
                }), request->requestHeaders.end());
        }
        const int redirectStatus = request->statusCode;
        request->stream.reset();
        request->pending.clear();
        request->pendingOffset = 0;
        request->responseHeaders.clear();
        request->rawResponseHeaders.clear();
        request->statusCode = 0;
        request->bodyDone = false;
        request->chunked = false;
        request->contentLengthKnown = false;
        request->contentLength = 0;
        request->responseContentLength = 0;
        request->contentRemaining = 0;
        request->chunkRemaining = 0;
        request->needChunkTerminator = false;
        if (redirectStatus == 303 || ((redirectStatus == 301 || redirectStatus == 302) && method == "POST")) {
            method = "GET";
            currentBody = nullptr;
            currentBodySize = 0;
            request->contentLength = 0;
        }
        current = std::move(next);
    }
    return ERROR_NETWORK;
}

int ReadChunkedBody(RequestData& request, void* output, const std::size_t size) {
    auto* bytes = static_cast<unsigned char*>(output);
    std::size_t written = 0;
    while (written < size && !request.bodyDone) {
        if (request.needChunkTerminator) {
            unsigned char ending[2]{};
            for (std::size_t i = 0; i < sizeof(ending);) {
                const int count = ReadPending(*request.stream, request.pending, request.pendingOffset, ending + i, sizeof(ending) - i);
                if (count <= 0) return ERROR_NETWORK;
                i += static_cast<std::size_t>(count);
            }
            if (ending[0] != '\r' || ending[1] != '\n') return ERROR_PARSE_HTTP_INVALID_RESPONSE;
            request.needChunkTerminator = false;
        }
        if (request.chunkRemaining == 0) {
            std::string line;
            const int lineResult = ReadLine(*request.stream, request.pending, request.pendingOffset, line, kMaximumChunkLine);
            if (lineResult != 0) return lineResult;
            if (const int parseResult = ParseChunkSize(line, request.chunkRemaining); parseResult != 0) return parseResult;
            if (request.chunkRemaining == 0) {
                for (;;) {
                    if (ReadLine(*request.stream, request.pending, request.pendingOffset, line, kMaximumChunkLine) != 0) return ERROR_NETWORK;
                    if (line.empty()) break;
                }
                request.bodyDone = true;
                request.stream.reset();
                break;
            }
        }
        const auto wanted = static_cast<std::size_t>(std::min<std::uint64_t>(size - written, request.chunkRemaining));
        const int count = ReadPending(*request.stream, request.pending, request.pendingOffset, bytes + written, wanted);
        if (count <= 0) return ERROR_NETWORK;
        written += static_cast<std::size_t>(count);
        request.chunkRemaining -= static_cast<std::uint64_t>(count);
        if (request.chunkRemaining == 0) request.needChunkTerminator = true;
    }
    return static_cast<int>(written);
}

}

void AddHttpTransportContext(const int contextId, const int memId, const int sslContextId) {
    auto context = std::make_shared<ContextData>(ContextData{memId, sslContextId});
    std::lock_guard lock(g_objectsMutex);
    g_contexts[contextId] = std::move(context);
}

void RemoveHttpTransportContext(const int contextId) {
    std::lock_guard lock(g_objectsMutex);
    const auto context = g_contexts.find(contextId);
    if (context == g_contexts.end()) return;
    const auto data = context->second;
    g_contexts.erase(context);
    for (auto it = g_requests.begin(); it != g_requests.end();) {
        if (it->second->connection->httpTemplate->context == data) it = g_requests.erase(it);
        else ++it;
    }
    for (auto it = g_connections.begin(); it != g_connections.end();) {
        if (it->second->httpTemplate->context == data) it = g_connections.erase(it);
        else ++it;
    }
    for (auto it = g_templates.begin(); it != g_templates.end();) {
        if (it->second->context == data) it = g_templates.erase(it);
        else ++it;
    }
}

int AddHttpTransportTemplate(const int templateId, const int contextId, const char* userAgent, const int httpVersion) {
    if (!userAgent) return ERROR_INVALID_VALUE;
    std::lock_guard lock(g_objectsMutex);
    const auto context = g_contexts.find(contextId);
    if (context == g_contexts.end()) return ERROR_INVALID_ID;
    TransportSettings settings;
    settings.userAgent = userAgent;
    settings.httpVersion = httpVersion;
    g_templates[templateId] = std::make_shared<TemplateData>(TemplateData{templateId, context->second, std::move(settings)});
    return 0;
}

int AddHttpTransportConnection(const int connectionId, const int templateId, const char* serverName, const char* scheme,
    const std::uint16_t port, const bool keepAlive) {
    if (const auto options = FindHttpOptions(connectionId); !options) return ERROR_INVALID_ID;
    Url url;
    if (const int result = MakeUrl(serverName, scheme, port, url); result != 0) return result;
    std::lock_guard lock(g_objectsMutex);
    const auto found = g_templates.find(templateId);
    if (found == g_templates.end()) return ERROR_INVALID_ID;
    g_connections[connectionId] = std::make_shared<ConnectionData>(ConnectionData{connectionId, found->second, std::move(url), keepAlive, found->second->settings});
    return 0;
}

int AddHttpTransportConnectionWithUrl(const int connectionId, const int templateId, const char* urlText, const bool keepAlive) {
    if (const auto options = FindHttpOptions(connectionId); !options) return ERROR_INVALID_ID;
    Url url;
    if (const int result = ParseUrl(urlText, url); result != 0) return result;
    std::lock_guard lock(g_objectsMutex);
    const auto found = g_templates.find(templateId);
    if (found == g_templates.end()) return ERROR_INVALID_ID;
    g_connections[connectionId] = std::make_shared<ConnectionData>(ConnectionData{connectionId, found->second, std::move(url), keepAlive, found->second->settings});
    return 0;
}

int AddHttpTransportRequest(const int requestId, const int connectionId, const int method, const char* path,
    const std::uint64_t contentLength) {
    if (!path) return ERROR_INVALID_VALUE;
    static constexpr std::array<std::string_view, 8> methods{"GET", "POST", "HEAD", "OPTIONS", "PUT", "DELETE", "TRACE", "CONNECT"};
    if (method < 0 || static_cast<std::size_t>(method) >= methods.size()) return ERROR_INVALID_VALUE;
    std::lock_guard lock(g_objectsMutex);
    const auto found = g_connections.find(connectionId);
    if (found == g_connections.end()) return ERROR_INVALID_ID;
    Url url = found->second->url;
    url.target = path[0] ? path : "/";
    if (!ValidRequestTarget(url.target)) return ERROR_INVALID_URL;
    g_requests[requestId] = std::make_shared<RequestData>(requestId, requestId, found->second, std::move(url),
        std::string(methods[static_cast<std::size_t>(method)]), contentLength, found->second->settings);
    return 0;
}

int AddHttpTransportRequestWithMethodAndPath(const int requestId, const int connectionId, const char* method, const char* path,
    const std::uint64_t contentLength) {
    if (!method || method[0] == '\0' || !path) return ERROR_INVALID_VALUE;
    if (!std::all_of(method, method + std::strlen(method), IsToken)) return ERROR_INVALID_VALUE;
    std::lock_guard lock(g_objectsMutex);
    const auto found = g_connections.find(connectionId);
    if (found == g_connections.end()) return ERROR_INVALID_ID;
    Url url = found->second->url;
    url.target = path[0] ? path : "/";
    if (!ValidRequestTarget(url.target)) return ERROR_INVALID_URL;
    g_requests[requestId] = std::make_shared<RequestData>(requestId, requestId, found->second, std::move(url), method, contentLength, found->second->settings);
    return 0;
}

int AddHttpTransportRequestWithMethod(const int requestId, const int connectionId, const char* method, const char* urlText,
    const std::uint64_t contentLength) {
    if (!method || method[0] == '\0') return ERROR_INVALID_VALUE;
    if (!std::all_of(method, method + std::strlen(method), IsToken)) return ERROR_INVALID_VALUE;
    Url url;
    if (const int result = ParseUrl(urlText, url); result != 0) return result;
    std::lock_guard lock(g_objectsMutex);
    const auto found = g_connections.find(connectionId);
    if (found == g_connections.end()) return ERROR_INVALID_ID;
    g_requests[requestId] = std::make_shared<RequestData>(requestId, requestId, found->second, std::move(url), method, contentLength, found->second->settings);
    return 0;
}

void RemoveHttpTransportTemplate(const int templateId) {
    std::lock_guard lock(g_objectsMutex);
    g_templates.erase(templateId);
}

void RemoveHttpTransportConnection(const int connectionId) {
    std::lock_guard lock(g_objectsMutex);
    g_connections.erase(connectionId);
}

void RemoveHttpTransportRequest(const int requestId) {
    std::lock_guard lock(g_objectsMutex);
    g_requests.erase(requestId);
}

int AddHttpRequestHeader(const int requestId, const char* name, const char* value, const std::uint32_t mode) {
    if (const auto request = FindRequest(requestId)) {
        std::lock_guard lock(request->mutex);
        if (request->sent) return ERROR_INVALID_VALUE;
        return AppendHeader(request->requestHeaders, name, value, mode);
    }
    return ERROR_INVALID_ID;
}

int AddHttpRequestHeaderRaw(const int requestId, const char* header, const int mode) {
    if (!header) return ERROR_INVALID_VALUE;
    const std::string_view line(header);
    const auto colon = line.find(':');
    if (colon == std::string_view::npos) return ERROR_INVALID_VALUE;
    const std::string name(Trim(line.substr(0, colon)));
    const std::string value(Trim(line.substr(colon + 1)));
    return AddHttpRequestHeader(requestId, name.c_str(), value.c_str(), static_cast<std::uint32_t>(mode));
}

int SetHttpRequestContentLength(const int requestId, const std::uint64_t contentLength) {
    const auto request = FindRequest(requestId);
    if (!request) return ERROR_INVALID_ID;
    std::lock_guard lock(request->mutex);
    if (request->sent) return ERROR_INVALID_VALUE;
    request->contentLength = contentLength;
    return 0;
}

bool SetHttpTransportTimeout(const int id, const int kind, const std::uint32_t usec) {
    std::lock_guard lock(g_objectsMutex);
    const auto apply = [kind, usec](TransportSettings& settings) {
        switch (kind) {
        case 0: settings.resolveTimeOutUsec = usec; break;
        case 1: settings.connectTimeOutUsec = usec; break;
        case 2: settings.sendTimeOutUsec = usec; break;
        case 3: settings.receiveTimeOutUsec = usec; break;
        default: return false;
        }
        return true;
    };
    if (const auto request = g_requests.find(id); request != g_requests.end()) {
        std::lock_guard requestLock(request->second->mutex);
        return apply(request->second->settings);
    }
    if (const auto connection = g_connections.find(id); connection != g_connections.end()) return apply(connection->second->settings);
    if (const auto httpTemplate = g_templates.find(id); httpTemplate != g_templates.end()) return apply(httpTemplate->second->settings);
    return false;
}

bool SetHttpTransportSslCallback(const int id, HttpsCallback callback, void* argument) {
    std::lock_guard lock(g_objectsMutex);
    const auto set = [callback, argument](TransportSettings& settings) {
        settings.sslCallback = callback;
        settings.sslCallbackArgument = argument;
    };
    if (const auto request = g_requests.find(id); request != g_requests.end()) {
        std::lock_guard requestLock(request->second->mutex);
        set(request->second->settings);
        return true;
    }
    if (const auto connection = g_connections.find(id); connection != g_connections.end()) {
        set(connection->second->settings);
        return true;
    }
    if (const auto httpTemplate = g_templates.find(id); httpTemplate != g_templates.end()) {
        set(httpTemplate->second->settings);
        return true;
    }
    return false;
}

bool SetHttpTransportResponseHeaderMaxSize(const int id, const std::uint64_t size) {
    std::lock_guard lock(g_objectsMutex);
    const auto set = [size](TransportSettings& settings) { settings.responseHeaderMaxSize = size; };
    if (const auto request = g_requests.find(id); request != g_requests.end()) {
        std::lock_guard requestLock(request->second->mutex);
        set(request->second->settings);
        return true;
    }
    if (const auto connection = g_connections.find(id); connection != g_connections.end()) {
        set(connection->second->settings);
        return true;
    }
    if (const auto httpTemplate = g_templates.find(id); httpTemplate != g_templates.end()) {
        set(httpTemplate->second->settings);
        return true;
    }
    return false;
}

int SendHttpRequest(const int requestId, const void* data, const std::size_t size) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    const auto request = FindRequest(requestId);
    if (!request) return ERROR_INVALID_ID;
    std::lock_guard lock(request->mutex);
    if (request->sent) return ERROR_INVALID_VALUE;
    const auto options = FindHttpOptions(request->optionsId);
    if (!options) return ERROR_INVALID_ID;
    request->sent = true;
    if (const int result = PerformSend(request, *options, data, size); result != 0) {
        if (!request->lastError) request->lastError = result;
        request->stream.reset();
        return result;
    }
    if (request->bodyDone) request->stream.reset();
    return 0;
}

int GetHttpRequestStatusCode(const int requestId, int* statusCode) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!statusCode) return ERROR_INVALID_VALUE;
    const auto request = FindRequest(requestId);
    if (!request) return ERROR_INVALID_ID;
    std::lock_guard lock(request->mutex);
    if (!request->sent || request->statusCode == 0) return ERROR_INVALID_VALUE;
    *statusCode = request->statusCode;
    return 0;
}

int GetHttpRequestContentLength(const int requestId, int* result, std::uint64_t* contentLength) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!result || !contentLength) return ERROR_INVALID_VALUE;
    const auto request = FindRequest(requestId);
    if (!request) return ERROR_INVALID_ID;
    std::lock_guard lock(request->mutex);
    if (!request->sent || request->statusCode == 0) return ERROR_INVALID_VALUE;
    *result = request->contentLengthKnown ? 0 : request->chunked ? 2 : 1;
    *contentLength = request->responseContentLength;
    return 0;
}

int GetHttpRequestAllResponseHeaders(const int requestId, char** headers, std::size_t* size) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!headers || !size) return ERROR_INVALID_VALUE;
    const auto request = FindRequest(requestId);
    if (!request) return ERROR_INVALID_ID;
    std::lock_guard lock(request->mutex);
    if (!request->sent || request->statusCode == 0) return ERROR_INVALID_VALUE;
    *headers = request->rawResponseHeaders.empty() ? nullptr : request->rawResponseHeaders.data();
    *size = request->rawResponseHeaders.size();
    return 0;
}

int ReadHttpRequestData(const int requestId, void* buffer, std::size_t size) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!buffer && size) return ERROR_INVALID_VALUE;
    const auto request = FindRequest(requestId);
    if (!request) return ERROR_INVALID_ID;
    std::lock_guard lock(request->mutex);
    if (!request->sent || request->statusCode == 0) return ERROR_INVALID_VALUE;
    if (size == 0 || request->bodyDone) return 0;
    const auto limit = std::min<std::size_t>(size, static_cast<std::size_t>(std::numeric_limits<int>::max()));
    if (request->chunked) {
        const int result = ReadChunkedBody(*request, buffer, limit);
        if (result < 0) {
            request->lastError = result;
            return ERROR_NETWORK;
        }
        return result;
    }
    if (!request->stream) return 0;
    const auto wanted = request->contentLengthKnown ? static_cast<std::size_t>(std::min<std::uint64_t>(limit, request->contentRemaining)) : limit;
    if (wanted == 0) {
        request->bodyDone = true;
        request->stream.reset();
        return 0;
    }
    const int result = ReadPending(*request->stream, request->pending, request->pendingOffset, buffer, wanted);
    if (result < 0) {
        request->lastError = result;
        return ERROR_NETWORK;
    }
    if (result == 0) {
        if (request->contentLengthKnown && request->contentRemaining) {
            request->lastError = ERROR_NETWORK;
            return ERROR_NETWORK;
        }
        request->bodyDone = true;
        request->stream.reset();
        return 0;
    }
    if (request->contentLengthKnown) {
        request->contentRemaining -= static_cast<std::uint64_t>(result);
        if (request->contentRemaining == 0) {
            request->bodyDone = true;
            request->stream.reset();
        }
    }
    return result;
}

int GetHttpRequestLastErrno(const int requestId, int* error) {
    if (!HttpInitialised()) return ERROR_BEFORE_INIT;
    if (!error) return ERROR_INVALID_VALUE;
    const auto request = FindRequest(requestId);
    if (!request) return ERROR_INVALID_ID;
    std::lock_guard lock(request->mutex);
    *error = request->lastError;
    return 0;
}
