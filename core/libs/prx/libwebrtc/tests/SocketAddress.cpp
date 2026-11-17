#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc SocketAddress check failed at line %u\n", location.line());
    std::abort();
}

enum class Access { ReadWrite, ReadOnly, None };

struct GuardedPages {
    std::size_t pageSize = 0;
    std::size_t size = 0;
    unsigned char* base = nullptr;

    explicit GuardedPages(std::size_t bytes = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        size = (bytes + pageSize - 1) / pageSize * pageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
#else
        pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        size = (bytes + pageSize - 1) / pageSize * pageSize;
        void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        base = mapped == MAP_FAILED ? nullptr : static_cast<unsigned char*>(mapped);
#endif
        Require(base != nullptr);
    }

    ~GuardedPages() {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, size);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    template<typename T> T* As(std::size_t offset = 0) const { return reinterpret_cast<T*>(base + offset); }

    void Protect(Access access, std::size_t offset = 0) const {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD mode = access == Access::ReadWrite ? PAGE_READWRITE : access == Access::ReadOnly ? PAGE_READONLY : PAGE_NOACCESS;
        Require(VirtualProtect(base + offset, size - offset, mode, &previous) != 0);
#else
        const int mode = access == Access::ReadWrite ? PROT_READ | PROT_WRITE : access == Access::ReadOnly ? PROT_READ : PROT_NONE;
        Require(mprotect(base + offset, size - offset, mode) == 0);
#endif
    }
};

template<typename TAction> bool ThrowsInvalidArgument(TAction action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

WebRtcSocketAddress Address(std::int32_t family, std::uint16_t port) {
    WebRtcSocketAddress address;
    std::memset(&address, 0, sizeof(address));
    address.ip.family = family;
    address.port = port;
    return address;
}

void SetIpv4(WebRtcSocketAddress& address, std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
    const std::uint8_t bytes[4] = {a, b, c, d};
    std::memcpy(&address.ip.address.ip4, bytes, sizeof(bytes));
}

void SetHostname(WebRtcSocketAddress& address, const char* name) {
    std::size_t length = std::strlen(name);
    Require(length <= 15);
    std::memcpy(address.hostname.buffer, name, length + 1);
    address.hostname.size = length;
    address.hostname.capacity = 15;
}

bool IsAnyIP(const WebRtcSocketAddress& address) {
    return _ZNK3rtc13SocketAddress7IsAnyIPEv_nid_postfix(&address);
}

bool IsComplete(const WebRtcSocketAddress& address) {
    return _ZNK3rtc13SocketAddress10IsCompleteEv_nid_postfix(&address);
}

bool IsNil(const WebRtcSocketAddress& address) {
    return _ZNK3rtc13SocketAddress5IsNilEv_nid_postfix(&address);
}

void TestPortAndIpaddr() {
    WebRtcSocketAddress address = Address(WebRtcAfInet, 3478);
    SetIpv4(address, 192, 168, 1, 7);
    Require(_ZNK3rtc13SocketAddress4portEv_nid_postfix(&address) == 3478);
    Require(_ZNK3rtc13SocketAddress6ipaddrEv_nid_postfix(&address) == &address.ip);
    address.port = 0xFFFF;
    Require(_ZNK3rtc13SocketAddress4portEv_nid_postfix(&address) == 0xFFFF);
}

void TestIpv4() {
    WebRtcSocketAddress address = Address(WebRtcAfInet, 0);
    Require(IsAnyIP(address) && !IsComplete(address) && !IsNil(address));
    address.port = 80;
    Require(IsAnyIP(address) && !IsComplete(address));
    SetIpv4(address, 127, 0, 0, 1);
    Require(!IsAnyIP(address) && IsComplete(address));
    address.port = 0;
    Require(!IsComplete(address) && !IsNil(address));
}

void TestIpv6() {
    WebRtcSocketAddress address = Address(WebRtcAfInet6, 443);
    Require(IsAnyIP(address) && !IsComplete(address));
    address.ip.address.ip6.bytes[10] = 0xFF;
    address.ip.address.ip6.bytes[11] = 0xFF;
    Require(IsAnyIP(address) && !IsComplete(address));
    address.ip.address.ip6.bytes[15] = 1;
    Require(!IsAnyIP(address) && IsComplete(address));
    WebRtcSocketAddress loopback = Address(WebRtcAfInet6, 443);
    loopback.ip.address.ip6.bytes[15] = 1;
    Require(!IsAnyIP(loopback) && IsComplete(loopback));
    WebRtcSocketAddress halfMapped = Address(WebRtcAfInet6, 443);
    halfMapped.ip.address.ip6.bytes[10] = 0xFF;
    Require(!IsAnyIP(halfMapped));
}

void TestUnspecified() {
    WebRtcSocketAddress address = Address(WebRtcAfUnspec, 0);
    Require(!IsAnyIP(address) && !IsComplete(address) && IsNil(address));
    address.port = 1;
    Require(!IsNil(address) && IsComplete(address));
    address.port = 0;
    SetHostname(address, "example.org");
    Require(!IsNil(address));
    address.hostname.size = 0;
    address.hostname.buffer[0] = '\0';
    Require(IsNil(address));
    char longName[] = "stun.long-hostname.example.org";
    address.hostname.pointer = longName;
    address.hostname.size = sizeof(longName) - 1;
    address.hostname.capacity = 31;
    Require(!IsNil(address));
}

void TestConstructors() {
    WebRtcSocketAddress defaulted;
    std::memset(&defaulted, 0x5A, sizeof(defaulted));
    _ZN3rtc13SocketAddressC1Ev_nid_postfix(&defaulted);
    Require(defaulted.hostname.size == 0);
    Require(defaulted.hostname.capacity == 15);
    Require(defaulted.hostname.buffer[0] == '\0');
    Require(defaulted.ip.family == WebRtcAfUnspec);
    Require(defaulted.port == 0);
    Require(defaulted.scopeId == 0);
    Require(!defaulted.literal);
    Require(IsNil(defaulted));

    WebRtcIpAddress ip;
    std::memset(&ip, 0, sizeof(ip));
    ip.family = WebRtcAfInet;
    const std::uint8_t bytes[4] = {10, 1, 2, 3};
    std::memcpy(&ip.address.ip4, bytes, sizeof(bytes));

    WebRtcSocketAddress address;
    std::memset(&address, 0x5A, sizeof(address));
    _ZN3rtc13SocketAddressC1ERKNS_9IPAddressEi_nid_postfix(&address, &ip, 8080);
    Require(address.hostname.size == 0);
    Require(address.hostname.capacity == 15);
    Require(_ZNK3rtc13SocketAddress4portEv_nid_postfix(&address) == 8080);
    Require(_ZNK3rtc13SocketAddress6ipaddrEv_nid_postfix(&address) != nullptr);
    Require(_ZNK3rtc9IPAddresseqERKS0__nid_postfix(&address.ip, &ip));
    Require(address.scopeId == 0);
    Require(!address.literal);
    Require(IsComplete(address));
    Require(!IsNil(address));
}

void TestNullAddressThrows() {
    try {
        _ZNK3rtc13SocketAddress4portEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

void TestInaccessibleAddressesThrow() {
    GuardedPages hidden;
    hidden.Protect(Access::None);
    auto* address = hidden.As<WebRtcSocketAddress>();
    Require(ThrowsInvalidArgument([&] { _ZNK3rtc13SocketAddress4portEv_nid_postfix(address); }));
    Require(ThrowsInvalidArgument([&] { _ZNK3rtc13SocketAddress7IsAnyIPEv_nid_postfix(address); }));
    Require(ThrowsInvalidArgument([&] { _ZNK3rtc13SocketAddress10IsCompleteEv_nid_postfix(address); }));
    Require(ThrowsInvalidArgument([&] { _ZNK3rtc13SocketAddress5IsNilEv_nid_postfix(address); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressC1Ev_nid_postfix(address); }));
    Require(_ZNK3rtc13SocketAddress6ipaddrEv_nid_postfix(address) == hidden.As<WebRtcIpAddress>(offsetof(WebRtcSocketAddress, ip)));
    WebRtcSocketAddress target;
    _ZN3rtc13SocketAddressC1Ev_nid_postfix(&target);
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressC1ERKNS_9IPAddressEi_nid_postfix(&target, hidden.As<WebRtcIpAddress>(), 80); }));
    GuardedPages readOnly;
    auto* constant = readOnly.As<WebRtcSocketAddress>();
    _ZN3rtc13SocketAddressC1Ev_nid_postfix(constant);
    readOnly.Protect(Access::ReadOnly);
    WebRtcIpAddress ip {};
    ip.family = WebRtcAfInet;
    ip.address.ip4.sAddr = 0x01020304;
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressC1Ev_nid_postfix(constant); }));
    Require(ThrowsInvalidArgument([&] { _ZN3rtc13SocketAddressC1ERKNS_9IPAddressEi_nid_postfix(constant, &ip, 80); }));
    Require(_ZNK3rtc13SocketAddress4portEv_nid_postfix(constant) == 0 && _ZNK3rtc13SocketAddress5IsNilEv_nid_postfix(constant));
}

int main() {
    TestPortAndIpaddr();
    TestIpv4();
    TestIpv6();
    TestUnspecified();
    TestConstructors();
    TestNullAddressThrows();
    TestInaccessibleAddressesThrow();
    return 0;
}
