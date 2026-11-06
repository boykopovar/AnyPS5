#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc rtc::IPAddress check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

WebRtcIpAddress Address(std::int32_t family, std::initializer_list<std::uint8_t> bytes) {
    WebRtcIpAddress address;
    std::memset(&address, 0x5A, sizeof(address));
    address.vtable = &address;
    address.family = family;
    std::memset(&address.address, 0, sizeof(address.address));
    std::size_t index = 0;
    for (std::uint8_t byte : bytes)
        address.address.ip6.bytes[index++] = byte;
    return address;
}

WebRtcIpAddress V4(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
    return Address(WebRtcAfInet, {a, b, c, d});
}

WebRtcIpAddress V6Prefix(int ones) {
    WebRtcIpAddress address = Address(WebRtcAfInet6, {});
    for (int bit = 0; bit < ones; ++bit)
        address.address.ip6.bytes[bit / 8] |= static_cast<std::uint8_t>(0x80u >> (bit % 8));
    return address;
}

bool Equal(const WebRtcIpAddress& left, const WebRtcIpAddress& right) {
    return _ZNK3rtc9IPAddresseqERKS0__nid_postfix(&left, &right);
}

int MaskBits(const WebRtcIpAddress& mask) {
    return _ZN3rtc15CountIPMaskBitsERKNS_9IPAddressE_nid_postfix(&mask);
}

void TestAccessors() {
    WebRtcIpAddress v4 = V4(192, 168, 1, 10);
    WebRtcInAddr ip4 = _ZNK3rtc9IPAddress12ipv4_addressEv_nid_postfix(&v4);
    Require(std::memcmp(&ip4, v4.address.ip6.bytes, 4) == 0);
    Require(_ZNK3rtc9IPAddress27v4AddressAsHostOrderIntegerEv_nid_postfix(&v4) == 0xC0A8010Au);

    WebRtcIpAddress v6 = Address(WebRtcAfInet6, {0x20, 0x01, 0x0D, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01});
    WebRtcIn6Addr ip6 = _ZNK3rtc9IPAddress12ipv6_addressEv_nid_postfix(&v6);
    Require(std::memcmp(ip6.bytes, v6.address.ip6.bytes, 16) == 0);
    WebRtcInAddr v6AsV4 = _ZNK3rtc9IPAddress12ipv4_addressEv_nid_postfix(&v6);
    Require(std::memcmp(&v6AsV4, v6.address.ip6.bytes, 4) == 0);
    WebRtcIn6Addr v4AsV6 = _ZNK3rtc9IPAddress12ipv6_addressEv_nid_postfix(&v4);
    Require(std::memcmp(v4AsV6.bytes, v4.address.ip6.bytes, 16) == 0);
    Require(_ZNK3rtc9IPAddress27v4AddressAsHostOrderIntegerEv_nid_postfix(&v6) == 0);

    WebRtcIpAddress unspec = Address(WebRtcAfUnspec, {1, 2, 3, 4});
    Require(_ZNK3rtc9IPAddress27v4AddressAsHostOrderIntegerEv_nid_postfix(&unspec) == 0);
}

void TestEquality() {
    Require(Equal(V4(10, 0, 0, 1), V4(10, 0, 0, 1)));
    Require(!Equal(V4(10, 0, 0, 1), V4(10, 0, 0, 2)));

    WebRtcIpAddress v4 = V4(10, 0, 0, 1);
    WebRtcIpAddress v4Tail = V4(10, 0, 0, 1);
    v4Tail.address.ip6.bytes[15] = 0xFF;
    Require(Equal(v4, v4Tail));

    WebRtcIpAddress v6 = Address(WebRtcAfInet6, {10, 0, 0, 1});
    Require(!Equal(v4, v6));
    Require(Equal(v6, Address(WebRtcAfInet6, {10, 0, 0, 1})));
    WebRtcIpAddress v6Tail = v6;
    v6Tail.address.ip6.bytes[15] = 1;
    Require(!Equal(v6, v6Tail));

    Require(Equal(Address(WebRtcAfUnspec, {1}), Address(WebRtcAfUnspec, {2})));
    WebRtcIpAddress other = Address(1, {});
    Require(!Equal(other, other));
}

void TestMaskBits() {
    Require(MaskBits(V4(255, 255, 255, 0)) == 24);
    Require(MaskBits(V4(255, 255, 255, 255)) == 32);
    Require(MaskBits(V4(128, 0, 0, 0)) == 1);
    Require(MaskBits(V4(0, 0, 0, 0)) == 0);
    Require(MaskBits(V4(255, 0, 255, 0)) == 24);
    Require(MaskBits(V4(0, 0, 0, 1)) == 32);

    for (int ones : {0, 1, 31, 32, 33, 60, 64, 96, 127, 128})
        Require(MaskBits(V6Prefix(ones)) == ones);
    WebRtcIpAddress gap = V6Prefix(64);
    gap.address.ip6.bytes[9] = 0x01;
    Require(MaskBits(gap) == 80);
    WebRtcIpAddress lateBit = Address(WebRtcAfInet6, {0, 0, 0, 0, 0, 0, 0, 0x01});
    Require(MaskBits(lateBit) == 0);

    Require(MaskBits(Address(WebRtcAfUnspec, {255, 255, 255, 0})) == 0);
    Require(MaskBits(Address(1, {255, 255, 255, 0})) == 0);
}

void TestNullAddress() {
    WebRtcIpAddress v4 = V4(10, 0, 0, 1);
    RequireInvalidArgument([] { _ZNK3rtc9IPAddress12ipv4_addressEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZNK3rtc9IPAddress12ipv6_addressEv_nid_postfix(nullptr); });
    RequireInvalidArgument([] { _ZNK3rtc9IPAddress27v4AddressAsHostOrderIntegerEv_nid_postfix(nullptr); });
    RequireInvalidArgument([&] { _ZNK3rtc9IPAddresseqERKS0__nid_postfix(nullptr, &v4); });
    RequireInvalidArgument([&] { _ZNK3rtc9IPAddresseqERKS0__nid_postfix(&v4, nullptr); });
    RequireInvalidArgument([] { _ZN3rtc15CountIPMaskBitsERKNS_9IPAddressE_nid_postfix(nullptr); });
}

}

int main() {
    TestAccessors();
    TestEquality();
    TestMaskBits();
    TestNullAddress();
    return 0;
}
