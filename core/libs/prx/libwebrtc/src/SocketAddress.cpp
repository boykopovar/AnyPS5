/* Derived from rtc_base/socket_address.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/* Derived from rtc_base/ip_address.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
#include <cstring>
#include <stdexcept>

namespace {

const WebRtcSocketAddress& Checked(const WebRtcSocketAddress* address, const char* function) {
    if (!address)
        throw std::invalid_argument(function);
    return *address;
}

WebRtcSocketAddress& Writable(WebRtcSocketAddress* address, const char* function) {
    if (!address)
        throw std::invalid_argument(function);
    return *address;
}

void InitHostname(WebRtcGuestString& hostname) {
    hostname.reserved = 0;
    std::memset(hostname.buffer, 0, sizeof(hostname.buffer));
    hostname.size = 0;
    hostname.capacity = 15;
}

bool IsAny(const WebRtcIpAddress& ip) {
    static const std::uint8_t mappedPrefix[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0};
    static const std::uint8_t any[16] = {};
    if (ip.family == WebRtcAfInet)
        return ip.address.ip4.sAddr == 0;
    if (ip.family == WebRtcAfInet6)
        return std::memcmp(ip.address.ip6.bytes, any, sizeof(any)) == 0 || std::memcmp(ip.address.ip6.bytes, mappedPrefix, sizeof(mappedPrefix)) == 0;
    return false;
}

}

extern "C" {

std::uint16_t APS5_VABI _ZNK3rtc13SocketAddress4portEv_nid_postfix(const WebRtcSocketAddress* address) {
    return Checked(address, "SocketAddress::port requires an address").port;
}

const WebRtcIpAddress* APS5_VABI _ZNK3rtc13SocketAddress6ipaddrEv_nid_postfix(const WebRtcSocketAddress* address) {
    return &Checked(address, "SocketAddress::ipaddr requires an address").ip;
}

bool APS5_VABI _ZNK3rtc13SocketAddress7IsAnyIPEv_nid_postfix(const WebRtcSocketAddress* address) {
    return IsAny(Checked(address, "SocketAddress::IsAnyIP requires an address").ip);
}

bool APS5_VABI _ZNK3rtc13SocketAddress10IsCompleteEv_nid_postfix(const WebRtcSocketAddress* address) {
    const WebRtcSocketAddress& checked = Checked(address, "SocketAddress::IsComplete requires an address");
    return !IsAny(checked.ip) && checked.port != 0;
}

bool APS5_VABI _ZNK3rtc13SocketAddress5IsNilEv_nid_postfix(const WebRtcSocketAddress* address) {
    const WebRtcSocketAddress& checked = Checked(address, "SocketAddress::IsNil requires an address");
    return checked.hostname.size == 0 && checked.ip.family == WebRtcAfUnspec && checked.port == 0;
}

void APS5_VABI _ZN3rtc13SocketAddressC1Ev_nid_postfix(WebRtcSocketAddress* address) {
    WebRtcSocketAddress& target = Writable(address, "SocketAddress::SocketAddress requires an address");
    InitHostname(target.hostname);
    target.ip = WebRtcIpAddress{};
    target.port = 0;
    target.scopeId = 0;
    target.literal = false;
}

void APS5_VABI _ZN3rtc13SocketAddressC1ERKNS_9IPAddressEi_nid_postfix(WebRtcSocketAddress* address, const WebRtcIpAddress* ip, std::int32_t port) {
    WebRtcSocketAddress& target = Writable(address, "SocketAddress::SocketAddress requires an address");
    if (!ip)
        throw std::invalid_argument("SocketAddress::SocketAddress requires an IP address");
    const WebRtcIpAddress source = *ip;
    InitHostname(target.hostname);
    target.ip = WebRtcIpAddress{};
    target.ip.family = source.family;
    target.ip.address = source.address;
    target.port = static_cast<std::uint16_t>(port);
    target.scopeId = 0;
    target.literal = false;
}

}
