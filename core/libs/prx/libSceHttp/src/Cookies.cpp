#include "prx/libSceHttp/include/Http.hpp"
#include "prx/libSceHttp/src/Cookies.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceHttp/src/HttpErrors.hpp"
#include "prx/libSceHttp/src/Options.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

static_assert(sizeof(HttpCookieStats) == 32);
static_assert(offsetof(HttpCookieStats, cookieCount) == 8);
static_assert(offsetof(HttpCookieStats, peakUsedBytes) == 16);
static_assert(offsetof(HttpCookieStats, peakCookieCount) == 24);
static_assert(offsetof(HttpCookieStats, evictedCookieCount) == 28);

namespace {

using Clock = std::chrono::system_clock;

constexpr std::uint32_t DefaultMaxSize = 4096;
constexpr std::uint32_t DefaultMaxNumPerDomain = 50;
constexpr std::uint32_t DefaultMaxNum = 3000;
constexpr std::uint32_t DefaultTotalMaxSize = 81920;
constexpr std::uint64_t StoredSizeSlack = 516;
constexpr size_t EvictedAtOnce = 10;

struct Cookie {
    std::string name;
    std::string value;
    std::string domainSpelling;
    std::string domain;
    std::string path;
    bool secure = false;
    std::optional<Clock::time_point> expiry;
    std::uint64_t creation = 0;
};

struct CookieJar {
    std::vector<Cookie> cookies;
    std::uint32_t maxNum = DefaultMaxNum;
    std::uint32_t maxNumPerDomain = DefaultMaxNumPerDomain;
    std::uint32_t maxSize = DefaultMaxSize;
    std::uint32_t totalMaxSize = DefaultTotalMaxSize;
    std::uint64_t peakSize = 0;
    std::uint32_t peakNum = 0;
    std::uint32_t removedNum = 0;
};

struct RequestUrl {
    std::string host;
    std::string path;
    bool secure = false;
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
    if (element.hostname[0] == '\0') return ERROR_PARSE_HTTP_INVALID_VALUE;
    parsed.host = lowered(element.hostname);
    parsed.path = element.path;
    parsed.secure = equalsIgnoreCase(element.scheme, "https");
    return 0;
}

bool domainMatches(std::string_view host, std::string_view domain) {
    if (host == domain) return true;
    return host.size() > domain.size() && host.ends_with(domain) && host[host.size() - domain.size() - 1] == '.';
}

bool allowedDomain(std::string_view domain) {
    constexpr std::array<std::string_view, 7> generic = {"com", "org", "net", "edu", "gov", "mil", "int"};
    const size_t dots = static_cast<size_t>(std::count(domain.begin(), domain.end(), '.'));
    if (dots == 0 || domain.back() == '.') return false;
    const std::string_view topLevel = domain.substr(domain.rfind('.') + 1);
    return dots >= (std::find(generic.begin(), generic.end(), topLevel) != generic.end() ? 1u : 2u);
}

std::string defaultPath(std::string_view path) {
    if (path.empty() || path.front() != '/') return "/";
    return std::string(path.substr(0, path.rfind('/') + 1));
}

bool pathMatches(std::string_view requestPath, std::string_view cookiePath) {
    if (requestPath.empty() || requestPath.front() != '/') requestPath = "/";
    return requestPath.starts_with(cookiePath);
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

Clock::time_point clampedTime(std::chrono::sys_seconds time) {
    if (time >= std::chrono::floor<std::chrono::seconds>(Clock::time_point::max())) return Clock::time_point::max();
    if (time <= std::chrono::ceil<std::chrono::seconds>(Clock::time_point::min())) return Clock::time_point::min();
    return std::chrono::time_point_cast<Clock::duration>(time);
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
    return clampedTime(std::chrono::sys_days{date} + std::chrono::hours{hour} + std::chrono::minutes{minute} + std::chrono::seconds{second});
}

std::optional<Clock::time_point> parseMaxAge(std::string_view text, Clock::time_point now) {
    if (text.empty()) return std::nullopt;
    const bool negative = text.front() == '-';
    if (negative) text.remove_prefix(1);
    if (text.empty() || text.find_first_not_of("0123456789") != std::string_view::npos) return std::nullopt;
    if (negative) return Clock::time_point::min();
    const std::int64_t remaining = std::chrono::duration_cast<std::chrono::seconds>(Clock::time_point::max() - now).count() - 1;
    std::int64_t seconds = 0;
    for (const char c : text) {
        seconds = seconds * 10 + (c - '0');
        if (seconds >= remaining) return Clock::time_point::max();
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

std::uint64_t storedSize(const Cookie& cookie) {
    return cookie.name.size() + cookie.value.size() + cookie.domain.size() + cookie.path.size() + 5;
}

std::uint64_t jarSize(const CookieJar& jar) {
    std::uint64_t size = 0;
    for (const Cookie& cookie : jar.cookies) size += storedSize(cookie);
    return size;
}

void evictOldest(CookieJar& jar, const std::string* domain, std::uint64_t newest) {
    auto oldest = jar.cookies.end();
    for (auto it = jar.cookies.begin(); it != jar.cookies.end(); ++it) {
        if ((domain && it->domain != *domain) || it->creation == newest) continue;
        if (oldest == jar.cookies.end() || it->creation < oldest->creation) oldest = it;
    }
    if (oldest == jar.cookies.end()) return;
    jar.cookies.erase(oldest);
    ++jar.removedNum;
}

void evictBatch(CookieJar& jar, std::uint64_t newest) {
    for (size_t index = 0; index < EvictedAtOnce && jar.cookies.size() > 1; ++index) evictOldest(jar, nullptr, newest);
}

void enforceLimits(CookieJar& jar, const std::string& domain, std::uint64_t newest) {
    while (std::count_if(jar.cookies.begin(), jar.cookies.end(), [&](const Cookie& cookie) { return cookie.domain == domain; }) > jar.maxNumPerDomain) evictOldest(jar, &domain, 0);
    if (jar.cookies.size() > jar.maxNum) evictBatch(jar, newest);
    while (jar.cookies.size() > 1 && jarSize(jar) > jar.totalMaxSize) evictBatch(jar, newest);
    jar.peakSize = std::max(jar.peakSize, jarSize(jar));
    jar.peakNum = std::max(jar.peakNum, static_cast<std::uint32_t>(jar.cookies.size()));
}

int storeCookie(CookieJar& jar, const RequestUrl& url, std::string_view header, Clock::time_point now) {
    const size_t pairEnd = header.find(';');
    const std::string_view pair = header.substr(0, pairEnd);
    const size_t equals = pair.find('=');
    Cookie cookie;
    if (equals == std::string_view::npos) {
        cookie.name = trimmed(pair);
    } else {
        cookie.name = trimmed(pair.substr(0, equals));
        cookie.value = trimmed(pair.substr(equals + 1));
    }
    if (cookie.name.empty() && cookie.value.empty()) return ERROR_INVALID_VALUE;
    std::optional<Clock::time_point> expires;
    std::optional<Clock::time_point> maxAge;
    std::optional<std::string> domainSpelling;
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
            if (!value.empty()) domainSpelling = std::string(value);
        } else if (equalsIgnoreCase(name, "path")) {
            path = value.empty() || value.front() != '/' ? defaultPath(url.path) : std::string(value);
        } else if (equalsIgnoreCase(name, "secure")) {
            cookie.secure = true;
        }
    }
    cookie.expiry = maxAge ? maxAge : expires;
    if (domainSpelling) {
        const std::string_view spelling = *domainSpelling;
        cookie.domain = lowered(spelling.starts_with('.') ? spelling.substr(1) : spelling);
        if (!allowedDomain(cookie.domain)) return ERROR_INVALID_VALUE;
        if (!domainMatches(url.host, cookie.domain)) return ERROR_PARSE_HTTP_INVALID_VALUE;
        cookie.domainSpelling = *domainSpelling;
    } else {
        cookie.domain = url.host;
        cookie.domainSpelling = url.host;
    }
    cookie.path = path ? *path : defaultPath(url.path);
    if (cookieSize(cookie) > jar.maxSize || storedSize(cookie) > jar.maxSize + StoredSizeSlack) return ERROR_OUT_OF_SIZE;
    std::erase_if(jar.cookies, [&](const Cookie& stored) {
        return stored.name == cookie.name && stored.domainSpelling == cookie.domainSpelling && stored.path == cookie.path;
    });
    if (expired(cookie, now)) return 0;
    cookie.creation = ++g_cookieClock;
    const std::uint64_t newest = cookie.creation;
    const std::string domain = cookie.domain;
    const auto last = std::find_if(jar.cookies.rbegin(), jar.cookies.rend(), [&](const Cookie& stored) { return stored.domain == domain; });
    jar.cookies.insert(last == jar.cookies.rend() ? jar.cookies.end() : std::prev(last.base()), std::move(cookie));
    enforceLimits(jar, domain, newest);
    return 0;
}

std::string headerItem(const Cookie& cookie) {
    if (cookie.name.empty()) return cookie.value;
    if (cookie.value.empty()) return cookie.name;
    return cookie.name + '=' + cookie.value;
}

CookieJar* findJar(int contextId) {
    return IsHttpContext(contextId) ? &g_jars[contextId] : nullptr;
}

int setLimit(int contextId, std::uint32_t value, void (*apply)(CookieJar&, std::uint32_t)) {
    std::lock_guard lock(g_cookieMutex);
    CookieJar* jar = findJar(contextId);
    if (!jar) return ERROR_INVALID_ID;
    jar->cookies.clear();
    apply(*jar, value);
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
    CookieJar* jar = findJar(contextId);
    if (!jar) return ERROR_INVALID_ID;
    if (!url || !cookie) return ERROR_INVALID_VALUE;
    RequestUrl parsed;
    if (const int result = parseUrl(url, parsed); result != 0) return result;
    const std::string_view header(cookie, size);
    return storeCookie(*jar, parsed, header.substr(0, header.find('\0')), Clock::now());
}

int APS5_VABI sceHttpGetCookie(int contextId, const char* url, char* cookie, std::size_t* required, std::size_t prepared) {
    std::lock_guard lock(g_cookieMutex);
    CookieJar* jar = findJar(contextId);
    if (!jar) return ERROR_INVALID_ID;
    if (!url) return ERROR_INVALID_VALUE;
    RequestUrl parsed;
    if (const int result = parseUrl(url, parsed); result != 0) return result;
    const auto now = Clock::now();
    std::erase_if(jar->cookies, [&](const Cookie& stored) { return expired(stored, now); });
    std::vector<const Cookie*> matches;
    for (const Cookie& stored : jar->cookies) {
        if (domainMatches(parsed.host, stored.domain) && pathMatches(parsed.path, stored.path) && (!stored.secure || parsed.secure)) matches.push_back(&stored);
    }
    if (matches.empty()) return ERROR_HTTP_NOT_FOUND;
    if (!cookie && !required) return ERROR_INVALID_VALUE;
    std::stable_sort(matches.begin(), matches.end(), [](const Cookie* left, const Cookie* right) {
        if (left->path.size() != right->path.size()) return left->path.size() > right->path.size();
        return left->domain.size() > right->domain.size();
    });
    std::vector<std::string> items;
    size_t length = 0;
    for (const Cookie* stored : matches) {
        items.push_back(headerItem(*stored));
        length += (items.size() > 1 ? 2 : 0) + items.back().size();
    }
    const size_t total = length + 3;
    if (!cookie) {
        *required = total;
        return 0;
    }
    if (prepared < total) {
        size_t written = 0;
        for (const std::string& item : items) {
            const size_t next = written + (written != 0 ? 2 : 0) + item.size();
            if (next + 1 > prepared) break;
            if (written != 0) {
                std::memcpy(cookie + written, "; ", 2);
                written += 2;
            }
            std::memcpy(cookie + written, item.data(), item.size());
            written += item.size();
        }
        if (prepared != 0) cookie[written] = '\0';
        return ERROR_OUT_OF_SIZE;
    }
    size_t written = 0;
    for (const std::string& item : items) {
        if (written != 0) {
            std::memcpy(cookie + written, "; ", 2);
            written += 2;
        }
        std::memcpy(cookie + written, item.data(), item.size());
        written += item.size();
    }
    std::memcpy(cookie + written, "\0 \0", 3);
    if (required) *required = total;
    return 0;
}

int APS5_VABI sceHttpSetCookieTotalMaxSize(int contextId, std::uint32_t size) {
    return setLimit(contextId, size, [](CookieJar& jar, std::uint32_t value) { jar.totalMaxSize = value; });
}

int APS5_VABI sceHttpSetCookieMaxSize(int contextId, std::uint32_t size) {
    return setLimit(contextId, size, [](CookieJar& jar, std::uint32_t value) { jar.maxSize = value; });
}

int APS5_VABI sceHttpSetCookieMaxNum(int contextId, std::uint32_t count) {
    return setLimit(contextId, count, [](CookieJar& jar, std::uint32_t value) { jar.maxNum = value; });
}

int APS5_VABI sceHttpSetCookieMaxNumPerDomain(int contextId, std::uint32_t count) {
    return setLimit(contextId, count, [](CookieJar& jar, std::uint32_t value) { jar.maxNumPerDomain = value; });
}

int APS5_VABI sceHttpGetCookieStats(int id, HttpCookieStats* stats) {
    std::lock_guard lock(g_cookieMutex);
    CookieJar* jar = findJar(id);
    if (!jar) return ERROR_INVALID_ID;
    if (!stats) return ERROR_INVALID_VALUE;
    std::erase_if(jar->cookies, [now = Clock::now()](const Cookie& stored) { return expired(stored, now); });
    stats->usedBytes = jarSize(*jar);
    stats->cookieCount = static_cast<std::uint32_t>(jar->cookies.size());
    stats->peakUsedBytes = jar->peakSize;
    stats->peakCookieCount = jar->peakNum;
    stats->evictedCookieCount = jar->removedNum;
    return 0;
}

}
