/* Derived from rtc_base/socket_address.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
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
#include "prx/libc/GuestStringStorage.hpp"

#include <cstring>
#include <stdexcept>

namespace {

constexpr std::size_t CapacityMask = 15;

char* Data(WebRtcGuestString& string) {
    return string.capacity > WebRtcGuestStringInlineCapacity ? string.pointer : string.buffer;
}

void InitEmpty(WebRtcGuestString& string) {
    string.reserved = 0;
    std::memset(string.buffer, 0, sizeof(string.buffer));
    string.size = 0;
    string.capacity = WebRtcGuestStringInlineCapacity;
}

void AssignString(WebRtcGuestString& string, const WebRtcGuestString& source) {
    const char* sourceData = WebRtcGuestStringData(source);
    const std::size_t count = source.size;
    if (count <= string.capacity) {
        if (count != 0)
            std::memmove(Data(string), sourceData, count);
        string.size = count;
        Data(string)[count] = 0;
        return;
    }
    const std::size_t capacity = count | CapacityMask;
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    std::memcpy(data, sourceData, count);
    data[count] = 0;
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
    string.pointer = data;
    string.capacity = capacity;
    string.size = count;
}

void CopyFields(WebRtcSocketAddress& address, const WebRtcSocketAddress& other) {
    address.ip.family = other.ip.family;
    address.ip.address = other.ip.address;
    address.port = other.port;
    address.literal = other.literal;
    address.scopeId = other.scopeId;
}

}

extern "C" {

WebRtcSocketAddress* APS5_VABI _ZN3rtc13SocketAddressaSERKS0__nid_postfix(WebRtcSocketAddress* address, const WebRtcSocketAddress* other) {
    if (!address || !other)
        throw std::invalid_argument("SocketAddress::operator= requires two addresses");
    if (address == other)
        return address;
    AssignString(address->hostname, other->hostname);
    CopyFields(*address, *other);
    return address;
}

void APS5_VABI _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(WebRtcSocketAddress* address, const WebRtcSocketAddress* other) {
    if (!address || !other)
        throw std::invalid_argument("SocketAddress::SocketAddress requires two addresses");
    InitEmpty(address->hostname);
    AssignString(address->hostname, other->hostname);
    address->ip = WebRtcIpAddress{};
    CopyFields(*address, *other);
}

}
