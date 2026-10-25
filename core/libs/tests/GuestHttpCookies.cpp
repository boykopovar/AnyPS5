#include "prx/libSceHttp/include/Http.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

constexpr int NotFound = static_cast<int>(0x80431025u);
constexpr int InvalidId = static_cast<int>(0x80431100u);
constexpr int OutOfSize = static_cast<int>(0x80431104u);
constexpr int InvalidValue = static_cast<int>(0x804311FEu);
constexpr int InvalidDomain = static_cast<int>(0x804321FEu);
constexpr int InvalidUrl = static_cast<int>(0x80433060u);

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

int AddResult(int context, const char* url, const char* cookie) {
    return sceHttpAddCookie(context, url, cookie, std::strlen(cookie));
}

void Add(int context, const char* url, const char* cookie) {
    Require(AddResult(context, url, cookie) == 0, cookie);
}

std::string Get(int context, const char* url) {
    std::size_t required = 0;
    Require(sceHttpGetCookie(context, url, nullptr, &required, 0) == 0 && required >= 4, url);
    std::string header(required, 'x');
    std::size_t written = 0;
    Require(sceHttpGetCookie(context, url, header.data(), &written, header.size()) == 0 && written == required, url);
    Require(header.ends_with(std::string("\0 \0", 3)), "the header ends with a terminator, a space and a terminator");
    header.resize(required - 3);
    return header;
}

bool Missing(int context, const char* url) {
    std::size_t required = 7;
    return sceHttpGetCookie(context, url, nullptr, &required, 0) == NotFound && required == 7;
}

HttpCookieStats Stats(int context) {
    HttpCookieStats stats;
    std::memset(&stats, 0xAB, sizeof(stats));
    Require(sceHttpGetCookieStats(context, &stats) == 0 && stats.reserved == 0xABABABABu, "stats are read without touching bytes 12-15");
    return stats;
}

int Init() {
    const int context = sceHttpInit_nid_postfix(0, 0, 0x10000);
    Require(context > 0, "init");
    return context;
}

void CheckErrors() {
    HttpCookieStats stats{};
    std::size_t required = 0;
    Require(sceHttpAddCookie(1, "http://a.test/", "a=1", 3) == InvalidId, "adding before init");
    Require(sceHttpGetCookie(1, "http://a.test/", nullptr, &required, 0) == InvalidId, "reading before init");
    Require(sceHttpSetCookieMaxNum(1, 1) == InvalidId, "a limit before init");
    Require(sceHttpGetCookieStats(1, &stats) == InvalidId, "stats before init");
    const int context = Init();
    Require(AddResult(context + 1000, "http://a.test/", "a=1") == InvalidId, "an unknown context");
    Require(sceHttpSetCookieMaxNumPerDomain(context + 1000, 1) == InvalidId, "a limit on an unknown context");
    Require(sceHttpAddCookie(context, nullptr, "a=1", 3) == InvalidValue, "a null url");
    Require(sceHttpAddCookie(context, "http://a.test/", nullptr, 0) == InvalidValue, "a null cookie");
    Require(AddResult(context, "http://a.test/", "") == InvalidValue, "an empty cookie");
    Require(AddResult(context, "http://[a.test/", "a=1") == InvalidUrl, "a broken host");
    Require(AddResult(context, "http:///x", "a=1") == InvalidDomain, "a url without a host");
    Require(sceHttpGetCookieStats(context, nullptr) == InvalidValue, "null stats");
    Require(Missing(context, "http://a.test/"), "an empty store has no cookie");
    Require(sceHttpGetCookie(context, "http://a.test/", nullptr, nullptr, 0) == NotFound, "no match comes before the missing outputs");
    Add(context, "http://a.test/", "a=1");
    Require(sceHttpGetCookie(context, "http://a.test/", nullptr, nullptr, 0) == InvalidValue, "no output at all");
    Require(sceHttpGetCookie(context, "http://a.test/", nullptr, &required, 0) == 0 && required == 6, "required counts the string and three bytes");
    char small[5] = {'x', 'x', 'x', 'x', 'x'};
    required = 9;
    Require(sceHttpGetCookie(context, "http://a.test/", small, &required, 3) == OutOfSize && required == 9 && small[0] == '\0' && small[1] == 'x', "a short buffer");
    Add(context, "http://a.test/", "b=2");
    char partial[7] = {'x', 'x', 'x', 'x', 'x', 'x', 'x'};
    Require(sceHttpGetCookie(context, "http://a.test/", partial, &required, 6) == OutOfSize && std::strcmp(partial, "b=2") == 0, "the cookies that fit are written");
    Require(sceHttpTerm_nid_postfix(context) == 0, "term");
    Require(sceHttpGetCookieStats(context, &stats) == InvalidId, "stats after term");
}

void CheckMatching(int context) {
    Add(context, "http://www.example.com/docs/index.html", "sid=31d4d96e407aad42");
    Add(context, "http://www.example.com/docs/index.html", "lang=en-US; Path=/; Domain=example.com");
    Add(context, "https://www.example.com/", "token=abc; Secure; HttpOnly");
    Add(context, "http://www.example.com/docs/a", "deep=1; Path=/docs/a");
    Require(Get(context, "http://www.example.com/docs/a") == "deep=1; sid=31d4d96e407aad42; lang=en-US", "longer paths come first");
    Require(Get(context, "http://www.example.com/docs") == "lang=en-US", "the default path keeps its slash");
    Require(Get(context, "http://www.example.com/") == "lang=en-US", "secure cookies need an https url");
    Require(Get(context, "https://www.example.com/") == "token=abc; lang=en-US", "an https url gets secure cookies");
    Require(Get(context, "HTTPS://www.example.com/") == "token=abc; lang=en-US", "the scheme compares without case");
    Require(Get(context, "http://example.com/") == "lang=en-US", "domain cookies reach the parent domain");
    Require(Get(context, "http://sub.www.example.com/docs/") == "sid=31d4d96e407aad42; lang=en-US", "cookies without a domain reach subdomains");
    Require(Missing(context, "http://badexample.com/"), "domain matching works on labels");
    Require(Get(context, "http://WWW.Example.COM/docs/a?x=1").starts_with("deep=1"), "hosts compare without case");
    Add(context, "http://p.test/", "px=1; Path=/docs");
    Require(Get(context, "http://p.test/docsx") == "px=1", "a path is a plain prefix");
    Add(context, "ftp://f.test/", "f=1");
    Require(Get(context, "ftp://f.test/") == "f=1", "any scheme stores and reads cookies");
    Add(context, "http://nv.test/", "novalue");
    Add(context, "http://nv.test/", "=1");
    Require(Get(context, "http://nv.test/") == "1; novalue", "a cookie may lack a name or a value");
    Add(context, "http://nv.test/", "novalue=2");
    Require(Stats(context).cookieCount == 8 && Get(context, "http://nv.test/") == "novalue=2; 1", "a value replaces a cookie without one");
    for (const char* cookie : {"x1=1", "x2=1", "x3=1", "x4=1", "x5=1"}) Add(context, "http://o.test/", cookie);
    Require(Get(context, "http://o.test/") == "x2=1; x3=1; x4=1; x5=1; x1=1", "paths of equal length follow the store order");
    Add(context, "http://o.test/", "x2=2");
    Require(Get(context, "http://o.test/") == "x3=1; x4=1; x5=1; x2=2; x1=1", "a replaced cookie moves");
    Add(context, "http://www.example.com/docs/index.html", "sid=gone; Max-Age=0");
    Add(context, "http://www.example.com/", "lang=x; Domain=.Example.com; Expires=Sun, 06 Nov 1994 08:49:37 GMT");
    Require(Get(context, "http://www.example.com/docs/") == "lang=en-US", "Max-Age=0 deletes, another domain spelling does not");
    Add(context, "http://www.example.com/", "lang=x; Domain=example.com; Expires=Sun, 06 Nov 1994 08:49:37 GMT");
    Require(Missing(context, "http://www.example.com/docs/"), "a past date deletes the same spelling");
    Add(context, "http://e.test/", "keep=1; Expires=Wed, 09 Jun 2099 10:18:14 GMT");
    Add(context, "http://e.test/", "rfc850=1; expires=Wednesday, 09-Jun-99 10:18:14 GMT");
    Add(context, "http://e.test/", "asctime=1; Expires=Thu Jan  1 00:00:01 1970");
    Add(context, "http://e.test/", "late=1; Max-Age=86400; Expires=Thu, 01 Jan 1970 00:00:01 GMT");
    Add(context, "http://e.test/", "bad=1; Expires=Feb 31 2099 10:00:00");
    Require(Get(context, "http://e.test/") == "late=1; bad=1; keep=1", "expiry dates and Max-Age");
    Add(context, "http://e.test/", "huge=1; Max-Age=10000000000");
    Add(context, "http://e.test/", "huger=1; Max-Age=99999999999999999999999999");
    Add(context, "http://e.test/", "far=1; Expires=Fri, 31 Dec 9999 23:59:59 GMT");
    Add(context, "http://e.test/", "early=1; Expires=Mon, 01 Jan 1601 00:00:00 GMT");
    Require(Get(context, "http://e.test/") == "late=1; bad=1; huge=1; huger=1; far=1; keep=1", "expiries beyond the clock range saturate");
    Require(sceHttpAddCookie(context, "http://c.test/", "cut=1234; Path=/x", 8) == 0, "the length bounds the cookie");
    Require(Get(context, "http://c.test/") == "cut=1234", "only the given length is parsed");
    Require(sceHttpCookieFlush(context) == 0 && Stats(context).cookieCount == 0, "flush empties the store");
}

void CheckDomains(int context) {
    Require(AddResult(context, "http://www.example.com/", "x=1; Domain=other.com") == InvalidDomain, "a domain the host does not end with");
    Require(AddResult(context, "http://www.d.com/", "x=1; Domain=sub.www.d.com") == InvalidDomain, "a subdomain of the host");
    Require(AddResult(context, "http://www.d.com/", "x=1; Domain=com") == InvalidValue, "a top-level domain");
    Require(AddResult(context, "http://www.d.com/", "x=1; Domain=.com") == InvalidValue, "a dotted top-level domain");
    Require(AddResult(context, "http://www.d.test/", "x=1; Domain=d.test") == InvalidValue, "two labels outside the generic domains");
    Require(AddResult(context, "http://www.r.test/", "x=1; Domain=.r.test") == InvalidValue, "two dotted labels outside the generic domains");
    Require(AddResult(context, "http://www.d.de/", "x=1; Domain=d.de") == InvalidValue, "two labels under a country domain");
    Require(Stats(context).cookieCount == 0, "refused domains store nothing");
    Add(context, "http://www.d.com/", "a=1; Domain=d.com");
    Add(context, "http://www.e.org/", "b=1; Domain=.e.org");
    Add(context, "http://a.x.d.test/", "c=1; Domain=x.d.test");
    Add(context, "http://www.d.co.uk/", "d=1; Domain=d.co.uk");
    Require(Get(context, "http://d.com/") == "a=1" && Get(context, "http://e.org/") == "b=1", "two labels under a generic domain");
    Require(Get(context, "http://x.d.test/") == "c=1" && Get(context, "http://d.co.uk/") == "d=1", "three labels elsewhere");
}

void CheckLimits(int context) {
    Add(context, "http://s.test/", "a=1");
    const HttpCookieStats one = Stats(context);
    Require(one.usedBytes == 14 && one.cookieCount == 1 && one.peakUsedBytes == 14 && one.peakCookieCount == 1 && one.evictedCookieCount == 0, "a cookie counts its domain and path");
    Require(sceHttpSetCookieMaxNum(context, 0) == 0 && Stats(context).cookieCount == 0 && Stats(context).evictedCookieCount == 0, "a setter empties the store without evicting");
    Require(sceHttpSetCookieMaxNum(context, 3000) == 0, "the total limit back to its default");
    const std::string fits = "n=" + std::string(4095, 'v');
    const std::string over = "n=" + std::string(4096, 'v');
    Add(context, "http://l.test/", fits.c_str());
    Require(AddResult(context, "http://l.test/", over.c_str()) == OutOfSize, "the default size limit is 4096");
    Require(sceHttpSetCookieMaxSize(context, 0) == 0 && AddResult(context, "http://l.test/", "a=1") == OutOfSize, "a size limit of 0 refuses every cookie");
    Require(sceHttpSetCookieMaxSize(context, 4096) == 0, "the size limit back to its default");
    for (int index = 0; index <= 50; ++index) Add(context, "http://m.test/", ("c" + std::to_string(index) + "=1").c_str());
    Require(Stats(context).cookieCount == 50 && Stats(context).evictedCookieCount == 1, "the default per-domain limit is 50");
    Require(Get(context, "http://m.test/").find("c0=1") == std::string::npos, "the oldest cookie of the domain goes");
    Require(sceHttpSetCookieMaxNumPerDomain(context, 50) == 0, "the per-domain limit stays at its default");
    for (int host = 0; host < 61; ++host) {
        for (int index = 0; index < 50 && host * 50 + index <= 3000; ++index) Add(context, ("http://h" + std::to_string(host) + ".test/").c_str(), ("c" + std::to_string(index) + "=1").c_str());
    }
    Require(Stats(context).cookieCount == 2991 && Stats(context).evictedCookieCount == 11, "over 3000 cookies the 10 oldest go");
    Require(Get(context, "http://h0.test/").find("c9=1") == std::string::npos && Get(context, "http://h0.test/").find("c10=1") != std::string::npos, "the evicted cookies are the oldest");
    Require(sceHttpSetCookieMaxNumPerDomain(context, 2) == 0, "per-domain limit");
    Add(context, "http://n.test/", "a=1");
    Add(context, "http://n.test/", "b=1");
    Require(Get(context, "http://n.test/") == "b=1; a=1", "two cookies fit");
    Add(context, "http://n.test/", "c=1");
    Require(Get(context, "http://n.test/") == "b=1; c=1", "the oldest cookie goes even after a read");
    Add(context, "http://n.test/", "b=2");
    Add(context, "http://n.test/", "d=1");
    Require(Get(context, "http://n.test/") == "b=2; d=1", "a replaced cookie counts as new");
    Require(sceHttpSetCookieTotalMaxSize(context, 30) == 0, "total size limit");
    Add(context, "http://t.test/", "a=1");
    Add(context, "http://u.test/", "b=1");
    Add(context, "http://v.test/", "c=1");
    Require(Missing(context, "http://t.test/") && Get(context, "http://v.test/") == "c=1" && Stats(context).usedBytes == 28, "the total size evicts the oldest");
}

}

int main() {
    CheckErrors();
    const int context = Init();
    const int other = Init();
    CheckMatching(context);
    Add(other, "http://a.test/", "only=other");
    Require(Missing(context, "http://a.test/"), "contexts keep separate stores");
    const int domains = Init();
    CheckDomains(domains);
    const int limited = Init();
    CheckLimits(limited);
    Require(Get(other, "http://a.test/") == "only=other", "the other store is untouched");
    for (const int id : {limited, domains, other, context}) Require(sceHttpTerm_nid_postfix(id) == 0, "term");
    return 0;
}
