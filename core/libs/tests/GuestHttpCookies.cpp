#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

struct HttpCookieStats {
    std::uint64_t currentInuseSize;
    std::uint32_t currentInuseNum;
    std::uint64_t maxInuseSize;
    std::uint32_t maxInuseNum;
    std::uint32_t removedNum;
    std::int32_t reserved;
};

extern "C" {
int APS5_VABI sceHttpInit_nid_postfix(int, int, std::uint64_t);
int APS5_VABI sceHttpTerm_nid_postfix(int);
int APS5_VABI sceHttpAddCookie(int, const char*, const char*, std::size_t);
int APS5_VABI sceHttpGetCookie(int, const char*, char*, std::size_t*, std::size_t, int);
int APS5_VABI sceHttpCookieFlush(int);
int APS5_VABI sceHttpSetCookieMaxNum(int, std::uint32_t);
int APS5_VABI sceHttpSetCookieMaxNumPerDomain(int, std::uint32_t);
int APS5_VABI sceHttpSetCookieMaxSize(int, std::uint32_t);
int APS5_VABI sceHttpSetCookieTotalMaxSize(int, std::uint32_t);
int APS5_VABI sceHttpGetCookieStats(int, HttpCookieStats*);
}

namespace {

constexpr int BeforeInit = static_cast<int>(0x80431001u);
constexpr int UnknownScheme = static_cast<int>(0x80431061u);
constexpr int InvalidId = static_cast<int>(0x80431100u);
constexpr int OutOfSize = static_cast<int>(0x80431104u);
constexpr int InvalidValue = static_cast<int>(0x804311FEu);
constexpr int InvalidUrl = static_cast<int>(0x80433060u);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

void Add(int context, const char* url, const char* cookie) {
    Require(sceHttpAddCookie(context, url, cookie, std::strlen(cookie)) == 0, cookie);
}

std::string Get(int context, const char* url, int secure = 0) {
    std::size_t required = 0;
    Require(sceHttpGetCookie(context, url, nullptr, &required, 0, secure) == 0 && required > 0, url);
    std::string header(required, '\0');
    std::size_t written = 0;
    Require(sceHttpGetCookie(context, url, header.data(), &written, header.size(), secure) == 0 && written == required, url);
    Require(header.back() == '\0', "the cookie header is terminated");
    header.pop_back();
    return header;
}

HttpCookieStats Stats(int context) {
    HttpCookieStats stats{};
    stats.reserved = -1;
    Require(sceHttpGetCookieStats(context, &stats) == 0 && stats.reserved == 0, "stats are read");
    return stats;
}

void CheckErrors() {
    HttpCookieStats stats{};
    std::size_t required = 0;
    Require(sceHttpAddCookie(1, "http://a.test/", "a=1", 3) == BeforeInit, "adding before init");
    Require(sceHttpGetCookie(1, "http://a.test/", nullptr, &required, 0, 0) == BeforeInit, "reading before init");
    Require(sceHttpCookieFlush(1) == BeforeInit, "flushing before init");
    Require(sceHttpSetCookieMaxNum(1, 1) == BeforeInit, "a limit before init");
    Require(sceHttpGetCookieStats(1, &stats) == BeforeInit, "stats before init");
    const int context = sceHttpInit_nid_postfix(0, 0, 0x10000);
    Require(context > 0, "init");
    Require(sceHttpAddCookie(context + 1000, "http://a.test/", "a=1", 3) == InvalidId, "an unknown context");
    Require(sceHttpSetCookieMaxNumPerDomain(context + 1000, 1) == InvalidId, "a limit on an unknown context");
    Require(sceHttpAddCookie(context, nullptr, "a=1", 3) == InvalidValue, "a null url");
    Require(sceHttpAddCookie(context, "http://a.test/", nullptr, 0) == InvalidValue, "a null cookie");
    Require(sceHttpAddCookie(context, "ftp://a.test/", "a=1", 3) == UnknownScheme, "an ftp url");
    Require(sceHttpAddCookie(context, "http://[a.test/", "a=1", 3) == InvalidUrl, "a broken host");
    Require(sceHttpGetCookie(context, "http://a.test/", nullptr, nullptr, 0, 0) == InvalidValue, "no output at all");
    Require(sceHttpGetCookieStats(context, nullptr) == InvalidValue, "null stats");
    Add(context, "http://a.test/", "a=1");
    char small[3] = {'x', 'x', 'x'};
    Require(sceHttpGetCookie(context, "http://a.test/", small, &required, sizeof(small), 0) == OutOfSize && required == 4 && small[0] == 'x', "a short buffer");
    Require(sceHttpTerm_nid_postfix(context) == 0, "term");
    Require(sceHttpGetCookieStats(context, &stats) == BeforeInit, "stats after term");
}

void CheckMatching(int context) {
    Add(context, "http://www.example.com/docs/index.html", "sid=31d4d96e407aad42");
    Add(context, "http://www.example.com/docs/index.html", "lang=en-US; Path=/; Domain=example.com");
    Add(context, "https://www.example.com/", "token=abc; Secure; HttpOnly");
    Add(context, "http://www.example.com/docs/a", "deep=1; Path=/docs/a");
    Require(Get(context, "http://www.example.com/docs/a") == "deep=1; sid=31d4d96e407aad42; lang=en-US", "longer paths come first");
    Require(Get(context, "http://www.example.com/docs") == "sid=31d4d96e407aad42; lang=en-US", "the default path is the directory");
    Require(Get(context, "http://www.example.com/docsx") == "lang=en-US", "a path prefix must end at a slash");
    Require(Get(context, "http://www.example.com/", 1) == "lang=en-US; token=abc", "secure cookies need a secure request");
    Require(Get(context, "http://example.com/") == "lang=en-US", "domain cookies reach the parent domain");
    Require(Get(context, "http://sub.www.example.com/docs/") == "lang=en-US", "host-only cookies stay on their host");
    Require(Get(context, "http://badexample.com/").empty(), "domain matching works on labels");
    Require(Get(context, "http://WWW.Example.COM/docs/a?x=1").starts_with("deep=1"), "hosts compare without case");
    Add(context, "http://www.example.com/", "x=1; Domain=other.com");
    Add(context, "http://www.example.com/", "novalue");
    Add(context, "http://www.example.com/", "=1");
    Require(Stats(context).currentInuseNum == 4, "rejected cookies are not stored");
    Add(context, "http://www.example.com/docs/index.html", "sid=new");
    Require(Get(context, "http://www.example.com/docs/") == "sid=new; lang=en-US", "a cookie replaces its namesake and keeps its place");
    Add(context, "http://www.example.com/docs/index.html", "sid=gone; Max-Age=0");
    Add(context, "http://www.example.com/", "lang=x; Domain=.Example.com; Expires=Sun, 06 Nov 1994 08:49:37 GMT");
    Require(Get(context, "http://www.example.com/docs/").empty(), "expired cookies delete their namesakes");
    Add(context, "http://www.example.com/", "keep=1; Expires=Wed, 09 Jun 2099 10:18:14 GMT");
    Add(context, "http://www.example.com/", "rfc850=1; expires=Wednesday, 09-Jun-99 10:18:14 GMT");
    Add(context, "http://www.example.com/", "asctime=1; Expires=Thu Jan  1 00:00:01 1970");
    Add(context, "http://www.example.com/", "late=1; Max-Age=86400; Expires=Thu, 01 Jan 1970 00:00:01 GMT");
    Add(context, "http://www.example.com/", "bad=1; Expires=Feb 31 2099 10:00:00");
    Require(Get(context, "http://www.example.com/") == "keep=1; late=1; bad=1", "expiry dates and Max-Age");
    Require(sceHttpAddCookie(context, "http://www.example.com/", "cut=1234; Path=/x", 8) == 0, "the length bounds the cookie");
    Require(Get(context, "http://www.example.com/x").ends_with("cut=1234"), "only the given length is parsed");
    Require(sceHttpCookieFlush(context) == 0 && Stats(context).currentInuseNum == 0, "flush empties the store");
}

void CheckLimits(int context) {
    Require(sceHttpSetCookieMaxSize(context, 4) == 0, "max size");
    Add(context, "http://l.test/", "abcd=e");
    Add(context, "http://l.test/", "ab=cd");
    Require(Get(context, "http://l.test/") == "ab=cd", "a cookie over the size limit is ignored");
    Require(sceHttpSetCookieMaxSize(context, 0) == 0, "no size limit");
    Require(sceHttpSetCookieMaxNumPerDomain(context, 2) == 0, "per-domain limit");
    Add(context, "http://l.test/", "b=1");
    Require(Get(context, "http://l.test/") == "ab=cd; b=1", "two cookies fit");
    Add(context, "http://l.test/", "c=1");
    Require(Get(context, "http://l.test/") == "b=1; c=1", "the least recently used cookie of the domain goes");
    Add(context, "http://m.test/", "m=1");
    Require(Get(context, "http://l.test/") == "b=1; c=1", "another domain has its own count");
    Require(sceHttpSetCookieMaxNum(context, 2) == 0, "lowering the total limit");
    Require(Get(context, "http://m.test/").empty() && Get(context, "http://l.test/") == "b=1; c=1", "lowering a limit evicts at once");
    Require(sceHttpSetCookieMaxNum(context, 0) == 0 && sceHttpSetCookieMaxNumPerDomain(context, 0) == 0, "no count limits");
    Require(sceHttpSetCookieTotalMaxSize(context, 7) == 0, "total size limit");
    Add(context, "http://n.test/", "nn=22");
    Require(Get(context, "http://l.test/") == "c=1" && Get(context, "http://n.test/") == "nn=22", "the total size evicts the oldest");
    const HttpCookieStats stats = Stats(context);
    Require(stats.currentInuseNum == 2 && stats.currentInuseSize == 6, "current use");
    Require(stats.maxInuseNum == 3 && stats.maxInuseSize == 6, "peak use");
    Require(stats.removedNum == 3, "evictions are counted");
}

}

int main() {
    CheckErrors();
    const int context = sceHttpInit_nid_postfix(0, 0, 0x10000);
    const int other = sceHttpInit_nid_postfix(0, 0, 0x10000);
    Require(context > 0 && other > 0, "init");
    CheckMatching(context);
    Add(other, "http://a.test/", "only=other");
    Require(Get(context, "http://a.test/").empty(), "contexts keep separate stores");
    const int limited = sceHttpInit_nid_postfix(0, 0, 0x10000);
    Require(limited > 0, "init");
    CheckLimits(limited);
    Require(Get(other, "http://a.test/") == "only=other", "the other store is untouched");
    Require(sceHttpTerm_nid_postfix(limited) == 0 && sceHttpTerm_nid_postfix(other) == 0 && sceHttpTerm_nid_postfix(context) == 0, "term");
    return 0;
}
