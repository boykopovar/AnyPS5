#include "prx/libSceNKWebKit/include/WebCoreNetworkHelpersExports.hpp"
#include "prx/libSceNKWebKit/include/ResourceRequestExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

namespace {

using NKWebKit::WebCoreAbi::OptionalSecurityOriginData;
using NKWebKit::WebCoreAbi::ProxyServer;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WebCoreAbi::SameSiteInfo;
using NKWebKit::WebCoreAbi::SleepDisablerClient;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::Vector;

static_assert(std::is_same_v<decltype(&_ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE), SameSiteInfo (APS5_VABI *)(const ResourceRequestBase*)>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore18SecurityOriginData22fromDatabaseIdentifierERKN3WTF6StringE), OptionalSecurityOriginData (APS5_VABI *)(const String*)>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE), String (APS5_VABI *)(const Vector<ProxyServer>*)>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore18proxyServersForURLERKN3WTF3URLE), Vector<ProxyServer> (APS5_VABI *)(const NKWebKit::WtfAbi::URL*)>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore19sleepDisablerClientEv), std::unique_ptr<SleepDisablerClient>& (APS5_VABI *)()>);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore network helpers check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

StringImpl Make(const char* text) {
    return StringImpl{2, static_cast<unsigned>(std::strlen(text)), text, 4};
}

StringImpl Make16(const char16_t* text) {
    return StringImpl{2, static_cast<unsigned>(std::char_traits<char16_t>::length(text)), text, 0};
}

String Heap(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

bool Equals(const String& string, const char* expected) {
    const auto* impl = string.impl;
    if (!impl) return false;
    std::size_t length = std::strlen(expected);
    if (impl->length != length) return false;
    for (std::size_t i = 0; i < length; ++i) {
        char16_t unit = impl->hashAndFlags & 4 ? static_cast<const unsigned char*>(impl->data)[i] : static_cast<const char16_t*>(impl->data)[i];
        if (unit != static_cast<unsigned char>(expected[i])) return false;
    }
    return true;
}

SameSiteInfo Create(bool isSameSite, bool isTopSite, const char* method) {
    ResourceRequestBase request = ResourceRequestBase();
    request.resourceRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase13setIsSameSiteEb(&request, isSameSite);
    _ZN7WebCore19ResourceRequestBase12setIsTopSiteEb(&request, isTopSite);
    if (method) {
        String value = Heap(method);
        _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(&request, &value);
    }
    auto info = _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(&request);
    request.httpMethod = String();
    return info;
}

void TestSameSiteInfo() {
    auto info = Create(true, false, "GET");
    Require(info.isSameSite && !info.isTopSite && info.isSafeHTTPMethod);
    info = Create(false, true, "get");
    Require(!info.isSameSite && info.isTopSite && info.isSafeHTTPMethod);
    info = Create(true, true, "HEAD");
    Require(info.isSameSite && info.isTopSite && info.isSafeHTTPMethod);
    Require(Create(false, false, "OPTIONS").isSafeHTTPMethod);
    Require(Create(false, false, "tRaCe").isSafeHTTPMethod);
    info = Create(true, true, "POST");
    Require(info.isSameSite && info.isTopSite && !info.isSafeHTTPMethod);
    Require(!Create(false, false, "GETS").isSafeHTTPMethod);
    Require(!Create(false, false, "GE").isSafeHTTPMethod);
    Require(!Create(false, false, "").isSafeHTTPMethod);
    info = Create(false, false, nullptr);
    Require(!info.isSameSite && !info.isTopSite && !info.isSafeHTTPMethod);
}

OptionalSecurityOriginData FromIdentifier(StringImpl& impl) {
    String identifier(&impl);
    auto result = _ZN7WebCore18SecurityOriginData22fromDatabaseIdentifierERKN3WTF6StringE(&identifier);
    identifier.impl = nullptr;
    return result;
}

OptionalSecurityOriginData FromIdentifier(const char* text) {
    auto impl = Make(text);
    auto result = FromIdentifier(impl);
    Require(impl.refCount == 2);
    return result;
}

void RequireOrigin(const OptionalSecurityOriginData& result, const char* protocol, const char* host, int port) {
    Require(result.hasValue);
    Require(Equals(result.value.protocol, protocol));
    Require(Equals(result.value.host, host));
    if (port < 0) {
        Require(!result.value.port.hasValue);
    } else {
        Require(result.value.port.hasValue);
        Require(result.value.port.value == port);
    }
}

void TestFromDatabaseIdentifier() {
    RequireOrigin(FromIdentifier("https_example.com_443"), "https", "example.com", 443);
    RequireOrigin(FromIdentifier("http_host_0"), "http", "host", -1);
    RequireOrigin(FromIdentifier("http_host_"), "http", "host", -1);
    RequireOrigin(FromIdentifier("file__0"), "file", "", -1);
    RequireOrigin(FromIdentifier("__"), "", "", -1);
    RequireOrigin(FromIdentifier("http_my_host_8080"), "http", "my_host", 8080);
    RequireOrigin(FromIdentifier("http_h_80x"), "http", "h", 80);
    RequireOrigin(FromIdentifier("http_h_ +80 "), "http", "h", 80);
    RequireOrigin(FromIdentifier("http_h_65535"), "http", "h", 65535);
    RequireOrigin(FromIdentifier("http_h_-0"), "http", "h", -1);
    Require(!FromIdentifier("a_b").hasValue);
    Require(!FromIdentifier("nosep").hasValue);
    Require(!FromIdentifier("").hasValue);
    Require(!FromIdentifier("http_h_x80").hasValue);
    Require(!FromIdentifier("http_h_-1").hasValue);
    Require(!FromIdentifier("http_h_65536").hasValue);
    Require(!FromIdentifier("http_h_70000").hasValue);
    Require(!FromIdentifier("http_h_2147483648").hasValue);
    Require(!FromIdentifier("http_h_-2147483648").hasValue);
    Require(!FromIdentifier("http_h_+").hasValue);
    Require(!FromIdentifier("http_h_ ").hasValue);

    auto wide = Make16(u"http_hé_8080");
    auto wideHost = FromIdentifier(wide);
    Require(wideHost.hasValue);
    Require(Equals(wideHost.value.protocol, "http"));
    Require(wideHost.value.port.hasValue && wideHost.value.port.value == 8080);
    Require(wideHost.value.host.impl->length == 2);
    Require(static_cast<const char16_t*>(wideHost.value.host.impl->data)[0] == u'h');
    Require(!(wideHost.value.host.impl->hashAndFlags & 4));
    Require(static_cast<const char16_t*>(wideHost.value.host.impl->data)[1] == u'é');
    auto wideSpace = Make16(u"http_h_　 443");
    RequireOrigin(FromIdentifier(wideSpace), "http", "h", 443);

    String null;
    Require(!_ZN7WebCore18SecurityOriginData22fromDatabaseIdentifierERKN3WTF6StringE(&null).hasValue);
}

String ProxyString(const Vector<ProxyServer>& servers) {
    return _ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(&servers);
}

void TestProxyServerToString() {
    Vector<ProxyServer> servers;
    Require(Equals(ProxyString(servers), "DIRECT"));
    servers.append(ProxyServer{ProxyServer::Direct, String(), -1});
    Require(Equals(ProxyString(servers), "DIRECT"));
    servers.append(ProxyServer{ProxyServer::HTTP, Heap("proxy.example"), 8080});
    servers.append(ProxyServer{ProxyServer::HTTPS, Heap("secure.example"), 443});
    servers.append(ProxyServer{ProxyServer::SOCKS, Heap("socks.example"), 1080});
    servers.append(ProxyServer{ProxyServer::Direct, Heap("ignored"), 1});
    Require(Equals(ProxyString(servers), "DIRECT; PROXY proxy.example:8080; PROXY secure.example:443; SOCKS socks.example:1080; DIRECT"));

    Vector<ProxyServer> single;
    single.append(ProxyServer{ProxyServer::SOCKS, Heap("s"), -1});
    Require(Equals(ProxyString(single), "SOCKS s:-1"));
}

void TestProxyServersForURL() {
    auto text = Make("http://example.com/");
    NKWebKit::WtfAbi::URL url{&text, 1, 7, 7, 7, 18, 19, 19, 19};
    auto servers = _ZN7WebCore18proxyServersForURLERKN3WTF3URLE(&url);
    Require(servers.buffer == nullptr && servers.capacity == 0 && servers.size == 0);
    Require(text.refCount == 2);
}

void TestSleepDisablerClient() {
    auto& slot = _ZN7WebCore19sleepDisablerClientEv();
    Require(&slot == &_ZN7WebCore19sleepDisablerClientEv());
    Require(slot.get() == nullptr);
    int marker = 0;
    reinterpret_cast<void*&>(slot) = &marker;
    Require(static_cast<void*>(_ZN7WebCore19sleepDisablerClientEv().get()) == &marker);
    reinterpret_cast<void*&>(slot) = nullptr;
    Require(_ZN7WebCore19sleepDisablerClientEv().get() == nullptr);
}

}  // namespace

int main() {
    TestSameSiteInfo();
    TestFromDatabaseIdentifier();
    TestProxyServerToString();
    TestProxyServersForURL();
    TestSleepDisablerClient();
    Require(NotImplementedCalls_nid_no_patch() == 0);
    std::puts("WebCore network helper checks passed");
    return 0;
}
