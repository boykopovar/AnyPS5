#include "prx/libSceNKWebKit/include/ProxyServerExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WtfAbi::String;
using ProxyVector = NKWebKit::WtfAbi::Vector<WebCore::ProxyServer>;
using WebCore::ProxyServer;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ProxyServer check failed at line %u\n", location.line());
        std::abort();
    }
}

String Make(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

String Make16(std::u16string_view text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
}

ProxyServer Server(ProxyServer::Type type, String hostName, int port) {
    ProxyServer server;
    server.m_type = type;
    server.m_hostName = std::move(hostName);
    server.m_port = port;
    return server;
}

ProxyVector List(std::initializer_list<ProxyServer> servers) {
    ProxyVector vector;
    for (const auto& server : servers) vector.append(server);
    return vector;
}

String Convert(const ProxyVector& servers) {
    return _ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(&servers);
}

bool Is8Bit(const String& string) {
    return string.impl->hashAndFlags & 4;
}

std::u16string Units(const String& string) {
    Check(string.impl != nullptr);
    std::u16string units;
    for (unsigned i = 0; i < string.impl->length; ++i) {
        units.push_back(Is8Bit(string) ? static_cast<const unsigned char*>(string.impl->data)[i] : static_cast<const char16_t*>(string.impl->data)[i]);
    }
    return units;
}

bool Equals8(const String& string, const char* expected) {
    if (!string.impl || !Is8Bit(string)) return false;
    return string.impl->length == std::strlen(expected) && !std::memcmp(string.impl->data, expected, string.impl->length);
}

template <typename Function>
void CheckThrows(Function function) {
    bool thrown = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Check(thrown);
}

void TestEmptyListIsDirect() {
    ProxyVector empty;
    Check(Equals8(Convert(empty), "DIRECT"));
}

void TestSingleServers() {
    Check(Equals8(Convert(List({Server(ProxyServer::Direct, Make("ignored"), 80)})), "DIRECT"));
    Check(Equals8(Convert(List({Server(ProxyServer::HTTP, Make("proxy.example"), 8080)})), "PROXY proxy.example:8080"));
    Check(Equals8(Convert(List({Server(ProxyServer::HTTPS, Make("secure.example"), 443)})), "PROXY secure.example:443"));
    Check(Equals8(Convert(List({Server(ProxyServer::SOCKS, Make("socks.example"), 1080)})), "SOCKS socks.example:1080"));
}

void TestSeparatorsBetweenServers() {
    const auto servers = List({
        Server(ProxyServer::HTTP, Make("a"), 1),
        Server(ProxyServer::SOCKS, Make("b"), 2),
        Server(ProxyServer::Direct, String(), -1),
    });
    Check(Equals8(Convert(servers), "PROXY a:1; SOCKS b:2; DIRECT"));
}

void TestNullOrEmptyHostAndPortValues() {
    Check(Equals8(Convert(List({Server(ProxyServer::HTTP, String(), -1)})), "PROXY :-1"));
    Check(Equals8(Convert(List({Server(ProxyServer::SOCKS, Make(""), 0)})), "SOCKS :0"));
    Check(Equals8(Convert(List({Server(ProxyServer::HTTP, Make("h"), INT_MIN)})), "PROXY h:-2147483648"));
    Check(Equals8(Convert(List({Server(ProxyServer::HTTP, Make("h"), INT_MAX)})), "PROXY h:2147483647"));
}

void TestUnknownTypeWritesOnlyHostAndPort() {
    Check(Equals8(Convert(List({Server(static_cast<ProxyServer::Type>(7), Make("x"), 9)})), " x:9"));
}

void TestSixteenBitHostUpconverts() {
    const String result = Convert(List({Server(ProxyServer::HTTP, Make16(u"hāst"), 80), Server(ProxyServer::SOCKS, Make("s"), 1)}));
    Check(!Is8Bit(result));
    Check(Units(result) == u"PROXY hāst:80; SOCKS s:1");
    const String latin = Convert(List({Server(ProxyServer::HTTP, Make16(u"ab"), 1)}));
    Check(!Is8Bit(latin));
    Check(Units(latin) == u"PROXY ab:1");
}

void TestSixteenBitSingleLatin1HostStaysEightBit() {
    Check(Equals8(Convert(List({Server(ProxyServer::HTTP, Make16(u"é"), 5)})), "PROXY \xe9:5"));
    const String wide = Convert(List({Server(ProxyServer::HTTP, Make16(u"Ā"), 5)}));
    Check(!Is8Bit(wide));
    Check(Units(wide) == u"PROXY Ā:5");
}

void TestResultIsFreshString() {
    const String host = Make("shared");
    const String result = Convert(List({Server(ProxyServer::HTTP, host, 1)}));
    Check(result.impl != host.impl);
    Check(result.impl->refCount == 2);
}

void TestNullArguments() {
    CheckThrows([] { _ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(nullptr); });
    ProxyVector broken;
    broken.size = 1;
    CheckThrows([&] { Convert(broken); });
    broken.size = 0;
}

}  // namespace

int main() {
    TestEmptyListIsDirect();
    TestSingleServers();
    TestSeparatorsBetweenServers();
    TestNullOrEmptyHostAndPortValues();
    TestUnknownTypeWritesOnlyHostAndPort();
    TestSixteenBitHostUpconverts();
    TestSixteenBitSingleLatin1HostStaysEightBit();
    TestResultIsFreshString();
    TestNullArguments();
    return 0;
}
