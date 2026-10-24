#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

struct Http2CookieStats {
    std::size_t currentInuseSize;
    std::uint32_t currentInuseNum;
    std::size_t maxInuseSize;
    std::uint32_t maxInuseNum;
    std::uint32_t removedNum;
    std::int32_t reserved;
};

extern "C" {
int APS5_VABI sceHttp2CreateCookieBox(std::size_t);
int APS5_VABI sceHttp2DeleteCookieBox(int);
int APS5_VABI sceHttp2CookieFlush(int);
int APS5_VABI sceHttp2AddCookie(int, const char*, const char*, std::size_t);
int APS5_VABI sceHttp2GetCookie(int, const char*, char*, std::size_t*, std::size_t);
int APS5_VABI sceHttp2GetCookieStats(int, Http2CookieStats*);
int APS5_VABI sceHttp2SetCookieMaxNum(int, std::uint32_t);
int APS5_VABI sceHttp2SetCookieMaxNumPerDomain(int, std::uint32_t);
int APS5_VABI sceHttp2SetCookieMaxSize(int, std::uint32_t);
}

static void Require(bool value) { if (!value) std::abort(); }

static constexpr int UnknownScheme = static_cast<int>(0x817B1061);
static constexpr int InvalidId = static_cast<int>(0x817B1100);
static constexpr int OutOfSize = static_cast<int>(0x817B1104);
static constexpr int NullPointer = static_cast<int>(0x817B1225);
static constexpr int InvalidUrl = static_cast<int>(0x817B3060);

static int Add(int box, const char* url, const char* cookie) {
    return sceHttp2AddCookie(box, url, cookie, std::strlen(cookie));
}

static std::string Get(int box, const char* url) {
    std::size_t required = 0;
    Require(sceHttp2GetCookie(box, url, nullptr, &required, 0) == 0 && required >= 1);
    std::string header(required, '\xff');
    std::size_t again = 0;
    Require(sceHttp2GetCookie(box, url, header.data(), &again, required) == 0 && again == required && header[required - 1] == '\0');
    header.pop_back();
    return header;
}

static Http2CookieStats Stats(int box) {
    Http2CookieStats stats{};
    std::memset(&stats, 0xff, sizeof(stats));
    Require(sceHttp2GetCookieStats(box, &stats) == 0 && stats.reserved == 0);
    return stats;
}

int main() {
    static_assert(sizeof(Http2CookieStats) == 40);
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0);

    Require(Get(box, "http://example.com/") == "");
    Require(Add(box, "http://example.com/a/b", "sid=1") == 0);
    Require(Add(box, "http://Example.COM:8080/", "lang=fr; Path=/; Domain=.example.com") == 0);
    Require(Add(box, "https://example.com/", "token=x; Secure; HttpOnly; Path=/") == 0);
    Require(Add(box, "http://example.com/", "wide=1; Path=/a; Max-Age=3600") == 0);
    Require(Get(box, "http://example.com/a/c") == "sid=1; wide=1; lang=fr");
    Require(Get(box, "https://example.com/a/b?q#f") == "sid=1; wide=1; lang=fr; token=x");
    Require(Get(box, "http://example.com/") == "lang=fr");
    Require(Get(box, "http://example.com/ab") == "lang=fr");
    Require(Get(box, "http://www.example.com/a") == "lang=fr");
    Require(Get(box, "http://badexample.com/a") == "");
    Require(Get(box, "http://user:pw@example.com/a/x") == "sid=1; wide=1; lang=fr");

    Require(Add(box, "http://example.com/a/b", "sid=2") == 0);
    Require(Get(box, "http://example.com/a/") == "sid=2; wide=1; lang=fr");
    Require(Add(box, "http://example.com/", "wide=0; Path=/a; Expires=Thu, 01 Jan 1970 00:00:00 GMT") == 0);
    Require(Add(box, "http://example.com/", "lang=; Domain=example.com; Max-Age=0") == 0);
    Require(Get(box, "http://example.com/a/") == "sid=2");
    Require(Add(box, "http://example.com/", "future=1; Expires=Wed, 09 Jun 2121 10:18:14 GMT") == 0);
    Require(Add(box, "http://example.com/", "invalid=1; Expires=Feb 30 2121 10:18:14") == 0);
    Require(Get(box, "http://example.com/") == "future=1; invalid=1");

    Require(Add(box, "http://example.com/", "noequals") == 0);
    Require(Add(box, "http://example.com/", "=value") == 0);
    Require(Add(box, "http://example.com/", "evil=1; Domain=other.com") == 0);
    Require(Add(box, "http://example.com/", "evil=1; Domain=ample.com") == 0);
    Require(Get(box, "http://other.com/") == "");
    Require(Get(box, "http://example.com/") == "future=1; invalid=1");
    Require(sceHttp2AddCookie(box, "http://example.com/", "cut=12345", 5) == 0);
    Require(Get(box, "http://example.com/") == "future=1; invalid=1; cut=1");

    Http2CookieStats stats = Stats(box);
    Require(stats.currentInuseNum == 5 && stats.currentInuseSize == 4 + 6 + 7 + 8 + 4);
    Require(stats.maxInuseNum == 5 && stats.removedNum == 0);

    std::size_t required = 0;
    char small[4] = {};
    Require(sceHttp2GetCookie(box, "http://example.com/", small, &required, sizeof(small)) == OutOfSize && required == 27);
    Require(sceHttp2GetCookie(box, "http://example.com/", small, nullptr, sizeof(small)) == NullPointer);
    Require(sceHttp2GetCookie(box, nullptr, small, &required, sizeof(small)) == NullPointer);
    Require(sceHttp2GetCookie(box, "ftp://example.com/", nullptr, &required, 0) == UnknownScheme);
    Require(sceHttp2GetCookie(box, "example.com/", nullptr, &required, 0) == InvalidUrl);
    Require(sceHttp2AddCookie(box, "http:///path", "a=1", 3) == InvalidUrl);
    Require(sceHttp2AddCookie(box, "http://example.com/", nullptr, 0) == NullPointer);
    Require(sceHttp2GetCookieStats(box, nullptr) == NullPointer);

    Require(sceHttp2CookieFlush(box) == 0);
    Require(Get(box, "http://example.com/") == "");
    stats = Stats(box);
    Require(stats.currentInuseNum == 0 && stats.currentInuseSize == 0 && stats.maxInuseNum == 5);

    Require(sceHttp2SetCookieMaxSize(box, 4) == 0);
    Require(Add(box, "http://example.com/", "abc=defg") == 0);
    Require(Add(box, "http://example.com/", "ab=cd") == 0);
    Require(Get(box, "http://example.com/") == "ab=cd");
    Require(sceHttp2SetCookieMaxSize(box, 0) == 0);

    Require(sceHttp2SetCookieMaxNumPerDomain(box, 2) == 0);
    Require(Add(box, "http://example.com/", "c=1") == 0);
    Require(Get(box, "http://example.com/") == "ab=cd; c=1");
    Require(Add(box, "http://example.com/", "d=1") == 0);
    Require(Get(box, "http://example.com/") == "c=1; d=1");
    Require(Add(box, "http://example.com/", "e=1") == 0);
    Require(Get(box, "http://example.com/") == "d=1; e=1");
    Require(Add(box, "http://other.com/", "o=1") == 0);
    Require(sceHttp2SetCookieMaxNum(box, 2) == 0);
    Require(Add(box, "http://third.com/", "t=1") == 0);
    Require(Get(box, "http://example.com/") == "");
    Require(Get(box, "http://other.com/") == "o=1");
    Require(Get(box, "http://third.com/") == "t=1");
    stats = Stats(box);
    Require(stats.currentInuseNum == 2 && stats.removedNum == 4);

    Require(sceHttp2AddCookie(0, "http://example.com/", "a=1", 3) == InvalidId);
    Require(sceHttp2SetCookieMaxNum(0x7fffffff, 1) == InvalidId);
    Require(sceHttp2SetCookieMaxNumPerDomain(-1, 1) == InvalidId);
    Require(sceHttp2SetCookieMaxSize(0, 1) == InvalidId);
    Require(sceHttp2GetCookie(0, "http://example.com/", nullptr, &required, 0) == InvalidId);
    Http2CookieStats ignored{};
    Require(sceHttp2GetCookieStats(0, &ignored) == InvalidId);

    const int other = sceHttp2CreateCookieBox(0x10000);
    Require(other > 0 && Get(other, "http://example.com/") == "");
    Require(sceHttp2DeleteCookieBox(box) == 0);
    Require(sceHttp2GetCookieStats(box, &ignored) == InvalidId);
    Require(sceHttp2AddCookie(box, "http://example.com/", "a=1", 3) == InvalidId);
    Require(sceHttp2DeleteCookieBox(other) == 0);
}
