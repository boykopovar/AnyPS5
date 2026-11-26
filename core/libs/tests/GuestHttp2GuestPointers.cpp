#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "SceTypes.hpp"
#include "prx/libSceHttp2/include/Http2.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

constexpr int nullPointer = static_cast<int>(0x817B1225);
constexpr int invalidValue = static_cast<int>(0x817B11FE);
constexpr int savedErrno = 71;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "HTTP2 guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

#define RequireErrno(call, expected)  \
    do {                              \
        errno = savedErrno;           \
        Require((call) == (expected)); \
        Require(errno == savedErrno); \
    } while (0)

void APS5_VABI TextCallback(std::int32_t, const char*, std::uint32_t, void*) {}
void APS5_VABI DataCallback(std::int32_t, const void*, std::size_t, std::uint32_t, void*) {}
void APS5_VABI CloseCallback(std::int32_t, const std::uint16_t*, const char*, std::uint32_t, void*) {}
void APS5_VABI ErrorCallback(std::int32_t, std::int32_t, void*) {}

#ifdef _WIN32
void CloseSocket(SOCKET socket) {
    closesocket(socket);
}
#else
void CloseSocket(int socket) {
    close(socket);
}
#endif

std::string ServePlainHttpOnce(const std::string& response) {
#ifdef _WIN32
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Require(listener != INVALID_SOCKET);
#else
    const int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    Require(listener >= 0);
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    Require(bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0);
    Require(listen(listener, 8) == 0);
#ifdef _WIN32
    int size = sizeof(address);
#else
    socklen_t size = sizeof(address);
#endif
    Require(getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) == 0);
    const std::string url = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port)) + "/path";
    std::thread([listener, response] {
#ifdef _WIN32
        const SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            CloseSocket(listener);
            return;
        }
#else
        const int client = accept(listener, nullptr, nullptr);
        if (client < 0) {
            CloseSocket(listener);
            return;
        }
#endif
        std::array<char, 4096> input;
        std::string seen;
        while (seen.find("\r\n\r\n") == std::string::npos) {
            const auto got = recv(client, input.data(), static_cast<int>(input.size()), 0);
            if (got <= 0) break;
            seen.append(input.data(), static_cast<std::size_t>(got));
        }
        std::size_t sent = 0;
        while (sent < response.size()) {
            const auto wrote = send(client, response.data() + sent, static_cast<int>(response.size() - sent), 0);
            if (wrote <= 0) break;
            sent += static_cast<std::size_t>(wrote);
        }
        for (;;) {
            const auto drained = recv(client, input.data(), static_cast<int>(input.size()), 0);
            if (drained <= 0) break;
        }
        CloseSocket(client);
        CloseSocket(listener);
    }).detach();
    return url;
}

void UnmappedReadData(int tmpl) {
    const std::string url = ServePlainHttpOnce("HTTP/1.1 200 OK\r\nContent-Length: 32\r\nConnection: close\r\n\r\n" + std::string(32, 'B'));
    const int request = sceHttp2CreateRequestWithURL(tmpl, "GET", url.c_str(), 0);
    Require(request > 0);
    Require(sceHttp2SendRequest(request, nullptr, 0) == 0);
    auto* const unmapped = reinterpret_cast<void*>(0x10);
    RequireErrno(sceHttp2ReadData(request, unmapped, 16), invalidValue);
    Require(sceHttp2ReadData(request, unmapped, 16) == invalidValue);
    Require(sceHttp2DeleteRequest(request) == 0);
}

void UnmappedSendRequest(int tmpl) {
    const std::string url = ServePlainHttpOnce("HTTP/1.1 200 OK\r\nContent-Length: 4\r\nConnection: close\r\n\r\ndone");
    const int request = sceHttp2CreateRequestWithURL(tmpl, "POST", url.c_str(), 8);
    Require(request > 0);
    auto* const unmapped = reinterpret_cast<const void*>(0x20);
    RequireErrno(sceHttp2SendRequest(request, unmapped, 8), invalidValue);
    std::array<char, 8> payload{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    Require(sceHttp2SendRequest(request, payload.data(), payload.size()) == 0);
    Require(sceHttp2DeleteRequest(request) == 0);
}

void UnmappedGzipReadData(int context) {
    const int tmpl = sceHttp2CreateTemplate(context, "gzip-agent", 3, 0);
    Require(tmpl > 0);
    Require(sceHttp2SetInflateGZIPEnabled(tmpl, 1) == 0);
    const std::array<unsigned char, 55> gzip{{
        0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        0x01, 0x20, 0x00, 0xdf, 0xff,
        'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B',
        'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B',
        0xbd, 0x31, 0x7d, 0x20, 0x20, 0x00, 0x00, 0x00}};
    const std::string body(gzip.begin(), gzip.end());
    const std::string url = ServePlainHttpOnce("HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 55\r\nConnection: close\r\n\r\n" + body);
    const int request = sceHttp2CreateRequestWithURL(tmpl, "GET", url.c_str(), 0);
    Require(request > 0);
    Require(sceHttp2SendRequest(request, nullptr, 0) == 0);
    auto* const unmapped = reinterpret_cast<void*>(0x30);
    RequireErrno(sceHttp2ReadData(request, unmapped, 16), invalidValue);
    Require(sceHttp2ReadData(request, unmapped, 16) == invalidValue);
    Require(sceHttp2DeleteRequest(request) == 0);
}

}

int main() {
    const int context = sceHttp2Init(1, 1, 0x10000, 4);
    Require(context > 0);
    const int tmpl = sceHttp2CreateTemplate(context, "agent", 3, 0);
    Require(tmpl > 0);
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0);

    auto* const unmapped = reinterpret_cast<void*>(0x10);
    int value = 0;

    RequireErrno(sceHttp2GetAuthEnabled(tmpl, static_cast<int*>(unmapped)), nullPointer);
    RequireErrno(sceHttp2GetAutoRedirect(tmpl, static_cast<int*>(unmapped)), nullPointer);
    RequireErrno(sceHttp2GetCookieBox(tmpl, static_cast<int*>(unmapped)), nullPointer);
    RequireErrno(sceHttp2GetCookieStats(box, static_cast<Http2CookieStats*>(unmapped)), invalidValue);
    RequireErrno(sceHttp2WebSocketCreateRequest(tmpl, "ws://127.0.0.1:8080/", TextCallback, DataCallback, CloseCallback, ErrorCallback, unmapped, 16, nullptr, nullptr), invalidValue);

    Require(sceHttp2GetAuthEnabled(tmpl, &value) == 0);
    Require(sceHttp2GetAutoRedirect(tmpl, &value) == 0);
    Http2CookieStats stats{};
    Require(sceHttp2GetCookieStats(box, &stats) == 0);

    const int request = sceHttp2CreateRequestWithURL(tmpl, "GET", "https://example.invalid/wait-async", 0);
    Require(request > 0);
#ifdef _WIN32
    _putenv("APS5_NET_DISCONNECTED=1");
#else
    setenv("APS5_NET_DISCONNECTED", "1", 1);
#endif
    Require(sceHttp2SendRequestAsync(request, nullptr, 0, nullptr, nullptr) == 0);
    RequireErrno(sceHttp2GetStatusCode(request, static_cast<int*>(unmapped)), nullPointer);
    RequireErrno(sceHttp2WaitAsync(request, static_cast<Http2AsyncResult*>(unmapped), nullptr, nullptr), nullPointer);
    static const Http2AsyncResult readOnlyResult{99, 99, 99};
    RequireErrno(sceHttp2WaitAsync(request, const_cast<Http2AsyncResult*>(&readOnlyResult), nullptr, nullptr), nullPointer);
    Require(readOnlyResult.req_id == 99 && readOnlyResult.event_type == 99 && readOnlyResult.result == 99);
    Http2AsyncResult completed{};
    Require(sceHttp2WaitAsync(request, &completed, nullptr, nullptr) == 0);
    Require(completed.req_id == request);
#ifdef _WIN32
    _putenv("APS5_NET_DISCONNECTED=");
#else
    unsetenv("APS5_NET_DISCONNECTED");
#endif
    Require(sceHttp2DeleteRequest(request) == 0);

    Require(sceNetInit_nid_postfix() == 0);
#ifdef _WIN32
    WSADATA startupData{};
    Require(WSAStartup(MAKEWORD(2, 2), &startupData) == 0);
#endif
    UnmappedReadData(tmpl);
    UnmappedSendRequest(tmpl);
    UnmappedGzipReadData(context);

    std::puts("guest_http2_guest_pointers ok");
    return 0;
}
