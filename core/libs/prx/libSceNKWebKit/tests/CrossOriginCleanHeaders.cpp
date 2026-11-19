#include "prx/libSceNKWebKit/include/CrossOriginRequestExports.hpp"
#include "prx/libSceNKWebKit/include/HttpHeaderMapExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using NKWebKit::HttpAbi::CommonHeader;
using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::HttpAbi::UncommonHeader;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "cleanHTTPRequestHeadersForAccessControl check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static constexpr HTTPHeaderName allHeaders[] = {
    HTTPHeaderName::ContentType,
    HTTPHeaderName::Referer,
    HTTPHeaderName::Origin,
    HTTPHeaderName::UserAgent,
    HTTPHeaderName::AcceptEncoding,
    HTTPHeaderName::CacheControl,
    HTTPHeaderName::SecFetchDest,
    HTTPHeaderName::SecFetchMode,
    HTTPHeaderName::SecFetchSite,
    HTTPHeaderName::Accept,
};

struct Fixture {
    std::string contentType;
    std::string other = "value";
    StringImpl contentTypeImpl;
    StringImpl otherImpl;
    std::vector<CommonHeader> entries;
    ResourceRequestBase request{};

    explicit Fixture(std::string type) : contentType(std::move(type)) {
        contentTypeImpl = StringImpl{1000, static_cast<unsigned>(contentType.size()), contentType.data(), 4};
        otherImpl = StringImpl{1000, static_cast<unsigned>(other.size()), other.data(), 4};
        for (auto name : allHeaders)
            entries.push_back(CommonHeader{name, String(name == HTTPHeaderName::ContentType ? &contentTypeImpl : &otherImpl)});
        request.resourceRequestUpdated = true;
        request.platformRequestUpdated = true;
        Headers() = {{entries.data(), static_cast<unsigned>(entries.size()), static_cast<unsigned>(entries.size())}, {nullptr, 0, 0}};
    }

    ~Fixture() {
        for (auto& entry : entries) entry.value.impl = nullptr;
    }

    HTTPHeaderMap& Headers() {
        return *reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
    }

    bool Has(HTTPHeaderName name) {
        return _ZNK7WebCore13HTTPHeaderMap8containsENS_14HTTPHeaderNameE(&Headers(), name);
    }

    void Clean(std::uint8_t keep) {
        _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(&request, keep);
    }
};

static void TestRemovesEverythingByDefault() {
    Fixture fixture("application/json");
    fixture.Clean(0);
    for (auto name : allHeaders) Require(fixture.Has(name) == (name == HTTPHeaderName::Accept));
    Require(fixture.Headers().common.size == 1);
    Require(!fixture.request.platformRequestUpdated);
}

static void TestSafeContentTypeStays() {
    for (const char* type : {"text/plain;charset=UTF-8", "application/x-www-form-urlencoded", "Multipart/Form-Data; boundary=x"}) {
        Fixture fixture(type);
        fixture.Clean(0);
        Require(fixture.Has(HTTPHeaderName::ContentType));
        Require(!fixture.Has(HTTPHeaderName::Referer));
        Require(fixture.Headers().common.size == 2);
    }
}

static void TestUnsafeContentTypesGo() {
    std::string longType = "text/plain;a=" + std::string(116, 'b');
    Require(longType.size() == 129);
    for (const std::string& type : {std::string("text/html"), std::string("text/plain;a=\"b\""), std::string("text/plain/x"), longType}) {
        Fixture fixture(type);
        fixture.Clean(0);
        Require(!fixture.Has(HTTPHeaderName::ContentType));
    }
    std::string limit = "text/plain;a=" + std::string(115, 'b');
    Fixture fixture(limit);
    fixture.Clean(0);
    Require(fixture.Has(HTTPHeaderName::ContentType));
}

static void TestKeepFlags() {
    Fixture fixture("application/json");
    fixture.Clean(0x3f);
    for (auto name : allHeaders) {
        bool secFetch = name == HTTPHeaderName::SecFetchDest || name == HTTPHeaderName::SecFetchMode || name == HTTPHeaderName::SecFetchSite;
        Require(fixture.Has(name) == !secFetch);
    }
    Require(!fixture.request.platformRequestUpdated);

    constexpr HTTPHeaderName single[] = {
        HTTPHeaderName::ContentType,
        HTTPHeaderName::Referer,
        HTTPHeaderName::Origin,
        HTTPHeaderName::UserAgent,
        HTTPHeaderName::AcceptEncoding,
        HTTPHeaderName::CacheControl,
    };
    for (int bit = 0; bit < 6; ++bit) {
        Fixture one("application/json");
        one.Clean(static_cast<std::uint8_t>(1u << bit) | 0xc0);
        for (auto name : allHeaders) Require(one.Has(name) == (name == single[bit] || name == HTTPHeaderName::Accept));
    }
}

static void TestUncommonHeadersStay() {
    Fixture fixture("text/plain");
    std::string key = "X-Custom";
    StringImpl keyImpl{1000, static_cast<unsigned>(key.size()), key.data(), 4};
    UncommonHeader uncommon{String(&keyImpl), String(&fixture.otherImpl)};
    fixture.Headers().uncommon = {&uncommon, 1, 1};
    fixture.Clean(0);
    Require(fixture.Headers().uncommon.size == 1);
    uncommon.key.impl = nullptr;
    uncommon.value.impl = nullptr;
}

static void TestNoContentTypeAndEmptyMap() {
    ResourceRequestBase request{};
    request.resourceRequestUpdated = true;
    request.platformRequestUpdated = true;
    _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(&request, 0);
    Require(!request.platformRequestUpdated);
}

template<class TException, class TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

static void TestErrors() {
    Require(Throws<std::invalid_argument>([] {
        _ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE(nullptr, 0);
    }));
    for (std::uint8_t keep : {std::uint8_t(0), std::uint8_t(0x3f)}) {
        Fixture fixture("text/html");
        fixture.request.resourceRequestUpdated = false;
        Require(Throws<std::logic_error>([&] { fixture.Clean(keep); }));
        Require(fixture.Has(HTTPHeaderName::ContentType));
        Require(fixture.request.platformRequestUpdated);
    }
}

int main() {
    TestRemovesEverythingByDefault();
    TestSafeContentTypeStays();
    TestUnsafeContentTypesGo();
    TestKeepFlags();
    TestUncommonHeadersStay();
    TestNoContentTypeAndEmptyMap();
    TestErrors();
    return 0;
}
