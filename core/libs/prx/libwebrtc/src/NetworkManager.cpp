/* Derived from rtc_base/network.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

constexpr std::int32_t Ipv6AddressFlagTemporary = 1 << 0;
constexpr std::int32_t Ipv6AddressFlagDeprecated = 1 << 1;

const WebRtcNetworkManagerBase& CheckedManager(const WebRtcNetworkManagerBase* manager, const char* function) {
    if (!manager)
        throw std::invalid_argument(std::string(function) + " requires a network manager");
    return *manager;
}

void AssignAddress(WebRtcIpAddress& destination, std::int32_t family, const WebRtcIn6Addr& address) {
    destination.family = family;
    destination.address.ip6 = address;
}

void AssignAddress(WebRtcIpAddress& destination, const WebRtcIpAddress& source) {
    AssignAddress(destination, source.family, source.address.ip6);
}

bool IsLinkLocal(const WebRtcInterfaceAddress& ip) {
    if (ip.family == WebRtcAfInet)
        return ip.address.ip6.bytes[0] == 169 && ip.address.ip6.bytes[1] == 254;
    if (ip.family == WebRtcAfInet6)
        return ip.address.ip6.bytes[0] == 0xfe && (ip.address.ip6.bytes[1] & 0xc0) == 0x80;
    return false;
}

bool IsUla(const WebRtcInterfaceAddress& ip) {
    return (ip.address.ip6.bytes[0] & 0xfe) == 0xfc;
}

const WebRtcInterfaceAddress* BestIp(const WebRtcNetwork& network) {
    if (network.ipsBegin == network.ipsEnd)
        return nullptr;
    if (network.prefix.family == WebRtcAfInet)
        return network.ipsBegin;
    const WebRtcInterfaceAddress* selected = nullptr;
    const WebRtcInterfaceAddress* linkLocal = nullptr;
    const WebRtcInterfaceAddress* ula = nullptr;
    for (const auto* ip = network.ipsBegin; ip != network.ipsEnd; ++ip) {
        if (ip->ipv6Flags & Ipv6AddressFlagDeprecated)
            continue;
        if (IsLinkLocal(*ip)) {
            linkLocal = ip;
            continue;
        }
        if (IsUla(*ip)) {
            ula = ip;
            continue;
        }
        selected = ip;
        if (ip->ipv6Flags & Ipv6AddressFlagTemporary)
            break;
    }
    if (!selected || selected->family == WebRtcAfUnspec) {
        if (linkLocal && linkLocal->family != WebRtcAfUnspec)
            return linkLocal;
        if (ula && ula->family != WebRtcAfUnspec)
            return ula;
    }
    return selected;
}

WebRtcNetwork* NetworkFromAddress(const WebRtcNetworkManagerBase& manager, const WebRtcIpAddress& ip) {
    for (auto** network = manager.networksBegin; network != manager.networksEnd; ++network) {
        for (const auto* existing = (*network)->ipsBegin; existing != (*network)->ipsEnd; ++existing) {
            if (_ZNK3rtc9IPAddresseqERKS0__nid_postfix(&ip, reinterpret_cast<const WebRtcIpAddress*>(existing)))
                return *network;
        }
    }
    return nullptr;
}

}

extern "C" {

std::int32_t APS5_VABI _ZNK3rtc18NetworkManagerBase22enumeration_permissionEv_nid_postfix(const WebRtcNetworkManagerBase* manager) {
    return CheckedManager(manager, "NetworkManagerBase::enumeration_permission").enumerationPermission;
}

void APS5_VABI _ZN3rtc18NetworkManagerBase27set_default_local_addressesERKNS_9IPAddressES3__nid_postfix(
    WebRtcNetworkManagerBase* manager, const WebRtcIpAddress* ipv4, const WebRtcIpAddress* ipv6) {
    CheckedManager(manager, "NetworkManagerBase::set_default_local_addresses");
    if (!ipv4 || !ipv6)
        throw std::invalid_argument("NetworkManagerBase::set_default_local_addresses requires addresses");
    if (ipv4->family == WebRtcAfInet)
        AssignAddress(manager->defaultLocalIpv4Address, *ipv4);
    if (ipv6->family == WebRtcAfInet6)
        AssignAddress(manager->defaultLocalIpv6Address, *ipv6);
}

bool APS5_VABI _ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(
    const WebRtcNetworkManagerBase* manager, std::int32_t family, WebRtcIpAddress* address) {
    const auto& checked = CheckedManager(manager, "NetworkManagerBase::GetDefaultLocalAddress");
    const WebRtcIpAddress* source = nullptr;
    if (family == WebRtcAfInet && checked.defaultLocalIpv4Address.family != WebRtcAfUnspec)
        source = &checked.defaultLocalIpv4Address;
    else if (family == WebRtcAfInet6 && checked.defaultLocalIpv6Address.family != WebRtcAfUnspec)
        source = &checked.defaultLocalIpv6Address;
    if (!source)
        return false;
    if (!address)
        throw std::invalid_argument("NetworkManagerBase::GetDefaultLocalAddress requires an address");
    if (family == WebRtcAfInet6) {
        if (const auto* network = NetworkFromAddress(checked, *source)) {
            const auto* best = BestIp(*network);
            AssignAddress(*address, best ? best->family : WebRtcAfUnspec, best ? best->address.ip6 : WebRtcIn6Addr{});
            return true;
        }
    }
    AssignAddress(*address, *source);
    return true;
}

WebRtcNetwork* APS5_VABI _ZNK3rtc18NetworkManagerBase21GetNetworkFromAddressERKNS_9IPAddressE_nid_postfix(
    const WebRtcNetworkManagerBase* manager, const WebRtcIpAddress* ip) {
    const auto& checked = CheckedManager(manager, "NetworkManagerBase::GetNetworkFromAddress");
    if (!ip)
        throw std::invalid_argument("NetworkManagerBase::GetNetworkFromAddress requires an address");
    return NetworkFromAddress(checked, *ip);
}


void* APS5_VABI _ZNK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(const void*) {
    return nullptr;
}

void* APS5_VABI _ZThn8_NK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(const void*) {
    return nullptr;
}

}
