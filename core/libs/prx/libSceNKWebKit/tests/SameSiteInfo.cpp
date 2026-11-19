#include "prx/libSceNKWebKit/include/SameSiteInfoExports.hpp"

#include <cstdlib>
#include <stdexcept>
#include <string_view>

using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WebCoreAbi::SameSiteInfo;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Require(bool value) {
    if (!value) std::abort();
}

static StringImpl Latin1(std::string_view text) {
    return {1u << 20, static_cast<unsigned>(text.size()), text.data(), 4};
}

static StringImpl Utf16(std::u16string_view text) {
    return {1u << 20, static_cast<unsigned>(text.size()), text.data(), 0};
}

static ResourceRequestBase Request(StringImpl* method, unsigned char disposition, bool isTopSite) {
    ResourceRequestBase request{};
    request.resourceRequestUpdated = true;
    request.httpMethod.impl = method;
    request.sameSiteDisposition = disposition;
    request.isTopSite = isTopSite;
    return request;
}

static SameSiteInfo Create(const ResourceRequestBase& request) {
    return _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(&request);
}

static void TestSiteFlags() {
    auto method = Latin1("GET");
    auto info = Create(Request(&method, NKWebKit::WebCoreAbi::SameSite, true));
    Require(info.isSameSite && info.isTopSite && info.isSafeHTTPMethod);

    info = Create(Request(&method, NKWebKit::WebCoreAbi::CrossSite, false));
    Require(!info.isSameSite && !info.isTopSite && info.isSafeHTTPMethod);

    info = Create(Request(&method, NKWebKit::WebCoreAbi::Unspecified, true));
    Require(!info.isSameSite && info.isTopSite);
}

static void TestSafeMethods() {
    for (std::string_view name : {"GET", "head", "Options", "tRaCe"}) {
        auto method = Latin1(name);
        Require(Create(Request(&method, NKWebKit::WebCoreAbi::SameSite, false)).isSafeHTTPMethod);
    }
    for (std::string_view name : {"POST", "PUT", "DELETE", "GETS", "GE", "", "CONNECT"}) {
        auto method = Latin1(name);
        Require(!Create(Request(&method, NKWebKit::WebCoreAbi::SameSite, false)).isSafeHTTPMethod);
    }
    auto wide = Utf16(u"Head");
    Require(Create(Request(&wide, NKWebKit::WebCoreAbi::SameSite, false)).isSafeHTTPMethod);
    auto nonAscii = Utf16(u"GİT");
    Require(!Create(Request(&nonAscii, NKWebKit::WebCoreAbi::SameSite, false)).isSafeHTTPMethod);
    Require(!Create(Request(nullptr, NKWebKit::WebCoreAbi::SameSite, false)).isSafeHTTPMethod);
}

static void TestInvalidArguments() {
    bool threw = false;
    try {
        _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    StringImpl broken{1u << 20, 3, nullptr, 4};
    threw = false;
    try {
        Create(Request(&broken, NKWebKit::WebCoreAbi::SameSite, false));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    TestSiteFlags();
    TestSafeMethods();
    TestInvalidArguments();
    return 0;
}
