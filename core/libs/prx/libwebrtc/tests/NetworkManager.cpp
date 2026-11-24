#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc NetworkManager check failed at line %u\n", location.line());
    std::abort();
}

const int ipVTable = 0;

WebRtcIpAddress Ipv4(std::uint8_t a, std::uint8_t b, std::uint8_t c, std::uint8_t d) {
    WebRtcIpAddress ip{&ipVTable, WebRtcAfInet, {}};
    ip.address.ip6.bytes[0] = a;
    ip.address.ip6.bytes[1] = b;
    ip.address.ip6.bytes[2] = c;
    ip.address.ip6.bytes[3] = d;
    return ip;
}

WebRtcInterfaceAddress Ipv6(std::uint8_t first, std::uint8_t second, std::uint8_t last, std::int32_t flags = 0) {
    WebRtcInterfaceAddress ip{&ipVTable, WebRtcAfInet6, {.ip6 = {}}, flags};
    ip.address.ip6.bytes[0] = first;
    ip.address.ip6.bytes[1] = second;
    ip.address.ip6.bytes[15] = last;
    return ip;
}

const WebRtcIpAddress& AsIp(const WebRtcInterfaceAddress& ip) {
    return *reinterpret_cast<const WebRtcIpAddress*>(&ip);
}

void SetIps(WebRtcNetwork& network, WebRtcInterfaceAddress* begin, std::size_t count) {
    network.ipsBegin = begin;
    network.ipsEnd = begin + count;
    network.ipsCapacity = begin + count;
}

template <typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestDefaultLocalAddresses() {
    WebRtcNetworkManagerBase manager{};
    manager.enumerationPermission = 1;
    Require(_ZNK3rtc18NetworkManagerBase22enumeration_permissionEv_nid_postfix(&manager) == 1);

    WebRtcIpAddress result{&ipVTable, WebRtcAfUnspec, {}};
    Require(!_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet, &result));
    Require(!_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet6, &result));

    const int managerVTable = 0;
    manager.defaultLocalIpv4Address.vtable = &managerVTable;
    const auto ipv4 = Ipv4(192, 168, 1, 7);
    const auto ipv6 = Ipv6(0x20, 0x01, 9);
    _ZN3rtc18NetworkManagerBase27set_default_local_addressesERKNS_9IPAddressES3__nid_postfix(&manager, &ipv4, &AsIp(ipv6));
    Require(manager.defaultLocalIpv4Address.vtable == &managerVTable);
    Require(manager.defaultLocalIpv4Address.family == WebRtcAfInet && manager.defaultLocalIpv4Address.address.ip6.bytes[3] == 7);
    Require(manager.defaultLocalIpv6Address.family == WebRtcAfInet6 && manager.defaultLocalIpv6Address.address.ip6.bytes[15] == 9);

    const auto unset = Ipv4(0, 0, 0, 0);
    WebRtcIpAddress nil{&ipVTable, WebRtcAfUnspec, {}};
    _ZN3rtc18NetworkManagerBase27set_default_local_addressesERKNS_9IPAddressES3__nid_postfix(&manager, &nil, &unset);
    Require(manager.defaultLocalIpv4Address.address.ip6.bytes[3] == 7 && manager.defaultLocalIpv6Address.family == WebRtcAfInet6);

    const int resultVTable = 0;
    result.vtable = &resultVTable;
    Require(_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet, &result));
    Require(result.vtable == &resultVTable && result.family == WebRtcAfInet && result.address.ip6.bytes[0] == 192 && result.address.ip6.bytes[3] == 7);
    Require(_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet6, &result));
    Require(result.family == WebRtcAfInet6 && result.address.ip6.bytes[15] == 9);
    Require(!_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfUnspec, &result));

    Require(Throws([] { _ZNK3rtc18NetworkManagerBase22enumeration_permissionEv_nid_postfix(nullptr); }));
    Require(Throws([&] { _ZN3rtc18NetworkManagerBase27set_default_local_addressesERKNS_9IPAddressES3__nid_postfix(&manager, nullptr, &ipv4); }));
    Require(Throws([&] { _ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet, nullptr); }));
}

void TestNetworkFromAddress() {
    WebRtcInterfaceAddress firstIps[] = {Ipv6(0x20, 0x01, 1)};
    WebRtcInterfaceAddress secondIps[] = {Ipv6(0xfe, 0x80, 2), Ipv6(0x20, 0x01, 3, 2), Ipv6(0xfc, 0x00, 4), Ipv6(0x20, 0x01, 5), Ipv6(0x20, 0x01, 6, 1), Ipv6(0x20, 0x01, 7)};
    WebRtcNetwork first{};
    WebRtcNetwork second{};
    first.prefix.family = WebRtcAfInet6;
    second.prefix.family = WebRtcAfInet6;
    SetIps(first, firstIps, 1);
    SetIps(second, secondIps, 6);
    WebRtcNetwork* networks[] = {&first, &second};
    WebRtcNetworkManagerBase manager{};
    manager.networksBegin = networks;
    manager.networksEnd = networks + 2;
    manager.networksCapacity = networks + 2;

    Require(_ZNK3rtc18NetworkManagerBase21GetNetworkFromAddressERKNS_9IPAddressE_nid_postfix(&manager, &AsIp(firstIps[0])) == &first);
    const auto lookup = Ipv6(0xfc, 0x00, 4);
    Require(_ZNK3rtc18NetworkManagerBase21GetNetworkFromAddressERKNS_9IPAddressE_nid_postfix(&manager, &AsIp(lookup)) == &second);
    const auto missing = Ipv6(0x20, 0x01, 99);
    Require(_ZNK3rtc18NetworkManagerBase21GetNetworkFromAddressERKNS_9IPAddressE_nid_postfix(&manager, &AsIp(missing)) == nullptr);
    const auto sameBytesIpv4 = Ipv4(0x20, 0x01, 0, 0);
    Require(_ZNK3rtc18NetworkManagerBase21GetNetworkFromAddressERKNS_9IPAddressE_nid_postfix(&manager, &sameBytesIpv4) == nullptr);
    Require(Throws([&] { _ZNK3rtc18NetworkManagerBase21GetNetworkFromAddressERKNS_9IPAddressE_nid_postfix(&manager, nullptr); }));

    const auto defaultIpv4 = Ipv4(10, 0, 0, 1);
    WebRtcIpAddress result{&ipVTable, WebRtcAfUnspec, {}};
    _ZN3rtc18NetworkManagerBase27set_default_local_addressesERKNS_9IPAddressES3__nid_postfix(&manager, &defaultIpv4, &AsIp(lookup));
    Require(_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet6, &result));
    Require(result.family == WebRtcAfInet6 && result.address.ip6.bytes[15] == 6);

    SetIps(second, secondIps, 4);
    Require(_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet6, &result));
    Require(result.address.ip6.bytes[15] == 5);

    SetIps(second, secondIps, 3);
    Require(_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet6, &result));
    Require(result.address.ip6.bytes[15] == 2);

    secondIps[0].ipv6Flags = 2;
    Require(_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet6, &result));
    Require(result.address.ip6.bytes[15] == 4);

    second.prefix.family = WebRtcAfInet;
    Require(_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet6, &result));
    Require(result.address.ip6.bytes[15] == 2);

    const auto onlyDeprecated = Ipv6(0x20, 0x01, 3);
    second.prefix.family = WebRtcAfInet6;
    SetIps(second, secondIps + 1, 1);
    _ZN3rtc18NetworkManagerBase27set_default_local_addressesERKNS_9IPAddressES3__nid_postfix(&manager, &defaultIpv4, &AsIp(onlyDeprecated));
    Require(_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet6, &result));
    Require(result.family == WebRtcAfUnspec);

    const auto unknown = Ipv6(0x20, 0x01, 42);
    _ZN3rtc18NetworkManagerBase27set_default_local_addressesERKNS_9IPAddressES3__nid_postfix(&manager, &defaultIpv4, &AsIp(unknown));
    Require(_ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(&manager, WebRtcAfInet6, &result));
    Require(result.family == WebRtcAfInet6 && result.address.ip6.bytes[15] == 42);
}

}

int main() {
    unsigned char manager[64] = {};
    Require(_ZNK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(manager) == nullptr);
    Require(_ZThn8_NK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(manager + 8) == nullptr);
    Require(_ZNK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(nullptr) == nullptr);
    TestDefaultLocalAddresses();
    TestNetworkFromAddress();
    return 0;
}
