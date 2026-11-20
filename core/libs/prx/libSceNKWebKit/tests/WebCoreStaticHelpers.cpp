#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string_view>

using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCoreStaticHelpers check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static String Text(std::string_view value) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(value.data()), static_cast<unsigned>(value.size()));
}

static void RequireDescription(std::uint32_t code, const char* name, const char* message, std::uint8_t legacyCode) {
    const auto& description = _ZN7WebCore12DOMException11descriptionENS_13ExceptionCodeE(code);
    Require(description.name && std::strcmp(description.name, name) == 0);
    Require(description.message && std::strcmp(description.message, message) == 0);
    Require(description.legacyCode == legacyCode);
}

static void TestDOMExceptionDescription() {
    RequireDescription(0, "IndexSizeError", "The index is not in the allowed range.", 1);
    RequireDescription(1, "HierarchyRequestError", "The operation would yield an incorrect node tree.", 3);
    RequireDescription(10, "InvalidModificationError", " The object can not be modified in this way.", 13);
    RequireDescription(15, "NetworkError", " A network error occurred.", 19);
    RequireDescription(22, "EncodingError", "The encoding operation (either encoded or decoding) failed.", 0);
    RequireDescription(28, "ReadOnlyError", "The mutating operation was attempted in a \"readonly\" transaction.", 0);
    RequireDescription(31, "NotAllowedError", "The request is not allowed by the user agent or the platform in the current context, possibly because the user denied permission.", 0);
    Require(&_ZN7WebCore12DOMException11descriptionENS_13ExceptionCodeE(31) - &_ZN7WebCore12DOMException11descriptionENS_13ExceptionCodeE(0) == 31);

    const auto& outside = _ZN7WebCore12DOMException11descriptionENS_13ExceptionCodeE(32);
    Require(!outside.name && !outside.message && outside.legacyCode == 0);
    Require(&_ZN7WebCore12DOMException11descriptionENS_13ExceptionCodeE(37) == &outside);
    Require(&_ZN7WebCore12DOMException11descriptionENS_13ExceptionCodeE(0xffffffffu) == &outside);
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

constexpr double kDate = 784111777;

static void RequireAge(const ResponseFixture& fixture, double responseTime, double correctedInitialAge) {
    const double before = _ZN3WTF8WallTime3nowEv().m_value;
    const auto age = _ZN7WebCore17computeCurrentAgeERKNS_16ResourceResponseEN3WTF8WallTimeE(fixture.response, WTF::WallTime{responseTime});
    const double after = _ZN3WTF8WallTime3nowEv().m_value;
    const double residentTime = age.m_value - correctedInitialAge;
    Require(residentTime >= before - responseTime - 1e-3);
    Require(residentTime <= after - responseTime + 1e-3);
}

static void TestComputeCurrentAge() {
    ResponseFixture fixture;
    const double now = _ZN3WTF8WallTime3nowEv().m_value;
    RequireAge(fixture, now - 5, 0);
    const double fresh = now + 5;
    const auto future = _ZN7WebCore17computeCurrentAgeERKNS_16ResourceResponseEN3WTF8WallTimeE(fixture.response, WTF::WallTime{fresh});
    Require(future.m_value < 0 && future.m_value > -5.5);

    fixture.Set(HTTPHeaderName::Age, "100");
    RequireAge(fixture, now - 5, 100);

    fixture.Set(HTTPHeaderName::Age, "");
    fixture.Set(HTTPHeaderName::Date, "Sun, 06 Nov 1994 08:49:37 GMT");
    RequireAge(fixture, kDate - 30, 0);
    RequireAge(fixture, kDate, 0);
    RequireAge(fixture, kDate + 50, 50);

    fixture.Set(HTTPHeaderName::Age, "10");
    RequireAge(fixture, kDate + 50, 50);
    fixture.Set(HTTPHeaderName::Age, "200");
    RequireAge(fixture, kDate + 50, 200);
    fixture.Set(HTTPHeaderName::Age, "-2.5");
    RequireAge(fixture, kDate - 30, 0);
    fixture.Set(HTTPHeaderName::Age, "7.25");
    RequireAge(fixture, kDate - 30, 7.25);

    fixture.Set(HTTPHeaderName::Date, "invalid");
    fixture.Set(HTTPHeaderName::Age, "broken");
    RequireAge(fixture, kDate + 50, 0);
}

static void TestMediaCacheDirectory() {
    const auto& initial = _ZN7WebCore16HTMLMediaElement19mediaCacheDirectoryEv();
    Require(!initial.impl);

    auto path = Text("/x");
    Require(path.impl->refCount == 2);
    _ZN7WebCore16HTMLMediaElement22setMediaCacheDirectoryERKN3WTF6StringE(path);
    const auto& stored = _ZN7WebCore16HTMLMediaElement19mediaCacheDirectoryEv();
    Require(&stored == &initial);
    Require(stored.impl == path.impl);
    Require(path.impl->refCount == 4);
    Require(stored.impl->length == 2 && std::memcmp(stored.impl->data, "/x", 2) == 0);

    _ZN7WebCore16HTMLMediaElement22setMediaCacheDirectoryERKN3WTF6StringE(stored);
    Require(stored.impl == path.impl && path.impl->refCount == 4);

    auto empty = Text("");
    const unsigned emptyCount = empty.impl ? empty.impl->refCount : 0;
    _ZN7WebCore16HTMLMediaElement22setMediaCacheDirectoryERKN3WTF6StringE(empty);
    Require(path.impl->refCount == 2);
    Require(_ZN7WebCore16HTMLMediaElement19mediaCacheDirectoryEv().impl == empty.impl);
    Require(!empty.impl || empty.impl->refCount == emptyCount + 2);
    Require(!empty.impl || empty.impl->length == 0);

    _ZN7WebCore16HTMLMediaElement22setMediaCacheDirectoryERKN3WTF6StringE(String());
    Require(!_ZN7WebCore16HTMLMediaElement19mediaCacheDirectoryEv().impl);
    Require(!empty.impl || empty.impl->refCount == emptyCount);
    Require(&_ZN7WebCore16HTMLMediaElement19mediaCacheDirectoryEv() == &initial);
}

int main() {
    TestDOMExceptionDescription();
    TestComputeCurrentAge();
    TestMediaCacheDirectory();
    std::puts("WebCoreStaticHelpers: all checks passed");
    return 0;
}
