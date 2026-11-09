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

namespace {

WebRtcGuestString CopyString(const WebRtcGuestString& source) {
    WebRtcGuestString result{};
    result.reserved = source.reserved;
    result.size = source.size;
    result.capacity = source.capacity;
    const char* sourceData = WebRtcGuestStringData(source);
    if (source.capacity <= WebRtcGuestStringInlineCapacity) {
        std::memcpy(result.buffer, sourceData, source.size + 1);
    } else {
        result.pointer = static_cast<char*>(GuestStringStorage::Allocate(source.capacity + 1));
        std::memcpy(result.pointer, sourceData, source.size + 1);
    }
    return result;
}

void ReleaseString(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
}

WebRtcSocketAddress* Assign(WebRtcSocketAddress* address, const WebRtcSocketAddress* other) {
    if (address == other)
        return address;
    WebRtcGuestString hostname = CopyString(other->hostname);
    ReleaseString(address->hostname);
    address->hostname = hostname;
    address->ip = other->ip;
    address->port = other->port;
    address->literal = other->literal;
    address->scopeId = other->scopeId;
    return address;
}

}

extern "C" {

WebRtcSocketAddress* APS5_VABI _ZN3rtc13SocketAddressaSERKS0__nid_postfix(WebRtcSocketAddress* address, const WebRtcSocketAddress* other) {
    return Assign(address, other);
}

void APS5_VABI _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(WebRtcSocketAddress* address, const WebRtcSocketAddress* other) {
    address->hostname = CopyString(other->hostname);
    address->ip = other->ip;
    address->port = other->port;
    address->literal = other->literal;
    address->scopeId = other->scopeId;
}

}
