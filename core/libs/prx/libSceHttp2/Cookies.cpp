#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "prx/libSceHttp2/include/Http2.hpp"
#include "SceTypes.hpp"
#include "Cookies.hpp"

namespace {

constexpr int ERROR_NOT_FOUND = static_cast<int>(0x817B1025);
constexpr int ERROR_INVALID_ID = static_cast<int>(0x817B1100);
constexpr int ERROR_OUT_OF_SIZE = static_cast<int>(0x817B1104);
constexpr int ERROR_INVALID_VALUE = static_cast<int>(0x817B11FE);
constexpr int ERROR_REJECTED = static_cast<int>(0x817B1219);
constexpr int ERROR_INVALID_URL = static_cast<int>(0x817B3060);

constexpr std::uint32_t DEFAULT_MAX_SIZE = 4096;
constexpr std::uint32_t DEFAULT_MAX_NUM_PER_DOMAIN = 50;
constexpr std::uint32_t DEFAULT_MAX_NUM = 3000;
constexpr std::size_t EVICTED_OVER_MAX_NUM = 10;
constexpr std::size_t HEADER_SLACK = 3;

constexpr std::int64_t NEVER = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t EARLIEST = std::numeric_limits<std::int64_t>::min();

struct Cookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path;
    std::int64_t expiry;
    bool hasValue;
    bool secure;
};

struct CookieBox {
    std::vector<Cookie> cookies;
    std::uint32_t maxNum = DEFAULT_MAX_NUM;
    std::uint32_t maxNumPerDomain = DEFAULT_MAX_NUM_PER_DOMAIN;
    std::uint32_t maxSize = DEFAULT_MAX_SIZE;
    std::size_t maxInuseSize = 0;
    std::uint32_t maxInuseNum = 0;
    std::uint32_t removedNum = 0;
};

struct Url {
    bool secure;
    std::string host;
    std::string path;
};

std::mutex g_cookiesMutex;
std::unordered_map<int, CookieBox> g_cookieBoxes;

std::int64_t Now() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

char Lower(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string LowerCopy(std::string_view text) {
    std::string lowered(text);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), Lower);
    return lowered;
}

bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) { return Lower(a) == Lower(b); });
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
    return text;
}

int ParseUrl(std::string_view text, Url& url) {
    const std::size_t separator = text.find("://");
    url.secure = separator != std::string_view::npos && EqualsIgnoreCase(text.substr(0, separator), "https");
    std::string_view rest = separator == std::string_view::npos ? text : text.substr(separator + 3);
    const std::size_t authorityEnd = std::min(rest.find_first_of("/?#"), rest.size());
    std::string_view authority = rest.substr(0, authorityEnd);
    if (const std::size_t at = authority.rfind('@'); at != std::string_view::npos) authority.remove_prefix(at + 1);
    if (!authority.empty() && authority.front() == '[') {
        const std::size_t close = authority.find(']');
        if (close == std::string_view::npos) return ERROR_INVALID_URL;
        authority = authority.substr(0, close + 1);
    } else if (const std::size_t colon = authority.find(':'); colon != std::string_view::npos) {
        authority = authority.substr(0, colon);
    }
    if (authority.empty()) return ERROR_REJECTED;
    url.host = LowerCopy(authority);
    std::string_view path = rest.substr(authorityEnd);
    path = path.substr(0, std::min(path.find_first_of("?#"), path.size()));
    url.path = path.empty() || path.front() != '/' ? "/" : std::string(path);
    return 0;
}

std::string DefaultPath(const std::string& path) {
    const std::size_t slash = path.rfind('/');
    return slash == std::string::npos ? "/" : path.substr(0, slash + 1);
}

bool IsIpAddress(const std::string& host) {
    return host.front() == '[' || host.find_first_not_of("0123456789.") == std::string::npos;
}

bool DomainMatches(const std::string& host, const std::string& domain) {
    if (host == domain) return true;
    return host.size() > domain.size() && host.compare(host.size() - domain.size(), domain.size(), domain) == 0 && host[host.size() - domain.size() - 1] == '.' && !IsIpAddress(host);
}

bool PathMatches(const std::string& requestPath, const std::string& cookiePath) {
    return requestPath.compare(0, cookiePath.size(), cookiePath) == 0;
}

bool IsDateDelimiter(char c) {
    const auto u = static_cast<unsigned char>(c);
    return u == 0x09 || (u >= 0x20 && u <= 0x2F) || (u >= 0x3B && u <= 0x40) || (u >= 0x5B && u <= 0x60) || (u >= 0x7B && u <= 0x7E);
}

bool IsDigit(char c) {
    return c >= '0' && c <= '9';
}

bool LeadingDigits(std::string_view token, std::size_t minimum, std::size_t maximum, int& value, std::size_t& used) {
    used = 0;
    value = 0;
    while (used < token.size() && IsDigit(token[used])) value = value * 10 + (token[used++] - '0');
    return used >= minimum && used <= maximum;
}

bool ParseTime(std::string_view token, int& hour, int& minute, int& second) {
    int* fields[3] = {&hour, &minute, &second};
    for (int i = 0; i < 3; ++i) {
        std::size_t used = 0;
        if (!LeadingDigits(token, 1, 2, *fields[i], used)) return false;
        token.remove_prefix(used);
        if (i < 2) {
            if (token.empty() || token.front() != ':') return false;
            token.remove_prefix(1);
        }
    }
    return true;
}

std::int64_t DaysFromCivil(int year, int month, int day) {
    year -= month <= 2;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t yearOfEra = year - era * 400;
    const std::int64_t dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const std::int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

std::optional<std::int64_t> ParseCookieDate(std::string_view text) {
    static constexpr const char* MONTHS[12] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
    bool foundTime = false;
    bool foundDay = false;
    bool foundMonth = false;
    bool foundYear = false;
    int hour = 0, minute = 0, second = 0, day = 0, month = 0, year = 0;
    std::size_t position = 0;
    while (position < text.size()) {
        while (position < text.size() && IsDateDelimiter(text[position])) ++position;
        const std::size_t start = position;
        while (position < text.size() && !IsDateDelimiter(text[position])) ++position;
        const std::string_view token = text.substr(start, position - start);
        if (token.empty()) continue;
        int value = 0;
        std::size_t used = 0;
        if (!foundTime && ParseTime(token, hour, minute, second)) {
            foundTime = true;
        } else if (!foundDay && LeadingDigits(token, 1, 2, value, used) && (used == token.size() || !IsDigit(token[used]))) {
            foundDay = true;
            day = value;
        } else if (const auto* name = token.size() >= 3 ? std::find_if(std::begin(MONTHS), std::end(MONTHS), [&](const char* candidate) { return EqualsIgnoreCase(token.substr(0, 3), candidate); }) : std::end(MONTHS);
                   !foundMonth && name != std::end(MONTHS)) {
            foundMonth = true;
            month = 1 + static_cast<int>(name - std::begin(MONTHS));
        } else if (!foundYear && LeadingDigits(token, 2, 4, value, used) && (used == token.size() || !IsDigit(token[used]))) {
            foundYear = true;
            year = value;
        }
    }
    if (foundYear && year >= 70 && year <= 99) year += 1900;
    if (foundYear && year >= 0 && year <= 69) year += 2000;
    if (!foundTime || !foundDay || !foundMonth || !foundYear) return std::nullopt;
    if (day < 1 || day > 31 || year < 1601 || hour > 23 || minute > 59 || second > 59) return std::nullopt;
    const std::int64_t monthStart = DaysFromCivil(year, month, 1);
    const std::int64_t nextMonthStart = month < 12 ? DaysFromCivil(year, month + 1, 1) : DaysFromCivil(year + 1, 1, 1);
    if (day > nextMonthStart - monthStart) return std::nullopt;
    return DaysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
}

std::optional<std::int64_t> ParseMaxAge(std::string_view text, std::int64_t now) {
    const bool negative = !text.empty() && text.front() == '-';
    if (negative) text.remove_prefix(1);
    if (text.empty() || !std::all_of(text.begin(), text.end(), IsDigit)) return std::nullopt;
    if (negative) return EARLIEST;
    std::int64_t seconds = 0;
    for (const char c : text) {
        if (seconds > (NEVER - (c - '0')) / 10) return NEVER;
        seconds = seconds * 10 + (c - '0');
    }
    if (seconds == 0) return EARLIEST;
    return seconds > NEVER - now ? NEVER : now + seconds;
}

std::size_t PairSize(const Cookie& cookie) {
    return cookie.name.size() + cookie.value.size();
}

std::size_t StoredSize(const Cookie& cookie) {
    return PairSize(cookie) + cookie.domain.size() + cookie.path.size() + 5;
}

std::size_t InuseSize(const CookieBox& box) {
    std::size_t size = 0;
    for (const Cookie& cookie : box.cookies) size += StoredSize(cookie);
    return size;
}

void RemoveExpired(CookieBox& box, std::int64_t now) {
    box.cookies.erase(std::remove_if(box.cookies.begin(), box.cookies.end(), [&](const Cookie& cookie) { return cookie.expiry <= now; }), box.cookies.end());
}

void MakeRoom(CookieBox& box, const std::string& domain) {
    const auto inDomain = [&](const Cookie& cookie) { return cookie.domain == domain; };
    while (true) {
        const auto count = static_cast<std::size_t>(std::count_if(box.cookies.begin(), box.cookies.end(), inDomain));
        if (count == 0 || count < box.maxNumPerDomain) break;
        box.cookies.erase(std::find_if(box.cookies.begin(), box.cookies.end(), inDomain));
        ++box.removedNum;
    }
    if (box.cookies.size() >= box.maxNum) {
        const std::size_t evicted = std::min(box.cookies.size(), EVICTED_OVER_MAX_NUM);
        box.cookies.erase(box.cookies.begin(), box.cookies.begin() + static_cast<std::ptrdiff_t>(evicted));
        box.removedNum += static_cast<std::uint32_t>(evicted);
    }
}

void RecordUsage(CookieBox& box) {
    box.maxInuseSize = std::max(box.maxInuseSize, InuseSize(box));
    box.maxInuseNum = std::max(box.maxInuseNum, static_cast<std::uint32_t>(box.cookies.size()));
}

int StoreCookie(CookieBox& box, const Url& url, std::string_view text, std::int64_t now) {
    const std::size_t pairEnd = std::min(text.find(';'), text.size());
    const std::string_view pair = text.substr(0, pairEnd);
    const std::size_t equals = pair.find('=');
    Cookie cookie{};
    cookie.name = std::string(Trim(pair.substr(0, equals)));
    cookie.hasValue = equals != std::string_view::npos;
    if (cookie.hasValue) cookie.value = std::string(Trim(pair.substr(equals + 1)));
    cookie.expiry = NEVER;
    if (PairSize(cookie) > box.maxSize || box.maxSize == 0) return ERROR_OUT_OF_SIZE;
    std::optional<std::int64_t> maxAge;
    std::optional<std::int64_t> expires;
    std::string domain;
    std::optional<std::string> path;
    std::string_view attributes = text.substr(pairEnd);
    while (!attributes.empty()) {
        attributes.remove_prefix(1);
        const std::size_t end = std::min(attributes.find(';'), attributes.size());
        const std::string_view attribute = attributes.substr(0, end);
        attributes.remove_prefix(end);
        const std::size_t split = attribute.find('=');
        const std::string_view key = Trim(attribute.substr(0, split));
        const std::string_view value = split == std::string_view::npos ? std::string_view{} : Trim(attribute.substr(split + 1));
        if (EqualsIgnoreCase(key, "expires")) {
            if (const auto parsed = ParseCookieDate(value)) expires = parsed;
        } else if (EqualsIgnoreCase(key, "max-age")) {
            if (const auto parsed = ParseMaxAge(value, now)) maxAge = parsed;
        } else if (EqualsIgnoreCase(key, "domain")) {
            if (!value.empty()) domain = LowerCopy(value.front() == '.' ? value.substr(1) : value);
        } else if (EqualsIgnoreCase(key, "path")) {
            path = !value.empty() && value.front() == '/' ? std::string(value) : DefaultPath(url.path);
        } else if (EqualsIgnoreCase(key, "secure")) {
            cookie.secure = true;
        }
    }
    if (maxAge) cookie.expiry = *maxAge;
    else if (expires) cookie.expiry = *expires;
    if (!domain.empty() && !DomainMatches(url.host, domain)) return ERROR_REJECTED;
    cookie.domain = domain.empty() ? url.host : domain;
    cookie.path = path ? *path : DefaultPath(url.path);
    const auto existing = std::find_if(box.cookies.begin(), box.cookies.end(), [&](const Cookie& stored) {
        return stored.name == cookie.name && stored.domain == cookie.domain && stored.path == cookie.path;
    });
    if (existing != box.cookies.end()) box.cookies.erase(existing);
    if (cookie.expiry <= now) return 0;
    RemoveExpired(box, now);
    MakeRoom(box, cookie.domain);
    box.cookies.push_back(std::move(cookie));
    RecordUsage(box);
    return 0;
}

void AppendCookie(std::string& header, const Cookie& cookie) {
    if (!header.empty()) header += "; ";
    if (cookie.name.empty()) {
        header += cookie.value;
        return;
    }
    header += cookie.name;
    if (!cookie.hasValue) return;
    header += '=';
    header += cookie.value;
}

CookieBox* FindBox(int cookieBoxId) {
    const auto box = g_cookieBoxes.find(cookieBoxId);
    return box != g_cookieBoxes.end() ? &box->second : nullptr;
}

std::vector<const Cookie*> Matches(CookieBox& box, const Url& url) {
    RemoveExpired(box, Now());
    std::vector<const Cookie*> matches;
    for (const Cookie& stored : box.cookies) {
        if (DomainMatches(url.host, stored.domain) && PathMatches(url.path, stored.path) && (!stored.secure || url.secure)) matches.push_back(&stored);
    }
    std::stable_sort(matches.begin(), matches.end(), [](const Cookie* left, const Cookie* right) { return left->path.size() > right->path.size(); });
    for (auto run = matches.begin(); run != matches.end();) {
        const auto end = std::find_if(run, matches.end(), [&](const Cookie* match) { return match->path.size() != (*run)->path.size(); });
        std::rotate(run, run + 1, end);
        run = end;
    }
    return matches;
}

}

std::optional<std::string> Http2CookieHeader(int cookieBoxId, std::string_view url) {
    std::lock_guard lock(g_cookiesMutex);
    CookieBox* box = FindBox(cookieBoxId);
    Url parsed{};
    if (!box || ParseUrl(url, parsed) != 0) return std::nullopt;
    const auto matches = Matches(*box, parsed);
    if (matches.empty()) return std::nullopt;
    std::string header;
    for (const Cookie* match : matches) AppendCookie(header, *match);
    return header;
}

void Http2StoreResponseCookie(int cookieBoxId, std::string_view url, std::string_view cookie) {
    std::lock_guard lock(g_cookiesMutex);
    CookieBox* box = FindBox(cookieBoxId);
    Url parsed{};
    if (box && ParseUrl(url, parsed) == 0) static_cast<void>(StoreCookie(*box, parsed, cookie, Now()));
}

void Http2AddCookieBox(int cookieBoxId) {
    std::lock_guard lock(g_cookiesMutex);
    g_cookieBoxes.try_emplace(cookieBoxId);
}

void Http2ForgetCookies(int cookieBoxId) {
    std::lock_guard lock(g_cookiesMutex);
    g_cookieBoxes.erase(cookieBoxId);
}

void Http2FlushCookies(int cookieBoxId) {
    std::lock_guard lock(g_cookiesMutex);
    const auto box = g_cookieBoxes.find(cookieBoxId);
    if (box != g_cookieBoxes.end()) box->second.cookies.clear();
}

extern "C" {

int APS5_VABI sceHttp2AddCookie(int cookieBoxId, const char* url, const char* cookie, std::size_t cookieLength) {
    std::lock_guard lock(g_cookiesMutex);
    CookieBox* box = FindBox(cookieBoxId);
    if (!box) return ERROR_INVALID_ID;
    if (!url || !cookie || cookieLength == 0) return ERROR_INVALID_VALUE;
    Url parsed{};
    if (const int result = ParseUrl(url, parsed); result != 0) return result;
    return StoreCookie(*box, parsed, std::string_view(cookie, cookieLength), Now());
}

int APS5_VABI sceHttp2GetCookie(int cookieBoxId, const char* url, char* cookie, std::size_t* required, std::size_t prepared) {
    std::lock_guard lock(g_cookiesMutex);
    CookieBox* box = FindBox(cookieBoxId);
    if (!box) return ERROR_INVALID_ID;
    if (!url) return ERROR_INVALID_VALUE;
    Url parsed{};
    if (const int result = ParseUrl(url, parsed); result == ERROR_REJECTED) return ERROR_NOT_FOUND;
    else if (result != 0) return result;
    const auto matches = Matches(*box, parsed);
    if (matches.empty()) return ERROR_NOT_FOUND;
    if (!cookie && !required) return ERROR_INVALID_VALUE;
    std::string header;
    for (const Cookie* match : matches) AppendCookie(header, *match);
    const std::size_t size = header.size() + HEADER_SLACK;
    if (!cookie) {
        *required = size;
        return 0;
    }
    if (prepared < size) {
        if (prepared != 0) cookie[0] = '\0';
        return ERROR_OUT_OF_SIZE;
    }
    header.append({'\0', ' ', '\0'});
    std::memcpy(cookie, header.data(), size);
    if (required) *required = size;
    return 0;
}

int APS5_VABI sceHttp2GetCookieStats(int cookieBoxId, Http2CookieStats* stats) {
    std::lock_guard lock(g_cookiesMutex);
    if (!stats) return ERROR_INVALID_VALUE;
    CookieBox* box = FindBox(cookieBoxId);
    if (!box) return ERROR_INVALID_ID;
    RemoveExpired(*box, Now());
    stats->currentInuseSize = InuseSize(*box);
    stats->currentInuseNum = static_cast<std::uint32_t>(box->cookies.size());
    stats->maxInuseSize = box->maxInuseSize;
    stats->maxInuseNum = box->maxInuseNum;
    stats->removedNum = box->removedNum;
    return 0;
}

int APS5_VABI sceHttp2SetCookieMaxNum(int cookieBoxId, std::uint32_t num) {
    std::lock_guard lock(g_cookiesMutex);
    CookieBox* box = FindBox(cookieBoxId);
    if (!box) return ERROR_INVALID_ID;
    box->maxNum = num;
    return 0;
}

int APS5_VABI sceHttp2SetCookieMaxNumPerDomain(int cookieBoxId, std::uint32_t num) {
    std::lock_guard lock(g_cookiesMutex);
    CookieBox* box = FindBox(cookieBoxId);
    if (!box) return ERROR_INVALID_ID;
    box->maxNumPerDomain = num;
    return 0;
}

int APS5_VABI sceHttp2SetCookieMaxSize(int cookieBoxId, std::uint32_t size) {
    std::lock_guard lock(g_cookiesMutex);
    CookieBox* box = FindBox(cookieBoxId);
    if (!box) return ERROR_INVALID_ID;
    box->maxSize = size;
    return 0;
}

}
