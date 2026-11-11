#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc SocketAddress check failed at line %u\n", location.line());
    std::abort();
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

void TestNullAddressThrows() {
    try {
        _ZNK3rtc13SocketAddress4portEv_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    TestPortAndIpaddr();
    TestIpv4();
    TestIpv6();
    TestUnspecified();
    TestNullAddressThrows();
    return 0;
}
