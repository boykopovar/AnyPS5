#include "prx/libSceHttp2/include/Http2.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

static void Require(bool value) { if (!value) std::abort(); }

static constexpr int NotFound = static_cast<int>(0x817B1025);
static constexpr int InvalidId = static_cast<int>(0x817B1100);
static constexpr int OutOfSize = static_cast<int>(0x817B1104);
static constexpr int InvalidValue = static_cast<int>(0x817B11FE);
static constexpr int PoolTooSmall = static_cast<int>(0x817B121D);
static constexpr int Rejected = static_cast<int>(0x817B1219);

static int Add(int box, const char* url, const std::string& cookie) {
    return sceHttp2AddCookie(box, url, cookie.c_str(), cookie.size());
}

static std::string Get(int box, const char* url) {
    std::size_t required = 0;
    Require(sceHttp2GetCookie(box, url, nullptr, &required, 0) == 0 && required >= 3);
    std::string header(required, '\x7f');
    std::size_t again = 0;
    Require(sceHttp2GetCookie(box, url, header.data(), &again, required) == 0 && again == required);
    Require(header.compare(required - 3, 3, std::string("\0 \0", 3)) == 0);
    header.resize(required - 3);
    return header;
}

static bool NoMatch(int box, const char* url) {
    std::size_t required = 7;
    char buffer[16] = {'x'};
    return sceHttp2GetCookie(box, url, nullptr, &required, 0) == NotFound && required == 7 &&
           sceHttp2GetCookie(box, url, buffer, &required, sizeof(buffer)) == NotFound && required == 7 && buffer[0] == 'x';
}

static void CheckUnmappedOutputs(int box) {
    auto clobberErrno = [](std::uintptr_t, std::size_t) { errno = 99; };
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(clobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(clobberErrno);
    const char* url = "http://example.com/a/b";
    auto* badSize = reinterpret_cast<std::size_t*>(0x10);
    auto* badCookie = reinterpret_cast<char*>(0x10);
    std::size_t required = 0;
    char header[0x1000];
    std::memset(header, 'Z', sizeof(header));
    errno = 71;
    Require(sceHttp2GetCookie(box, url, nullptr, badSize, 0) == InvalidValue);
    Require(sceHttp2GetCookie(box, url, badCookie, &required, 1) == InvalidValue);
    Require(sceHttp2GetCookie(box, url, badCookie, &required, sizeof(header)) == InvalidValue && required == 0);
    Require(sceHttp2GetCookie(box, url, header, badSize, sizeof(header)) == InvalidValue);
    for (char ch : header) Require(ch == 'Z');

    std::size_t pageSize = 4096u;
    void* roSizePage = nullptr;
    void* roCookiePage = nullptr;
    const std::size_t roSizeInitial = 0x55u;
    char roCookieInitial[16];
    std::memset(roCookieInitial, 'Q', sizeof(roCookieInitial));
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    if (info.dwPageSize != 0) pageSize = info.dwPageSize;
    roSizePage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    roCookiePage = VirtualAlloc(nullptr, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    Require(roSizePage != nullptr && roCookiePage != nullptr);
    std::memcpy(roSizePage, &roSizeInitial, sizeof(roSizeInitial));
    std::memcpy(roCookiePage, roCookieInitial, sizeof(roCookieInitial));
    DWORD oldProtect = 0;
    Require(VirtualProtect(roSizePage, pageSize, PAGE_READONLY, &oldProtect) != 0);
    Require(VirtualProtect(roCookiePage, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
    const long sysPage = sysconf(_SC_PAGESIZE);
    if (sysPage > 0) pageSize = static_cast<std::size_t>(sysPage);
    roSizePage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    roCookiePage = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(roSizePage != MAP_FAILED && roCookiePage != MAP_FAILED);
    std::memcpy(roSizePage, &roSizeInitial, sizeof(roSizeInitial));
    std::memcpy(roCookiePage, roCookieInitial, sizeof(roCookieInitial));
    Require(mprotect(roSizePage, pageSize, PROT_READ) == 0);
    Require(mprotect(roCookiePage, pageSize, PROT_READ) == 0);
#endif
    Require(sceHttp2GetCookie(box, url, header, static_cast<std::size_t*>(roSizePage), sizeof(header)) == InvalidValue);
    for (char ch : header) Require(ch == 'Z');
    Require(std::memcmp(roSizePage, &roSizeInitial, sizeof(roSizeInitial)) == 0);

    std::size_t untouchedRequired = 0x77u;
    Require(sceHttp2GetCookie(box, url, static_cast<char*>(roCookiePage), &untouchedRequired, sizeof(header)) == InvalidValue);
    Require(untouchedRequired == 0x77u);
    Require(std::memcmp(roCookiePage, roCookieInitial, sizeof(roCookieInitial)) == 0);
#ifdef _WIN32
    VirtualFree(roSizePage, 0, MEM_RELEASE);
    VirtualFree(roCookiePage, 0, MEM_RELEASE);
#else
    munmap(roSizePage, pageSize);
    munmap(roCookiePage, pageSize);
#endif

    const char* const badText = reinterpret_cast<const char*>(0x10);
    Require(sceHttp2GetCookie(box, badText, header, &required, sizeof(header)) == InvalidValue);
    Require(sceHttp2AddCookie(box, badText, "a=b", 3) == InvalidValue);
    Require(sceHttp2AddCookie(box, url, badText, 3) == InvalidValue);
#ifndef _WIN32
    rusage before{};
    getrusage(RUSAGE_SELF, &before);
#endif
    Require(sceHttp2AddCookie(box, url, badText, std::size_t{1} << 40) == InvalidValue);
#ifndef _WIN32
    rusage after{};
    getrusage(RUSAGE_SELF, &after);
#ifdef __APPLE__
    Require(after.ru_maxrss - before.ru_maxrss < 64 * 1024 * 1024);
#else
    Require(after.ru_maxrss - before.ru_maxrss < 64 * 1024);
#endif
#endif
    Require(errno == 71);
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);
}

static Http2CookieStats Stats(int box) {
    alignas(8) unsigned char raw[40];
    std::memset(raw, 0xff, sizeof(raw));
    Require(sceHttp2GetCookieStats(box, reinterpret_cast<Http2CookieStats*>(raw)) == 0);
    for (std::size_t i = 12; i < 16; ++i) Require(raw[i] == 0xff);
    for (std::size_t i = 32; i < 40; ++i) Require(raw[i] == 0xff);
    Http2CookieStats stats{};
    std::memcpy(&stats, raw, sizeof(stats));
    return stats;
}

static int FillDomain(int box, int count) {
    for (int i = 0; i < count; ++i) {
        if (Add(box, "http://example.com/", "c" + std::to_string(i) + "=1") != 0) return -1;
    }
    return 0;
}

static void CheckDateTokens(int box) {
    const std::string longDigits(256, '9');
    const struct {
        std::string date;
        bool expired;
    } cases[] = {
        {"31 Dec 1999 23:59:59", true},
        {"01 Jan 70 00:00:00", true},
        {"01 Jan 1970 00:00:00; Expires=01 Jan 9999 00:00:00", false},
        {"01suffix Jan 1970 00:00:00GMT", true},
        {"001 Jan 1970 00:00:00", false},
        {"01 Jan 01970 00:00:00", false},
        {"01 Jan 1970 000:00:00", false},
        {"01 Jan 1970 00:000:00", false},
        {"01 Jan 1970 00:00:000", false},
        {"4294967297 Jan 1970 00:00:00", false},
        {"01 Jan 4294969266 00:00:00", false},
        {"01 Jan 1970 4294967296:00:00", false},
        {"01 Jan 1970 00:4294967296:00", false},
        {"01 Jan 1970 00:00:4294967296", false},
        {longDigits + " Jan 1970 00:00:00", false},
        {"01 Jan " + longDigits + " 00:00:00", false},
        {"01 Jan 1970 " + longDigits + ":00:00", false},
        {"01 Jan 1970 00:" + longDigits + ":00", false},
        {"01 Jan 1970 00:00:" + longDigits, false},
        {longDigits + " 01 Jan 1970 00:00:00", true},
    };
    for (std::size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const auto& entry = cases[index];
        const std::string url = "http://date" + std::to_string(index) + ".test/";
        Require(Add(box, url.c_str(), "value=1; Expires=" + entry.date) == 0);
        const bool matched = !NoMatch(box, url.c_str());
        if (matched == entry.expired) std::fprintf(stderr, "Unexpected cookie expiry for %s\n", entry.date.c_str());
        Require(matched != entry.expired);
        if (matched) Require(Get(box, url.c_str()) == "value=1");
    }
}

static void CheckStoreValidation() {
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0);

    Require(sceHttp2AddCookie(box, "http://example.com/", "   ", 3) == InvalidValue);
    Require(sceHttp2AddCookie(box, "http://example.com/", "=", 1) == InvalidValue);
    Require(sceHttp2AddCookie(box, "http://example.com/", " = ; Path=/", 11) == InvalidValue);
    Require(sceHttp2AddCookie(box, "http://example.com/", "; Path=/; Domain=example.com", 28) == InvalidValue);
    const char leadingNul[] = {'\0', 'a', '=', '1'};
    Require(sceHttp2AddCookie(box, "http://example.com/", leadingNul, sizeof(leadingNul)) == InvalidValue);
    const char spaceBeforeNul[] = {' ', ' ', '\0', 'a', '=', '1'};
    Require(sceHttp2AddCookie(box, "http://example.com/", spaceBeforeNul, sizeof(spaceBeforeNul)) == InvalidValue);

    Require(Add(box, "http://example.com/", "bad=1; Domain=.") == Rejected);
    Require(Add(box, "http://example.com/", "bad=1; Domain=..") == Rejected);
    Require(Add(box, "http://sub.example.com/", "bad=1; Domain=..example.com") == Rejected);
    Require(Add(box, "http://example.com/", "bad=1; Domain=example.com.") == Rejected);
    Require(Add(box, "http://example.com/", "bad=1; Domain=.example.com.") == Rejected);
    Require(Add(box, "http://a..example.com/", "bad=1; Domain=a..example.com") == Rejected);
    Require(Add(box, "http://example.com/", "bad=1; Domain=.example.com; Domain=.") == Rejected);
    Require(NoMatch(box, "http://example.com/"));
    Require(NoMatch(box, "http://sub.example.com/"));

    const char trailingAfterNul[] = "sid=abc\0; Domain=other.com; Max-Age=0";
    Require(sceHttp2AddCookie(box, "http://example.com/", trailingAfterNul, sizeof(trailingAfterNul) - 1) == 0);
    Require(Get(box, "http://example.com/") == "sid=abc");
    Require(sceHttp2AddCookie(box, "http://example.com/", "plain=ok", sizeof("plain=ok")) == 0);
    Require(Get(box, "http://example.com/") == "plain=ok; sid=abc");

    Require(Add(box, "http://sub.other.com/", "hostonly=1; Domain=") == 0);
    Require(Get(box, "http://sub.other.com/") == "hostonly=1");
    Require(NoMatch(box, "http://other.com/"));

    Require(sceHttp2DeleteCookieBox(box) == 0);
}

int main() {
    static_assert(sizeof(Http2CookieStats) == 32);
    static_assert(offsetof(Http2CookieStats, currentInuseNum) == 8 && offsetof(Http2CookieStats, maxInuseSize) == 16);
    static_assert(offsetof(Http2CookieStats, maxInuseNum) == 24 && offsetof(Http2CookieStats, removedNum) == 28);
    const int dates = sceHttp2CreateCookieBox(0x10000);
    Require(dates > 0);
    CheckDateTokens(dates);
    Require(sceHttp2DeleteCookieBox(dates) == 0);
    CheckStoreValidation();
    constexpr std::size_t maxSize = static_cast<std::size_t>(-1);
    Require(sceHttp2CreateCookieBox(maxSize) < 0);
    Require(sceHttp2CreateCookieBox(maxSize - 0x3fff) < 0);
    Require(sceHttp2CreateCookieBox(maxSize / 2) < 0);
    Require(sceHttp2CreateCookieBox(0xffff) == PoolTooSmall);
    const int box = sceHttp2CreateCookieBox(0x10000);
    Require(box > 0);

    Require(NoMatch(box, "http://example.com/"));
    Require(Add(box, "http://example.com/a/b", "sid=1") == 0);
    Require(Add(box, "http://Example.COM:8080/", "lang=fr; Path=/; Domain=.example.com") == 0);
    Require(Add(box, "https://example.com/", "token=x; Secure; HttpOnly; Path=/") == 0);
    Require(Add(box, "http://example.com/", "wide=1; Path=/a; Max-Age=3600") == 0);
    CheckUnmappedOutputs(box);
    Require(Get(box, "http://example.com/a/c") == "sid=1; wide=1; lang=fr");
    Require(Get(box, "https://example.com/a/b?q#f") == "sid=1; wide=1; token=x; lang=fr");
    Require(Get(box, "HTTPS://example.com/") == "token=x; lang=fr");
    Require(Get(box, "http://example.com/") == "lang=fr");
    Require(Get(box, "http://example.com/a") == "wide=1; lang=fr");
    Require(Get(box, "http://example.com/ab") == "wide=1; lang=fr");
    Require(Get(box, "http://www.example.com/a/") == "sid=1; wide=1; lang=fr");
    Require(Get(box, "ftp://example.com/a/b") == "sid=1; wide=1; lang=fr");
    Require(Get(box, "example.com/a/b") == "sid=1; wide=1; lang=fr");
    Require(Get(box, "http://user:pw@example.com/a/x") == "sid=1; wide=1; lang=fr");
    Require(NoMatch(box, "http://badexample.com/a"));
    Require(NoMatch(box, "http:///a"));
    Require(NoMatch(box, ""));

    Require(Add(box, "http://example.com/a/b", "sid=2") == 0);
    Require(Get(box, "http://example.com/a/") == "sid=2; wide=1; lang=fr");
    Require(Add(box, "http://example.com/", "wide=0; Path=/a; Expires=Thu, 01 Jan 1970 00:00:00 GMT") == 0);
    Require(Add(box, "http://example.com/", "lang=; Domain=example.com; Max-Age=0") == 0);
    Require(Get(box, "http://example.com/a/") == "sid=2");
    Require(Add(box, "http://example.com/", "future=1; Expires=Wed, 09 Jun 2121 10:18:14 GMT") == 0);
    Require(Add(box, "http://example.com/", "invalid=1; Expires=Feb 30 2121 10:18:14") == 0);
    Require(Get(box, "http://example.com/") == "invalid=1; future=1");

    Require(Add(box, "http://example.com/", "noequals") == 0);
    Require(Add(box, "http://example.com/", "=value") == 0);
    Require(Add(box, "http://example.com/", "evil=1; Domain=other.com") == Rejected);
    Require(Add(box, "http://example.com/", "evil=1; Domain=ample.com") == Rejected);
    Require(NoMatch(box, "http://other.com/"));
    Require(Get(box, "http://example.com/") == "invalid=1; noequals; value; future=1");
    Require(sceHttp2AddCookie(box, "http://example.com/", "cut=12345", 5) == 0);
    Require(Get(box, "http://example.com/") == "invalid=1; noequals; value; cut=1; future=1");

    Http2CookieStats stats = Stats(box);
    Require(stats.currentInuseNum == 7 && stats.currentInuseSize == 23 + 23 + 24 + 25 + 25 + 22 + 21);
    Require(stats.maxInuseNum == 7 && stats.maxInuseSize == stats.currentInuseSize && stats.removedNum == 0);

    const std::string header = "invalid=1; noequals; value; cut=1; future=1";
    std::size_t required = 0;
    char small[8] = {'x', 'x'};
    Require(sceHttp2GetCookie(box, "http://example.com/", small, &required, sizeof(small)) == OutOfSize && required == 0 && small[0] == '\0' && small[1] == 'x');
    std::string large(64, 'x');
    Require(sceHttp2GetCookie(box, "http://example.com/", large.data(), nullptr, large.size()) == 0 && large.compare(0, header.size() + 1, header.c_str(), header.size() + 1) == 0);
    Require(sceHttp2GetCookie(box, "http://example.com/", nullptr, nullptr, 0) == InvalidValue);
    Require(sceHttp2GetCookie(box, nullptr, nullptr, &required, 0) == InvalidValue);
    Require(sceHttp2AddCookie(box, nullptr, "a=1", 3) == InvalidValue);
    Require(sceHttp2AddCookie(box, "http://example.com/", nullptr, 3) == InvalidValue);
    Require(sceHttp2AddCookie(box, "http://example.com/", "a=1", 0) == InvalidValue);
    Require(sceHttp2AddCookie(box, "http:///path", "a=1", 3) == Rejected);
    Require(sceHttp2AddCookie(box, "", "a=1", 3) == Rejected);
    Require(sceHttp2GetCookieStats(box, nullptr) == InvalidValue);
    Require(sceHttp2GetCookieStats(0, nullptr) == InvalidValue);

    Require(sceHttp2CookieFlush(box) == 0);
    Require(NoMatch(box, "http://example.com/"));
    stats = Stats(box);
    Require(stats.currentInuseNum == 0 && stats.currentInuseSize == 0 && stats.maxInuseNum == 7);

    Require(sceHttp2SetCookieMaxSize(box, 4) == 0);
    Require(Add(box, "http://example.com/", "abc=defg") == OutOfSize);
    Require(Add(box, "http://example.com/", "ab=cd") == 0);
    Require(Get(box, "http://example.com/") == "ab=cd");
    Require(Stats(box).currentInuseSize == 21);
    Require(sceHttp2SetCookieMaxSize(box, 0) == 0);
    Require(Add(box, "http://example.com/", "x=1") == OutOfSize);
    Require(sceHttp2SetCookieMaxSize(box, 4096) == 0);

    Require(sceHttp2SetCookieMaxNumPerDomain(box, 2) == 0);
    Require(Add(box, "http://example.com/", "c=1") == 0);
    Require(Get(box, "http://example.com/") == "c=1; ab=cd");
    Require(Add(box, "http://example.com/", "d=1") == 0);
    Require(Get(box, "http://example.com/") == "d=1; c=1");
    Require(Add(box, "http://example.com/", "e=1") == 0);
    Require(Get(box, "http://example.com/") == "e=1; d=1");
    Require(Add(box, "http://other.com/", "o=1") == 0);
    Require(sceHttp2SetCookieMaxNum(box, 2) == 0);
    Require(Add(box, "http://third.com/", "t=1") == 0);
    Require(NoMatch(box, "http://example.com/"));
    Require(NoMatch(box, "http://other.com/"));
    Require(Get(box, "http://third.com/") == "t=1");
    stats = Stats(box);
    Require(stats.currentInuseNum == 1 && stats.removedNum == 5);

    const int defaults = sceHttp2CreateCookieBox(0x10000);
    Require(defaults > 0);
    Require(Add(defaults, "http://big.test/", "n=" + std::string(4096, 'v')) == OutOfSize);
    Require(Add(defaults, "http://big.test/", "n=" + std::string(4095, 'v')) == 0);
    Require(FillDomain(defaults, 50) == 0);
    Require(Get(defaults, "http://example.com/").compare(0, 6, "c1=1; ") == 0);
    for (int i = 50; i < 60; ++i) Require(Add(defaults, "http://example.com/", "c" + std::to_string(i) + "=1") == 0);
    const std::string kept = Get(defaults, "http://example.com/");
    Require(kept.compare(0, 7, "c11=1; ") == 0 && kept.compare(kept.size() - 5, 5, "c10=1") == 0 && kept.find("c0=") == std::string::npos);
    stats = Stats(defaults);
    Require(stats.currentInuseNum == 51 && stats.removedNum == 10);

    const int noTotal = sceHttp2CreateCookieBox(0x10000);
    Require(noTotal > 0 && sceHttp2SetCookieMaxNum(noTotal, 0) == 0 && FillDomain(noTotal, 60) == 0);
    stats = Stats(noTotal);
    Require(stats.currentInuseNum == 1 && stats.removedNum == 59);
    const int noPerDomain = sceHttp2CreateCookieBox(0x10000);
    Require(noPerDomain > 0 && sceHttp2SetCookieMaxNumPerDomain(noPerDomain, 0) == 0 && FillDomain(noPerDomain, 60) == 0);
    stats = Stats(noPerDomain);
    Require(stats.currentInuseNum == 1 && stats.removedNum == 59);
    Require(Get(noPerDomain, "http://example.com/") == "c59=1");

    Require(sceHttp2AddCookie(0, "http://example.com/", "a=1", 3) == InvalidId);
    Require(sceHttp2SetCookieMaxNum(0x7fffffff, 1) == InvalidId);
    Require(sceHttp2SetCookieMaxNumPerDomain(-1, 1) == InvalidId);
    Require(sceHttp2SetCookieMaxSize(0, 1) == InvalidId);
    Require(sceHttp2GetCookie(0, "http://example.com/", nullptr, &required, 0) == InvalidId);
    Http2CookieStats ignored{};
    Require(sceHttp2GetCookieStats(0, &ignored) == InvalidId);

    const int other = sceHttp2CreateCookieBox(0x10000);
    Require(other > 0 && NoMatch(other, "http://example.com/"));
    Require(sceHttp2DeleteCookieBox(box) == 0);
    Require(sceHttp2GetCookieStats(box, &ignored) == InvalidId);
    Require(sceHttp2AddCookie(box, "http://example.com/", "a=1", 3) == InvalidId);
    Require(sceHttp2DeleteCookieBox(other) == 0);
    Require(sceHttp2DeleteCookieBox(defaults) == 0);
    Require(sceHttp2DeleteCookieBox(noTotal) == 0);
    Require(sceHttp2DeleteCookieBox(noPerDomain) == 0);
}
