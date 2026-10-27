#include "prx/libSceHttp/include/Http.hpp"
#include "SceTypes.hpp"
#include "ProcessParametersFixture.hpp"
#include <array>
#include "prx/libc/include/general/VabiMacros.hpp"
#include <atomic>
#include <chrono>
#include <clocale>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <thread>

alignas(16) static std::array<std::byte, 0x40> processParameters{};

static void Require(bool value) { if (!value) std::abort(); }

static bool Equal(const char* left, const char* right) { return std::strcmp(left, right) == 0; }


static_assert(sizeof(HttpMemoryPoolStats) == 24);

template <typename TCall>
static bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static void CheckPoolStatsRejected(int id, int expected) {
    alignas(HttpMemoryPoolStats) unsigned char buffer[sizeof(HttpMemoryPoolStats) + 16];
    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(sceHttpGetMemoryPoolStats(id, reinterpret_cast<HttpMemoryPoolStats*>(buffer)) == expected);
    for (unsigned char byte : buffer) Require(byte == 0x5a);
}

static void CheckPoolStats(int id, std::size_t poolSize) {
    alignas(HttpMemoryPoolStats) unsigned char buffer[sizeof(HttpMemoryPoolStats) + 16];
    std::memset(buffer, 0x5a, sizeof(buffer));
    Require(sceHttpGetMemoryPoolStats(id, reinterpret_cast<HttpMemoryPoolStats*>(buffer)) == 0);
    HttpMemoryPoolStats stats;
    std::memcpy(&stats, buffer, sizeof(stats));
    Require(stats.poolSize == poolSize);
    Require(stats.peakUsedSize == 0 && stats.usedSize == 0);
    for (std::size_t i = sizeof(stats); i < sizeof(buffer); ++i) Require(buffer[i] == 0x5a);
}

static void CheckUriParseCharacters() {
    char pool[256];
    SceHttpUriElement element{};
    Require(sceHttpUriParse(&element, "HtTpS://AZaz09-._/path", pool, nullptr, sizeof(pool)) == 0);
    Require(Equal(element.scheme, "HtTpS") && Equal(element.hostname, "AZaz09-._") && element.port == 443);
    Require(sceHttpUriParse(&element, "aZ09+.-://host/", pool, nullptr, sizeof(pool)) == 0);
    Require(Equal(element.scheme, "aZ09+.-") && element.port == 0);
    for (unsigned value = 128; value <= 255; ++value) {
        const std::string byte(1, static_cast<char>(value));
        for (const auto& url : {"http://[" + byte + "]/", "http://host:" + byte + "/"}) {
            std::size_t required = 0;
            Require(sceHttpUriParse(nullptr, url.c_str(), nullptr, &required, 0) == static_cast<int>(0x80433060));
            Require(sceHttpUriParse(&element, url.c_str(), pool, nullptr, sizeof(pool)) == static_cast<int>(0x80433060));
        }
        Require(sceHttpUriParse(&element, ("http://" + byte + "/").c_str(), pool, nullptr, sizeof(pool)) == 0);
        Require(Equal(element.hostname, "") && element.path == byte + "/");
        Require(sceHttpUriParse(&element, (byte + "://host/").c_str(), pool, nullptr, sizeof(pool)) == 0);
        Require(element.opaque && Equal(element.scheme, "") && Equal(element.hostname, "") && element.path == byte + "://host/");
        Require(sceHttpUriParse(&element, ("a" + byte + "://host/").c_str(), pool, nullptr, sizeof(pool)) == 0);
        Require(Equal(element.scheme, "") && Equal(element.hostname, "a") && element.path == byte + "://host/");
    }
}

struct UriParseCase {
    const char* url;
    int opaque;
    std::uint16_t port;
    const char* scheme;
    const char* username;
    const char* password;
    const char* hostname;
    const char* path;
    const char* query;
    const char* fragment;
    std::size_t pathSpace = 0;
};

static void CheckUriParseAsMeasured() {
    constexpr UriParseCase cases[] = {
        {"http://bad host/", 0, 80, "http", "", "", "bad", " host/", "", ""},
        {"http://ex%41mple.com/", 0, 80, "http", "", "", "ex", "%41mple.com/", "", ""},
        {"http://h\t/", 0, 80, "http", "", "", "h", "\t/", "", ""},
        {"not a url", 1, 0, "", "", "", "not", " a url", "", ""},
        {"example.com/a", 1, 0, "", "", "", "example.com", "/a", "", ""},
        {"mailto:x", 1, 0, "mailto", "", "", "x", "", "", ""},
        {"abc:def", 1, 0, "abc", "", "", "def", "", "", ""},
        {"http:", 1, 80, "http", "", "", "", "", "", ""},
        {"https:h", 1, 443, "https", "", "", "h", "", "", ""},
        {"//host/x", 0, 0, "", "", "", "host", "/x", "", ""},
        {"", 1, 0, "", "", "", "", "", "", ""},
        {"?q", 1, 0, "", "", "", "", "", "?q", ""},
        {"http://host:65536/", 0, 0, "http", "", "", "host", "/", "", ""},
        {"http://h:65535", 0, 65535, "http", "", "", "h", "", "", ""},
        {"http://h:0080/", 0, 80, "http", "", "", "h", "/", "", ""},
        {"http://h:00000/", 0, 0, "http", "", "", "h", "/", "", ""},
        {"http://host:/", 0, 80, "http", "", "", "host", "/", "", ""},
        {"http://[::1]x/", 0, 80, "http", "", "", "::1", "x/", "", ""},
        {"http://[::1]]/", 0, 80, "http", "", "", "::1", "]/", "", ""},
        {"http://[::1]:/", 0, 80, "http", "", "", "::1", "/", "", ""},
        {"http://[]/", 0, 80, "http", "", "", "", "/", "", ""},
        {"http://[.]/", 0, 80, "http", "", "", ".", "/", "", ""},
        {"http://u@h@i/", 0, 80, "http", "u", "", "h", "@i/", "", ""},
        {"http://u s@h/", 0, 80, "http", "", "", "u", " s@h/", "", ""},
        {"http://u:p:q@h/", 0, 80, "http", "u", "p:q", "h", "/", "", ""},
        {"http://u%41@h/", 0, 80, "http", "u%41", "", "h", "/", "", ""},
        {"http://h@/", 0, 80, "http", "h", "", "", "/", "", ""},
        {"http://[::1]@h/", 0, 80, "http", "", "", "::1", "@h/", "", ""},
        {"http://u@[::1]:8/", 0, 8, "http", "u", "", "::1", "/", "", ""},
        {".@h/", 1, 0, "", ".", "", "h", "/", "", ""},
        {"http://.a/", 0, 80, "http", "", "", "", ".a/", "", ""},
        {"http://-a/", 0, 80, "http", "", "", "-a", "/", "", ""},
        {"http://a..b/", 0, 80, "http", "", "", "a..b", "/", "", ""},
        {"a.", 1, 0, "", "", "", "a.", "", "", ""},
        {"http://h/a/..", 0, 80, "http", "", "", "h", "/a/..", "", ""},
        {"http://h/a/.", 0, 80, "http", "", "", "h", "/a/.", "", ""},
        {"http://h/a/b/../../..", 0, 80, "http", "", "", "h", "/..", "", "", 13},
        {"http://h/../../a", 0, 80, "http", "", "", "h", "/a", "", "", 8},
        {"http://h/a//b/../c", 0, 80, "http", "", "", "h", "/a//c", "", "", 10},
        {"http://h/a/.../../b", 0, 80, "http", "", "", "h", "/a/b", "", "", 11},
        {"http://h/./a/../b/", 0, 80, "http", "", "", "h", "/b/", "", "", 10},
        {"./a", 1, 0, "", "", "", "", "./a", "", ""},
        {"a/../b", 1, 0, "", "", "", "a", "/b", "", "", 5},
        {"1x://h/", 1, 0, "", "", "", "1x", "//h/", "", ""},
        {"-x://h/", 0, 0, "-x", "", "", "h", "/", "", ""},
        {"s_x://h/p", 1, 0, "", "", "", "s_x", "//h/p", "", ""},
        {"s:x://h/p", 1, 0, "s", "", "", "x", "//h/p", "", ""},
        {"s@x://h/p", 1, 0, "", "s", "", "x", "//h/p", "", ""},
        {"httpx://h/", 0, 80, "httpx", "", "", "h", "/", "", ""},
        {"httpsx://h/", 0, 443, "httpsx", "", "", "h", "/", "", ""},
        {"HTTPx://h/", 0, 80, "HTTPx", "", "", "h", "/", "", ""},
        {"htt://h/", 0, 0, "htt", "", "", "h", "/", "", ""},
        {"ws://h/", 0, 0, "ws", "", "", "h", "/", "", ""},
        {"https://h:80/", 0, 80, "https", "", "", "h", "/", "", ""},
        {"http://h/p?q#f#g", 0, 80, "http", "", "", "h", "/p", "?q", "#f#g"},
        {"http://h/p#f?q", 0, 80, "http", "", "", "h", "/p", "", "#f?q"},
        {"http://h?", 0, 80, "http", "", "", "h", "", "?", ""},
    };
    char pool[256];
    for (const auto& row : cases) {
        SceHttpUriElement element{};
        std::size_t required = 0;
        Require(sceHttpUriParse(nullptr, row.url, nullptr, &required, 0) == 0);
        const std::size_t size = std::strlen(row.scheme) + std::strlen(row.username) + std::strlen(row.password) + std::strlen(row.hostname)
            + (row.pathSpace != 0 ? row.pathSpace : std::strlen(row.path)) + std::strlen(row.query) + std::strlen(row.fragment) + 7;
        Require(required == size);
        required = 77;
        Require(sceHttpUriParse(&element, row.url, pool, &required, size - 1) == static_cast<int>(0x80431022) && required == 77);
        Require(sceHttpUriParse(&element, row.url, pool, &required, sizeof(pool)) == 0 && required == size);
        Require(element.opaque == row.opaque && element.port == row.port);
        Require(Equal(element.scheme, row.scheme) && Equal(element.username, row.username) && Equal(element.password, row.password));
        Require(Equal(element.hostname, row.hostname) && Equal(element.path, row.path));
        Require(Equal(element.query, row.query) && Equal(element.fragment, row.fragment));
    }
    for (const char* url : {"http://[::1/", "http://[a b]/", "http://[a%b]/", "http://[a@b]/", "http://host:80a/", "http://h:65537/", "http://h:99999/",
                            "http://h:000080/", "http://h:80?q", "http://h:80#f", "http://h:80 /", "http://h: 80/", "http://h:+80/", "http://h:-1/",
                            "http://h:80:81/", "http://[::1]:8a/", "http://a:b/p"}) {
        std::size_t required = 77;
        Require(sceHttpUriParse(nullptr, url, nullptr, &required, 0) == static_cast<int>(0x80433060) && required == 77);
    }
}

struct UriMergeCase {
    const char* base;
    const char* relative;
    std::size_t sizeOnly;
    const char* merged;
};

static void CheckUriMergeAsMeasured() {
    constexpr UriMergeCase cases[] = {
        {"http://user:pw@example.com:8080/a/b/c?q=1#f", "?x", 0x84, "http://user:pw@example.com:8080/a/b/?x"},
        {"http://user:pw@example.com:8080/a/b/c?q=1#f", "#y", 0x84, "http://user:pw@example.com:8080/a/b/#y"},
        {"http://user:pw@example.com:8080/a/b/c?q=1#f", "../../../../d", 0x9a, "http://user:pw@example.com:8080/a/b/../../../../d"},
        {"http://user:pw@example.com:8080/a/b/c?q=1#f", "http:d", 0x8c, "http://user:pw@example.com:8080/a/b/http:d"},
        {"http://user:pw@example.com:8080/a/b/c?q=1#f", "//other.com/x", 0x9a, "//other.com/x"},
        {"http://example.com", "../d", 0x44, "http://example.com/../d"},
        {"http://example.com/a/./b/../c", "d", 0x5f, "http://example.com/a/d"},
        {"not a url", "d", 0x26, "not a url/d"},
        {"example.com/a", "b", 0x32, "example.com/b"},
        {"http://[::1]:80/a/b", "c", 0x3c, "http://::1/a/c"},
        {"http://ex ample.com/a", "c", 0x47, "http://ex ample.com/c"},
        {"http://example.com/a", "//bad host/x", 0x5a, "//bad host/x"},
        {"//host/a/b", "c", 0x27, "//host/a/c"},
        {"/a/b", "c", 0x17, "/a/c"},
        {"a/b", "c", 0x14, "a/c"},
        {"", "c", 0xb, "/c"},
        {"", "", 0x9, "/"},
        {"mailto:x@y", "c", 0x27, "mailto:x@y/c"},
        {"http://h", "?q", 0x22, "http://h/?q"},
        {"https://h:443/a", "c", 0x31, "https://h/c"},
        {"http://h:0/a", "c", 0x2a, "http://h/c"},
        {"http://h:65536/a", "c", 0x32, "http://h/c"},
        {"http://:p@h/a", "c", 0x2d, "http://:p@h/c"},
        {"http://bad host/", "./x", 0x3c, "http://bad host/./x"},
        {"http://h/a", "http://bad host/", 0x44, "http://bad host/"},
        {"http://h/a", "HTTPS://Other.com/", 0x48, "HTTPS://Other.com/"},
        {"http://h/a", "//o", 0x2a, "//o"},
        {"http://h/a", "x:y", 0x2a, "http://h/x:y"},
        {"http://h/a", "d e", 0x2a, "http://h/d e"},
        {"http://h/a", "..", 0x28, "http://h/.."},
        {"http://h/a/./b/../c", "../d", 0x47, "http://h/a/../d"},
        {"http://h/a?q#f", "", 0x30, "http://h/"},
        {"not a url", "//x/y", 0x2e, "//x/y"},
    };
    char merged[512];
    for (const auto& row : cases) {
        std::size_t required = 0;
        Require(sceHttpUriMerge(nullptr, row.base, row.relative, &required, 0, 0) == 0 && required == row.sizeOnly);
        std::memset(merged, 'Z', sizeof(merged));
        required = 77;
        Require(sceHttpUriMerge(merged, row.base, row.relative, &required, row.sizeOnly - 1, 0) == static_cast<int>(0x80431022));
        Require(merged[0] == 'Z');
        Require(sceHttpUriMerge(merged, row.base, row.relative, &required, sizeof(merged), 0) == 0 && Equal(merged, row.merged));
    }
    for (const char* base : {"http://[::1/a", "http://h:80a/a"}) {
        std::size_t required = 77;
        Require(sceHttpUriMerge(merged, base, "c", &required, sizeof(merged), 0) == static_cast<int>(0x80433060) && required == 77);
    }
}

static void CheckUriEscapeBytes() {
    constexpr char unreserved[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned value = 1; value <= 255; ++value) {
        const char input[] = {static_cast<char>(value), '\0'};
        char expected[] = {'%', hex[value >> 4], hex[value & 15], '\0'};
        if (std::strchr(unreserved, input[0])) {
            expected[0] = input[0];
            expected[1] = '\0';
        }
        const auto size = std::strlen(expected) + 1;
        std::size_t required = 0;
        Require(sceHttpUriEscape(nullptr, &required, 0, input) == 0 && required == size);
        char output[5] = {'Z', 'Z', 'Z', 'Z', 'Z'};
        Require(sceHttpUriEscape(output, &required, size - 1, input) == static_cast<int>(0x80431022));
        for (char byte : output) Require(byte == 'Z');
        Require(sceHttpUriEscape(output, &required, size, input) == 0 && required == size);
        Require(Equal(output, expected) && output[size] == 'Z');
    }
}

struct StatusLineCase {
    bool tildeFill;
    const char* line;
    std::size_t length;
    int result;
    std::int32_t major;
    std::int32_t minor;
    std::int32_t code;
    long reasonOffset;
    std::size_t phraseLength;
};

static void CheckStatusLineAsMeasured() {
    constexpr int parseInvalidResponse = static_cast<int>(0x80432060);
    constexpr StatusLineCase cases[] = {
        {false, "HTTP/1.1 200 OK\r\n", 17, 17, 1, 1, 200, 12, 0x3},
        {false, "HTTP/1.0 404 Not Found\n", 23, 23, 1, 0, 404, 12, 0xa},
        {false, "HTTP/1.1 200 OK", 15, parseInvalidResponse, 1, 1, 200, -1, 0xabcd},
        {false, "HTTP/1.1 200\r\n", 14, 14, 1, 1, 200, 12, 0x0},
        {false, "HTTP/1.1 200 \r\n", 15, 15, 1, 1, 200, 12, 0x1},
        {false, "HTTP/2 200 OK\r\n", 15, parseInvalidResponse, 2, 0, -7, -1, 0xabcd},
        {false, "HTTP/10.20 999 X\r\n", 18, 18, 10, 20, 999, 14, 0x2},
        {false, "HTTP/1.1 20 OK\r\n", 16, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {false, "HTTP/1.1 2000 OK\r\n", 18, 18, 1, 1, 200, 12, 0x4},
        {false, "HTTP/1.1 abc OK\r\n", 17, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {false, "http/1.1 200 OK\r\n", 17, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {false, "HTTP/1.1  200 OK\r\n", 18, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {false, "HTTP/1.1\t200 OK\r\n", 17, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {false, "HTTP/1.1 200OK\r\n", 16, 16, 1, 1, 200, 12, 0x2},
        {false, "ICY 200 OK\r\n", 12, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {false, "HTTP/1.1 200 OK\r", 16, parseInvalidResponse, 1, 1, 200, -1, 0xabcd},
        {false, "HTTP/1.1 200 OK\r\nX: y", 21, 17, 1, 1, 200, 12, 0x3},
        {false, "HTTP/1.1 200 OK\r\r\n", 18, 18, 1, 1, 200, 12, 0x4},
        {false, "HTTP/.1 200 OK\r\n", 16, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {false, "HTTP/1. 200 OK\r\n", 16, parseInvalidResponse, 1, 0, -7, -1, 0xabcd},
        {false, "HTTP/4294967297.1 200 OK\n", 25, 25, 1, 1, 200, 21, 0x3},
        {false, "HTTP/1.1 -12 OK\n", 16, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {false, " HTTP/1.1 200 OK\n", 17, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {false, "HTTP/1.1 200 A\nB\n", 17, 15, 1, 1, 200, 12, 0x2},
        {false, "HTTP/1.1 200\n", 13, 13, 1, 1, 200, 12, 0x0},
        {false, "HTTP/1.1 200 OK\n", 15, 16, 1, 1, 200, 12, 0x3},
        {false, "HTTP/1.1 200 OK\n", 12, parseInvalidResponse, 1, 1, 200, -1, 0xabcd},
        {false, "HTTP/1.1 200 OK\n", 11, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {false, "HTTP/1.1 200 OK\n", 8, 16, 1, 1, 200, 12, 0x3},
        {false, "HTTP/1.1 200 OK\n", 7, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {false, "HTTP/1.1 200 OK\n", 0, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 0, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 1, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 2, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 3, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 4, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 5, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 6, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 7, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 8, 16, 1, 1, 200, 12, 0x3},
        {true, "HTTP/1.1 200 OK\n", 9, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 10, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 11, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 12, parseInvalidResponse, 1, 1, 200, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 13, parseInvalidResponse, 1, 1, 200, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 14, parseInvalidResponse, 1, 1, 200, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK\n", 15, 16, 1, 1, 200, 12, 0x3},
        {true, "HTTP/1.1 200 OK\n", 16, 16, 1, 1, 200, 12, 0x3},
        {true, "HTTP/10.20 999 X\n", 0, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 1, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 2, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 3, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 4, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 5, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 6, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 7, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 8, 17, 10, 20, 999, 14, 0x2},
        {true, "HTTP/10.20 999 X\n", 9, 17, 10, 20, 999, 14, 0x2},
        {true, "HTTP/10.20 999 X\n", 10, 17, 10, 20, 999, 14, 0x2},
        {true, "HTTP/10.20 999 X\n", 11, parseInvalidResponse, 10, 20, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 12, parseInvalidResponse, 10, 20, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 13, parseInvalidResponse, 10, 20, -7, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 14, parseInvalidResponse, 10, 20, 999, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 15, parseInvalidResponse, 10, 20, 999, -1, 0xabcd},
        {true, "HTTP/10.20 999 X\n", 16, 17, 10, 20, 999, 14, 0x2},
        {true, "HTTP/10.20 999 X\n", 17, 17, 10, 20, 999, 14, 0x2},
        {true, "HTTP/1.12 200 OK\n", 0, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 1, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 2, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 3, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 4, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 5, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 6, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 7, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 8, 17, 1, 12, 200, 13, 0x3},
        {true, "HTTP/1.12 200 OK\n", 9, 17, 1, 12, 200, 13, 0x3},
        {true, "HTTP/1.12 200 OK\n", 10, parseInvalidResponse, 1, 12, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 11, parseInvalidResponse, 1, 12, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 12, parseInvalidResponse, 1, 12, -7, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 13, parseInvalidResponse, 1, 12, 200, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 14, parseInvalidResponse, 1, 12, 200, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 15, parseInvalidResponse, 1, 12, 200, -1, 0xabcd},
        {true, "HTTP/1.12 200 OK\n", 16, 17, 1, 12, 200, 13, 0x3},
        {true, "HTTP/1.12 200 OK\n", 17, 17, 1, 12, 200, 13, 0x3},
        {true, "HTTP/12.1 200 OK\n", 0, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 1, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 2, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 3, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 4, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 5, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 6, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 7, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 8, 17, 12, 1, 200, 13, 0x3},
        {true, "HTTP/12.1 200 OK\n", 9, 17, 12, 1, 200, 13, 0x3},
        {true, "HTTP/12.1 200 OK\n", 10, parseInvalidResponse, 12, 1, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 11, parseInvalidResponse, 12, 1, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 12, parseInvalidResponse, 12, 1, -7, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 13, parseInvalidResponse, 12, 1, 200, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 14, parseInvalidResponse, 12, 1, 200, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 15, parseInvalidResponse, 12, 1, 200, -1, 0xabcd},
        {true, "HTTP/12.1 200 OK\n", 16, 17, 12, 1, 200, 13, 0x3},
        {true, "HTTP/12.1 200 OK\n", 17, 17, 12, 1, 200, 13, 0x3},
        {true, "HTTP/1.1 200\n", 0, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 1, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 2, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 3, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 4, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 5, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 6, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 7, parseInvalidResponse, 0, 0, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 8, 13, 1, 1, 200, 12, 0x0},
        {true, "HTTP/1.1 200\n", 9, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 10, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 11, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200\n", 12, 13, 1, 1, 200, 12, 0x0},
        {true, "HTTP/1.1 200\n", 13, 13, 1, 1, 200, 12, 0x0},
        {true, "HTTP/1.1 200 OK", 15, parseInvalidResponse, 1, 1, 200, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK", 8, 255, 1, 1, 200, 12, 0xf2},
        {true, "HTTP/1.1 200 OK", 9, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK", 12, parseInvalidResponse, 1, 1, 200, -1, 0xabcd},
        {true, "HTTP/1.1 200 OK", 13, parseInvalidResponse, 1, 1, 200, -1, 0xabcd},
        {true, "HTTP/1.1 2", 8, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {true, "HTTP/1.1 20", 8, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
        {true, "HTTP/1.1", 8, parseInvalidResponse, 1, 1, -7, -1, 0xabcd},
    };
    static char buffer[256];
    for (const auto& row : cases) {
        std::memset(buffer, row.tildeFill ? '~' : '\0', sizeof(buffer));
        std::memcpy(buffer, row.line, std::strlen(row.line));
        if (row.tildeFill) buffer[sizeof(buffer) - 2] = '\n';
        buffer[sizeof(buffer) - 1] = '\0';
        std::int32_t major = -7;
        std::int32_t minor = -7;
        std::int32_t code = -7;
        const char* reason = nullptr;
        std::size_t phraseLength = 0xabcd;
        Require(sceHttpParseStatusLine(buffer, row.length, &major, &minor, &code, &reason, &phraseLength) == row.result);
        Require(major == row.major && minor == row.minor && code == row.code && phraseLength == row.phraseLength);
        Require(row.reasonOffset < 0 ? reason == nullptr : reason == buffer + row.reasonOffset);
    }
}

static int APS5_VABI AuthInfo(int, int, const char*, char*, char*, int, std::uint8_t**, std::uint64_t*, int*, void*) { std::abort(); }
static void APS5_VABI RequestStatusCallback(int, int, void* argument) {
    ++*static_cast<int*>(argument);
}

static void CheckCaList() {
    constexpr int invalidValue = static_cast<int>(0x804311FE);
    int certificate = 0;
    HttpsCaList list{&certificate, 7};
    Require(sceHttpsGetCaList(1, nullptr) == invalidValue);
    Require(sceHttpsGetCaList(1, &list) == 0 && list.certs == nullptr && list.certsNum == 0);
    list = {&certificate, 7};
    Require(sceHttpsFreeCaList(1, nullptr) == invalidValue);
    Require(sceHttpsFreeCaList(1, &list) == 0 && list.certs == nullptr && list.certsNum == 0);
}

int main() {
    const std::uint64_t processSize = processParameters.size();
    const std::uint32_t processMagic = 0x4942524f;
    std::uint32_t sdkVersion = 0x1000000;
    std::memcpy(processParameters.data(), &processSize, sizeof(processSize));
    std::memcpy(processParameters.data() + 8, &processMagic, sizeof(processMagic));
    std::memcpy(processParameters.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(ProcessParametersFixture::Install(processParameters.data(), processParameters.size()));
    CheckCaList();
    for (const std::uint32_t version : {0u, 0xffffffu, 0x1000000u}) {
        std::memcpy(processParameters.data() + 16, &version, sizeof(version));
        for (unsigned value = 1; value < 256; ++value) {
            if (value == '%') continue;
            const char input[]{static_cast<char>(value), 0};
            char output[]{'x', 'x'};
            std::size_t required = 123;
            const bool allowed = version >= 0x1000000 || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '*' || value == '-' || value == '.' || value == '_' || value == '+';
            Require(sceHttpUriUnescape(output, &required, sizeof(output), input) == (allowed ? 0 : static_cast<int>(0x80433060)));
            Require(allowed ? output[0] == input[0] && output[1] == 0 && required == 2 : output[0] == 'x' && output[1] == 'x' && required == 123);
        }
        char output[2]{};
        Require(sceHttpUriUnescape(output, nullptr, sizeof(output), "%7E") == 0 && output[0] == '~');
    }
    std::memcpy(processParameters.data() + 16, &sdkVersion, sizeof(sdkVersion));
    int callbackCount = 0;
    const auto request = sceHttpCreateRequest2(1, "GET", "/", 0);
    Require(sceHttpSetRequestStatusCallback(request, RequestStatusCallback, &callbackCount) == 0);
    Require(sceHttpSendRequest(request, nullptr, 0) == static_cast<int>(0x80431063u));
    Require(callbackCount == 0);
    Require(sceHttpSetRequestStatusCallback(request, nullptr, nullptr) == 0);
    Require(sceHttpSetRequestStatusCallback(-1, RequestStatusCallback, &callbackCount) == static_cast<int>(0x80431100u));
    constexpr int outOfMemory = static_cast<int>(0x80431022);
    constexpr int invalidValue = static_cast<int>(0x804311FE);
    constexpr int invalidUrl = static_cast<int>(0x80433060);
    constexpr int network = static_cast<int>(0x80431063);
    constexpr std::uint32_t buildAll = 0xFF;

    const char* url = "https://user:secret@example.com:8443/a/./b/../c?x=1&y=2#top";
    std::size_t required = 0;
    Require(sceHttpUriParse(nullptr, url, nullptr, &required, 0) == 0);
    Require(required == 6 + 5 + 7 + 12 + 12 + 9 + 5);

    char pool[256];
    SceHttpUriElement element;
    Require(sceHttpUriParse(&element, url, pool, nullptr, required - 1) == outOfMemory);
    Require(sceHttpUriParse(&element, url, pool, nullptr, required) == 0);
    Require(element.opaque == 0);
    Require(Equal(element.scheme, "https") && Equal(element.username, "user") && Equal(element.password, "secret"));
    Require(Equal(element.hostname, "example.com") && element.port == 8443);
    Require(Equal(element.path, "/a/c") && Equal(element.query, "?x=1&y=2") && Equal(element.fragment, "#top"));
    Require(element.path == pool + 30 && element.query == pool + 42 && element.fragment == pool + 51);

    char built[256];
    Require(sceHttpUriBuild(nullptr, &required, 0, &element, buildAll) == 0);
    Require(required == std::strlen("https://user:secret@example.com:8443/a/c?x=1&y=2#top") + 1);
    Require(sceHttpUriBuild(built, nullptr, required - 1, &element, buildAll) == outOfMemory);
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, buildAll) == 0);
    Require(Equal(built, "https://user:secret@example.com:8443/a/c?x=1&y=2#top"));
    Require(sceHttpUriBuild(nullptr, &required, 0, &element, 0x01) == 0);
    Require(required == sizeof("https://"));
    std::memset(built, 'Z', sizeof(built));
    Require(sceHttpUriBuild(built, nullptr, required - 1, &element, 0x01) == outOfMemory);
    for (char c : built) Require(c == 'Z');
    Require(sceHttpUriBuild(built, nullptr, required, &element, 0x01) == 0);
    Require(Equal(built, "https://") && built[required] == 'Z');
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, 0) == 0);
    Require(Equal(built, "//"));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, 0x02) == 0);
    Require(Equal(built, "//example.com"));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, 0x08 | 0x40) == 0);
    Require(Equal(built, "///a/c?x=1&y=2"));

    Require(sceHttpUriParse(&element, "HTTP://Example.com/", pool, nullptr, sizeof(pool)) == 0);
    Require(element.port == 80 && Equal(element.username, "") && Equal(element.query, ""));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, buildAll) == 0);
    Require(Equal(built, "HTTP://Example.com/"));

    Require(sceHttpUriParse(&element, "http://[::1]:8080/index.html", pool, nullptr, sizeof(pool)) == 0);
    Require(Equal(element.hostname, "::1") && element.port == 8080 && Equal(element.path, "/index.html"));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, buildAll) == 0);
    Require(Equal(built, "http://::1:8080/index.html"));

    Require(sceHttpUriParse(&element, "mailto:someone@example.com", pool, nullptr, sizeof(pool)) == 0);
    Require(element.opaque != 0 && element.port == 0);
    Require(Equal(element.username, "someone") && Equal(element.hostname, "example.com"));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, buildAll) == 0);
    Require(Equal(built, "mailto:someone@example.com"));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, 0x01) == 0);
    Require(Equal(built, "mailto:"));
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), &element, 0) == 0);
    Require(Equal(built, ""));

    Require(sceHttpUriParse(&element, "/path/only?q", pool, nullptr, sizeof(pool)) == 0);
    Require(Equal(element.scheme, "") && Equal(element.hostname, "") && Equal(element.path, "/path/only"));

    Require(sceHttpUriParse(&element, nullptr, pool, nullptr, sizeof(pool)) == invalidUrl);
    Require(sceHttpUriParse(nullptr, url, nullptr, nullptr, 0) == invalidValue);
    Require(sceHttpUriParse(nullptr, "http://host:65537/", nullptr, &required, 0) == invalidUrl);
    Require(sceHttpUriParse(nullptr, "http://host:80a/", nullptr, &required, 0) == invalidUrl);
    Require(sceHttpUriParse(nullptr, "http://[::1/", nullptr, &required, 0) == invalidUrl);
    Require(sceHttpUriParse(nullptr, "http://host:80?q", nullptr, &required, 0) == invalidUrl);
    CheckUriParseAsMeasured();
    Require(sceHttpUriBuild(built, nullptr, sizeof(built), nullptr, buildAll) == invalidUrl);
    Require(sceHttpUriBuild(nullptr, nullptr, 0, &element, buildAll) == invalidValue);

    {
        SceHttpUriElement bounded{};
        bounded.opaque = 2;
        char scheme[33];
        char hostname[256];
        std::memset(scheme, 's', sizeof(scheme));
        std::memset(hostname, 'h', sizeof(hostname));
        bounded.scheme = scheme;
        bounded.hostname = hostname;
        char output[512];
        Require(sceHttpUriBuild(output, &required, sizeof(output), &bounded, 3) == 0);
        Require(required == 32 + 1 + 2 + 255 + 1);
        Require(output[32] == ':' && output[33] == '/' && output[34] == '/');
        Require(output[required - 1] == '\0');
        scheme[0] = '\0';
        Require(sceHttpUriBuild(output, &required, sizeof(output), &bounded, 1) == 0);
        Require(required == 4 && Equal(output, "//"));
        char prefix[] = "HTTPS-extra";
        bounded.scheme = prefix;
        bounded.port = 443;
        Require(sceHttpUriBuild(output, &required, sizeof(output), &bounded, 4) == 0);
        Require(required == 3 && Equal(output, "//"));
    }

    char escaped[64];
    Require(sceHttpUriEscape(nullptr, &required, 0, "a b/~\xC3\xA9") == 0);
    Require(required == std::strlen("a%20b%2F~%C3%A9") + 1);
    Require(sceHttpUriEscape(escaped, nullptr, required - 1, "a b/~\xC3\xA9") == outOfMemory);
    Require(sceHttpUriEscape(escaped, nullptr, sizeof(escaped), "a b/~\xC3\xA9") == 0);
    Require(Equal(escaped, "a%20b%2F~%C3%A9"));
    Require(sceHttpUriEscape(escaped, nullptr, sizeof(escaped), nullptr) == invalidValue);
    const std::string previousCtype = std::setlocale(LC_CTYPE, nullptr);
    Require(std::setlocale(LC_CTYPE, "C") != nullptr);
    CheckUriEscapeBytes();
    CheckUriParseCharacters();
#ifdef _WIN32
    Require(std::setlocale(LC_CTYPE, ".1252") != nullptr);
    CheckUriEscapeBytes();
    CheckUriParseCharacters();
#else
    for (const char* locale : {"en_US.ISO8859-1", "en_US.iso88591", "en_US.UTF-8"}) {
        if (!std::setlocale(LC_CTYPE, locale)) continue;
        CheckUriEscapeBytes();
        CheckUriParseCharacters();
    }
#endif
    Require(std::setlocale(LC_CTYPE, previousCtype.c_str()) != nullptr);

    constexpr const char* unescapeCases[][2] = {
        {"", ""}, {"plain+text", "plain+text"}, {"a%20b%2F~%c3%a9", "a b/~\xC3\xA9"},
        {"%41%4a%4F%ff", "AJO\xFF"}, {"%2520", "%20"}
    };
    for (const auto& row : unescapeCases) {
        const std::size_t size = std::strlen(row[1]) + 1;
        Require(sceHttpUriUnescape(nullptr, &required, 0, row[0]) == 0 && required == size);
        std::memset(escaped, 'Z', sizeof(escaped));
        required = 123;
        Require(sceHttpUriUnescape(escaped, &required, size - 1, row[0]) == outOfMemory && required == 123);
        Require(std::memcmp(escaped, row[1], size - 1) == 0 && escaped[size - 1] == 'Z');
        Require(sceHttpUriUnescape(escaped, nullptr, size, row[0]) == 0);
        Require(Equal(escaped, row[1]) && escaped[size] == 'Z');
    }
    for (const char* malformed : {"%", "%1", "%1g", "%gg", "%+1", "%-1", "% 1", "abc%", "%%41", "%4%41"}) {
        std::memset(escaped, 'Z', sizeof(escaped));
        required = 77;
        Require(sceHttpUriUnescape(escaped, &required, sizeof(escaped), malformed) == invalidUrl && required == 77 && escaped[0] == 'Z');
        Require(sceHttpUriUnescape(nullptr, &required, 0, malformed) == invalidUrl && required == 77);
    }
    Require(sceHttpUriUnescape(escaped, &required, sizeof(escaped), "a%00b") == 0);
    Require(required == 4 && std::memcmp(escaped, "a\0b", 4) == 0);
    std::strcpy(escaped, "%41%2f%2520");
    Require(sceHttpUriUnescape(escaped, &required, sizeof(escaped), escaped) == 0);
    Require(Equal(escaped, "A/%20") && required == 6);
    Require(sceHttpUriUnescape(nullptr, nullptr, 0, "valid") == invalidValue);
    required = 123;
    Require(sceHttpUriUnescape(escaped, &required, sizeof(escaped), nullptr) == invalidValue);
    Require(required == 123 && Equal(escaped, "A/%20"));

    for (const char* malformed : {"%", "%1", "%1g", "%gg", "%+1", "%-1", "prefix%4", "prefix%xy"}) {
        std::memset(escaped, 'Z', sizeof(escaped));
        required = 123;
        Require(sceHttpUriUnescape(escaped, &required, sizeof(escaped), malformed) == static_cast<int>(0x80433060));
        Require(required == 123);
        for (char value : escaped) Require(value == 'Z');
    }
    std::memset(escaped, 'Z', sizeof(escaped));
    Require(sceHttpUriUnescape(escaped, &required, 2, "a%20b") == outOfMemory);
    Require(required == 123 && escaped[0] == 'a' && escaped[1] == ' ' && escaped[2] == 'Z');

    char bytes[256], encodedBytes[766], decodedBytes[256];
    for (std::size_t i = 1; i < 256; ++i) bytes[i - 1] = static_cast<char>(i);
    bytes[255] = '\0';
    Require(sceHttpUriEscape(encodedBytes, nullptr, sizeof(encodedBytes), bytes) == 0);
    Require(sceHttpUriUnescape(decodedBytes, &required, sizeof(decodedBytes), encodedBytes) == 0);
    Require(required == sizeof(bytes) && std::memcmp(bytes, decodedBytes, sizeof(bytes)) == 0);

    const char* base = "http://foo.com/foo/index.html";
    const std::size_t baseMergeSize = 5 + 1 + 1 + 8 + 16 + 1 + 1 + 2;
    char merged[512];
    auto merge = [&](const char* mergeBase, const char* relative, const char* expected) {
        std::memset(merged, 'Z', sizeof(merged));
        return sceHttpUriMerge(merged, mergeBase, relative, &required, sizeof(merged), 0) == 0 && Equal(merged, expected);
    };
    Require(merge(base, "./default.html", "http://foo.com/foo/./default.html"));
    Require(required == baseMergeSize + 2 * (29 + 14));
    Require(merge(base, "../sibling.html", "http://foo.com/foo/../sibling.html"));
    Require(merge(base, "", "http://foo.com/foo/"));
    Require(merge(base, "/root.html", "http://foo.com/root.html"));
    Require(merge(base, "a?q=1#f", "http://foo.com/foo/a?q=1#f"));
    Require(merge("https://u:p@foo.com:8443/a/b?x=1#top", "c", "https://u:p@foo.com:8443/a/c"));
    Require(merge("http://foo.com", "x", "http://foo.com/x"));
    Require(merge("http://foo.com:80/", "x", "http://foo.com/x"));
    Require(merge("http://foo.com/a/b", "mailto:x", "http://foo.com/a/mailto:x"));
    Require(merge(base, "a%20b.html", "http://foo.com/foo/a%20b.html"));
    Require(merge(base, "~user/x", "http://foo.com/foo/~user/x"));
    Require(merge(base, "file(1).png", "http://foo.com/foo/file(1).png"));
    Require(merge(base, "a+b=c;d!e", "http://foo.com/foo/a+b=c;d!e"));

    Require(merge(base, "http://bar.com/other", "http://bar.com/other") && required == 21);
    const std::size_t absoluteSize = baseMergeSize + 2 * (29 + 20);
    for (std::size_t i = 21; i < absoluteSize; ++i) Require(merged[i] == '\0');
    Require(merged[absoluteSize] == 'Z');
    Require(merge(base, "//bar.com/x", "//bar.com/x") && required == 12);

    required = 0;
    Require(sceHttpUriMerge(nullptr, base, "./default.html", &required, 0, 0) == 0);
    Require(required == baseMergeSize + 2 * (29 + 14));
    Require(sceHttpUriMerge(nullptr, base, "http://bar.com/other", &required, 0, 0) == 0);
    Require(required == absoluteSize);
    Require(sceHttpUriMerge(nullptr, base, "x", nullptr, 0, 0) == 0);
    std::memset(merged, 'Z', sizeof(merged));
    Require(sceHttpUriMerge(merged, base, "./default.html", &required, baseMergeSize + 2 * (29 + 14) - 1, 0) == outOfMemory);
    Require(required == baseMergeSize + 2 * (29 + 14) && merged[0] == 'Z');
    Require(sceHttpUriMerge(merged, base, "http://bar.com/other", &required, absoluteSize - 1, 0) == outOfMemory);
    Require(merged[0] == 'Z');
    Require(sceHttpUriMerge(merged, base, "./default.html", nullptr, baseMergeSize + 2 * (29 + 14), 0) == 0);
    Require(Equal(merged, "http://foo.com/foo/./default.html"));

    required = 123;
    Require(sceHttpUriMerge(merged, nullptr, "./x", &required, sizeof(merged), 0) == invalidValue);
    Require(sceHttpUriMerge(merged, base, nullptr, &required, sizeof(merged), 0) == invalidValue);
    Require(sceHttpUriMerge(merged, base, "./x", &required, sizeof(merged), 1) == invalidValue);
    Require(sceHttpUriMerge(nullptr, nullptr, nullptr, &required, 0, 1) == invalidValue);
    Require(sceHttpUriMerge(merged, "http://[::1/", "./x", &required, sizeof(merged), 0) == invalidUrl);
    Require(sceHttpUriMerge(merged, "http://h:80a/a", "c", &required, sizeof(merged), 0) == invalidUrl);
    Require(required == 123);
    CheckUriMergeAsMeasured();

    constexpr int beforeInit = static_cast<int>(0x80431001);
    constexpr int aborted = static_cast<int>(0x80431080);
    constexpr int invalidId = static_cast<int>(0x80431100);
    const auto flushCaches = [](int id, int expected) {
        Require(sceHttpAuthCacheFlush(id) == expected);
        Require(sceHttpCookieFlush(id) == expected);
    };
    flushCaches(0, beforeInit);
    flushCaches(-1, beforeInit);
    flushCaches(request, beforeInit);
    CheckPoolStatsRejected(0, beforeInit);
    CheckPoolStatsRejected(-1, beforeInit);
    CheckPoolStatsRejected(request, beforeInit);
    Require(Throws([] { sceHttpGetMemoryPoolStats(0, nullptr); }));
    HttpEpollHandle epoll = nullptr;
    HttpNBEvent events[2]{};
    Require(sceHttpCreateEpoll(1, &epoll) == beforeInit);
    Require(sceHttpAbortWaitRequest(nullptr) == beforeInit);
    Require(sceHttpTerm_nid_postfix(1) == beforeInit);
    const int context = sceHttpInit_nid_postfix(0, 0, 0x10000);
    Require(context > 0);
    flushCaches(context, 0);
    flushCaches(context, 0);
    flushCaches(0, invalidId);
    flushCaches(-1, invalidId);
    flushCaches(INT32_MAX, invalidId);
    flushCaches(request, invalidId);
    CheckPoolStats(context, 0x10000);
    CheckPoolStats(context, 0x10000);
    CheckPoolStatsRejected(0, invalidId);
    CheckPoolStatsRejected(-1, invalidId);
    CheckPoolStatsRejected(INT32_MAX, invalidId);
    CheckPoolStatsRejected(request, invalidId);
    Require(Throws([context] { sceHttpGetMemoryPoolStats(context, nullptr); }));
    Require(Throws([] { sceHttpGetMemoryPoolStats(INT32_MAX, nullptr); }));
    Require(sceHttpCreateEpoll(-1, &epoll) == invalidId);
    Require(sceHttpCreateEpoll(context, nullptr) == invalidValue);
    Require(sceHttpCreateEpoll(context, &epoll) == 0 && epoll != nullptr);
    Require(sceHttpWaitRequest(epoll, events, 2, 0) == 0);
    Require(sceHttpWaitRequest(epoll, events, 2, 1000) == 0);
    Require(sceHttpWaitRequest(epoll, nullptr, 2, 0) == invalidValue);
    Require(sceHttpWaitRequest(epoll, events, 0, 0) == invalidValue);
    Require(sceHttpWaitRequest(nullptr, events, 2, 0) == invalidValue);
    const auto unknownEpoll = reinterpret_cast<HttpEpollHandle>(&events);
    Require(sceHttpAbortWaitRequest(nullptr) == invalidValue);
    Require(sceHttpAbortWaitRequest(unknownEpoll) == invalidId);
    Require(sceHttpWaitRequest(unknownEpoll, events, 2, -1) == invalidId);
    Require(sceHttpDestroyEpoll(context, nullptr) == invalidValue);
    Require(sceHttpDestroyEpoll(context, unknownEpoll) == invalidId);
    Require(sceHttpDestroyEpoll(-1, epoll) == invalidId);

    auto waitWokenBy = [&](auto wake) {
        std::atomic<bool> started{false};
        std::atomic<int> result{1};
        std::thread waiter([&] {
            started = true;
            result = sceHttpWaitRequest(epoll, events, 2, -1);
        });
        while (!started) std::this_thread::yield();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        Require(result == 1);
        wake();
        waiter.join();
        return result.load();
    };
    Require(waitWokenBy([&] { Require(sceHttpAbortWaitRequest(epoll) == 0); }) == aborted);
    Require(sceHttpWaitRequest(epoll, events, 2, 0) == aborted);
    Require(sceHttpWaitRequest(epoll, events, 2, -1) == aborted);
    Require(sceHttpDestroyEpoll(context, epoll) == 0);
    Require(sceHttpAbortWaitRequest(epoll) == invalidId);
    Require(sceHttpDestroyEpoll(context, epoll) == invalidId);

    Require(sceHttpCreateEpoll(context, &epoll) == 0);
    Require(waitWokenBy([&] { Require(sceHttpDestroyEpoll(context, epoll) == 0); }) == 0);

    Require(sceHttpCreateEpoll(context, &epoll) == 0);
    const int otherContext = sceHttpInit_nid_postfix(0, 0, 0x23000);
    Require(otherContext > 0 && otherContext != context);
    flushCaches(otherContext, 0);
    CheckPoolStats(otherContext, 0x23000);
    CheckPoolStats(context, 0x10000);
    Require(sceHttpTerm_nid_postfix(otherContext) == 0);
    flushCaches(otherContext, invalidId);
    CheckPoolStatsRejected(otherContext, invalidId);
    CheckPoolStats(context, 0x10000);
    flushCaches(context, 0);
    Require(sceHttpTerm_nid_postfix(otherContext) == invalidId);
    Require(sceHttpWaitRequest(epoll, events, 2, 0) == 0);
    Require(waitWokenBy([&] { Require(sceHttpTerm_nid_postfix(context) == 0); }) == 0);
    Require(sceHttpWaitRequest(epoll, events, 2, 0) == beforeInit);
    Require(sceHttpTerm_nid_postfix(context) == beforeInit);
    flushCaches(context, beforeInit);
    CheckPoolStatsRejected(context, beforeInit);
    const int newContext = sceHttpInit_nid_postfix(0, 0, 0x123456000ull);
    Require(newContext > 0 && newContext != context && newContext != otherContext);
    flushCaches(context, invalidId);
    flushCaches(otherContext, invalidId);
    flushCaches(newContext, 0);
    CheckPoolStatsRejected(context, invalidId);
    CheckPoolStatsRejected(otherContext, invalidId);
    CheckPoolStats(newContext, 0x123456000ull);
    Require(sceHttpTerm_nid_postfix(newContext) == 0);
    flushCaches(newContext, beforeInit);
    CheckPoolStatsRejected(newContext, beforeInit);
    const int emptyPoolContext = sceHttpInit_nid_postfix(0, 0, 0);
    Require(emptyPoolContext > 0);
    CheckPoolStats(emptyPoolContext, 0);
    CheckPoolStatsRejected(newContext, invalidId);
    Require(sceHttpTerm_nid_postfix(emptyPoolContext) == 0);

    char data[16];
    Require(sceHttpReadData(1, data, sizeof(data)) == network);

    Require(sceHttpCreateRequest2(1, "GET", "/", 0) > 0);
    Require(sceHttpSetInflateGZIPEnabled(1, 0) == 0);
    Require(sceHttpSetInflateGZIPEnabled(1, 1) == 0);
    Require(sceHttpSetInflateGZIPEnabled(1, 2) == invalidValue);
    Require(sceHttpSetInflateGZIPEnabled(1, -1) == invalidValue);
    Require(sceHttpsEnableOption(1, 0) == 0);
    Require(sceHttpsLoadCert(1, 0, nullptr, nullptr, nullptr) == 0);
    Require(sceHttpsUnloadCert(1) == 0);
    Require(sceHttpSetResponseHeaderMaxSize(1, 8192) == 0);
    Require(sceHttpSetRecvBlockSize(1, 0x4000) == 0);
    Require(sceHttpRedirectCacheFlush(1) == 0);
    int authUserArg = 0;
    Require(sceHttpSetAuthInfoCallback(1, AuthInfo, &authUserArg) == 0);
    Require(sceHttpReadData(1, data, sizeof(data)) == network);
    int httpErrno = -1;
    Require(sceHttpGetLastErrno(1, &httpErrno) == 0);
    Require(httpErrno == 0);
    Require(sceHttpGetLastErrno(1, nullptr) == invalidValue);
    Require(sceHttpsSetSslVersion(1, 0) == 0);
    Require(sceHttpSetRedirectCallback(1, nullptr, nullptr) == 0);
    Require(sceHttpSetCookieRecvCallback(1, nullptr, nullptr) == 0);
    int sslError = -1;
    std::uint32_t sslDetail = 0xFFFFFFFFu;
    Require(sceHttpsGetSslError(1, &sslError, &sslDetail) == 0);
    Require(sslError == 0 && sslDetail == 0);
    Require(sceHttpsGetSslError(1, nullptr, &sslDetail) == invalidValue);
    Require(sceHttpsGetSslError(1, &sslError, nullptr) == invalidValue);

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
    Require(phrase == nullptr && phraseLength == 0);
    Require(parse(response, 16) == 17 && phrase == response + 12 && phraseLength == 3);
    CheckStatusLineAsMeasured();
    auto sweep = [](const char* src, const char* expected) {
        char swept[128];
        std::memset(swept, 'x', sizeof(swept));
        return sceHttpUriSweepPath(swept, src, std::strlen(src) + 1) == 0 && Equal(swept, expected);
    };
    Require(sceHttpUriSweepPath(nullptr, nullptr, 0) == 0);
    Require(sceHttpUriSweepPath(nullptr, "/foo", 5) == invalidValue);
    char sweptPath[16];
    Require(sceHttpUriSweepPath(sweptPath, nullptr, 5) == invalidValue);
    Require(sweep("foo/../bar", "foo/../bar"));
    Require(sweep("/foo/../bar", "/bar"));
    Require(sweep("/foo/./bar", "/foo/bar"));
    Require(sweep("/foo/.", "/foo/."));
    Require(sweep("/foo/..", "/foo/.."));
    Require(sweep("/foo/bar/../foo/././../../../test/index.html", "/test/index.html"));
    Require(sweep("/", "/"));
    Require(sweep("", ""));
    Require(sweep("/a/b/c/../../d/", "/a/d/"));
    Require(sweep("/../a", "/a"));
    Require(sweep("/a/..b/.c", "/a/..b/.c"));
    Require(sceHttpUriSweepPath(sweptPath, "/a/b/../c", 6) == 0 && Equal(sweptPath, "/a/b/"));
    Require(sceHttpUriSweepPath(sweptPath, "/ab/c", 3) == 0 && Equal(sweptPath, "/a"));
    Require(sceHttpUriSweepPath(sweptPath, "/a/./b", 5) == 0 && Equal(sweptPath, "/a/."));
    Require(sceHttpUriSweepPath(sweptPath, "/a/b/../c", 8) == 0 && Equal(sweptPath, "/a/"));
    Require(sceHttpUriSweepPath(sweptPath, "/a/b/../c", 5) == 0 && Equal(sweptPath, "/a/b"));
    Require(sweep("/a/b/..", "/a/b/.."));
    Require(sweep("/a/b/./.", "/a/b/."));
    char sweptByte[3] = {'x', 'x', 'x'};
    Require(sceHttpUriSweepPath(sweptByte, "/a", 1) == 0 && sweptByte[0] == '/' && sweptByte[1] == '\0' && sweptByte[2] == 'x');
    sweptByte[0] = 'x';
    Require(sceHttpUriSweepPath(sweptByte, "", 1) == 0 && sweptByte[0] == '\0');

    {
        SceHttpUriElement source{};
        char parsedPool[256];
        Require(sceHttpUriParse(&source, "http://user:pw@example.com:8080/a/b?q=1#frag", parsedPool, nullptr, sizeof(parsedPool)) == 0);
        SceHttpUriElement copy{};
        char pool[256];
        std::size_t needed = 0;
        Require(sceHttpUriCopy(nullptr, &source, nullptr, 0, &needed) == 0 && needed == 5 + 12 + 5 + 5 + 3);
        Require(sceHttpUriCopy(&copy, &source, pool, sizeof(pool), nullptr) == 0);
        Require(std::strcmp(copy.scheme, "http") == 0 && std::strcmp(copy.hostname, "example.com") == 0 && std::strcmp(copy.path, "/a/b") == 0);
        Require(std::strcmp(copy.username, "user") == 0 && std::strcmp(copy.password, "pw") == 0 && copy.port == 8080);
        Require(copy.query == nullptr && copy.fragment == nullptr && copy.opaque == 0);
        Require(sceHttpUriCopy(&copy, &source, pool, needed - 1, nullptr) == static_cast<int>(0x80431022));
        Require(copy.scheme == nullptr && copy.hostname == nullptr && pool[0] == 0);
        Require(sceHttpUriCopy(&copy, &source, pool, needed, &needed) == 0 && needed == 30);
        Require(sceHttpUriCopy(&copy, nullptr, pool, sizeof(pool), nullptr) == static_cast<int>(0x80433060));
        Require(sceHttpUriCopy(nullptr, &source, nullptr, 0, nullptr) == static_cast<int>(0x804311FE));
        SceHttpUriElement empty{};
        Require(sceHttpUriCopy(&copy, &empty, pool, sizeof(pool), &needed) == 0 && needed == 5 && copy.scheme != nullptr && copy.scheme[0] == 0);
        Require(copy.scheme == pool && copy.hostname == pool + 1 && copy.path == pool + 2 && copy.username == pool + 3 && copy.password == pool + 4);

        auto allBytes = [](const void* data, std::size_t size, unsigned char value) {
            const auto* bytes = static_cast<const unsigned char*>(data);
            for (std::size_t i = 0; i < size; ++i) {
                if (bytes[i] != value) return false;
            }
            return true;
        };
        std::memset(pool, '~', sizeof(pool));
        std::memset(&copy, 0x5a, sizeof(copy));
        Require(sceHttpUriCopy(&copy, &source, pool, 64, &needed) == 0 && needed == 30);
        Require(copy.scheme == pool && copy.hostname == pool + 5 && copy.path == pool + 17 && copy.username == pool + 22 && copy.password == pool + 27);
        Require(allBytes(pool + 30, 34, 0) && pool[64] == '~' && copy.query == nullptr && copy.fragment == nullptr);
        Require(allBytes(reinterpret_cast<const unsigned char*>(&copy) + 4, 4, 0) && allBytes(copy.reserved, sizeof(copy.reserved), 0));

        std::memset(pool, '~', sizeof(pool));
        std::memset(&copy, 0x5a, sizeof(copy));
        needed = 0xabcd;
        Require(sceHttpUriCopy(&copy, &source, pool, 29, &needed) == static_cast<int>(0x80431022) && needed == 0xabcd);
        Require(allBytes(&copy, sizeof(copy), 0) && allBytes(pool, 29, 0) && pool[29] == '~');
        std::memset(pool, '~', sizeof(pool));
        Require(sceHttpUriCopy(&copy, &empty, pool, 0, &needed) == static_cast<int>(0x80431022) && pool[0] == '~' && needed == 0xabcd);
        Require(sceHttpUriCopy(&copy, &source, pool, 4, nullptr) == static_cast<int>(0x80431022));

        std::memset(pool, '~', sizeof(pool));
        Require(sceHttpUriCopy(nullptr, &source, pool, sizeof(pool), &needed) == 0 && needed == 30 && pool[0] == '~');
        Require(sceHttpUriCopy(nullptr, &source, pool, sizeof(pool), nullptr) == static_cast<int>(0x804311FE));
        std::memset(&copy, 0x5a, sizeof(copy));
        Require(sceHttpUriCopy(&copy, &source, nullptr, 64, &needed) == 0 && needed == 30 && allBytes(&copy, sizeof(copy), 0x5a));
        Require(sceHttpUriCopy(&copy, &source, nullptr, 64, nullptr) == static_cast<int>(0x804311FE) && allBytes(&copy, sizeof(copy), 0x5a));
        needed = 0xabcd;
        Require(sceHttpUriCopy(&copy, nullptr, pool, sizeof(pool), &needed) == static_cast<int>(0x80433060) && needed == 0xabcd && allBytes(&copy, sizeof(copy), 0x5a));
        Require(sceHttpUriCopy(nullptr, nullptr, nullptr, 0, nullptr) == static_cast<int>(0x80433060));

        char scheme[] = "SCH";
        char user[] = "usr";
        char pass[] = "pwd";
        char host[] = "hostname";
        char path[] = "/p/q";
        char query[] = "?query";
        char fragment[] = "#frag";
        char blank[] = "";
        SceHttpUriElement manual{};
        manual.opaque = 7;
        manual.scheme = scheme;
        manual.username = user;
        manual.password = pass;
        manual.hostname = host;
        manual.path = path;
        manual.query = query;
        manual.fragment = fragment;
        manual.port = 1234;
        std::memset(manual.reserved, 0x11, sizeof(manual.reserved));
        Require(sceHttpUriCopy(&copy, &manual, pool, sizeof(pool), &needed) == 0 && needed == 26 && copy.opaque == 0 && copy.port == 1234);
        Require(Equal(copy.scheme, "SCH") && copy.hostname == pool + 4 && Equal(copy.hostname, "hostname") && copy.path == pool + 13 && Equal(copy.path, "/p/q"));
        Require(copy.username == pool + 18 && Equal(copy.username, "usr") && copy.password == pool + 22 && Equal(copy.password, "pwd"));
        Require(copy.query == nullptr && copy.fragment == nullptr && allBytes(copy.reserved, sizeof(copy.reserved), 0));

        SceHttpUriElement mixed{};
        mixed.hostname = host;
        mixed.username = blank;
        mixed.query = query;
        mixed.port = 80;
        Require(sceHttpUriCopy(&copy, &mixed, pool, sizeof(pool), &needed) == 0 && needed == 13 && copy.port == 80);
        Require(copy.scheme == pool && copy.hostname == pool + 1 && copy.path == pool + 10 && copy.username == pool + 11 && copy.password == pool + 12);

        SceHttpUriElement inPlace = manual;
        Require(sceHttpUriCopy(&inPlace, &inPlace, pool, sizeof(pool), &needed) == 0 && needed == 5 && inPlace.port == 0);
        Require(inPlace.scheme == pool && inPlace.hostname == pool + 1 && inPlace.password == pool + 4 && inPlace.password[0] == 0);
        inPlace = manual;
        Require(sceHttpUriCopy(&inPlace, &inPlace, nullptr, 0, &needed) == 0 && needed == 26 && inPlace.scheme == scheme);

        alignas(SceHttpUriElement) char hostPool[256];
        auto* inside = reinterpret_cast<SceHttpUriElement*>(hostPool + 160);
        std::memcpy(static_cast<void*>(inside), &manual, sizeof(manual));
        Require(sceHttpUriCopy(&copy, inside, hostPool, sizeof(hostPool), &needed) == 0 && needed == 5 && copy.port == 0 && copy.hostname[0] == 0);

        std::memcpy(hostPool + 200, "HOSTINPOOL", 11);
        SceHttpUriElement pooled = manual;
        pooled.hostname = hostPool + 200;
        Require(sceHttpUriCopy(nullptr, &pooled, hostPool, sizeof(hostPool), &needed) == 0 && needed == 28);
        Require(sceHttpUriCopy(&copy, &pooled, hostPool, sizeof(hostPool), &needed) == 0 && needed == 18 && copy.hostname == hostPool + 4 && copy.hostname[0] == 0);
        Require(Equal(copy.path, "/p/q") && copy.path == hostPool + 5 && copy.port == 1234);
    }
}
