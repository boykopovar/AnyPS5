#pragma once

#include "prx/libwebrtc/include/GuestString.hpp"
#include "prx/libwebrtc/include/IpAddress.hpp"

#include <cstddef>
#include <cstdint>

struct WebRtcSigslotSignal {
    void* slotDisconnect;
    void* slotDuplicate;
    void* connectedSlots[3];
    void* currentIterator;
    bool eraseCurrentIterator;
};

struct WebRtcInterfaceAddress {
    const void* vtable;
    std::int32_t family;
    union {
        WebRtcInAddr ip4;
        WebRtcIn6Addr ip6;
    } address;
    std::int32_t ipv6Flags;
};

struct WebRtcNetwork {
    WebRtcSigslotSignal signalTypeChanged;
    WebRtcSigslotSignal signalNetworkPreferenceChanged;
    const void* defaultLocalAddressProvider;
    const void* mdnsResponderProvider;
    WebRtcGuestString name;
    WebRtcGuestString description;
    WebRtcIpAddress prefix;
    std::int32_t prefixLength;
    WebRtcGuestString key;
    WebRtcInterfaceAddress* ipsBegin;
    WebRtcInterfaceAddress* ipsEnd;
    WebRtcInterfaceAddress* ipsCapacity;
};

struct WebRtcNetworkManagerBase {
    const void* defaultLocalAddressProviderVTable;
    const void* mdnsResponderProviderVTable;
    WebRtcSigslotSignal signalNetworksChanged;
    WebRtcSigslotSignal signalError;
    std::int32_t enumerationPermission;
    WebRtcNetwork** networksBegin;
    WebRtcNetwork** networksEnd;
    WebRtcNetwork** networksCapacity;
    void* networksMap[3];
    WebRtcNetwork* ipv4AnyAddressNetwork;
    WebRtcNetwork* ipv6AnyAddressNetwork;
    WebRtcIpAddress defaultLocalIpv4Address;
    WebRtcIpAddress defaultLocalIpv6Address;
    std::uint16_t nextAvailableNetworkId;
};

static_assert(sizeof(WebRtcSigslotSignal) == 56);
static_assert(offsetof(WebRtcSigslotSignal, eraseCurrentIterator) == 48);
static_assert(sizeof(WebRtcInterfaceAddress) == 32);
static_assert(offsetof(WebRtcInterfaceAddress, ipv6Flags) == 28);
static_assert(offsetof(WebRtcNetwork, defaultLocalAddressProvider) == 112);
static_assert(offsetof(WebRtcNetwork, name) == 128);
static_assert(offsetof(WebRtcNetwork, description) == 168);
static_assert(offsetof(WebRtcNetwork, prefix) == 208);
static_assert(offsetof(WebRtcNetwork, prefixLength) == 240);
static_assert(offsetof(WebRtcNetwork, key) == 248);
static_assert(offsetof(WebRtcNetwork, ipsBegin) == 288);
static_assert(sizeof(WebRtcNetworkManagerBase) == 272);
static_assert(offsetof(WebRtcNetworkManagerBase, signalNetworksChanged) == 16);
static_assert(offsetof(WebRtcNetworkManagerBase, signalError) == 72);
static_assert(offsetof(WebRtcNetworkManagerBase, enumerationPermission) == 128);
static_assert(offsetof(WebRtcNetworkManagerBase, networksBegin) == 136);
static_assert(offsetof(WebRtcNetworkManagerBase, networksMap) == 160);
static_assert(offsetof(WebRtcNetworkManagerBase, ipv4AnyAddressNetwork) == 184);
static_assert(offsetof(WebRtcNetworkManagerBase, defaultLocalIpv4Address) == 200);
static_assert(offsetof(WebRtcNetworkManagerBase, defaultLocalIpv6Address) == 232);
static_assert(offsetof(WebRtcNetworkManagerBase, nextAvailableNetworkId) == 264);
