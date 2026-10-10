#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

extern "C" {
int APS5_VABI sceHttpUriParse(SceHttpUriElement*, const char*, void*, std::size_t*, std::size_t);
int APS5_VABI sceHttpUriMerge(char*, const char*, const char*, std::size_t*, std::size_t, std::uint32_t);
int APS5_VABI sceHttpSetInflateGZIPEnabled(int, int);
int APS5_VABI sceHttpUriBuild(char*, std::size_t*, std::size_t, const SceHttpUriElement*, std::uint32_t);
int APS5_VABI sceHttpUriEscape(char*, std::size_t*, std::size_t, const char*);
int APS5_VABI sceHttpUriUnescape(char*, std::size_t*, std::size_t, const char*);
int APS5_VABI sceHttpUriSweepPath(char*, const char*, std::size_t);
int APS5_VABI sceHttpCreateEpoll(int, HttpEpollHandle*);
int APS5_VABI sceHttpDestroyEpoll(int, HttpEpollHandle);
int APS5_VABI sceHttpWaitRequest(HttpEpollHandle, HttpNBEvent*, int, int);
int APS5_VABI sceHttpReadData(int, void*, std::size_t);
int APS5_VABI sceHttpCreateRequest2(int, const char*, const char*, std::uint64_t);
int APS5_VABI sceHttpsEnableOption(int, std::uint32_t);
int APS5_VABI sceHttpsLoadCert(int, int, void*, void*, void*);
int APS5_VABI sceHttpGetLastErrno(int, int*);
int APS5_VABI sceHttpSetResponseHeaderMaxSize(int, std::uint64_t);
int APS5_VABI sceHttpRedirectCacheFlush(int);
int APS5_VABI sceHttpsUnloadCert(int);
int APS5_VABI sceHttpsSetSslVersion(int, int);
int APS5_VABI sceHttpsGetSslError(int, int*, std::uint32_t*);
int APS5_VABI sceHttpSetRedirectCallback(int, HttpRedirectCallback, void*);
int APS5_VABI sceHttpSetCookieRecvCallback(int, HttpCookieRecvCallback, void*);
int APS5_VABI sceHttpSetAuthInfoCallback(int, HttpAuthInfoCallback, void*);
int APS5_VABI sceHttpParseStatusLine(const char*, std::size_t, std::int32_t*, std::int32_t*, std::int32_t*, const char**, std::size_t*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int outOfMemory = static_cast<int>(0x80431022);
constexpr int invalidValue = static_cast<int>(0x804311FE);
constexpr int invalidUrl = static_cast<int>(0x80433060);
constexpr int network = static_cast<int>(0x80431063);
constexpr int parseInvalidResponse = static_cast<int>(0x80432060);
constexpr int parseInvalidValue = static_cast<int>(0x804321FE);
constexpr std::uint32_t buildAll = 0xFF;
constexpr std::uint32_t buildPathAndQuery = 0x08 | 0x40;

constexpr const char* fullUrl = "https://user:secret@example.com:8443/a/./b/../c?x=1&y=2#top";
constexpr const char* fullUrlBuilt = "https://user:secret@example.com:8443/a/c?x=1&y=2#top";
constexpr std::size_t fullUrlPoolSize = 6 + 5 + 7 + 12 + 5 + 9 + 5;

constexpr const char* mergeBase = "http://foo.com/foo/index.html";
constexpr std::size_t baseMergeSize = 5 + 1 + 1 + 8 + 16 + 1 + 1 + 2;
constexpr std::size_t relativeMergeSize = baseMergeSize + 2 * (29 + 14);
constexpr std::size_t absoluteMergeSize = baseMergeSize + 2 * (29 + 20);

std::string_view Text(const char* value) {
    return value == nullptr ? std::string_view("<null>") : std::string_view(value);
}

struct ParsedUri {
    SceHttpUriElement element;
    char pool[256];
};

void Parse(ParsedUri& parsed, const char* url) {
    RequireEqual(sceHttpUriParse(&parsed.element, url, parsed.pool, nullptr, sizeof(parsed.pool)), 0, std::string("parse ") + url);
}

std::string Build(const SceHttpUriElement& element, std::uint32_t option) {
    char built[256];
    RequireEqual(sceHttpUriBuild(built, nullptr, sizeof(built), &element, option), 0, "build the URI with option " + std::to_string(option));
    return built;
}

struct MergeResult {
    int status;
    std::size_t required;
    char merged[512];
};

void Merge(MergeResult& result, const char* base, const char* relative) {
    std::memset(result.merged, 'Z', sizeof(result.merged));
    result.status = sceHttpUriMerge(result.merged, base, relative, &result.required, sizeof(result.merged), 0);
}

void RequireMerged(const char* base, const char* relative, const char* expected) {
    MergeResult result{};
    Merge(result, base, relative);
    const std::string where = std::string("merge \"") + relative + "\" into " + base;
    RequireEqual(result.status, 0, where);
    RequireEqual(std::string_view(result.merged), std::string_view(expected), where);
}

struct StatusLine {
    std::int32_t major = -1;
    std::int32_t minor = -1;
    std::int32_t code = -1;
    const char* phrase = nullptr;
    std::size_t phraseLength = 0;
};

int ParseStatus(StatusLine& status, const char* line, std::size_t length) {
    return sceHttpParseStatusLine(line, length, &status.major, &status.minor, &status.code, &status.phrase, &status.phraseLength);
}

class Epoll {
public:
    Epoll() {
        RequireEqual(sceHttpCreateEpoll(1, &handle), 0, "create an epoll");
        Require(handle != nullptr, "create an epoll returns a handle");
    }

    ~Epoll() {
        if (handle != nullptr) sceHttpDestroyEpoll(1, handle);
    }

    Epoll(const Epoll&) = delete;
    Epoll& operator=(const Epoll&) = delete;

    HttpEpollHandle Handle() const {
        return handle;
    }

    int Destroy() {
        const int result = sceHttpDestroyEpoll(1, handle);
        handle = nullptr;
        return result;
    }

private:
    HttpEpollHandle handle = nullptr;
};

int RecordAuthInfoCall(int, int, const char*, char*, char*, int, std::uint8_t**, std::uint64_t*, int*, void* userArg) {
    *static_cast<bool*>(userArg) = true;
    return 0;
}

const Case parseSizeQuery{"UriParse_SizeQuery_ReportsPoolSize", [] {
    std::size_t required = 0;
    RequireEqual(sceHttpUriParse(nullptr, fullUrl, nullptr, &required, 0), 0, "query the pool size");
    RequireEqual(required, fullUrlPoolSize, "pool size");
}};

const Case parseSmallPool{"UriParse_PoolTooSmall_FailsWithOutOfMemory", [] {
    ParsedUri parsed;
    RequireEqual(sceHttpUriParse(&parsed.element, fullUrl, parsed.pool, nullptr, fullUrlPoolSize - 1), outOfMemory, "parse into a pool one byte short");
}};

const Case parseFullUrl{"UriParse_FullUrl_SplitsEveryComponent", [] {
    ParsedUri parsed;
    RequireEqual(sceHttpUriParse(&parsed.element, fullUrl, parsed.pool, nullptr, fullUrlPoolSize), 0, "parse into an exact pool");
    RequireEqual(parsed.element.opaque, 0, "opaque flag");
    RequireEqual(Text(parsed.element.scheme), std::string_view("https"), "scheme");
    RequireEqual(Text(parsed.element.username), std::string_view("user"), "username");
    RequireEqual(Text(parsed.element.password), std::string_view("secret"), "password");
    RequireEqual(Text(parsed.element.hostname), std::string_view("example.com"), "hostname");
    RequireEqual(parsed.element.port, 8443, "port");
    RequireEqual(Text(parsed.element.path), std::string_view("/a/c"), "path");
    RequireEqual(Text(parsed.element.query), std::string_view("?x=1&y=2"), "query");
    RequireEqual(Text(parsed.element.fragment), std::string_view("#top"), "fragment");
}};

const Case parseDefaultPort{"UriParse_HttpWithoutPort_UsesPort80AndEmptyFields", [] {
    ParsedUri parsed;
    Parse(parsed, "HTTP://Example.com/");
    RequireEqual(parsed.element.port, 80, "port");
    RequireEqual(Text(parsed.element.username), std::string_view(""), "username");
    RequireEqual(Text(parsed.element.query), std::string_view(""), "query");
}};

const Case parseIpv6{"UriParse_Ipv6Host_StripsBrackets", [] {
    ParsedUri parsed;
    Parse(parsed, "http://[::1]:8080/index.html");
    RequireEqual(Text(parsed.element.hostname), std::string_view("::1"), "hostname");
    RequireEqual(parsed.element.port, 8080, "port");
    RequireEqual(Text(parsed.element.path), std::string_view("/index.html"), "path");
}};

const Case parseOpaque{"UriParse_MailtoUri_IsOpaqueWithUserAndHost", [] {
    ParsedUri parsed;
    Parse(parsed, "mailto:someone@example.com");
    Require(parsed.element.opaque != 0, "mailto URI must be opaque");
    RequireEqual(parsed.element.port, 0, "port");
    RequireEqual(Text(parsed.element.username), std::string_view("someone"), "username");
    RequireEqual(Text(parsed.element.hostname), std::string_view("example.com"), "hostname");
}};

const Case parsePathOnly{"UriParse_PathOnly_HasEmptySchemeAndHost", [] {
    ParsedUri parsed;
    Parse(parsed, "/path/only?q");
    RequireEqual(Text(parsed.element.scheme), std::string_view(""), "scheme");
    RequireEqual(Text(parsed.element.hostname), std::string_view(""), "hostname");
    RequireEqual(Text(parsed.element.path), std::string_view("/path/only"), "path");
}};

const Case parseNullUrl{"UriParse_NullUrl_FailsWithInvalidUrl", [] {
    ParsedUri parsed;
    RequireEqual(sceHttpUriParse(&parsed.element, nullptr, parsed.pool, nullptr, sizeof(parsed.pool)), invalidUrl, "parse a null URL");
}};

const Case parseNoOutput{"UriParse_NoOutputAndNoSizeQuery_FailsWithInvalidValue", [] {
    RequireEqual(sceHttpUriParse(nullptr, fullUrl, nullptr, nullptr, 0), invalidValue, "parse without any output");
}};

const Case parseMalformed{"UriParse_MalformedUrl_FailsWithInvalidUrl", [] {
    constexpr const char* urls[] = {"http://host:65536/", "http://host:80a/", "http://[::1/", "http://bad host/"};
    for (const char* url : urls) {
        std::size_t required = 0;
        RequireEqual(sceHttpUriParse(nullptr, url, nullptr, &required, 0), invalidUrl, std::string("parse ") + url);
    }
}};

const Case buildSizeQuery{"UriBuild_SizeQuery_ReportsLengthWithTerminator", [] {
    ParsedUri parsed;
    Parse(parsed, fullUrl);
    std::size_t required = 0;
    RequireEqual(sceHttpUriBuild(nullptr, &required, 0, &parsed.element, buildAll), 0, "query the build size");
    RequireEqual(required, std::strlen(fullUrlBuilt) + 1, "build size");
}};

const Case buildSmallBuffer{"UriBuild_BufferTooSmall_FailsWithOutOfMemory", [] {
    ParsedUri parsed;
    Parse(parsed, fullUrl);
    char built[256];
    RequireEqual(sceHttpUriBuild(built, nullptr, std::strlen(fullUrlBuilt), &parsed.element, buildAll), outOfMemory, "build into a buffer one byte short");
}};

const Case buildRoundTrips{"UriBuild_AllComponents_RebuildsParsedUri", [] {
    struct Row {
        const char* url;
        const char* expected;
    };
    constexpr Row rows[] = {
        {fullUrl, fullUrlBuilt},
        {"HTTP://Example.com/", "HTTP://Example.com/"},
        {"http://[::1]:8080/index.html", "http://[::1]:8080/index.html"},
        {"mailto:someone@example.com", "mailto:someone@example.com"},
    };
    for (const Row& row : rows) {
        ParsedUri parsed;
        Parse(parsed, row.url);
        RequireEqual(Build(parsed.element, buildAll), std::string(row.expected), std::string("rebuild ") + row.url);
    }
}};

const Case buildPathQuery{"UriBuild_PathAndQueryOption_BuildsOnlyThoseComponents", [] {
    ParsedUri parsed;
    Parse(parsed, fullUrl);
    RequireEqual(Build(parsed.element, buildPathAndQuery), std::string("/a/c?x=1&y=2"), "build path and query");
}};

const Case buildNullElement{"UriBuild_NullElement_FailsWithInvalidUrl", [] {
    char built[256];
    RequireEqual(sceHttpUriBuild(built, nullptr, sizeof(built), nullptr, buildAll), invalidUrl, "build without an element");
}};

const Case buildNoOutput{"UriBuild_NoOutputAndNoSizeQuery_FailsWithInvalidValue", [] {
    ParsedUri parsed;
    Parse(parsed, fullUrl);
    RequireEqual(sceHttpUriBuild(nullptr, nullptr, 0, &parsed.element, buildAll), invalidValue, "build without any output");
}};

const Case escapeSizeQuery{"UriEscape_SizeQuery_ReportsEscapedLength", [] {
    std::size_t required = 0;
    RequireEqual(sceHttpUriEscape(nullptr, &required, 0, "a b/~\xC3\xA9"), 0, "query the escaped size");
    RequireEqual(required, std::strlen("a%20b%2F~%C3%A9") + 1, "escaped size");
}};

const Case escapeSmallBuffer{"UriEscape_BufferTooSmall_FailsWithOutOfMemory", [] {
    char escaped[64];
    RequireEqual(sceHttpUriEscape(escaped, nullptr, std::strlen("a%20b%2F~%C3%A9"), "a b/~\xC3\xA9"), outOfMemory, "escape into a buffer one byte short");
}};

const Case escapeText{"UriEscape_ReservedAndNonAsciiBytes_ArePercentEncoded", [] {
    char escaped[64];
    RequireEqual(sceHttpUriEscape(escaped, nullptr, sizeof(escaped), "a b/~\xC3\xA9"), 0, "escape the text");
    RequireEqual(std::string_view(escaped), std::string_view("a%20b%2F~%C3%A9"), "escaped text");
}};

const Case escapeNull{"UriEscape_NullInput_FailsWithInvalidValue", [] {
    char escaped[64];
    RequireEqual(sceHttpUriEscape(escaped, nullptr, sizeof(escaped), nullptr), invalidValue, "escape a null string");
}};

struct UnescapeRow {
    const char* input;
    const char* expected;
};

constexpr UnescapeRow unescapeRows[] = {
    {"", ""}, {"plain+text", "plain+text"}, {"a%20b%2F~%c3%a9", "a b/~\xC3\xA9"},
    {"%41%4a%4F%ff", "AJO\xFF"}, {"%2520", "%20"},
    {"%", "%"}, {"%1", "%1"}, {"%1g%gg%+1%-1", "%1g%gg%+1%-1"},
};

const Case unescapeSizeQuery{"UriUnescape_SizeQuery_ReportsDecodedLength", [] {
    for (const UnescapeRow& row : unescapeRows) {
        const std::size_t size = std::strlen(row.expected) + 1;
        std::size_t required = 0;
        RequireEqual(sceHttpUriUnescape(nullptr, &required, 0, row.input), 0, std::string("query the size of \"") + row.input + "\"");
        RequireEqual(required, size, std::string("decoded size of \"") + row.input + "\"");
    }
}};

const Case unescapeSmallBuffer{"UriUnescape_BufferTooSmall_FailsWithoutWriting", [] {
    for (const UnescapeRow& row : unescapeRows) {
        const std::size_t size = std::strlen(row.expected) + 1;
        const std::string where = std::string("unescape \"") + row.input + "\" into a buffer one byte short";
        char unescaped[64];
        std::memset(unescaped, 'Z', sizeof(unescaped));
        std::size_t required = 0;
        RequireEqual(sceHttpUriUnescape(unescaped, &required, size - 1, row.input), outOfMemory, where);
        RequireEqual(required, size, "reported size, " + where);
        for (std::size_t i = 0; i < sizeof(unescaped); ++i) RequireEqual(unescaped[i], 'Z', "byte " + std::to_string(i) + " untouched, " + where);
    }
}};

const Case unescapeExactBuffer{"UriUnescape_ExactBuffer_WritesDecodedTextOnly", [] {
    for (const UnescapeRow& row : unescapeRows) {
        const std::size_t size = std::strlen(row.expected) + 1;
        const std::string where = std::string("unescape \"") + row.input + "\"";
        char unescaped[64];
        std::memset(unescaped, 'Z', sizeof(unescaped));
        RequireEqual(sceHttpUriUnescape(unescaped, nullptr, size, row.input), 0, where);
        RequireEqual(std::string_view(unescaped), std::string_view(row.expected), "decoded text, " + where);
        RequireEqual(unescaped[size], 'Z', "byte after the terminator untouched, " + where);
    }
}};

const Case unescapeEmbeddedNul{"UriUnescape_EncodedNul_KeepsBytesAfterIt", [] {
    char unescaped[64];
    std::size_t required = 0;
    RequireEqual(sceHttpUriUnescape(unescaped, &required, sizeof(unescaped), "a%00b"), 0, "unescape a%00b");
    RequireEqual(required, std::size_t{4}, "decoded size");
    Require(std::memcmp(unescaped, "a\0b", 4) == 0, "decoded bytes must be a, NUL, b, NUL");
}};

const Case unescapeInPlace{"UriUnescape_SameBufferAsInput_DecodesInPlace", [] {
    char unescaped[64];
    std::strcpy(unescaped, "%41%2f%2520");
    std::size_t required = 0;
    RequireEqual(sceHttpUriUnescape(unescaped, &required, sizeof(unescaped), unescaped), 0, "unescape in place");
    RequireEqual(std::string_view(unescaped), std::string_view("A/%20"), "decoded text");
    RequireEqual(required, std::size_t{6}, "decoded size");
}};

const Case unescapeNoOutput{"UriUnescape_NoOutputAndNoSizeQuery_Succeeds", [] {
    RequireEqual(sceHttpUriUnescape(nullptr, nullptr, 0, "valid"), 0, "unescape without any output");
}};

const Case unescapeNull{"UriUnescape_NullInput_FailsWithoutWriting", [] {
    char unescaped[64];
    std::strcpy(unescaped, "A/%20");
    std::size_t required = 123;
    RequireEqual(sceHttpUriUnescape(unescaped, &required, sizeof(unescaped), nullptr), invalidValue, "unescape a null string");
    RequireEqual(required, std::size_t{123}, "reported size untouched");
    RequireEqual(std::string_view(unescaped), std::string_view("A/%20"), "buffer untouched");
}};

const Case escapeRoundTrip{"UriEscapeUnescape_EveryByte_RoundTrips", [] {
    char bytes[256];
    char encoded[766];
    char decoded[256];
    for (std::size_t i = 1; i < 256; ++i) bytes[i - 1] = static_cast<char>(i);
    bytes[255] = '\0';
    RequireEqual(sceHttpUriEscape(encoded, nullptr, sizeof(encoded), bytes), 0, "escape bytes 1 to 255");
    std::size_t required = 0;
    RequireEqual(sceHttpUriUnescape(decoded, &required, sizeof(decoded), encoded), 0, "unescape the escaped bytes");
    RequireEqual(required, sizeof(bytes), "decoded size");
    Require(std::memcmp(bytes, decoded, sizeof(bytes)) == 0, "decoded bytes must equal the original bytes");
}};

const Case mergeRelative{"UriMerge_RelativeReference_ResolvesAgainstBase", [] {
    struct Row {
        const char* base;
        const char* relative;
        const char* expected;
    };
    constexpr Row rows[] = {
        {mergeBase, "./default.html", "http://foo.com/foo/./default.html"},
        {mergeBase, "../sibling.html", "http://foo.com/foo/../sibling.html"},
        {mergeBase, "", "http://foo.com/foo/"},
        {mergeBase, "/root.html", "http://foo.com/root.html"},
        {mergeBase, "a?q=1#f", "http://foo.com/foo/a?q=1#f"},
        {"https://u:p@foo.com:8443/a/b?x=1#top", "c", "https://u:p@foo.com:8443/a/c"},
        {"http://foo.com", "x", "http://foo.com/x"},
        {"http://foo.com:80/", "x", "http://foo.com/x"},
        {"http://foo.com/a/b", "mailto:x", "http://foo.com/a/mailto:x"},
        {mergeBase, "a%20b.html", "http://foo.com/foo/a%20b.html"},
        {mergeBase, "~user/x", "http://foo.com/foo/~user/x"},
        {mergeBase, "file(1).png", "http://foo.com/foo/file(1).png"},
        {mergeBase, "a+b=c;d!e", "http://foo.com/foo/a+b=c;d!e"},
    };
    for (const Row& row : rows) RequireMerged(row.base, row.relative, row.expected);
}};

const Case mergeRelativeRequired{"UriMerge_RelativeReference_ReportsWorstCaseSize", [] {
    MergeResult result{};
    Merge(result, mergeBase, "./default.html");
    RequireEqual(result.status, 0, "merge ./default.html");
    RequireEqual(result.required, relativeMergeSize, "required size");
}};

const Case mergeAbsolute{"UriMerge_AbsoluteReference_ReplacesBaseAndZeroFillsWorkArea", [] {
    MergeResult result{};
    Merge(result, mergeBase, "http://bar.com/other");
    RequireEqual(result.status, 0, "merge an absolute URL");
    RequireEqual(std::string_view(result.merged), std::string_view("http://bar.com/other"), "merged URL");
    RequireEqual(result.required, std::size_t{21}, "required size");
    for (std::size_t i = 21; i < absoluteMergeSize; ++i) RequireEqual(result.merged[i], '\0', "work area byte " + std::to_string(i));
    RequireEqual(result.merged[absoluteMergeSize], 'Z', "byte after the work area untouched");
}};

const Case mergeNetworkPath{"UriMerge_NetworkPathReference_IsKeptAsIs", [] {
    MergeResult result{};
    Merge(result, mergeBase, "//bar.com/x");
    RequireEqual(result.status, 0, "merge //bar.com/x");
    RequireEqual(std::string_view(result.merged), std::string_view("//bar.com/x"), "merged URL");
    RequireEqual(result.required, std::size_t{12}, "required size");
}};

const Case mergeSizeQuery{"UriMerge_SizeQuery_ReportsWorkAreaSize", [] {
    std::size_t required = 0;
    RequireEqual(sceHttpUriMerge(nullptr, mergeBase, "./default.html", &required, 0, 0), 0, "query the size of a relative merge");
    RequireEqual(required, relativeMergeSize, "relative merge size");
    RequireEqual(sceHttpUriMerge(nullptr, mergeBase, "http://bar.com/other", &required, 0, 0), 0, "query the size of an absolute merge");
    RequireEqual(required, absoluteMergeSize, "absolute merge size");
}};

const Case mergeNoOutput{"UriMerge_NoOutputAndNoSizeQuery_Succeeds", [] {
    RequireEqual(sceHttpUriMerge(nullptr, mergeBase, "x", nullptr, 0, 0), 0, "merge without any output");
}};

const Case mergeSmallBuffer{"UriMerge_BufferTooSmall_FailsWithoutWriting", [] {
    struct Row {
        const char* relative;
        std::size_t size;
    };
    constexpr Row rows[] = {{"./default.html", relativeMergeSize}, {"http://bar.com/other", absoluteMergeSize}};
    for (const Row& row : rows) {
        const std::string where = std::string("merge \"") + row.relative + "\" into a buffer one byte short";
        char merged[512];
        std::memset(merged, 'Z', sizeof(merged));
        std::size_t required = 0;
        RequireEqual(sceHttpUriMerge(merged, mergeBase, row.relative, &required, row.size - 1, 0), outOfMemory, where);
        RequireEqual(required, row.size, "reported size, " + where);
        RequireEqual(merged[0], 'Z', "first byte untouched, " + where);
    }
}};

const Case mergeExactBuffer{"UriMerge_ExactBuffer_Succeeds", [] {
    char merged[512];
    RequireEqual(sceHttpUriMerge(merged, mergeBase, "./default.html", nullptr, relativeMergeSize, 0), 0, "merge into an exact buffer");
    RequireEqual(std::string_view(merged), std::string_view("http://foo.com/foo/./default.html"), "merged URL");
}};

const Case mergeInvalidArguments{"UriMerge_InvalidArguments_FailWithoutReportingSize", [] {
    struct Row {
        const char* name;
        bool output;
        const char* base;
        const char* relative;
        std::uint32_t option;
        int expected;
    };
    constexpr Row rows[] = {
        {"null base", true, nullptr, "./x", 0, invalidValue},
        {"null relative", true, mergeBase, nullptr, 0, invalidValue},
        {"option 1", true, mergeBase, "./x", 1, invalidValue},
        {"everything null with option 1", false, nullptr, nullptr, 1, invalidValue},
        {"base with a space in the host", true, "http://bad host/", "./x", 0, invalidUrl},
        {"relative with a space in the host", true, mergeBase, "http://bad host/", 0, invalidUrl},
    };
    for (const Row& row : rows) {
        char merged[512];
        std::size_t required = 123;
        RequireEqual(sceHttpUriMerge(row.output ? merged : nullptr, row.base, row.relative, &required, row.output ? sizeof(merged) : 0, row.option),
                     row.expected, std::string("merge with ") + row.name);
        RequireEqual(required, std::size_t{123}, std::string("reported size untouched, merge with ") + row.name);
    }
}};

const Case createEpollNull{"CreateEpoll_NullHandle_FailsWithInvalidValue", [] {
    RequireEqual(sceHttpCreateEpoll(1, nullptr), invalidValue, "create an epoll without an output");
}};

const Case createEpoll{"CreateEpoll_ValidOutput_ReturnsHandle", [] {
    const Epoll epoll;
    Require(epoll.Handle() != nullptr, "epoll handle must not be null");
}};

const Case waitRequestTimeouts{"WaitRequest_NoRequests_ReturnsNoEvents", [] {
    const Epoll epoll;
    HttpNBEvent events[2]{};
    RequireEqual(sceHttpWaitRequest(epoll.Handle(), events, 2, 0), 0, "wait with a zero timeout");
    RequireEqual(sceHttpWaitRequest(epoll.Handle(), events, 2, 1000), 0, "wait with a 1000 microsecond timeout");
}};

const Case waitRequestInvalid{"WaitRequest_InvalidArguments_FailWithInvalidValue", [] {
    const Epoll epoll;
    HttpNBEvent events[2]{};
    RequireEqual(sceHttpWaitRequest(epoll.Handle(), nullptr, 2, 0), invalidValue, "wait without an event array");
    RequireEqual(sceHttpWaitRequest(epoll.Handle(), events, 0, 0), invalidValue, "wait for zero events");
    RequireEqual(sceHttpWaitRequest(nullptr, events, 2, 0), invalidValue, "wait on a null epoll");
}};

const Case destroyEpoll{"DestroyEpoll_CreatedHandle_Succeeds", [] {
    Epoll epoll;
    RequireEqual(epoll.Destroy(), 0, "destroy the epoll");
}};

const Case readData{"ReadData_AnyRequest_FailsWithNetwork", [] {
    char data[16];
    RequireEqual(sceHttpReadData(1, data, sizeof(data)), network, "read data of request 1");
}};

const Case createRequest{"CreateRequest2_AnyConnection_ReturnsPositiveId", [] {
    const int request = sceHttpCreateRequest2(1, "GET", "/", 0);
    Require(request > 0, "request id must be positive, got " + std::to_string(request));
}};

const Case inflateValid{"SetInflateGZIPEnabled_ZeroOrOne_Succeeds", [] {
    RequireEqual(sceHttpSetInflateGZIPEnabled(1, 0), 0, "disable gzip inflation");
    RequireEqual(sceHttpSetInflateGZIPEnabled(1, 1), 0, "enable gzip inflation");
}};

const Case inflateInvalid{"SetInflateGZIPEnabled_OtherValues_FailWithInvalidValue", [] {
    RequireEqual(sceHttpSetInflateGZIPEnabled(1, 2), invalidValue, "enable value 2");
    RequireEqual(sceHttpSetInflateGZIPEnabled(1, -1), invalidValue, "enable value -1");
}};

const Case acceptedSettings{"RequestSettings_ValidArguments_Succeed", [] {
    struct Row {
        const char* name;
        int (*call)();
    };
    constexpr Row rows[] = {
        {"sceHttpsEnableOption", [] { return sceHttpsEnableOption(1, 0); }},
        {"sceHttpsLoadCert", [] { return sceHttpsLoadCert(1, 0, nullptr, nullptr, nullptr); }},
        {"sceHttpsUnloadCert", [] { return sceHttpsUnloadCert(1); }},
        {"sceHttpSetResponseHeaderMaxSize", [] { return sceHttpSetResponseHeaderMaxSize(1, 8192); }},
        {"sceHttpRedirectCacheFlush", [] { return sceHttpRedirectCacheFlush(1); }},
        {"sceHttpsSetSslVersion", [] { return sceHttpsSetSslVersion(1, 0); }},
        {"sceHttpSetRedirectCallback", [] { return sceHttpSetRedirectCallback(1, nullptr, nullptr); }},
        {"sceHttpSetCookieRecvCallback", [] { return sceHttpSetCookieRecvCallback(1, nullptr, nullptr); }},
    };
    for (const Row& row : rows) RequireEqual(row.call(), 0, row.name);
}};

const Case authInfoCallback{"SetAuthInfoCallback_ThenReadData_DoesNotInvokeCallback", [] {
    bool invoked = false;
    RequireEqual(sceHttpSetAuthInfoCallback(1, RecordAuthInfoCall, &invoked), 0, "set the auth info callback");
    char data[16];
    RequireEqual(sceHttpReadData(1, data, sizeof(data)), network, "read data after setting the callback");
    Require(!invoked, "the auth info callback must not be invoked");
}};

const Case lastErrno{"GetLastErrno_ValidOutput_ReportsZero", [] {
    int httpErrno = -1;
    RequireEqual(sceHttpGetLastErrno(1, &httpErrno), 0, "get the last errno");
    RequireEqual(httpErrno, 0, "last errno");
}};

const Case lastErrnoNull{"GetLastErrno_NullOutput_FailsWithInvalidValue", [] {
    RequireEqual(sceHttpGetLastErrno(1, nullptr), invalidValue, "get the last errno without an output");
}};

const Case sslError{"GetSslError_ValidOutputs_ReportZero", [] {
    int error = -1;
    std::uint32_t detail = 0xFFFFFFFFu;
    RequireEqual(sceHttpsGetSslError(1, &error, &detail), 0, "get the SSL error");
    RequireEqual(error, 0, "SSL error");
    RequireEqual(detail, 0u, "SSL error detail");
}};

const Case sslErrorNull{"GetSslError_NullOutput_FailsWithInvalidValue", [] {
    int error = -1;
    std::uint32_t detail = 0xFFFFFFFFu;
    RequireEqual(sceHttpsGetSslError(1, nullptr, &detail), invalidValue, "get the SSL error without an error output");
    RequireEqual(sceHttpsGetSslError(1, &error, nullptr), invalidValue, "get the SSL error without a detail output");
}};

const Case statusLineOk{"ParseStatusLine_CrLfLine_ParsesVersionCodeAndPhrase", [] {
    const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    StatusLine status;
    RequireEqual(ParseStatus(status, response, std::strlen(response)), 17, "consumed bytes");
    RequireEqual(status.major, 1, "major version");
    RequireEqual(status.minor, 1, "minor version");
    RequireEqual(status.code, 200, "status code");
    Require(status.phrase == response + 12, "phrase must start after the status code");
    RequireEqual(status.phraseLength, std::size_t{3}, "phrase length");
    Require(std::strncmp(status.phrase, " OK", 3) == 0, "phrase must be \" OK\"");
}};

const Case statusLineLf{"ParseStatusLine_LfLineWithMultiDigitVersion_ParsesFields", [] {
    const char* response = "HTTP/10.25 404 Not Found\n";
    StatusLine status;
    RequireEqual(ParseStatus(status, response, std::strlen(response)), 25, "consumed bytes");
    RequireEqual(status.major, 10, "major version");
    RequireEqual(status.minor, 25, "minor version");
    RequireEqual(status.code, 404, "status code");
    Require(status.phrase == response + 14, "phrase must start after the status code");
    RequireEqual(status.phraseLength, std::size_t{10}, "phrase length");
}};

const Case statusLineNoPhrase{"ParseStatusLine_NoPhrase_ReportsEmptyPhrase", [] {
    const char* response = "HTTP/2.0 204\r\n";
    StatusLine status;
    RequireEqual(ParseStatus(status, response, std::strlen(response)), 14, "consumed bytes");
    RequireEqual(status.major, 2, "major version");
    RequireEqual(status.minor, 0, "minor version");
    RequireEqual(status.code, 204, "status code");
    Require(status.phrase == response + 12, "phrase must start after the status code");
    RequireEqual(status.phraseLength, std::size_t{0}, "phrase length");
}};

const Case statusLineNull{"ParseStatusLine_NullLine_FailsWithInvalidResponse", [] {
    StatusLine status;
    RequireEqual(ParseStatus(status, nullptr, 17), parseInvalidResponse, "parse a null line");
    Require(status.phrase == nullptr, "phrase untouched");
    RequireEqual(status.phraseLength, std::size_t{0}, "phrase length untouched");
}};

const Case statusLineNullOutputs{"ParseStatusLine_NullOutput_FailsWithInvalidValue", [] {
    const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    StatusLine status;
    RequireEqual(sceHttpParseStatusLine(response, 17, nullptr, &status.minor, &status.code, &status.phrase, &status.phraseLength), parseInvalidValue,
                 "parse without a major output");
    RequireEqual(sceHttpParseStatusLine(response, 17, &status.major, &status.minor, &status.code, &status.phrase, nullptr), parseInvalidValue,
                 "parse without a phrase length output");
    Require(status.phrase == nullptr, "phrase untouched");
    RequireEqual(status.phraseLength, std::size_t{0}, "phrase length untouched");
}};

const Case statusLineBadProtocol{"ParseStatusLine_WrongProtocol_FailsAndZeroesVersion", [] {
    StatusLine status;
    RequireEqual(ParseStatus(status, "HTTX/1.1 200 OK\n", 16), parseInvalidResponse, "parse an HTTX line");
    RequireEqual(status.major, 0, "major version");
    RequireEqual(status.minor, 0, "minor version");
    Require(status.phrase == nullptr, "phrase untouched");
    RequireEqual(status.phraseLength, std::size_t{0}, "phrase length untouched");
}};

const Case statusLineMalformed{"ParseStatusLine_MalformedLine_FailsWithInvalidResponse", [] {
    struct Row {
        const char* line;
        std::size_t length;
    };
    const char* response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    const Row rows[] = {
        {"HTTP/1.1", 7}, {"HTTP/123", 8}, {"HTTP/x.1 200 OK\n", 16}, {"HTTP/1 200 OK\n", 14},
        {"HTTP/1. 200 OK\n", 15}, {"HTTP/1.1/200 OK\n", 16}, {"HTTP/1.1 2x0 OK\n", 16}, {"HTTP/1.1 20", 11},
        {response, 15}, {response, 16},
    };
    for (const Row& row : rows) {
        const std::string where = "parse the first " + std::to_string(row.length) + " bytes of \"" + std::string(row.line, std::strcspn(row.line, "\r\n")) + "\"";
        StatusLine status;
        RequireEqual(ParseStatus(status, row.line, row.length), parseInvalidResponse, where);
        Require(status.phrase == nullptr, "phrase untouched, " + where);
        RequireEqual(status.phraseLength, std::size_t{0}, "phrase length untouched, " + where);
    }
}};

const Case sweepNothing{"UriSweepPath_NullBuffersWithZeroSize_Succeeds", [] {
    RequireEqual(sceHttpUriSweepPath(nullptr, nullptr, 0), 0, "sweep nothing");
}};

const Case sweepNullBuffer{"UriSweepPath_NullBuffer_FailsWithInvalidValue", [] {
    char swept[16];
    RequireEqual(sceHttpUriSweepPath(nullptr, "/foo", 5), invalidValue, "sweep into a null buffer");
    RequireEqual(sceHttpUriSweepPath(swept, nullptr, 5), invalidValue, "sweep a null path");
}};

const Case sweepPaths{"UriSweepPath_DotSegments_AreRemovedFromAbsolutePaths", [] {
    struct Row {
        const char* path;
        const char* expected;
    };
    constexpr Row rows[] = {
        {"foo/../bar", "foo/../bar"},
        {"/foo/../bar", "/bar"},
        {"/foo/./bar", "/foo/bar"},
        {"/foo/.", "/foo/."},
        {"/foo/..", "/foo/.."},
        {"/foo/bar/../foo/././../../../test/index.html", "/test/index.html"},
        {"/", "/"},
        {"", ""},
        {"/a/b/c/../../d/", "/a/d/"},
        {"/../a", "/a"},
        {"/a/..b/.c", "/a/..b/.c"},
    };
    for (const Row& row : rows) {
        char swept[128];
        std::memset(swept, 'x', sizeof(swept));
        const std::string where = std::string("sweep \"") + row.path + "\"";
        RequireEqual(sceHttpUriSweepPath(swept, row.path, std::strlen(row.path) + 1), 0, where);
        RequireEqual(std::string_view(swept), std::string_view(row.expected), where);
    }
}};

const Case sweepTruncated{"UriSweepPath_ShortSize_SweepsOnlyThePrefix", [] {
    struct Row {
        const char* path;
        std::size_t size;
        const char* expected;
    };
    constexpr Row rows[] = {
        {"/a/b/../c", 6, "/a/b/"},
        {"/ab/c", 3, "/a"},
        {"/a/./b", 5, "/a/."},
        {"/a/b/../c", 8, "/a/b/.."},
    };
    for (const Row& row : rows) {
        char swept[16];
        const std::string where = std::string("sweep the first ") + std::to_string(row.size) + " bytes of \"" + row.path + "\"";
        RequireEqual(sceHttpUriSweepPath(swept, row.path, row.size), 0, where);
        RequireEqual(std::string_view(swept), std::string_view(row.expected), where);
    }
}};

const Case sweepSingleByte{"UriSweepPath_SizeOne_WritesOnlyTerminator", [] {
    char swept[2] = {'x', 'x'};
    RequireEqual(sceHttpUriSweepPath(swept, "/", 1), 0, "sweep one byte of \"/\"");
    RequireEqual(swept[0], '\0', "first byte");
    RequireEqual(swept[1], 'x', "second byte untouched");
}};

} // namespace
