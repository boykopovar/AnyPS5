#include "prx/libSceHttpCache/libSceHttpCache.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libSceRtc/libSceRtc.hpp"
#include "prx/libSceUserService/libSceUserService.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(HttpCacheHeader) == 16);
static_assert(sizeof(HttpCacheRequest) == 32);
static_assert(offsetof(HttpCacheRequest, url) == 8);
static_assert(offsetof(HttpCacheRequest, headerCount) == 16);
static_assert(offsetof(HttpCacheRequest, headers) == 24);
static_assert(sizeof(HttpCacheResponseHeader) == 24);
static_assert(offsetof(HttpCacheResponseHeader, headerCount) == 8);
static_assert(offsetof(HttpCacheResponseHeader, headers) == 16);
static_assert(sizeof(HttpCacheResponse) == 32);
static_assert(offsetof(HttpCacheResponse, headerCount) == 8);
static_assert(offsetof(HttpCacheResponse, headers) == 16);
static_assert(offsetof(HttpCacheResponse, contentLength) == 24);
static_assert(sizeof(HttpCacheQuotaEntry) == 16);
static_assert(sizeof(HttpCacheQuota) == 16);

namespace {

constexpr int ErrorUnknown = static_cast<int>(0x817a0000);
constexpr int ErrorNoMemory = static_cast<int>(0x817a0001);
constexpr int ErrorTooManyCaches = static_cast<int>(0x817a0002);
constexpr int ErrorNotFound = static_cast<int>(0x817a0011);
constexpr int ErrorInvalidValue = static_cast<int>(0x817a0012);
constexpr int ErrorInvalidUser = static_cast<int>(0x817a0013);
constexpr int UserServiceAlreadyInitialized = static_cast<int>(0x80960003);
constexpr int UserServiceNoMemory = static_cast<int>(0x80960004);
constexpr int UserServiceInvalidArgument = static_cast<int>(0x80960005);
constexpr std::size_t MaxCaches = 127;
constexpr std::uint32_t RequestCounterLimit = 0x1000000;
constexpr std::int32_t SharedUser = 0xfe;
constexpr std::uint64_t HeuristicWarningSeconds = 86400;

constexpr int ResultMiss = 0;
constexpr int ResultHit = 1;
constexpr int ResultRevalidate = 2;

enum class State {
    Idle,
    Miss,
    Hit,
    Revalidate,
    RevalidateWithStale,
    Replacing,
    Validated,
    Dropped,
};

constexpr std::string_view StaleWarning = "110 - \"Response is Stale\"";
constexpr std::string_view HeuristicWarning = "113 - \"Heuristic expiration\"";

constexpr std::array<std::string_view, 13> UnmergedHeaders{
    "allow", "connection", "etag", "keep-alive", "last-modified", "proxy-authenticate", "proxy-connection",
    "trailer", "transfer-encoding", "upgrade", "www-authenticate", "x-frame-options", "x-xss-protection"};
constexpr std::array<std::string_view, 3> UnmergedHeaderNames{"content-", "x-content-", "x-webkit-"};
constexpr std::array<std::string_view, 5> ConditionalHeaders{
    "If-Match", "If-Modified-Since", "If-None-Match", "If-Range", "If-Unmodified-Since"};

struct Header {
    std::string name;
    std::string value;
};

using Headers = std::vector<Header>;

struct CacheControl {
    bool present = false;
    bool maxAge = false;
    std::uint64_t maxAgeTicks = 0;
    bool sMaxage = false;
    std::uint64_t sMaxageTicks = 0;
    bool maxStale = false;
    bool maxStaleLimited = false;
    std::uint64_t maxStaleTicks = 0;
    bool staleWhileRevalidate = false;
    std::uint64_t staleWhileRevalidateTicks = 0;
    bool staleIfError = false;
    std::uint64_t staleIfErrorTicks = 0;
    bool noCache = false;
    bool noStore = false;
    bool mustRevalidate = false;
    bool proxyRevalidate = false;
    bool isPrivate = false;
    bool isPublic = false;
};

using StoreKey = std::tuple<std::int32_t, std::string, std::string>;

struct Entry {
    std::uint32_t status = 0;
    Headers headers;
    std::uint64_t responseTime = 0;
    std::uint64_t lifetime = 0;
    Headers vary;
    std::string body;
    bool isPublic = false;
    bool pending = false;
    std::size_t readOffset = 0;
    StoreKey key;
};

struct RequestData {
    std::uint32_t method = 0;
    std::string url;
    Headers headers;
};

struct ResponseData {
    std::uint32_t status = 0;
    Headers headers;
    std::uint64_t time = 0;
};

struct Request {
    std::int32_t storeUser = -1;
    bool hasStore = false;
    State state = State::Idle;
    int lastResult = ResultMiss;
    std::optional<RequestData> current;
    std::optional<RequestData> next;
    std::shared_ptr<Entry> entry;
    std::deque<std::string> strings;
    std::deque<std::vector<HttpCacheHeader>> headerArrays;
};

struct Cache {
    std::uint32_t nextRequest = 1;
    std::map<std::int32_t, Request> requests;
};

std::mutex Mutex;
std::map<std::int32_t, Cache> Caches;
std::map<StoreKey, Entry> Store;
std::int32_t LastCacheId = 0;
bool UserServiceStarted = false;
std::uint8_t CacheSharing = 1;

Cache* FindCache(std::int32_t cacheId) {
    const auto found = Caches.find(cacheId);
    return found == Caches.end() ? nullptr : &found->second;
}

Cache* RequestCache(std::int32_t requestId) {
    return FindCache(static_cast<std::int32_t>(static_cast<std::uint32_t>(requestId) >> 24));
}

Request* FindRequest(std::int32_t requestId) {
    Cache* cache = RequestCache(requestId);
    if (cache == nullptr) return nullptr;
    const auto found = cache->requests.find(requestId);
    return found == cache->requests.end() ? nullptr : &found->second;
}

bool RegisteredUser(std::int32_t userId) {
    if (userId == SharedUser) return true;
    std::int32_t list[20];
    for (auto& entry : list) entry = -1;
    if (sceUserServiceGetRegisteredUserIdList(reinterpret_cast<UserServiceRegisteredUserIdList*>(list)) < 0) return false;
    for (const auto entry : list) {
        if (entry != -1 && entry == userId) return true;
    }
    return false;
}

bool SystemUser(std::int32_t userId) {
    return ((static_cast<std::uint32_t>(userId) + 1) & ~0x100u) == 0;
}

std::uint64_t Now() {
    RtcTick tick{};
    sceRtcGetCurrentTick(&tick);
    return tick.tick;
}

std::uint64_t Resolution() {
    return static_cast<std::uint32_t>(sceRtcGetTickResolution());
}

bool EqualsIgnoreCase(std::string_view first, std::string_view second) {
    if (first.size() != second.size()) return false;
    for (std::size_t i = 0; i < first.size(); ++i) {
        const auto lower = [](char value) { return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value; };
        if (lower(first[i]) != lower(second[i])) return false;
    }
    return true;
}

std::string Lower(std::string_view text) {
    std::string result(text);
    for (auto& value : result) {
        if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
    }
    return result;
}

Headers::const_iterator Find(const Headers& headers, std::string_view name) {
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        if (EqualsIgnoreCase(it->name, name)) return it;
    }
    return headers.end();
}

bool Has(const Headers& headers, std::string_view name) {
    return Find(headers, name) != headers.end();
}

std::string FirstValue(const Headers& headers, std::string_view name) {
    const auto found = Find(headers, name);
    return found == headers.end() ? std::string() : found->value;
}

std::string JoinedValues(const Headers& headers, std::string_view name) {
    std::string result;
    bool first = true;
    for (const auto& header : headers) {
        if (!EqualsIgnoreCase(header.name, name)) continue;
        if (!first) result += ',';
        result += header.value;
        first = false;
    }
    return result;
}

void Append(Headers& headers, std::string_view name, std::string_view value) {
    headers.push_back(Header{std::string(name), std::string(value)});
}

bool SetExisting(Headers& headers, std::string_view name, std::string_view value) {
    for (auto& header : headers) {
        if (EqualsIgnoreCase(header.name, name)) {
            header.value = std::string(value);
            return true;
        }
    }
    return false;
}

void SetOrAppend(Headers& headers, std::string_view name, std::string_view value) {
    if (!SetExisting(headers, name, value)) Append(headers, name, value);
}

void AppendUnlessSame(Headers& headers, std::string_view name, std::string_view value) {
    const auto found = Find(headers, name);
    if (found != headers.end() && EqualsIgnoreCase(found->value, value)) return;
    Append(headers, name, value);
}

void RemoveFirst(Headers& headers, std::string_view name) {
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        if (EqualsIgnoreCase(it->name, name)) {
            headers.erase(it);
            return;
        }
    }
}

std::vector<std::string> Split(std::string_view text, char delimiter) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find(delimiter, start);
        if (end == std::string_view::npos) end = text.size();
        parts.emplace_back(text.substr(start, end - start));
        start = end == text.size() ? end : end + 1;
    }
    return parts;
}

std::string Trim(std::string_view text) {
    constexpr std::string_view spaces = " \t\v\r\n";
    const auto first = text.find_first_not_of(spaces);
    if (first == std::string_view::npos) return {};
    const auto last = text.find_last_not_of(spaces);
    return std::string(text.substr(first, last - first + 1));
}

std::uint64_t ParseUnsigned(const std::string& text) {
    errno = 0;
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text.c_str(), &end, 10);
    if (end == text.c_str() || errno == ERANGE) NotImplemented_nid_no_patch("sceHttpCache header number that the firmware's stoul rejects");
    return value;
}

std::int64_t ParseSigned(const std::string& text) {
    errno = 0;
    char* end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (end == text.c_str() || errno == ERANGE) NotImplemented_nid_no_patch("sceHttpCache header number that the firmware's stol rejects");
    return value;
}

bool ParseDate(const std::string& text, std::uint64_t& tick) {
    if (text.empty()) return false;
    RtcTick parsed{};
    if (sceRtcParseDateTime(&parsed, text.c_str()) != 0) return false;
    tick = parsed.tick;
    return true;
}

CacheControl ParseCacheControl(const Headers& headers) {
    CacheControl control;
    const auto tokens = Split(FirstValue(headers, "Cache-Control"), ',');
    if (tokens.empty()) return control;
    control.present = true;
    const std::uint64_t resolution = Resolution();
    for (const auto& token : tokens) {
        std::string unquoted;
        for (const char value : token) {
            if (value != '"') unquoted += value;
        }
        const auto parts = Split(Trim(unquoted), '=');
        std::string name;
        std::string value;
        if (parts.size() == 1) {
            name = parts[0];
        } else if (parts.size() == 2) {
            name = parts[0];
            value = parts[1];
        }
        if (!value.empty()) {
            if (EqualsIgnoreCase(name, "max-age")) {
                if (!control.maxAge) {
                    control.maxAgeTicks = ParseUnsigned(value) * resolution;
                    control.maxAge = true;
                }
            } else if (EqualsIgnoreCase(name, "s-maxage")) {
                if (!control.sMaxage) {
                    control.sMaxageTicks = ParseUnsigned(value) * resolution;
                    control.sMaxage = true;
                }
            } else if (EqualsIgnoreCase(name, "max-stale")) {
                if (!control.maxStale) {
                    control.maxStale = true;
                    const std::int64_t seconds = ParseSigned(value);
                    if (seconds < 0) {
                        control.maxStale = false;
                    } else {
                        control.maxStaleTicks = static_cast<std::uint64_t>(seconds) * resolution;
                        control.maxStaleLimited = true;
                    }
                }
            } else if (EqualsIgnoreCase(name, "stale-while-revalidate")) {
                if (!control.staleWhileRevalidate) {
                    control.staleWhileRevalidateTicks = ParseUnsigned(value) * resolution;
                    control.staleWhileRevalidate = true;
                }
            } else if (EqualsIgnoreCase(name, "stale-if-error")) {
                if (!control.staleIfError) {
                    control.staleIfErrorTicks = ParseUnsigned(value) * resolution;
                    control.staleIfError = true;
                }
            }
        } else if (EqualsIgnoreCase(name, "no-cache")) {
            control.noCache = true;
        } else if (EqualsIgnoreCase(name, "no-store")) {
            control.noStore = true;
        } else if (EqualsIgnoreCase(name, "must-revalidate")) {
            control.mustRevalidate = true;
        } else if (EqualsIgnoreCase(name, "proxy-revalidate")) {
            control.proxyRevalidate = true;
        } else if (EqualsIgnoreCase(name, "private")) {
            control.isPrivate = true;
        } else if (EqualsIgnoreCase(name, "public")) {
            control.isPublic = true;
        } else if (EqualsIgnoreCase(name, "max-stale")) {
            control.maxStale = true;
        }
    }
    return control;
}

bool StorableHeaders(const Headers& headers) {
    const CacheControl control = ParseCacheControl(headers);
    if (control.present && control.noStore) return false;
    if (Has(headers, "Expires") || Has(headers, "Last-Modified")) return true;
    return control.present && (control.maxAge || control.sMaxage);
}

bool StorableStatus(std::uint32_t status) {
    return status == 200 || status == 301 || status == 308;
}

bool ServerError(std::uint32_t status) {
    return status == 500 || status == 502 || status == 503 || status == 504;
}

std::uint64_t Lifetime(const ResponseData& response) {
    const CacheControl control = ParseCacheControl(response.headers);
    if (control.present && control.maxAge) return control.maxAgeTicks;
    if (control.present && control.sMaxage) return control.sMaxageTicks;
    std::uint64_t date = response.time;
    ParseDate(FirstValue(response.headers, "Date"), date);
    std::uint64_t expires = 0;
    if (ParseDate(FirstValue(response.headers, "Expires"), expires)) return expires - date;
    std::uint64_t lastModified = 0;
    if (ParseDate(FirstValue(response.headers, "Last-Modified"), lastModified)) return (date - lastModified) / 10;
    return 0;
}

bool Heuristic(const Headers& headers) {
    const CacheControl control = ParseCacheControl(headers);
    if (control.present && (control.sMaxage || control.maxAge)) return false;
    return !Has(headers, "Expires");
}

std::uint64_t CurrentAge(const Entry& entry) {
    const std::uint64_t now = Now();
    std::uint64_t date = 0;
    std::uint64_t apparentAge = 0;
    if (ParseDate(FirstValue(entry.headers, "Date"), date)) apparentAge = entry.responseTime > date ? entry.responseTime - date : 0;
    const std::string ageText = FirstValue(entry.headers, "Age");
    const std::uint64_t ageValue = ageText.empty() ? 0 : ParseUnsigned(ageText) * Resolution();
    const std::uint64_t correctedAge = apparentAge > ageValue ? apparentAge : ageValue;
    const std::uint64_t residentTime = now > entry.responseTime ? now - entry.responseTime : 0;
    return correctedAge + residentTime;
}

bool VaryMatches(const Headers& vary, const Headers& requestHeaders) {
    for (const auto& header : vary) {
        if (FirstValue(requestHeaders, header.name) != header.value) return false;
    }
    return true;
}

Headers SelectVary(const Headers& requestHeaders, const Headers& responseHeaders) {
    Headers vary;
    const std::string names = JoinedValues(responseHeaders, "Vary");
    if (names.empty()) return vary;
    for (const auto& name : Split(names, ',')) {
        const std::string trimmed = Trim(name);
        Append(vary, trimmed, FirstValue(requestHeaders, trimmed));
    }
    return vary;
}

bool AddValidators(const Headers& entryHeaders, Headers& headers) {
    const std::string lastModified = FirstValue(entryHeaders, "Last-Modified");
    const std::string etag = FirstValue(entryHeaders, "ETag");
    if (!lastModified.empty()) Append(headers, "If-Modified-Since", lastModified);
    if (!etag.empty()) Append(headers, "If-None-Match", etag);
    return !lastModified.empty() || !etag.empty();
}

bool StaleUsable(const Entry* entry) {
    if (entry == nullptr) return false;
    const CacheControl control = ParseCacheControl(entry->headers);
    if (!Has(entry->headers, "Age") || !control.staleWhileRevalidate) return false;
    for (const auto& header : entry->headers) {
        if (EqualsIgnoreCase(header.name, "Warning") && EqualsIgnoreCase(header.value, StaleWarning)) return true;
    }
    return false;
}

struct Decision {
    int result = ResultMiss;
    bool remove = false;
    Headers next;
};

Decision Validate(const Headers& requestHeaders, Entry& entry) {
    Decision decision;
    if (!entry.vary.empty() && !VaryMatches(entry.vary, requestHeaders)) return decision;
    for (const auto name : ConditionalHeaders) {
        if (Has(requestHeaders, name)) return decision;
    }
    const auto revalidate = [&] {
        decision.next = requestHeaders;
        if (AddValidators(entry.headers, decision.next)) {
            decision.result = ResultRevalidate;
        } else {
            decision.remove = true;
        }
        return decision;
    };
    const CacheControl requestControl = ParseCacheControl(requestHeaders);
    if (requestControl.present && requestControl.noCache) return revalidate();
    if (EqualsIgnoreCase(FirstValue(requestHeaders, "Pragma"), "no-cache")) return revalidate();
    const CacheControl responseControl = ParseCacheControl(entry.headers);
    if (responseControl.present && (responseControl.noCache || responseControl.mustRevalidate || responseControl.proxyRevalidate)) return revalidate();
    const std::uint64_t lifetime = entry.lifetime;
    const std::uint64_t age = CurrentAge(entry);
    if (Heuristic(entry.headers) && age >= HeuristicWarningSeconds * Resolution()) Append(entry.headers, "Warning", HeuristicWarning);
    if (age < lifetime) {
        decision.result = ResultHit;
        return decision;
    }
    if (requestControl.maxStale && (!requestControl.maxStaleLimited || age - lifetime < requestControl.maxStaleTicks)) {
        Append(entry.headers, "Warning", StaleWarning);
        decision.result = ResultHit;
        return decision;
    }
    revalidate();
    if (decision.result == ResultRevalidate && responseControl.staleWhileRevalidate && responseControl.staleWhileRevalidateTicks + lifetime > age) {
        AppendUnlessSame(entry.headers, "Warning", StaleWarning);
        SetOrAppend(entry.headers, "Age", std::to_string(age / Resolution()));
    }
    return decision;
}

int ConvertHeaders(std::size_t count, const HttpCacheHeader* source, Headers& headers) {
    if (source == nullptr) return count != 0 ? ErrorInvalidValue : 0;
    for (std::size_t i = 0; i < count; ++i) {
        if (source[i].name == nullptr || source[i].value == nullptr) return ErrorInvalidValue;
        Append(headers, source[i].name, source[i].value);
    }
    return 0;
}

StoreKey KeyFor(const Request& request) {
    return StoreKey{request.storeUser, request.current->url, FirstValue(request.current->headers, "User-Agent")};
}

void RejectSharedEntry(const Request& request) {
    if (request.storeUser == SharedUser) return;
    const auto& key = KeyFor(request);
    if (Store.count(StoreKey{SharedUser, std::get<1>(key), std::get<2>(key)}) != 0) {
        NotImplemented_nid_no_patch("sceHttpCache lookup by a user of an entry kept for all users");
    }
}

void ApplySharing(Request& request, Entry& entry) {
    entry.isPublic = CacheSharing != 0 && ParseCacheControl(entry.headers).isPublic;
    if (!entry.isPublic) return;
    if (request.storeUser != SharedUser) NotImplemented_nid_no_patch("sceHttpCache storing a public response for all users");
}

void RemoveStored(const Request& request) {
    if (request.hasStore && request.current) Store.erase(KeyFor(request));
}

std::shared_ptr<Entry> LookupStored(const Request& request) {
    RejectSharedEntry(request);
    const auto found = Store.find(KeyFor(request));
    if (found == Store.end()) return nullptr;
    auto entry = std::make_shared<Entry>(found->second);
    entry->pending = false;
    entry->readOffset = 0;
    entry->key = found->first;
    return entry;
}

void FillEntry(Request& request, Entry& entry, const ResponseData& response) {
    entry.status = response.status;
    entry.headers = response.headers;
    entry.responseTime = response.time;
    entry.lifetime = Lifetime(response);
    entry.vary = SelectVary(request.current->headers, response.headers);
    ApplySharing(request, entry);
}

const HttpCacheHeader* ExportHeaders(Request& request, const Headers& headers) {
    if (headers.empty()) NotImplemented_nid_no_patch("sceHttpCache returning an empty header list");
    auto& array = request.headerArrays.emplace_back();
    for (const auto& header : headers) {
        const char* name = request.strings.emplace_back(header.name).c_str();
        const char* value = request.strings.emplace_back(header.value).c_str();
        array.push_back(HttpCacheHeader{name, value});
    }
    return array.data();
}

void FillResponse(Request& request, const Entry& entry, HttpCacheResponse* response) {
    response->statusCode = entry.status;
    response->headerCount = entry.headers.size();
    response->headers = ExportHeaders(request, entry.headers);
    response->contentLength = entry.pending ? 0 : entry.body.size();
}

void FillNextRequest(Request& request, const RequestData& next, HttpCacheRequest* nextRequest) {
    nextRequest->method = next.method;
    nextRequest->headerCount = next.headers.size();
    nextRequest->headers = ExportHeaders(request, next.headers);
    nextRequest->url = request.strings.emplace_back(next.url).c_str();
}

int RetrieveFor(Request& request, RequestData data, std::shared_ptr<Entry>& found, const RequestData*& next) {
    found = nullptr;
    next = nullptr;
    if (request.entry && request.current && request.current->url == data.url) {
        found = request.entry;
        if (request.lastResult == ResultRevalidate) next = request.next ? &*request.next : nullptr;
        return request.lastResult;
    }
    request.state = State::Idle;
    if (data.method != 0) return ResultMiss;
    request.current = std::move(data);
    if (request.hasStore) request.entry = LookupStored(request);
    if (!request.entry) {
        request.state = State::Miss;
        request.lastResult = ResultMiss;
        return ResultMiss;
    }
    Decision decision = Validate(request.current->headers, *request.entry);
    if (decision.result == ResultHit) {
        request.state = State::Hit;
        found = request.entry;
    } else if (decision.result == ResultRevalidate) {
        request.next = RequestData{request.current->method, request.current->url, std::move(decision.next)};
        next = &*request.next;
        if (StaleUsable(request.entry.get())) {
            found = request.entry;
            request.state = State::RevalidateWithStale;
        } else {
            request.state = State::Revalidate;
        }
    } else {
        if (decision.remove) RemoveStored(request);
        request.state = State::Miss;
    }
    request.lastResult = decision.result;
    return decision.result;
}

bool HeadersUnchanged(const Headers& stored, const Headers& received) {
    if (stored.size() != received.size()) return false;
    for (const auto& header : received) {
        if (!EqualsIgnoreCase(FirstValue(stored, header.name), header.value)) return false;
    }
    return true;
}

bool MergedHeader(std::string_view name) {
    const std::string lower = Lower(name);
    for (const auto excluded : UnmergedHeaders) {
        if (lower == excluded) return false;
    }
    for (const auto excluded : UnmergedHeaderNames) {
        if (lower == excluded) return false;
    }
    return true;
}

bool Revalidated(const ResponseData& response, Entry& entry) {
    if (ServerError(response.status)) {
        const CacheControl control = ParseCacheControl(entry.headers);
        if (!control.present || !control.staleIfError) return false;
        const std::uint64_t age = CurrentAge(entry);
        if (control.staleIfErrorTicks + entry.lifetime <= age) return false;
        AppendUnlessSame(entry.headers, "Warning", StaleWarning);
        SetOrAppend(entry.headers, "Age", std::to_string(age / Resolution()));
        return true;
    }
    if (response.status != 304) return false;
    Headers merged = entry.headers;
    for (const auto& header : response.headers) {
        if (MergedHeader(header.name)) SetExisting(merged, header.name, header.value);
    }
    RemoveFirst(merged, "Warning");
    RemoveFirst(merged, "Age");
    entry.headers = std::move(merged);
    return true;
}

int Revalidate(Request& request, const ResponseData& response, std::shared_ptr<Entry>& found) {
    found = nullptr;
    if (request.state < State::Revalidate || !request.entry) return ResultMiss;
    const bool serverError = ServerError(response.status);
    if (!serverError && !StorableHeaders(response.headers)) {
        RemoveStored(request);
        return ResultMiss;
    }
    Entry& entry = *request.entry;
    const bool unchanged = HeadersUnchanged(entry.headers, response.headers);
    int result = ResultMiss;
    if (Revalidated(response, entry)) {
        if (!serverError && !unchanged) {
            entry.responseTime = Now();
            entry.lifetime = Lifetime(response);
            ApplySharing(request, entry);
            const auto stored = Store.find(entry.key);
            if (!entry.pending && stored != Store.end()) {
                stored->second.status = entry.status;
                stored->second.headers = entry.headers;
                stored->second.responseTime = entry.responseTime;
                stored->second.lifetime = entry.lifetime;
                stored->second.isPublic = entry.isPublic;
            }
        }
        result = ResultHit;
        request.state = State::Validated;
        found = request.entry;
    } else if (StorableStatus(response.status)) {
        FillEntry(request, entry, response);
        entry.body.clear();
        entry.pending = true;
        entry.key = KeyFor(request);
        request.state = State::Replacing;
    } else {
        RemoveStored(request);
        request.state = State::Dropped;
    }
    request.lastResult = result;
    return result;
}

int MakeResponse(const HttpCacheResponseHeader* header, ResponseData& response) {
    response.status = header->statusCode;
    if (const int status = ConvertHeaders(header->headerCount, header->headers, response.headers); status != 0) return status;
    response.time = Now();
    return 0;
}

}

extern "C" {

int APS5_VABI sceHttpCacheClear(std::int32_t cacheId, std::int32_t userId, const char* tag) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheClearAll(std::int32_t cacheId, std::int32_t userId) {
    std::lock_guard lock(Mutex);
    if (FindCache(cacheId) == nullptr) return ErrorNotFound;
    if (SystemUser(userId) || !RegisteredUser(userId)) return 0;
    for (const auto& stored : Store) {
        if (std::get<0>(stored.first) == SharedUser && userId != SharedUser) {
            NotImplemented_nid_no_patch("sceHttpCacheClearAll for a user while entries kept for all users exist");
        }
    }
    for (auto it = Store.begin(); it != Store.end();) {
        it = std::get<0>(it->first) == userId ? Store.erase(it) : std::next(it);
    }
    return 0;
}

int APS5_VABI sceHttpCacheCompleteRequest(std::int32_t requestId) {
    std::lock_guard lock(Mutex);
    Request* request = FindRequest(requestId);
    if (request == nullptr) return ErrorNotFound;
    if (request->hasStore && request->entry && request->entry->pending) {
        Entry stored = *request->entry;
        stored.pending = false;
        stored.readOffset = 0;
        Store[stored.key] = std::move(stored);
        request->entry->pending = false;
    }
    return 0;
}

int APS5_VABI sceHttpCacheCreateRequest(std::int32_t cacheId, std::int32_t userId) {
    std::lock_guard lock(Mutex);
    Cache* cache = FindCache(cacheId);
    if (cache == nullptr) return ErrorNotFound;
    if (!SystemUser(userId) && !RegisteredUser(userId)) return ErrorInvalidUser;
    const std::uint32_t sequence = cache->nextRequest;
    cache->nextRequest = sequence + 1 < RequestCounterLimit ? sequence + 1 : 1;
    const auto requestId = static_cast<std::int32_t>(static_cast<std::uint32_t>(cacheId) << 24 | sequence);
    Request request;
    request.storeUser = userId;
    request.hasStore = !SystemUser(userId);
    cache->requests[requestId] = std::move(request);
    return requestId;
}

int APS5_VABI sceHttpCacheCreateRequestWithTag(std::int32_t cacheId, std::int32_t userId, const char* tag) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheDeleteRequest(std::int32_t requestId) {
    std::lock_guard lock(Mutex);
    Cache* cache = RequestCache(requestId);
    if (cache == nullptr) return ErrorNotFound;
    cache->requests.erase(requestId);
    return 0;
}

int APS5_VABI sceHttpCacheInit(std::size_t poolSize) {
    std::lock_guard lock(Mutex);
    if (!UserServiceStarted) {
        const int status = sceUserServiceInitialize(nullptr);
        if (status < 0 && status != UserServiceAlreadyInitialized) {
            if (status == UserServiceNoMemory) return ErrorNoMemory;
            if (status == UserServiceInvalidArgument) return ErrorInvalidValue;
            return ErrorUnknown;
        }
        UserServiceStarted = true;
    }
    if (Caches.size() >= MaxCaches) return ErrorTooManyCaches;
    (void)poolSize;
    std::int32_t cacheId = LastCacheId;
    do {
        cacheId = cacheId % static_cast<std::int32_t>(MaxCaches) + 1;
    } while (Caches.count(cacheId) != 0);
    LastCacheId = cacheId;
    Caches[cacheId] = Cache{};
    return cacheId;
}

int APS5_VABI sceHttpCacheReadData(std::int32_t requestId, std::size_t size, void* data) {
    if (data == nullptr && size != 0) return ErrorInvalidValue;
    std::lock_guard lock(Mutex);
    Request* request = FindRequest(requestId);
    if (request == nullptr) return ErrorNotFound;
    if (request->state != State::Hit && request->state != State::RevalidateWithStale && request->state != State::Validated) return 0;
    Entry& entry = *request->entry;
    if (entry.pending) return 0;
    const std::size_t remaining = entry.body.size() - entry.readOffset;
    const std::size_t count = size < remaining ? size : remaining;
    if (count != 0) std::memcpy(data, entry.body.data() + entry.readOffset, count);
    entry.readOffset += count;
    return static_cast<int>(count);
}

int APS5_VABI sceHttpCacheRetrieve(std::int32_t requestId, const HttpCacheRequest* request, HttpCacheResponse* response, HttpCacheRequest* nextRequest, std::int32_t* result) {
    if (request == nullptr || result == nullptr) return ErrorInvalidValue;
    std::lock_guard lock(Mutex);
    Request* state = FindRequest(requestId);
    if (state == nullptr) return ErrorNotFound;
    RequestData data;
    if (const int status = ConvertHeaders(request->headerCount, request->headers, data.headers); status != 0) return status;
    if (request->url == nullptr) NotImplemented_nid_no_patch("sceHttpCacheRetrieve with a null URL");
    data.method = request->method;
    data.url = request->url;
    std::shared_ptr<Entry> found;
    const RequestData* next = nullptr;
    *result = RetrieveFor(*state, std::move(data), found, next);
    if (*result == ResultHit) {
        if (response == nullptr) return ErrorInvalidValue;
        FillResponse(*state, *found, response);
        return 0;
    }
    if (*result != ResultRevalidate) return 0;
    if (nextRequest == nullptr) return ErrorInvalidValue;
    if (next == nullptr) NotImplemented_nid_no_patch("sceHttpCacheRetrieve revalidation without a next request");
    FillNextRequest(*state, *next, nextRequest);
    if (!StaleUsable(found.get())) return 0;
    if (response == nullptr) return ErrorInvalidValue;
    FillResponse(*state, *found, response);
    return 0;
}

int APS5_VABI sceHttpCacheRetrieveWithMemoryPool(std::int32_t requestId, const HttpCacheRequest* request, HttpCacheResponse* response, HttpCacheRequest* nextRequest, std::int32_t* result, void* pool, std::size_t* requiredSize, std::size_t poolSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheRevalidate(std::int32_t requestId, const HttpCacheResponseHeader* header, HttpCacheResponse* response, std::int32_t* result) {
    if (header == nullptr || result == nullptr) return ErrorInvalidValue;
    std::lock_guard lock(Mutex);
    Request* state = FindRequest(requestId);
    if (state == nullptr) return ErrorNotFound;
    ResponseData received;
    if (const int status = MakeResponse(header, received); status != 0) return status;
    std::shared_ptr<Entry> found;
    *result = Revalidate(*state, received, found);
    if (!found) return 0;
    if (response == nullptr) return ErrorInvalidValue;
    FillResponse(*state, *found, response);
    return 0;
}

int APS5_VABI sceHttpCacheRevalidateWithMemoryPool(std::int32_t requestId, const HttpCacheResponseHeader* header, HttpCacheResponse* response, std::int32_t* result, void* pool, std::size_t* requiredSize, std::size_t poolSize) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSetCacheSharing(std::uint8_t enabled) {
    std::lock_guard lock(Mutex);
    CacheSharing = enabled;
    return 0;
}

int APS5_VABI sceHttpCacheSetData(std::int32_t requestId, const void* data, std::size_t size) {
    if (data == nullptr) return ErrorInvalidValue;
    std::lock_guard lock(Mutex);
    Request* request = FindRequest(requestId);
    if (request == nullptr) return ErrorNotFound;
    if ((request->state == State::Miss || request->state == State::Replacing) && request->entry && request->entry->pending) {
        request->entry->body.append(static_cast<const char*>(data), size);
    }
    return 0;
}

int APS5_VABI sceHttpCacheSetQuota(std::int32_t cacheId, const HttpCacheQuota* quota) {
    {
        std::lock_guard lock(Mutex);
        if (FindCache(cacheId) == nullptr) return ErrorNotFound;
    }
    if (quota == nullptr) return ErrorInvalidValue;
    if (quota->count != 0) {
        if (quota->entries == nullptr) return ErrorInvalidValue;
        for (std::size_t i = 0; i < quota->count; ++i) {
            if (quota->entries[i].name == nullptr || quota->entries[i].name[0] == '\0') return ErrorInvalidValue;
        }
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSetResponseHeader(std::int32_t requestId, const HttpCacheResponseHeader* header) {
    if (header == nullptr) return ErrorInvalidValue;
    std::lock_guard lock(Mutex);
    Request* request = FindRequest(requestId);
    if (request == nullptr) return ErrorNotFound;
    ResponseData received;
    if (const int status = MakeResponse(header, received); status != 0) return status;
    if (request->state == State::Idle || !request->hasStore || !request->current) return 0;
    if (!StorableStatus(received.status) || !StorableHeaders(received.headers)) {
        RemoveStored(*request);
        return 0;
    }
    auto entry = std::make_shared<Entry>();
    FillEntry(*request, *entry, received);
    entry->pending = true;
    entry->key = KeyFor(*request);
    request->entry = std::move(entry);
    return 0;
}

int APS5_VABI sceHttpCacheSystemClearAll(const char* path) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSystemInit(const char* path) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSystemSendStatistics() {
    return 0;
}

int APS5_VABI sceHttpCacheSystemShutdown() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheSystemTerm() {
    return 0;
}

int APS5_VABI sceHttpCacheTerm(std::int32_t cacheId) {
    std::lock_guard lock(Mutex);
    if (Caches.erase(cacheId) == 0) return ErrorNotFound;
    if (Caches.empty() && UserServiceStarted) {
        sceUserServiceTerminate();
        UserServiceStarted = false;
    }
    return 0;
}


int APS5_VABI sceHttpCacheClearAllUserTags(std::int32_t cacheId, const std::int32_t* userIds, std::size_t count) {
    {
        std::lock_guard lock(Mutex);
        if (FindCache(cacheId) == nullptr) return ErrorNotFound;
    }
    if (count > 20 || (userIds == nullptr && count != 0)) return ErrorInvalidValue;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheClearCacheInfo() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheDebugSetUrl() {
    return 0;
}

int APS5_VABI sceHttpCacheRegisterCacheInfoUrl(const void* info) {
    if (info == nullptr) return ErrorInvalidValue;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceHttpCacheRemoveTag() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
