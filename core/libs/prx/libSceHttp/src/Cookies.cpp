#include "prx/libSceHttp/src/Cookies.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceHttp/src/HttpErrors.hpp"
#include "prx/libSceHttp/src/Options.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

extern "C" int APS5_VABI sceHttpUriParse(SceHttpUriElement* out, const char* src_url, void* pool, size_t* require, size_t prepare);

struct HttpCookieStats {
    std::uint64_t currentInuseSize;
    std::uint32_t currentInuseNum;
    std::uint64_t maxInuseSize;
    std::uint32_t maxInuseNum;
    std::uint32_t removedNum;
    std::int32_t reserved;
};

static_assert(sizeof(HttpCookieStats) == 40);
static_assert(offsetof(HttpCookieStats, maxInuseSize) == 16);
static_assert(offsetof(HttpCookieStats, removedNum) == 28);

namespace {

using Clock = std::chrono::system_clock;

struct Cookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    bool hostOnly = true;
    bool secure = false;
    std::optional<Clock::time_point> expiry;
    std::uint64_t creation = 0;
    std::uint64_t lastAccess = 0;
};

struct CookieJar {
    std::vector<Cookie> cookies;
    std::uint32_t maxNum = 0;
    std::uint32_t maxNumPerDomain = 0;
    std::uint32_t maxSize = 0;
    std::uint32_t totalMaxSize = 0;
    std::uint64_t maxInuseSize = 0;
    std::uint32_t maxInuseNum = 0;
    std::uint32_t removedNum = 0;
};

struct RequestUrl {
    std::string host;
    std::string path;
};

std::mutex g_cookieMutex;
std::unordered_map<int, CookieJar> g_jars;
std::uint64_t g_cookieClock = 0;

char lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string lowered(std::string_view text) {
    std::string result(text);
    for (char& c : result) c = lower(c);
    return result;
}

bool equalsIgnoreCase(std::string_view left, std::string_view right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) { return lower(a) == lower(b); });
}

std::string_view trimmed(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

int parseUrl(const char* url, RequestUrl& parsed) {
    size_t required = 0;
    if (const int result = sceHttpUriParse(nullptr, url, nullptr, &required, 0); result != 0) return result;
    std::vector<char> pool(required);
    SceHttpUriElement element;
    if (const int result = sceHttpUriParse(&element, url, pool.data(), &required, pool.size()); result != 0) return result;
    if (!equalsIgnoreCase(element.scheme, "http") && !equalsIgnoreCase(element.scheme, "https")) return ERROR_UNKNOWN_SCHEME;
    if (element.opaque || element.hostname[0] == '\0') return ERROR_INVALID_URL;
    parsed.host = lowered(element.hostname);
    parsed.path = element.path;
    return 0;
}

bool isIpAddress(std::string_view host) {
    return host.find(':') != std::string_view::npos || host.find_first_not_of("0123456789.") == std::string_view::npos;
}

bool domainMatches(std::string_view host, std::string_view domain) {
    if (host == domain) return true;
    return host.size() > domain.size() && host.ends_with(domain) && host[host.size() - domain.size() - 1] == '.' && !isIpAddress(host);
}

std::string defaultPath(std::string_view path) {
    if (path.empty() || path.front() != '/') return "/";
    const size_t slash = path.rfind('/');
    return slash == 0 ? "/" : std::string(path.substr(0, slash));
}

bool pathMatches(std::string_view requestPath, std::string_view cookiePath) {
    if (requestPath.empty() || requestPath.front() != '/') requestPath = "/";
    if (requestPath == cookiePath) return true;
    if (!requestPath.starts_with(cookiePath)) return false;
    return cookiePath.back() == '/' || requestPath[cookiePath.size()] == '/';
}

bool isDateDelimiter(char c) {
    const auto u = static_cast<unsigned char>(c);
    return u == 0x09 || (u >= 0x20 && u <= 0x2F) || (u >= 0x3B && u <= 0x40) || (u >= 0x5B && u <= 0x60) || (u >= 0x7B && u <= 0x7E);
}

bool leadingDigits(std::string_view token, size_t minimum, size_t maximum, int& value, size_t& used) {
    used = 0;
    value = 0;
    while (used < token.size() && used < maximum && token[used] >= '0' && token[used] <= '9') value = value * 10 + (token[used++] - '0');
    return used >= minimum && (used == token.size() || token[used] < '0' || token[used] > '9');
}

bool parseTime(std::string_view token, int& hour, int& minute, int& second) {
    int* fields[] = {&hour, &minute, &second};
    for (size_t index = 0; index < 3; ++index) {
        size_t used = 0;
        if (!leadingDigits(token, 1, 2, *fields[index], used)) return false;
        token.remove_prefix(used);
        if (index < 2) {
            if (!token.starts_with(':')) return false;
            token.remove_prefix(1);
        }
    }
    return true;
}

std::optional<Clock::time_point> parseCookieDate(std::string_view text) {
    constexpr std::string_view months[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
    bool foundTime = false;
    bool foundDay = false;
    bool foundMonth = false;
    bool foundYear = false;
    int hour = 0;
    int minute = 0;
    int second = 0;
    int day = 0;
    int month = 0;
    int year = 0;
    while (!text.empty()) {
        while (!text.empty() && isDateDelimiter(text.front())) text.remove_prefix(1);
        size_t length = 0;
        while (length < text.size() && !isDateDelimiter(text[length])) ++length;
        const std::string_view token = text.substr(0, length);
        text.remove_prefix(length);
        if (token.empty()) continue;
        size_t used = 0;
        int value = 0;
        if (!foundTime && parseTime(token, hour, minute, second)) {
            foundTime = true;
        } else if (!foundDay && leadingDigits(token, 1, 2, value, used)) {
            foundDay = true;
            day = value;
        } else if (!foundMonth && token.size() >= 3 && std::find(std::begin(months), std::end(months), lowered(token.substr(0, 3))) != std::end(months)) {
            foundMonth = true;
            month = static_cast<int>(std::find(std::begin(months), std::end(months), lowered(token.substr(0, 3))) - std::begin(months)) + 1;
        } else if (!foundYear && leadingDigits(token, 2, 4, value, used)) {
            foundYear = true;
            year = value;
        }
    }
    if (year >= 70 && year <= 99) year += 1900;
    else if (year >= 0 && year <= 69) year += 2000;
    if (!foundTime || !foundDay || !foundMonth || !foundYear) return std::nullopt;
    if (day < 1 || day > 31 || year < 1601 || hour > 23 || minute > 59 || second > 59) return std::nullopt;
    const std::chrono::year_month_day date{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)}, std::chrono::day{static_cast<unsigned>(day)}};
    if (!date.ok()) return std::nullopt;
    return std::chrono::sys_days{date} + std::chrono::hours{hour} + std::chrono::minutes{minute} + std::chrono::seconds{second};
}

std::optional<Clock::time_point> parseMaxAge(std::string_view text, Clock::time_point now) {
    if (text.empty()) return std::nullopt;
    const bool negative = text.front() == '-';
    if (negative) text.remove_prefix(1);
    if (text.empty() || text.find_first_not_of("0123456789") != std::string_view::npos) return std::nullopt;
    if (negative) return Clock::time_point::min();
    std::int64_t seconds = 0;
    for (const char c : text) {
        seconds = seconds * 10 + (c - '0');
        if (seconds > 0x7FFFFFFFFFLL) return Clock::time_point::max();
    }
    if (seconds == 0) return Clock::time_point::min();
    return now + std::chrono::seconds{seconds};
}

bool expired(const Cookie& cookie, Clock::time_point now) {
    return cookie.expiry && *cookie.expiry <= now;
}

std::uint64_t cookieSize(const Cookie& cookie) {
    return cookie.name.size() + cookie.value.size();
}

std::uint64_t jarSize(const CookieJar& jar) {
    std::uint64_t size = 0;
    for (const Cookie& cookie : jar.cookies) size += cookieSize(cookie);
    return size;
}

void evictOldest(CookieJar& jar, std::string_view domain) {
    auto oldest = jar.cookies.end();
    for (auto it = jar.cookies.begin(); it != jar.cookies.end(); ++it) {
        if (!domain.empty() && it->domain != domain) continue;
        if (oldest == jar.cookies.end() || it->lastAccess < oldest->lastAccess) oldest = it;
    }
    if (oldest == jar.cookies.end()) return;
    jar.cookies.erase(oldest);
    ++jar.removedNum;
}

std::uint32_t domainCount(const CookieJar& jar, std::string_view domain) {
    return static_cast<std::uint32_t>(std::count_if(jar.cookies.begin(), jar.cookies.end(), [&](const Cookie& cookie) { return cookie.domain == domain; }));
}

void enforceLimits(CookieJar& jar, Clock::time_point now) {
    std::erase_if(jar.cookies, [&](const Cookie& cookie) { return expired(cookie, now); });
    if (jar.maxNumPerDomain != 0) {
        for (size_t index = 0; index < jar.cookies.size(); ++index) {
            const std::string domain = jar.cookies[index].domain;
            while (domainCount(jar, domain) > jar.maxNumPerDomain) evictOldest(jar, domain);
        }
    }
    while ((jar.maxNum != 0 && jar.cookies.size() > jar.maxNum) || (jar.totalMaxSize != 0 && jarSize(jar) > jar.totalMaxSize)) evictOldest(jar, {});
    jar.maxInuseSize = std::max(jar.maxInuseSize, jarSize(jar));
    jar.maxInuseNum = std::max(jar.maxInuseNum, static_cast<std::uint32_t>(jar.cookies.size()));
}

void storeCookie(CookieJar& jar, const RequestUrl& url, std::string_view header, Clock::time_point now) {
    const size_t pairEnd = header.find(';');
    const std::string_view pair = header.substr(0, pairEnd);
    const size_t equals = pair.find('=');
    if (equals == std::string_view::npos) return;
    Cookie cookie;
    cookie.name = trimmed(pair.substr(0, equals));
    cookie.value = trimmed(pair.substr(equals + 1));
    if (cookie.name.empty()) return;
    std::optional<Clock::time_point> expires;
    std::optional<Clock::time_point> maxAge;
    std::string domain;
    std::optional<std::string> path;
    std::string_view attributes = pairEnd == std::string_view::npos ? std::string_view{} : header.substr(pairEnd + 1);
    while (!attributes.empty()) {
        const size_t end = attributes.find(';');
        const std::string_view attribute = attributes.substr(0, end);
        attributes = end == std::string_view::npos ? std::string_view{} : attributes.substr(end + 1);
        const size_t split = attribute.find('=');
        const std::string_view name = trimmed(attribute.substr(0, split));
        const std::string_view value = split == std::string_view::npos ? std::string_view{} : trimmed(attribute.substr(split + 1));
        if (equalsIgnoreCase(name, "expires")) {
            if (const auto date = parseCookieDate(value)) expires = date;
        } else if (equalsIgnoreCase(name, "max-age")) {
            if (const auto age = parseMaxAge(value, now)) maxAge = age;
        } else if (equalsIgnoreCase(name, "domain")) {
            if (!value.empty()) domain = lowered(value.starts_with('.') ? value.substr(1) : value);
        } else if (equalsIgnoreCase(name, "path")) {
            path = value.empty() || value.front() != '/' ? defaultPath(url.path) : std::string(value);
        } else if (equalsIgnoreCase(name, "secure")) {
            cookie.secure = true;
        }
    }
    cookie.expiry = maxAge ? maxAge : expires;
    if (domain.empty()) {
        cookie.domain = url.host;
    } else {
        if (!domainMatches(url.host, domain)) return;
        cookie.domain = domain;
        cookie.hostOnly = false;
    }
    cookie.path = path ? *path : defaultPath(url.path);
    if (jar.maxSize != 0 && cookieSize(cookie) > jar.maxSize) return;
    cookie.creation = ++g_cookieClock;
    cookie.lastAccess = cookie.creation;
    const auto existing = std::find_if(jar.cookies.begin(), jar.cookies.end(), [&](const Cookie& stored) {
        return stored.name == cookie.name && stored.domain == cookie.domain && stored.path == cookie.path;
    });
    if (existing != jar.cookies.end()) {
        cookie.creation = existing->creation;
        jar.cookies.erase(existing);
    }
    if (expired(cookie, now)) return;
    jar.cookies.push_back(std::move(cookie));
    enforceLimits(jar, now);
}

CookieJar* findJar(int contextId, int& error) {
    if (!HttpInitialised()) {
        error = ERROR_BEFORE_INIT;
        return nullptr;
    }
    if (!IsHttpContext(contextId)) {
        error = ERROR_INVALID_ID;
        return nullptr;
    }
    error = 0;
    return &g_jars[contextId];
}

int setLimit(int contextId, std::uint32_t CookieJar::*limit, std::uint32_t value) {
    std::lock_guard lock(g_cookieMutex);
    int error = 0;
    CookieJar* jar = findJar(contextId, error);
    if (!jar) return error;
    jar->*limit = value;
    enforceLimits(*jar, Clock::now());
    return 0;
}

}

void ClearHttpCookieJar(int contextId) {
    std::lock_guard lock(g_cookieMutex);
    if (const auto jar = g_jars.find(contextId); jar != g_jars.end()) jar->second.cookies.clear();
}

void RemoveHttpCookieJar(int contextId) {
    std::lock_guard lock(g_cookieMutex);
    g_jars.erase(contextId);
}

extern "C" {

int APS5_VABI sceHttpAddCookie(int contextId, const char* url, const char* cookie, std::size_t size) {
    std::lock_guard lock(g_cookieMutex);
    int error = 0;
    CookieJar* jar = findJar(contextId, error);
    if (!jar) return error;
    if (!url || !cookie) return ERROR_INVALID_VALUE;
    RequestUrl parsed;
    if (const int result = parseUrl(url, parsed); result != 0) return result;
    const std::string_view header(cookie, size);
    storeCookie(*jar, parsed, header.substr(0, header.find('\0')), Clock::now());
    return 0;
}

int APS5_VABI sceHttpGetCookie(int contextId, const char* url, char* cookie, std::size_t* required, std::size_t prepared, int isSecure) {
    std::lock_guard lock(g_cookieMutex);
    int error = 0;
    CookieJar* jar = findJar(contextId, error);
    if (!jar) return error;
    if (!url || (!cookie && !required)) return ERROR_INVALID_VALUE;
    RequestUrl parsed;
    if (const int result = parseUrl(url, parsed); result != 0) return result;
    const auto now = Clock::now();
    std::erase_if(jar->cookies, [&](const Cookie& stored) { return expired(stored, now); });
    std::vector<Cookie*> matches;
    for (Cookie& stored : jar->cookies) {
        const bool hostMatches = stored.hostOnly ? parsed.host == stored.domain : domainMatches(parsed.host, stored.domain);
        if (hostMatches && pathMatches(parsed.path, stored.path) && (!stored.secure || isSecure != 0)) matches.push_back(&stored);
    }
    std::stable_sort(matches.begin(), matches.end(), [](const Cookie* left, const Cookie* right) {
        if (left->path.size() != right->path.size()) return left->path.size() > right->path.size();
        return left->creation < right->creation;
    });
    std::string header;
    for (Cookie* stored : matches) {
        if (!header.empty()) header += "; ";
        header += stored->name;
        header += '=';
        header += stored->value;
    }
    if (required) *required = header.size() + 1;
    if (!cookie) return 0;
    if (prepared < header.size() + 1) return ERROR_OUT_OF_SIZE;
    std::memcpy(cookie, header.c_str(), header.size() + 1);
    for (Cookie* stored : matches) stored->lastAccess = ++g_cookieClock;
    return 0;
}

int APS5_VABI sceHttpSetCookieTotalMaxSize(int contextId, std::uint32_t size) {
    return setLimit(contextId, &CookieJar::totalMaxSize, size);
}

int APS5_VABI sceHttpSetCookieMaxSize(int contextId, std::uint32_t size) {
    return setLimit(contextId, &CookieJar::maxSize, size);
}

int APS5_VABI sceHttpSetCookieMaxNum(int contextId, std::uint32_t count) {
    return setLimit(contextId, &CookieJar::maxNum, count);
}

int APS5_VABI sceHttpSetCookieMaxNumPerDomain(int contextId, std::uint32_t count) {
    return setLimit(contextId, &CookieJar::maxNumPerDomain, count);
}

int APS5_VABI sceHttpGetCookieStats(int id, HttpCookieStats* stats) {
    std::lock_guard lock(g_cookieMutex);
    int error = 0;
    CookieJar* jar = findJar(id, error);
    if (!jar) return error;
    if (!stats) return ERROR_INVALID_VALUE;
    std::erase_if(jar->cookies, [now = Clock::now()](const Cookie& stored) { return expired(stored, now); });
    *stats = {};
    stats->currentInuseSize = jarSize(*jar);
    stats->currentInuseNum = static_cast<std::uint32_t>(jar->cookies.size());
    stats->maxInuseSize = jar->maxInuseSize;
    stats->maxInuseNum = jar->maxInuseNum;
    stats->removedNum = jar->removedNum;
    return 0;
}

}
