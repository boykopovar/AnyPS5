#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

namespace {

const auto Clean = &_ZN7WebCore39cleanHTTPRequestHeadersForAccessControlERNS_15ResourceRequestEN3WTF9OptionSetINS_29HTTPHeadersToKeepFromCleaningEEE;
const auto Update = &_ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "CORS request check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename TAction> bool Throws(TAction action) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct Text {
    StringImpl impl;
    explicit Text(const char* characters) : impl{0x40000002, static_cast<unsigned>(std::strlen(characters)), characters, 4} {}
    String Value() {
        impl.refCount += 2;
        return String(&impl);
    }
};

HTTPHeaderMap& Headers(ResourceRequestBase& request) {
    return *reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
}

std::string Read(const String& value) {
    if (!value.impl) return "<null>";
    std::string text;
    for (unsigned index = 0; index < value.impl->length; ++index) {
        text.push_back(value.impl->hashAndFlags & 4 ? static_cast<const char*>(value.impl->data)[index]
                                                    : static_cast<char>(static_cast<const char16_t*>(value.impl->data)[index]));
    }
    return text;
}

std::string Header(ResourceRequestBase& request, HTTPHeaderName name) {
    const String value = _ZNK7WebCore13HTTPHeaderMap3getENS_14HTTPHeaderNameE(&Headers(request), name);
    return Read(value);
}

void Set(ResourceRequestBase& request, HTTPHeaderName name, Text& value) {
    const String text = value.Value();
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, name, &text);
}

ResourceRequestBase* NewRequest() {
    auto* request = new ResourceRequestBase{};
    request->allowCookies = true;
    request->resourceRequestUpdated = true;
    request->platformRequestUpdated = true;
    request->resourceRequestBodyUpdated = true;
    return request;
}

Text json("application/json");
Text plain("text/plain; charset=utf-8");
Text referrer("https://a.example/page");
Text originValue("https://a.example");
Text agent("agent/1.0");
Text encoding("gzip");
Text cache("no-cache");
Text dest("empty");
Text mode("cors");
Text site("cross-site");
Text accept("text/html");

ResourceRequestBase* FilledRequest(Text& contentType) {
    auto* request = NewRequest();
    Set(*request, HTTPHeaderName::ContentType, contentType);
    Set(*request, HTTPHeaderName::Referer, referrer);
    Set(*request, HTTPHeaderName::Origin, originValue);
    Set(*request, HTTPHeaderName::UserAgent, agent);
    Set(*request, HTTPHeaderName::AcceptEncoding, encoding);
    Set(*request, HTTPHeaderName::CacheControl, cache);
    Set(*request, HTTPHeaderName::SecFetchDest, dest);
    Set(*request, HTTPHeaderName::SecFetchMode, mode);
    Set(*request, HTTPHeaderName::SecFetchSite, site);
    Set(*request, HTTPHeaderName::Accept, accept);
    request->platformRequestUpdated = true;
    return request;
}

struct Expectation {
    std::uint8_t bit;
    HTTPHeaderName name;
    const char* value;
};

const Expectation Kept[] = {
    {NKWebKit::CorsAbi::KeepContentType, HTTPHeaderName::ContentType, "application/json"},
    {NKWebKit::CorsAbi::KeepReferer, HTTPHeaderName::Referer, "https://a.example/page"},
    {NKWebKit::CorsAbi::KeepOrigin, HTTPHeaderName::Origin, "https://a.example"},
    {NKWebKit::CorsAbi::KeepUserAgent, HTTPHeaderName::UserAgent, "agent/1.0"},
    {NKWebKit::CorsAbi::KeepAcceptEncoding, HTTPHeaderName::AcceptEncoding, "gzip"},
    {NKWebKit::CorsAbi::KeepCacheControl, HTTPHeaderName::CacheControl, "no-cache"},
};

void TestClean() {
    for (unsigned keep = 0; keep < 256; ++keep) {
        auto* request = FilledRequest(json);
        Clean(request, static_cast<std::uint8_t>(keep));
        for (const auto& expectation : Kept)
            Require(Header(*request, expectation.name) == ((keep & expectation.bit) ? expectation.value : "<null>"));
        Require(Header(*request, HTTPHeaderName::SecFetchDest) == "<null>");
        Require(Header(*request, HTTPHeaderName::SecFetchMode) == "<null>");
        Require(Header(*request, HTTPHeaderName::SecFetchSite) == "<null>");
        Require(Header(*request, HTTPHeaderName::Accept) == "text/html");
        Require(!request->platformRequestUpdated);
        Require(request->allowCookies && request->resourceRequestUpdated);
    }
}

void TestCleanContentType() {
    auto* safe = FilledRequest(plain);
    Clean(safe, 0);
    Require(Header(*safe, HTTPHeaderName::ContentType) == "text/plain; charset=utf-8");
    Require(Header(*safe, HTTPHeaderName::Referer) == "<null>");

    auto* absent = NewRequest();
    Clean(absent, 0);
    Require(Header(*absent, HTTPHeaderName::ContentType) == "<null>");
    Require(!absent->platformRequestUpdated);
}

void TestCleanRejects() {
    Require(Throws([] { Clean(nullptr, 0); }));
    for (const std::uint8_t keep : {std::uint8_t(0), std::uint8_t(0x3F), std::uint8_t(0xFF)}) {
        auto* request = FilledRequest(json);
        request->resourceRequestUpdated = false;
        Require(Throws([&] { Clean(request, keep); }));
        request->resourceRequestUpdated = true;
        for (const auto& expectation : Kept)
            Require(Header(*request, expectation.name) == expectation.value);
        Require(Header(*request, HTTPHeaderName::SecFetchDest) == "empty");
        Require(request->platformRequestUpdated);
    }
}

Text https("https");
Text host("b.example");

SecurityOrigin* TupleOrigin() {
    auto* origin = new SecurityOrigin{};
    new (&origin->data.storage.tuple) NKWebKit::WebCoreAbi::SecurityOriginTuple{https.Value(), host.Value(), {0, false}};
    origin->data.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
    return origin;
}

SecurityOrigin* OpaqueOrigin() {
    auto* origin = new SecurityOrigin{};
    origin->data.index = NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex;
    return origin;
}

void TestUpdate() {
    for (const unsigned policy : {0u, 1u, 2u, 3u, 7u, 255u}) {
        for (const bool allowed : {false, true}) {
            auto* request = FilledRequest(json);
            request->allowCookies = allowed;
            Update(request, TupleOrigin(), static_cast<std::uint8_t>(policy));
            Require(request->allowCookies == (allowed && policy == 1));
            Require(Header(*request, HTTPHeaderName::Origin) == "https://b.example");
            Require(Header(*request, HTTPHeaderName::Referer) == "https://a.example/page");
            Require(Header(*request, HTTPHeaderName::ContentType) == "application/json");
            Require(!request->platformRequestUpdated);
        }
    }

    auto* fresh = NewRequest();
    Update(fresh, OpaqueOrigin(), 1);
    Require(Header(*fresh, HTTPHeaderName::Origin) == "null");
    Require(fresh->allowCookies);
    Require(!fresh->platformRequestUpdated);
}

void TestUpdateRejects() {
    auto* request = FilledRequest(json);
    Require(Throws([&] { Update(nullptr, TupleOrigin(), 1); }));
    Require(Throws([&] { Update(request, nullptr, 0); }));
    request->resourceRequestUpdated = false;
    Require(Throws([&] { Update(request, TupleOrigin(), 0); }));
    request->resourceRequestUpdated = true;
    Require(request->allowCookies && request->platformRequestUpdated);
    Require(Header(*request, HTTPHeaderName::Origin) == "https://a.example");
}

}

int main() {
    TestClean();
    TestCleanContentType();
    TestCleanRejects();
    TestUpdate();
    TestUpdateRejects();
    return 0;
}
