#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "prx/libSceHttp/include/Client.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <limits>
#include <memory>
#include <iomanip>
#include <locale>
#include <map>
#include <set>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class HttpFailure : public std::runtime_error {
public:
    explicit HttpFailure(const char* message) : std::runtime_error(message) {}
};

std::string Lower(std::string text) {
    for (char& byte : text) if (byte >= 'A' && byte <= 'Z') byte += 'a' - 'A';
    return text;
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

std::uint64_t Number(std::string_view text, int base = 10) {
    std::uint64_t number = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), number, base);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) throw HttpFailure("invalid HTTP size");
    return number;
}

std::optional<std::chrono::sys_seconds> HttpDate(const std::string& text) {
    std::tm time{};
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    input >> std::get_time(&time, "%a, %d %b %Y %H:%M:%S GMT");
    if (input.fail() || input.peek() != std::char_traits<char>::eof()) return {};
    const std::chrono::year_month_day day{std::chrono::year{time.tm_year + 1900}, std::chrono::month{static_cast<unsigned>(time.tm_mon + 1)}, std::chrono::day{static_cast<unsigned>(time.tm_mday)}};
    if (!day.ok() || time.tm_hour > 23 || time.tm_min > 59 || time.tm_sec > 60) return {};
    return std::chrono::sys_days{day} + std::chrono::hours{time.tm_hour} + std::chrono::minutes{time.tm_min} + std::chrono::seconds{time.tm_sec};
}

struct Url {
    std::string authority;
    std::string host;
    std::string port;
    std::string target;
};

Url Parse(const std::string& text) {
    if (Lower(text.substr(0, 8)) == "https://") NotImplemented_nid_no_patch("HTTP client HTTPS sources");
    if (Lower(text.substr(0, 7)) != "http://") throw HttpFailure("unsupported HTTP source scheme");
    for (const unsigned char byte : text) {
        if (byte >= 0x80) NotImplemented_nid_no_patch("HTTP client non-ASCII HTTP URLs");
        if (byte <= 0x20 || byte == 0x7f) throw HttpFailure("invalid HTTP URL byte");
    }
    const auto end = text.find_first_of("/?#", 7);
    Url url;
    url.authority = text.substr(7, end == std::string::npos ? end : end - 7);
    if (url.authority.find('@') != std::string::npos) NotImplemented_nid_no_patch("HTTP client HTTP URL user information");
    if (url.authority.empty()) throw HttpFailure("invalid HTTP authority");
    url.port = "80";
    std::size_t colon;
    if (url.authority.front() == '[') {
        const auto close = url.authority.find(']');
        if (close == std::string::npos) throw HttpFailure("invalid IPv6 HTTP authority");
        url.host = url.authority.substr(1, close - 1);
        colon = close + 1;
        if (colon < url.authority.size() && url.authority[colon] != ':') throw HttpFailure("invalid HTTP port");
    } else {
        colon = url.authority.find(':');
        url.host = url.authority.substr(0, colon);
    }
    if (colon != std::string::npos && colon < url.authority.size()) {
        url.port = url.authority.substr(colon + 1);
        const auto port = Number(url.port);
        if (port == 0 || port > 65535) throw HttpFailure("invalid HTTP port");
    }
    if (url.host.empty()) throw HttpFailure("empty HTTP host");
    url.target = end == std::string::npos ? "/" : text.substr(end);
    url.target.erase(url.target.find('#') == std::string::npos ? url.target.size() : url.target.find('#'));
    if (url.target.empty() || url.target.front() != '/') url.target.insert(0, "/");
    return url;
}

std::string Redirect(const Url& base, std::string location) {
    const auto fragment = location.find('#');
    if (fragment != std::string::npos) location.erase(fragment);
    if (location.find(':') < location.find_first_of("/?")) return location;
    if (location.starts_with("//")) return "http:" + location;
    std::string target;
    if (location.empty()) target = base.target;
    else if (location.front() == '/') target = location;
    else if (location.front() == '?') target = base.target.substr(0, base.target.find('?')) + location;
    else {
        target = base.target.substr(0, base.target.find('?'));
        target.erase(target.rfind('/') + 1);
        target += location;
    }
    const auto queryAt = target.find('?');
    const std::string query = queryAt == std::string::npos ? "" : target.substr(queryAt);
    std::string input = target.substr(0, queryAt);
    std::string output;
    while (!input.empty()) {
        if (input.starts_with("../")) input.erase(0, 3);
        else if (input.starts_with("./")) input.erase(0, 2);
        else if (input.starts_with("/./")) input.erase(0, 2);
        else if (input == "/.") input = "/";
        else if (input.starts_with("/../") || input == "/..") {
            input.erase(0, 3);
            if (input.empty()) input = "/";
            const auto last = output.rfind('/');
            output.erase(last == std::string::npos ? 0 : last);
        } else if (input == "." || input == "..") input.clear();
        else {
            const auto next = input.find('/', input.front() == '/' ? 1 : 0);
            output += input.substr(0, next);
            input.erase(0, next == std::string::npos ? input.size() : next);
        }
    }
    return "http://" + base.authority + output + query;
}

class Connection {
public:
    Connection(const Url& url, std::atomic_bool& stop) : stop(stop) {
        addrinfo hint{};
        hint.ai_socktype = SOCK_STREAM;
        addrinfo* raw = nullptr;
        if (getaddrinfo(url.host.c_str(), url.port.c_str(), &hint, &raw) != 0) throw HttpFailure("HTTP name resolution failed");
        const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addresses(raw, freeaddrinfo);
        for (auto* address = raw; address != nullptr; address = address->ai_next) {
            socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (socket == invalid) continue;
#ifdef _WIN32
            u_long enabled = 1;
            if (ioctlsocket(socket, FIONBIO, &enabled) != 0) { Close(); continue; }
#else
            if (fcntl(socket, F_SETFL, fcntl(socket, F_GETFL, 0) | O_NONBLOCK) < 0) { Close(); continue; }
#endif
#ifdef SO_NOSIGPIPE
            const int enabled = 1;
            setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
            if (::connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen)) == 0) return;
#ifdef _WIN32
            const int connectError = WSAGetLastError();
            const bool pending = connectError == WSAEWOULDBLOCK || connectError == WSAEINPROGRESS || connectError == WSAEINTR;
#else
            const bool pending = errno == EINPROGRESS || errno == EINTR;
#endif
            if (pending) {
                try {
                    Wait(true);
                    int error = 0;
#ifdef _WIN32
                    int length = sizeof(error);
                    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == 0 && error == 0) return;
#else
                    socklen_t length = sizeof(error);
                    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &length) == 0 && error == 0) return;
#endif
                } catch (...) {
                    Close();
                    throw;
                }
            }
            Close();
        }
        throw HttpFailure("HTTP connection failed");
    }

    ~Connection() {
        Close();
    }

    void Send(const std::string& data) {
        std::size_t sent = 0;
        while (sent < data.size()) {
            Wait(true);
#ifdef MSG_NOSIGNAL
            constexpr int flags = MSG_NOSIGNAL;
#else
            constexpr int flags = 0;
#endif
            const auto length = static_cast<int>(std::min<std::size_t>(data.size() - sent, std::numeric_limits<int>::max()));
            const auto result = ::send(socket, data.data() + sent, length, flags);
            if (result < 0 && Retry()) continue;
            if (result <= 0) throw HttpFailure("HTTP request failed");
            sent += static_cast<std::size_t>(result);
        }
    }

    std::string Line() {
        for (;;) {
            const auto end = pending.find("\r\n");
            if (end != std::string::npos) {
                auto line = pending.substr(0, end);
                pending.erase(0, end + 2);
                return line;
            }
            if (!Receive()) throw HttpFailure("truncated HTTP headers");
        }
    }

    void Append(std::vector<std::uint8_t>& body, std::uint64_t size) {
        while (size != 0) {
            if (pending.empty() && !Receive()) throw HttpFailure("truncated HTTP body");
            const auto take = static_cast<std::size_t>(std::min<std::uint64_t>(size, pending.size()));
            body.insert(body.end(), pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(take));
            pending.erase(0, take);
            size -= take;
        }
    }

    void UntilClosed(std::vector<std::uint8_t>& body) {
        do {
            body.insert(body.end(), pending.begin(), pending.end());
            pending.clear();
        } while (Receive());
    }

private:
    bool Receive() {
        std::array<char, 8192> bytes{};
        for (;;) {
            Wait(false);
            const auto count = ::recv(socket, bytes.data(), static_cast<int>(bytes.size()), 0);
            if (count < 0 && Retry()) continue;
            if (count < 0) throw HttpFailure("HTTP read failed");
            pending.append(bytes.data(), static_cast<std::size_t>(count));
            return count != 0;
        }
    }

    static bool Retry() {
#ifdef _WIN32
        const int error = WSAGetLastError();
        return error == WSAEWOULDBLOCK || error == WSAEINTR;
#else
        return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
    }

    void Wait(bool write) {
        for (;;) {
            if (stop.load()) throw HttpFailure("HTTP source stopped");
#ifdef _WIN32
            WSAPOLLFD entry{socket, static_cast<short>(write ? POLLWRNORM : POLLRDNORM), 0};
            const int result = WSAPoll(&entry, 1, 100);
#else
            pollfd entry{socket, static_cast<short>(write ? POLLOUT : POLLIN), 0};
            const int result = poll(&entry, 1, 100);
#endif
            if (result > 0) return;
            if (result < 0 && !Retry()) throw HttpFailure("HTTP socket polling failed");
        }
    }

    void Close() {
        if (socket == invalid) return;
#ifdef _WIN32
        closesocket(socket);
#else
        ::close(socket);
#endif
        socket = invalid;
    }
#ifdef _WIN32
    struct Runtime {
        Runtime() {
            WSADATA data{};
            if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw HttpFailure("socket initialization failed");
        }
        ~Runtime() { WSACleanup(); }
    } runtime;
    static constexpr SOCKET invalid = INVALID_SOCKET;
    SOCKET socket = invalid;
#else
    static constexpr int invalid = -1;
    int socket = invalid;
#endif
    std::string pending;
    std::atomic_bool& stop;
};

struct HttpClientState : Aps5HttpClient {
    std::string url;
    std::string validator;
    std::string etag;
    std::string modified;
    std::vector<std::uint8_t> cache;
    std::uint64_t cacheStart = 0;
    std::uint64_t position = 0;
    std::uint64_t size = 0;
    bool initialized = false;
    std::atomic_bool stopped = false;

    void Reset(const char* source) {
        url = source;
        validator.clear();
        etag.clear();
        modified.clear();
        cache.clear();
        cacheStart = 0;
        position = 0;
        size = 0;
        initialized = false;
    }

    void CheckStopped() const {
        if (stopped.load()) throw HttpFailure("HTTP client stopped");
    }

    void Fetch(std::uint64_t start, std::uint64_t length) {
        CheckStopped();
        std::set<std::string> visited;
        for (;;) {
            if (!visited.insert(url).second) throw HttpFailure("HTTP redirect loop");
            const auto parsed = Parse(url);
            Connection connection(parsed, stopped);
            connection.Send("GET " + parsed.target + " HTTP/1.1\r\nHost: " + parsed.authority + "\r\nRange: bytes=" + std::to_string(start) + "-" + std::to_string(start + length - 1) + (validator.empty() ? "" : "\r\nIf-Range: " + validator) + "\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
            std::map<std::string, std::string> headers;
            int status;
            for (;;) {
                const auto line = connection.Line();
                if (line.size() < 12 || (!line.starts_with("HTTP/1.1 ") && !line.starts_with("HTTP/1.0 "))) throw HttpFailure("invalid HTTP status line");
                status = static_cast<int>(Number(std::string_view(line).substr(9, 3)));
                headers.clear();
                for (;;) {
                    const auto field = connection.Line();
                    if (field.empty()) break;
                    const auto colon = field.find(':');
                    if (colon == std::string::npos || colon == 0) throw HttpFailure("invalid HTTP header");
                    const auto name = Lower(field.substr(0, colon));
                    const std::string value(Trim(std::string_view(field).substr(colon + 1)));
                    for (const unsigned char byte : value) if ((byte < 0x20 && byte != '\t') || byte == 0x7f) throw HttpFailure("invalid HTTP header value");
                    if (name != "content-range" && name != "content-length" && name != "content-encoding" && name != "transfer-encoding" && name != "location" && name != "etag" && name != "last-modified" && name != "date") continue;
                    const auto [found, inserted] = headers.emplace(name, value);
                    if (!inserted && found->second != value) throw HttpFailure("conflicting HTTP headers");
                }
                if (status < 100 || status >= 200) break;
                if (status == 101) throw HttpFailure("HTTP upgrade is not a media file");
            }
            if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
                if (!headers.contains("location")) throw HttpFailure("HTTP redirect has no location");
                url = Redirect(parsed, headers.at("location"));
                continue;
            }
            if (status != 200 && status != 206) throw HttpFailure("HTTP source request was refused");
            if (headers.contains("content-encoding") && Lower(headers.at("content-encoding")) != "identity") NotImplemented_nid_no_patch("HTTP client encoded HTTP representations");
            std::vector<std::uint8_t> body;
            if (headers.contains("transfer-encoding")) {
                if (Lower(headers.at("transfer-encoding")) != "chunked") NotImplemented_nid_no_patch("HTTP client HTTP transfer coding");
                for (;;) {
                    const auto chunk = connection.Line();
                    const auto count = Number(Trim(std::string_view(chunk).substr(0, chunk.find(';'))), 16);
                    if (count == 0) {
                        while (!connection.Line().empty()) {}
                        break;
                    }
                    connection.Append(body, count);
                    if (!connection.Line().empty()) throw HttpFailure("invalid HTTP chunk terminator");
                }
            } else if (headers.contains("content-length")) connection.Append(body, Number(headers.at("content-length")));
            else connection.UntilClosed(body);
            std::uint64_t total = body.size();
            std::uint64_t first = 0;
            if (status == 206) {
                if (!headers.contains("content-range")) throw HttpFailure("HTTP range response has no bounds");
                const auto& range = headers.at("content-range");
                const auto dash = range.find('-');
                const auto slash = range.find('/');
                if (!range.starts_with("bytes ") || dash == std::string::npos || slash <= dash) throw HttpFailure("invalid HTTP content range");
                first = Number(std::string_view(range).substr(6, dash - 6));
                const auto last = Number(std::string_view(range).substr(dash + 1, slash - dash - 1));
                total = Number(std::string_view(range).substr(slash + 1));
                if (first != start || last < first || last >= total || last - first + 1 != body.size() || body.size() > length) throw HttpFailure("HTTP range response does not match request");
            }
            if (total > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) || (initialized && total != size)) throw HttpFailure("HTTP source size changed or is not representable");
            if (initialized && ((!etag.empty() && headers.contains("etag") && headers.at("etag") != etag) || (!modified.empty() && headers.contains("last-modified") && headers.at("last-modified") != modified))) throw HttpFailure("HTTP source representation changed");
            if (!initialized) {
                if (headers.contains("etag")) etag = headers.at("etag");
                if (headers.contains("last-modified")) modified = headers.at("last-modified");
                if (etag.starts_with('"') && etag.ends_with('"')) validator = etag;
                else if (etag.empty() && headers.contains("date")) {
                    const auto date = HttpDate(headers.at("date"));
                    const auto last = HttpDate(modified);
                    if (date && last && *date - *last >= std::chrono::seconds{60}) validator = modified;
                }
            }
            CheckStopped();
            cache = std::move(body);
            cacheStart = first;
            size = total;
            initialized = true;
            return;
        }
    }

};

HttpClientState& Checked(Aps5HttpClient* client) {
    if (!client) throw std::invalid_argument("HTTP client is null");
    return *static_cast<HttpClientState*>(client);
}

const HttpClientState& Checked(const Aps5HttpClient* client) {
    if (!client) throw std::invalid_argument("HTTP client is null");
    return *static_cast<const HttpClientState*>(client);
}

}

extern "C" {

Aps5HttpClient* APS5_VABI Aps5HttpClient_Create_nid_postfix() {
    return new HttpClientState;
}

void APS5_VABI Aps5HttpClient_Destroy_nid_postfix(Aps5HttpClient* client) {
    delete static_cast<HttpClientState*>(client);
}

std::int32_t APS5_VABI Aps5HttpClient_Open_nid_postfix(Aps5HttpClient* client, const char* url) {
    auto& state = Checked(client);
    if (!url) throw std::invalid_argument("HTTP client URL is null");
    state.Reset(url);
    try {
        state.Fetch(0, 65536);
        return 1;
    } catch (const HttpFailure&) {
        return 0;
    }
}

std::int32_t APS5_VABI Aps5HttpClient_Read_nid_postfix(Aps5HttpClient* client, std::uint8_t* destination, std::int32_t capacity) {
    auto& state = Checked(client);
    if (capacity < 0) throw std::invalid_argument("HTTP client read capacity is negative");
    if (capacity == 0) return 0;
    if (!destination) throw std::invalid_argument("HTTP client read destination is null");
    if (!state.initialized) throw HttpFailure("HTTP client is not open");
    state.CheckStopped();
    if (state.position >= state.size) return 0;
    if (state.position < state.cacheStart || state.position - state.cacheStart >= state.cache.size())
        state.Fetch(state.position, std::min<std::uint64_t>(65536, state.size - state.position));
    const auto offset = static_cast<std::size_t>(state.position - state.cacheStart);
    const auto count = std::min<std::size_t>(static_cast<std::size_t>(capacity), state.cache.size() - offset);
    if (count == 0) throw HttpFailure("HTTP client read made no progress");
    std::memcpy(destination, state.cache.data() + offset, count);
    state.position += count;
    return static_cast<std::int32_t>(count);
}

std::int64_t APS5_VABI Aps5HttpClient_Seek_nid_postfix(Aps5HttpClient* client, std::int64_t offset, std::int32_t origin) {
    auto& state = Checked(client);
    if (!state.initialized) return -1;
    std::int64_t base;
    if (origin == 0) base = 0;
    else if (origin == 1) base = static_cast<std::int64_t>(state.position);
    else if (origin == 2) base = static_cast<std::int64_t>(state.size);
    else return -1;
    if (offset < -base || offset > std::numeric_limits<std::int64_t>::max() - base) return -1;
    state.position = static_cast<std::uint64_t>(base + offset);
    return static_cast<std::int64_t>(state.position);
}

std::int64_t APS5_VABI Aps5HttpClient_GetSize_nid_postfix(const Aps5HttpClient* client) {
    const auto& state = Checked(client);
    return state.initialized ? static_cast<std::int64_t>(state.size) : -1;
}

void APS5_VABI Aps5HttpClient_Start_nid_postfix(Aps5HttpClient* client) {
    Checked(client).stopped.store(false);
}

void APS5_VABI Aps5HttpClient_Stop_nid_postfix(Aps5HttpClient* client) {
    Checked(client).stopped.store(true);
}

}
