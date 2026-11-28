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
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Ipv6AddressFlagTemporary = 1 << 0;
constexpr std::int32_t Ipv6AddressFlagDeprecated = 1 << 1;

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void RequireReadable(const void* source, std::size_t bytes, const std::string& message) {
    if (!bytes) return;
    ErrnoGuard guard;
    if (!source)
        throw std::invalid_argument(message);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
    if (!GuestReadable_nid_no_patch(source, bytes))
        throw std::invalid_argument(message);
}

void CopyIn(void* destination, const void* source, std::size_t bytes, const std::string& message) {
    if (!bytes) return;
    RequireReadable(source, bytes, message);
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

class GuestWriteScope {
public:
    GuestWriteScope(void* pointer, std::size_t bytes, const std::string& message)
        : guard(pointer ? pointer : nullptr, pointer ? bytes : 0) {
        if (!bytes) return;
        ErrnoGuard guardErrno;
        if (!pointer || !guard.Open() || !GuestWritable_nid_no_patch(pointer, bytes))
            throw std::invalid_argument(message);
    }
    GuestWriteScope(const GuestWriteScope&) = delete;
    GuestWriteScope& operator=(const GuestWriteScope&) = delete;

private:
    GuestArena::HostWrite guard;
};

void RequireWritable(void* pointer, std::size_t bytes, const std::string& message) {
    GuestWriteScope scope(pointer, bytes, message);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const std::string& message) {
    if (!bytes) return;
    GuestWriteScope scope(destination, bytes, message);
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

template <typename T>
std::vector<T> CopyRange(const T* begin, const T* end, const std::string& message) {
    if (begin == end)
        return {};
    const auto beginAddr = reinterpret_cast<std::uintptr_t>(begin);
    const auto endAddr = reinterpret_cast<std::uintptr_t>(end);
    if (!begin || !end || endAddr < beginAddr || (endAddr - beginAddr) % sizeof(T) != 0)
        throw std::invalid_argument(message);
    const std::size_t bytes = endAddr - beginAddr;
    RequireReadable(begin, bytes, message);
    std::vector<T> items(bytes / sizeof(T));
    CopyIn(items.data(), begin, bytes, message);
    return items;
}

WebRtcNetworkManagerBase ReadManager(const WebRtcNetworkManagerBase* manager, const char* function) {
    const std::string message = std::string(function) + " requires a network manager";
    if (!manager)
        throw std::invalid_argument(message);
    WebRtcNetworkManagerBase copy{};
    CopyIn(&copy, manager, sizeof(copy), message);
    return copy;
}

WebRtcIpAddress ReadIp(const WebRtcIpAddress* ip, const char* message) {
    if (!ip)
        throw std::invalid_argument(message);
    WebRtcIpAddress copy{};
    CopyIn(&copy, ip, sizeof(copy), message);
    return copy;
}

void RequireAddressWritable(void* base, std::size_t addressOffset, const char* message) {
    auto* bytes = static_cast<unsigned char*>(base) + addressOffset;
    RequireWritable(bytes + offsetof(WebRtcIpAddress, family), sizeof(std::int32_t), message);
    RequireWritable(bytes + offsetof(WebRtcIpAddress, address), sizeof(WebRtcIn6Addr), message);
}

void WriteAddress(void* base, std::size_t addressOffset, std::int32_t family, const WebRtcIn6Addr& address, const char* message) {
    auto* bytes = static_cast<unsigned char*>(base) + addressOffset;
    CopyOut(bytes + offsetof(WebRtcIpAddress, family), &family, sizeof(family), message);
    CopyOut(bytes + offsetof(WebRtcIpAddress, address), &address, sizeof(address), message);
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

std::optional<WebRtcInterfaceAddress> BestIp(const WebRtcNetwork& network, const std::vector<WebRtcInterfaceAddress>& ips) {
    if (ips.empty())
        return std::nullopt;
    if (network.prefix.family == WebRtcAfInet)
        return ips.front();
    const WebRtcInterfaceAddress* selected = nullptr;
    const WebRtcInterfaceAddress* linkLocal = nullptr;
    const WebRtcInterfaceAddress* ula = nullptr;
    for (const auto& ip : ips) {
        if (ip.ipv6Flags & Ipv6AddressFlagDeprecated)
            continue;
        if (IsLinkLocal(ip)) {
            linkLocal = &ip;
            continue;
        }
        if (IsUla(ip)) {
            ula = &ip;
            continue;
        }
        selected = &ip;
        if (ip.ipv6Flags & Ipv6AddressFlagTemporary)
            break;
    }
    if (!selected || selected->family == WebRtcAfUnspec) {
        if (linkLocal && linkLocal->family != WebRtcAfUnspec)
            return *linkLocal;
        if (ula && ula->family != WebRtcAfUnspec)
            return *ula;
    }
    if (!selected)
        return std::nullopt;
    return *selected;
}

struct MatchedNetwork {
    WebRtcNetwork* guest = nullptr;
    WebRtcNetwork copy{};
    std::vector<WebRtcInterfaceAddress> ips;
};

std::optional<MatchedNetwork> NetworkFromAddress(const WebRtcNetworkManagerBase& manager, const WebRtcIpAddress& ip, const char* function) {
    const std::string message = std::string(function) + " requires readable network entries";
    const auto networks = CopyRange(manager.networksBegin, manager.networksEnd, message);
    for (auto* network : networks) {
        if (!network)
            throw std::invalid_argument(message);
        WebRtcNetwork netCopy{};
        CopyIn(&netCopy, network, sizeof(netCopy), message);
        auto ips = CopyRange(netCopy.ipsBegin, netCopy.ipsEnd, message);
        for (const auto& existing : ips) {
            if (_ZNK3rtc9IPAddresseqERKS0__nid_postfix(&ip, reinterpret_cast<const WebRtcIpAddress*>(&existing)))
                return MatchedNetwork{network, netCopy, std::move(ips)};
        }
    }
    return std::nullopt;
}

}

extern "C" {

std::int32_t APS5_VABI _ZNK3rtc18NetworkManagerBase22enumeration_permissionEv_nid_postfix(const WebRtcNetworkManagerBase* manager) {
    return ReadManager(manager, "NetworkManagerBase::enumeration_permission").enumerationPermission;
}

void APS5_VABI _ZN3rtc18NetworkManagerBase27set_default_local_addressesERKNS_9IPAddressES3__nid_postfix(
    WebRtcNetworkManagerBase* manager, const WebRtcIpAddress* ipv4, const WebRtcIpAddress* ipv6) {
    ReadManager(manager, "NetworkManagerBase::set_default_local_addresses");
    if (!ipv4 || !ipv6)
        throw std::invalid_argument("NetworkManagerBase::set_default_local_addresses requires addresses");
    const auto ipv4Copy = ReadIp(ipv4, "NetworkManagerBase::set_default_local_addresses requires addresses");
    const auto ipv6Copy = ReadIp(ipv6, "NetworkManagerBase::set_default_local_addresses requires addresses");
    const char* unwritable = "NetworkManagerBase::set_default_local_addresses requires a writable network manager";
    if (ipv4Copy.family == WebRtcAfInet)
        RequireAddressWritable(manager, offsetof(WebRtcNetworkManagerBase, defaultLocalIpv4Address), unwritable);
    if (ipv6Copy.family == WebRtcAfInet6)
        RequireAddressWritable(manager, offsetof(WebRtcNetworkManagerBase, defaultLocalIpv6Address), unwritable);
    if (ipv4Copy.family == WebRtcAfInet)
        WriteAddress(manager, offsetof(WebRtcNetworkManagerBase, defaultLocalIpv4Address), ipv4Copy.family, ipv4Copy.address.ip6, unwritable);
    if (ipv6Copy.family == WebRtcAfInet6)
        WriteAddress(manager, offsetof(WebRtcNetworkManagerBase, defaultLocalIpv6Address), ipv6Copy.family, ipv6Copy.address.ip6, unwritable);
}

bool APS5_VABI _ZNK3rtc18NetworkManagerBase22GetDefaultLocalAddressEiPNS_9IPAddressE_nid_postfix(
    const WebRtcNetworkManagerBase* manager, std::int32_t family, WebRtcIpAddress* address) {
    const auto checked = ReadManager(manager, "NetworkManagerBase::GetDefaultLocalAddress");
    const WebRtcIpAddress* source = nullptr;
    if (family == WebRtcAfInet && checked.defaultLocalIpv4Address.family != WebRtcAfUnspec)
        source = &checked.defaultLocalIpv4Address;
    else if (family == WebRtcAfInet6 && checked.defaultLocalIpv6Address.family != WebRtcAfUnspec)
        source = &checked.defaultLocalIpv6Address;
    if (!source)
        return false;
    const char* badAddress = "NetworkManagerBase::GetDefaultLocalAddress requires an address";
    if (!address)
        throw std::invalid_argument(badAddress);
    RequireAddressWritable(address, 0, badAddress);
    if (family == WebRtcAfInet6) {
        if (const auto network = NetworkFromAddress(checked, *source, "NetworkManagerBase::GetDefaultLocalAddress")) {
            const auto best = BestIp(network->copy, network->ips);
            WriteAddress(address, 0, best ? best->family : WebRtcAfUnspec, best ? best->address.ip6 : WebRtcIn6Addr{}, badAddress);
            return true;
        }
    }
    WriteAddress(address, 0, source->family, source->address.ip6, badAddress);
    return true;
}

WebRtcNetwork* APS5_VABI _ZNK3rtc18NetworkManagerBase21GetNetworkFromAddressERKNS_9IPAddressE_nid_postfix(
    const WebRtcNetworkManagerBase* manager, const WebRtcIpAddress* ip) {
    const auto checked = ReadManager(manager, "NetworkManagerBase::GetNetworkFromAddress");
    const auto ipCopy = ReadIp(ip, "NetworkManagerBase::GetNetworkFromAddress requires an address");
    const auto match = NetworkFromAddress(checked, ipCopy, "NetworkManagerBase::GetNetworkFromAddress");
    return match ? match->guest : nullptr;
}


void* APS5_VABI _ZNK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(const void*) {
    return nullptr;
}

void* APS5_VABI _ZThn8_NK3rtc14NetworkManager16GetMdnsResponderEv_nid_postfix(const void*) {
    return nullptr;
}

}
