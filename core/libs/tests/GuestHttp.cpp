#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceHttpUriParse(SceHttpUriElement*, const char*, void*, std::size_t*, std::size_t);
int APS5_VABI sceHttpSetInflateGZIPEnabled(int, int);
int APS5_VABI sceHttpUriBuild(char*, std::size_t*, std::size_t, const SceHttpUriElement*, std::uint32_t);
int APS5_VABI sceHttpUriEscape(char*, std::size_t*, std::size_t, const char*);
int APS5_VABI sceHttpCreateEpoll(int, HttpEpollHandle*);
int APS5_VABI sceHttpDestroyEpoll(int, HttpEpollHandle);
int APS5_VABI sceHttpReadData(int, void*, std::size_t);
int APS5_VABI sceHttpCreateRequest2(int, const char*, const char*, std::uint64_t);
int APS5_VABI sceHttpsEnableOption(int, std::uint32_t);
int APS5_VABI sceHttpsLoadCert(int, int, void*, void*, void*);
int APS5_VABI sceHttpGetLastErrno(int, int*);
int APS5_VABI sceHttpParseStatusLine(const char*, std::size_t, std::int32_t*, std::int32_t*, std::int32_t*, const char**, std::size_t*);
}

static void Require(bool value) { if (!value) std::abort(); }

static bool Equal(const char* left, const char* right) { return std::strcmp(left, right) == 0; }

int main() {
    constexpr int outOfMemory = static_cast<int>(0x80431022);
    constexpr int invalidValue = static_cast<int>(0x804311FE);
    constexpr int invalidUrl = static_cast<int>(0x80433060);
    constexpr int network = static_cast<int>(0x80431063);
    constexpr std::uint32_t buildAll = 0xFF;

    const char* url = "https://user:secret@example.com:8443/a/./b/../c?x=1&y=2#top";
    std::size_t required = 0;
    Require(sceHttpUriParse(nullptr, url, nullptr, &required, 0) == 0);
    Require(required == 6 + 5 + 7 + 12 + 5 + 9 + 5);

    char pool[256];
    SceHttpUriElement element;
    Require(sceHttpUriParse(&element, url, pool, nullptr, required - 1) == outOfMemory);
    Require(sceHttpUriParse(&element, url, pool, nullptr, required) == 0);
    Require(element.opaque == 0);
    Require(Equal(element.scheme, "https") && Equal(element.username, "user") && Equal(element.password, "secret"));
    Require(Equal(element.hostname, "example.com") && element.port == 8443);
    Require(Equal(element.path, "/a/c") && Equal(element.query, "?x=1&y=2") && Equal(element.fragment, "#top"));

    char built[256];
    Require(sceHttpUriBuild(nullptr, &required, 0, &element, buildAll) == 0);
    Require(required == std::strlen("https://user:secret@example.com:8443/a/c?x=1&y=2#top") + 1);
    Require(sceHttpUriBuild(built, nullptr, required - 1, &element, buildAll) == outOfMemory);
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, buildAll) == 0);
    Require(Equal(built, "https://user:secret@example.com:8443/a/c?x=1&y=2#top"));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, 0x08 | 0x40) == 0);
    Require(Equal(built, "/a/c?x=1&y=2"));

    Require(sceHttpUriParse(&element, "HTTP://Example.com/", pool, nullptr, sizeof(pool)) == 0);
    Require(element.port == 80 && Equal(element.username, "") && Equal(element.query, ""));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, buildAll) == 0);
    Require(Equal(built, "HTTP://Example.com/"));

    Require(sceHttpUriParse(&element, "http://[::1]:8080/index.html", pool, nullptr, sizeof(pool)) == 0);
    Require(Equal(element.hostname, "::1") && element.port == 8080 && Equal(element.path, "/index.html"));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, buildAll) == 0);
    Require(Equal(built, "http://[::1]:8080/index.html"));

    Require(sceHttpUriParse(&element, "mailto:someone@example.com", pool, nullptr, sizeof(pool)) == 0);
    Require(element.opaque != 0 && element.port == 0);
    Require(Equal(element.username, "someone") && Equal(element.hostname, "example.com"));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, buildAll) == 0);
    Require(Equal(built, "mailto:someone@example.com"));

    Require(sceHttpUriParse(&element, "/path/only?q", pool, nullptr, sizeof(pool)) == 0);
    Require(Equal(element.scheme, "") && Equal(element.hostname, "") && Equal(element.path, "/path/only"));

    Require(sceHttpUriParse(&element, nullptr, pool, nullptr, sizeof(pool)) == invalidUrl);
    Require(sceHttpUriParse(nullptr, url, nullptr, nullptr, 0) == invalidValue);
    Require(sceHttpUriParse(nullptr, "http://host:65536/", nullptr, &required, 0) == invalidUrl);
    Require(sceHttpUriParse(nullptr, "http://host:80a/", nullptr, &required, 0) == invalidUrl);
    Require(sceHttpUriParse(nullptr, "http://[::1/", nullptr, &required, 0) == invalidUrl);
    Require(sceHttpUriParse(nullptr, "http://bad host/", nullptr, &required, 0) == invalidUrl);
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), nullptr, buildAll) == invalidUrl);
    Require(sceHttpUriBuild(nullptr, nullptr, 0, &element, buildAll) == invalidValue);

    char escaped[64];
    Require(sceHttpUriEscape(nullptr, &required, 0, "a b/~\xC3\xA9") == 0);
    Require(required == std::strlen("a%20b%2F~%C3%A9") + 1);
    Require(sceHttpUriEscape(escaped, nullptr, required - 1, "a b/~\xC3\xA9") == outOfMemory);
    Require(sceHttpUriEscape(escaped, nullptr, sizeof(escaped), "a b/~\xC3\xA9") == 0);
    Require(Equal(escaped, "a%20b%2F~%C3%A9"));
    Require(sceHttpUriEscape(escaped, nullptr, sizeof(escaped), nullptr) == invalidValue);

    HttpEpollHandle epoll = nullptr;
    Require(sceHttpCreateEpoll(1, nullptr) == invalidValue);
    Require(sceHttpCreateEpoll(1, &epoll) == 0 && epoll != nullptr);
    Require(sceHttpDestroyEpoll(1, epoll) == 0);

    char data[16];
    Require(sceHttpReadData(1, data, sizeof(data)) == network);

    Require(sceHttpCreateRequest2(1, "GET", "/", 0) > 0);
    Require(sceHttpSetInflateGZIPEnabled(1, 0) == 0);
    Require(sceHttpSetInflateGZIPEnabled(1, 1) == 0);
    Require(sceHttpSetInflateGZIPEnabled(1, 2) == invalidValue);
    Require(sceHttpSetInflateGZIPEnabled(1, -1) == invalidValue);
    Require(sceHttpsEnableOption(1, 0) == 0);
    Require(sceHttpsLoadCert(1, 0, nullptr, nullptr, nullptr) == 0);
    int httpErrno = -1;
    Require(sceHttpGetLastErrno(1, &httpErrno) == 0);
    Require(httpErrno == 0);
    Require(sceHttpGetLastErrno(1, nullptr) == invalidValue);

    constexpr int parseInvalidResponse = static_cast<int>(0x80432060);
    constexpr int parseInvalidValue = static_cast<int>(0x804321FE);
    std::int32_t major = -1;
    std::int32_t minor = -1;
    std::int32_t code = -1;
    const char* phrase = nullptr;
    std::size_t phraseLength = 0;
    auto parse = [&](const char* line, std::size_t length) {
        return sceHttpParseStatusLine(line, length, &major, &minor, &code, &phrase, &phraseLength);
    };

    const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    Require(parse(response, std::strlen(response)) == 17);
    Require(major == 1 && minor == 1 && code == 200);
    Require(phrase == response + 12 && phraseLength == 3 && std::strncmp(phrase, " OK", 3) == 0);

    const char* lineFeedOnly = "HTTP/10.25 404 Not Found\n";
    Require(parse(lineFeedOnly, std::strlen(lineFeedOnly)) == 25);
    Require(major == 10 && minor == 25 && code == 404);
    Require(phrase == lineFeedOnly + 14 && phraseLength == 10);

    const char* noPhrase = "HTTP/2.0 204\r\n";
    Require(parse(noPhrase, std::strlen(noPhrase)) == 14);
    Require(major == 2 && minor == 0 && code == 204);
    Require(phrase == noPhrase + 12 && phraseLength == 0);

    phrase = nullptr;
    phraseLength = 0;
    Require(parse(nullptr, 17) == parseInvalidResponse);
    Require(sceHttpParseStatusLine(response, 17, nullptr, &minor, &code, &phrase, &phraseLength) == parseInvalidValue);
    Require(sceHttpParseStatusLine(response, 17, &major, &minor, &code, &phrase, nullptr) == parseInvalidValue);
    major = -1;
    minor = -1;
    Require(parse("HTTX/1.1 200 OK\n", 16) == parseInvalidResponse);
    Require(major == 0 && minor == 0);
    Require(parse("HTTP/1.1", 7) == parseInvalidResponse);
    Require(parse("HTTP/123", 8) == parseInvalidResponse);
    Require(parse("HTTP/x.1 200 OK\n", 16) == parseInvalidResponse);
    Require(parse("HTTP/1 200 OK\n", 14) == parseInvalidResponse);
    Require(parse("HTTP/1. 200 OK\n", 15) == parseInvalidResponse);
    Require(parse("HTTP/1.1/200 OK\n", 16) == parseInvalidResponse);
    Require(parse("HTTP/1.1 2x0 OK\n", 16) == parseInvalidResponse);
    Require(parse("HTTP/1.1 20", 11) == parseInvalidResponse);
    Require(parse(response, 15) == parseInvalidResponse);
    Require(parse(response, 16) == parseInvalidResponse);
    Require(phrase == nullptr && phraseLength == 0);
}
