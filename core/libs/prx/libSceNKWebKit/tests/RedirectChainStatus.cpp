#include "prx/libSceNKWebKit/include/RedirectChainStatusExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceResponseExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>

using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WtfAbi::String;
using Status = WebCore::RedirectChainCacheStatus::Status;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "updateRedirectChainStatus check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static String Text(std::string_view value) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(value.data()), static_cast<unsigned>(value.size()));
}

template<class Entry>
static void ReleaseHeaders(NKWebKit::HttpAbi::HeaderVector<Entry>& entries) {
    if (entries.buffer) {
        std::destroy_n(entries.buffer, entries.size);
        _ZN3WTF8fastFreeEPv(entries.buffer);
    }
}

struct ResponseFixture {
    ResourceResponseBase response{};

    ~ResponseFixture() {
        ReleaseHeaders(response.httpHeaderFields.common);
        ReleaseHeaders(response.httpHeaderFields.uncommon);
    }

    void Set(HTTPHeaderName header, std::string_view value) {
        _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&response, header, Text(value));
    }
};

static void Update(WebCore::RedirectChainCacheStatus& status, const ResourceResponseBase& response) {
    _ZN7WebCore25updateRedirectChainStatusERNS_24RedirectChainCacheStatusERKNS_16ResourceResponseE(status, response);
}

static void TestNotCachedStaysNotCached() {
    ResponseFixture fixture;
    WebCore::RedirectChainCacheStatus status{{42.5}, Status::NotCachedRedirection};
    Update(status, fixture.response);
    Require(status.status == Status::NotCachedRedirection);
    Require(status.endOfValidity.m_value == 42.5);
}

static void TestDirectiveMarksNotCached(HTTPHeaderName header, std::string_view value, Status initial) {
    ResponseFixture fixture;
    fixture.Set(header, value);
    WebCore::RedirectChainCacheStatus status{{7.0}, initial};
    Update(status, fixture.response);
    Require(status.status == Status::NotCachedRedirection);
    Require(status.endOfValidity.m_value == 7.0);
}

int main() {
    TestNotCachedStaysNotCached();
    TestDirectiveMarksNotCached(HTTPHeaderName::CacheControl, "no-store", Status::NoRedirection);
    TestDirectiveMarksNotCached(HTTPHeaderName::CacheControl, "max-age=60, no-cache", Status::CachedRedirection);
    TestDirectiveMarksNotCached(HTTPHeaderName::CacheControl, "must-revalidate", Status::NoRedirection);
    TestDirectiveMarksNotCached(HTTPHeaderName::Pragma, "no-cache", Status::CachedRedirection);
    return 0;
}
