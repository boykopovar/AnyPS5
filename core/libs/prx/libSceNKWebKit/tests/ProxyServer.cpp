#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

using NKWebKit::ProxyAbi::ProxyServer;
using NKWebKit::ProxyAbi::ProxyServerVector;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

namespace {

const auto ToString = &_ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE;
const auto ForURL = &_ZN7WebCore18proxyServersForURLERKN3WTF3URLE;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ProxyServer check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

StringImpl Narrow(const char* text) {
    return StringImpl{2, static_cast<unsigned>(std::strlen(text)), text, 4};
}

StringImpl Wide(const std::u16string& text) {
    return StringImpl{2, static_cast<unsigned>(text.size()), text.data(), 0};
}

bool Is8Bit(const String& value) {
    return value.impl && (value.impl->hashAndFlags & 4) != 0;
}

std::u16string Text(const String& value) {
    Require(value.impl != nullptr);
    std::u16string text;
    for (unsigned index = 0; index < value.impl->length; ++index) {
        text.push_back(Is8Bit(value) ? static_cast<const unsigned char*>(value.impl->data)[index]
                                     : static_cast<const char16_t*>(value.impl->data)[index]);
    }
    return text;
}

String Convert(std::vector<ProxyServer>& servers) {
    ProxyServerVector vector{servers.data(), static_cast<unsigned>(servers.size()), static_cast<unsigned>(servers.size())};
    return ToString(&vector);
}

template<typename TAction> bool ThrowsInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

void TestEmpty() {
    ProxyServerVector none{nullptr, 0, 0};
    const String direct = ToString(&none);
    Require(Text(direct) == u"DIRECT" && Is8Bit(direct));

    ProxyServer unused{NKWebKit::ProxyAbi::ProxyTypeSOCKS, nullptr, 9};
    ProxyServerVector reserved{&unused, 1, 0};
    const String alsoDirect = ToString(&reserved);
    Require(Text(alsoDirect) == u"DIRECT" && Is8Bit(alsoDirect));
}

void TestTypes() {
    StringImpl host = Narrow("proxy.example");
    const unsigned before = host.refCount;
    std::vector<ProxyServer> servers{{NKWebKit::ProxyAbi::ProxyTypeHTTP, &host, 8080}};
    String text = Convert(servers);
    Require(Text(text) == u"PROXY proxy.example:8080" && Is8Bit(text));

    servers[0].type = NKWebKit::ProxyAbi::ProxyTypeHTTPS;
    text = Convert(servers);
    Require(Text(text) == u"PROXY proxy.example:8080");

    servers[0].type = NKWebKit::ProxyAbi::ProxyTypeSOCKS;
    text = Convert(servers);
    Require(Text(text) == u"SOCKS proxy.example:8080");

    servers[0].type = NKWebKit::ProxyAbi::ProxyTypeDirect;
    text = Convert(servers);
    Require(Text(text) == u"DIRECT");

    for (const std::int32_t type : {4, 7, -1, INT_MIN, INT_MAX}) {
        servers[0].type = type;
        text = Convert(servers);
        Require(Text(text) == u" proxy.example:8080");
    }
    Require(host.refCount == before);
}

void TestList() {
    StringImpl first = Narrow("a");
    StringImpl second = Narrow("b.example");
    std::vector<ProxyServer> servers{
        {NKWebKit::ProxyAbi::ProxyTypeHTTP, &first, 1},
        {NKWebKit::ProxyAbi::ProxyTypeDirect, &second, 77},
        {NKWebKit::ProxyAbi::ProxyTypeSOCKS, &second, 1080},
        {NKWebKit::ProxyAbi::ProxyTypeHTTPS, &first, 443},
    };
    const String text = Convert(servers);
    Require(Text(text) == u"PROXY a:1; DIRECT; SOCKS b.example:1080; PROXY a:443" && Is8Bit(text));
}

void TestHostAndPort() {
    StringImpl empty = Narrow("");
    StringImpl host = Narrow("h");
    std::vector<ProxyServer> servers{{NKWebKit::ProxyAbi::ProxyTypeHTTP, nullptr, 80}};
    String text = Convert(servers);
    Require(Text(text) == u"PROXY :80" && Is8Bit(text));

    servers[0].hostName = &empty;
    text = Convert(servers);
    Require(Text(text) == u"PROXY :80");

    servers[0].hostName = &host;
    servers[0].port = -1;
    text = Convert(servers);
    Require(Text(text) == u"PROXY h:-1");
    servers[0].port = 0;
    text = Convert(servers);
    Require(Text(text) == u"PROXY h:0");
    servers[0].port = INT_MAX;
    text = Convert(servers);
    Require(Text(text) == u"PROXY h:2147483647");
    servers[0].port = INT_MIN;
    text = Convert(servers);
    Require(Text(text) == u"PROXY h:-2147483648");
}

void TestWidth() {
    StringImpl latin1 = Narrow("caf\xE9");
    std::vector<ProxyServer> servers{{NKWebKit::ProxyAbi::ProxyTypeHTTP, &latin1, 1}};
    String text = Convert(servers);
    Require(Text(text) == std::u16string(u"PROXY caf") + char16_t(0xE9) + u":1" && Is8Bit(text));

    const std::u16string asciiWide = u"ab";
    StringImpl wide = Wide(asciiWide);
    servers[0].hostName = &wide;
    text = Convert(servers);
    Require(Text(text) == u"PROXY ab:1" && !Is8Bit(text));

    const std::u16string oneLatin1 = std::u16string(1, char16_t(0xE9));
    StringImpl single = Wide(oneLatin1);
    servers[0].hostName = &single;
    text = Convert(servers);
    Require(Text(text) == std::u16string(u"PROXY ") + char16_t(0xE9) + u":1" && Is8Bit(text));

    const std::u16string oneWide = std::u16string(1, char16_t(0x0100));
    StringImpl singleWide = Wide(oneWide);
    servers[0].hostName = &singleWide;
    text = Convert(servers);
    Require(Text(text) == std::u16string(u"PROXY ") + char16_t(0x0100) + u":1" && !Is8Bit(text));

    const std::u16string emptyWide;
    StringImpl none = Wide(emptyWide);
    servers[0].hostName = &none;
    text = Convert(servers);
    Require(Text(text) == u"PROXY :1" && Is8Bit(text));

    StringImpl narrow = Narrow("n");
    std::vector<ProxyServer> mixed{
        {NKWebKit::ProxyAbi::ProxyTypeHTTP, &narrow, 1},
        {NKWebKit::ProxyAbi::ProxyTypeSOCKS, &wide, 2},
        {NKWebKit::ProxyAbi::ProxyTypeHTTP, &single, 3},
        {NKWebKit::ProxyAbi::ProxyTypeDirect, nullptr, 4},
    };
    text = Convert(mixed);
    Require(Text(text) == std::u16string(u"PROXY n:1; SOCKS ab:2; PROXY ") + char16_t(0xE9) + u":3; DIRECT" && !Is8Bit(text));
}

void TestInvalid() {
    Require(ThrowsInvalidArgument([] { String text = ToString(nullptr); }));
    ProxyServerVector broken{nullptr, 0, 2};
    Require(ThrowsInvalidArgument([&] { String text = ToString(&broken); }));
}

void TestProxyServersForURL() {
    ProxyServer placeholder{};
    ProxyServerVector result{&placeholder, 5, 3};
    URL url{};
    Require(ForURL(&result, &url) == &result);
    Require(result.buffer == nullptr && result.capacity == 0 && result.size == 0);

    result = {&placeholder, 5, 3};
    Require(ForURL(&result, nullptr) == &result);
    Require(result.buffer == nullptr && result.capacity == 0 && result.size == 0);

    const String text = ToString(&result);
    Require(Text(text) == u"DIRECT");
    Require(ThrowsInvalidArgument([&] { ForURL(nullptr, &url); }));
}

struct GuardedPages {
    unsigned char* base{nullptr};
    std::size_t pageSize{0};

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        Require(VirtualFree(base, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(base, pageSize * 3) == 0);
#endif
    }

    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template<typename TAction> bool ThrowsNotImplemented(TAction action, const char* symbol) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(symbol) + " not implemented";
    } catch (...) {
        return false;
    }
    return false;
}

void TestGuestPointers() {
    constexpr const char* kToStringSymbol = "_ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE";
    constexpr const char* kForUrlSymbol = "_ZN7WebCore18proxyServersForURLERKN3WTF3URLE";

    GuardedPages pages;
    Require(ThrowsNotImplemented([&] { static_cast<void>(ToString(reinterpret_cast<const ProxyServerVector*>(0x10))); }, kToStringSymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(ToString(reinterpret_cast<const ProxyServerVector*>(pages.Guard()))); }, kToStringSymbol));
    Require(ThrowsNotImplemented([&] { static_cast<void>(ToString(reinterpret_cast<const ProxyServerVector*>(pages.Guard() - 8))); }, kToStringSymbol));

    ProxyServerVector guardBuffer{reinterpret_cast<ProxyServer*>(pages.Guard()), 1, 1};
    Require(ThrowsNotImplemented([&] { static_cast<void>(ToString(&guardBuffer)); }, kToStringSymbol));

    ProxyServerVector splitBuffer{reinterpret_cast<ProxyServer*>(pages.Guard() - 12), 1, 1};
    Require(ThrowsNotImplemented([&] { static_cast<void>(ToString(&splitBuffer)); }, kToStringSymbol));

    ProxyServerVector hugeCount{reinterpret_cast<ProxyServer*>(pages.ReadOnly()), 100000, 100000};
    Require(ThrowsNotImplemented([&] { static_cast<void>(ToString(&hugeCount)); }, kToStringSymbol));

    std::vector<ProxyServer> guardHost{{NKWebKit::ProxyAbi::ProxyTypeHTTP, reinterpret_cast<StringImpl*>(pages.Guard()), 8080}};
    Require(ThrowsNotImplemented([&] { static_cast<void>(Convert(guardHost)); }, kToStringSymbol));

    std::vector<ProxyServer> splitHost{{NKWebKit::ProxyAbi::ProxyTypeHTTP, reinterpret_cast<StringImpl*>(pages.Guard() - 8), 8080}};
    Require(ThrowsNotImplemented([&] { static_cast<void>(Convert(splitHost)); }, kToStringSymbol));

    StringImpl guardData{2, 8, pages.Guard() - 4, 4};
    std::vector<ProxyServer> badData{{NKWebKit::ProxyAbi::ProxyTypeHTTP, &guardData, 8080}};
    Require(ThrowsNotImplemented([&] { static_cast<void>(Convert(badData)); }, kToStringSymbol));

    StringImpl guardWideData{2, 4, pages.Guard() - 4, 0};
    std::vector<ProxyServer> badWideData{{NKWebKit::ProxyAbi::ProxyTypeHTTP, &guardWideData, 8080}};
    Require(ThrowsNotImplemented([&] { static_cast<void>(Convert(badWideData)); }, kToStringSymbol));

    StringImpl nullData{2, 4, nullptr, 4};
    std::vector<ProxyServer> nullHostData{{NKWebKit::ProxyAbi::ProxyTypeHTTP, &nullData, 8080}};
    Require(ThrowsInvalidArgument([&] { static_cast<void>(Convert(nullHostData)); }));

    URL url{};
    Require(ThrowsNotImplemented([&] { ForURL(reinterpret_cast<ProxyServerVector*>(0x10), &url); }, kForUrlSymbol));
    Require(ThrowsNotImplemented([&] { ForURL(reinterpret_cast<ProxyServerVector*>(pages.ReadOnly()), &url); }, kForUrlSymbol));
    Require(ThrowsNotImplemented([&] { ForURL(reinterpret_cast<ProxyServerVector*>(pages.Guard()), &url); }, kForUrlSymbol));

    unsigned char* splitOut = pages.ReadOnly() - 8;
    std::memset(splitOut, 0x5A, 8);
    Require(ThrowsNotImplemented([&] { ForURL(reinterpret_cast<ProxyServerVector*>(splitOut), &url); }, kForUrlSymbol));
    for (std::size_t i = 0; i < 8; ++i) Require(splitOut[i] == 0x5A);
}

}

int main() {
    TestEmpty();
    TestTypes();
    TestList();
    TestHostAndPort();
    TestWidth();
    TestInvalid();
    TestProxyServersForURL();
    TestGuestPointers();
    return 0;
}
