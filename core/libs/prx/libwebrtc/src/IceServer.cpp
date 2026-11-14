/* Derived from api/peer_connection_interface.cc@8d78f5de6c27b2c793039989ea381f1428fb0100 */
/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

constexpr std::int32_t TlsCertPolicySecure = 0;
constexpr std::size_t StringCapacityMask = 15;

void InitString(WebRtcGuestString& string) {
    string.reserved = 0;
    string.buffer[0] = '\0';
    string.size = 0;
    string.capacity = WebRtcGuestStringInlineCapacity;
}

void ValidateString(const WebRtcGuestString& string) {
    if (string.capacity < WebRtcGuestStringInlineCapacity || string.size > string.capacity ||
        (string.capacity > WebRtcGuestStringInlineCapacity && !string.pointer))
        throw std::invalid_argument("IceServer requires valid strings");
}

void ReleaseString(WebRtcGuestString& string) {
    if (string.capacity > WebRtcGuestStringInlineCapacity)
        GuestStringStorage::Deallocate(string.pointer, string.capacity + 1);
    InitString(string);
}

void CopyString(WebRtcGuestString& string, const WebRtcGuestString& other) {
    InitString(string);
    const std::size_t size = other.size;
    char* data = string.buffer;
    if (size > WebRtcGuestStringInlineCapacity) {
        const std::size_t capacity = size | StringCapacityMask;
        data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
        string.pointer = data;
        string.capacity = capacity;
    }
    std::memcpy(data, WebRtcGuestStringData(other), size);
    data[size] = '\0';
    string.size = size;
}

void InitVector(WebRtcGuestStringVector& vector) {
    vector.begin = nullptr;
    vector.end = nullptr;
    vector.capacityEnd = nullptr;
}

void ValidateVector(const WebRtcGuestStringVector& vector) {
    const auto begin = reinterpret_cast<std::uintptr_t>(vector.begin);
    const auto end = reinterpret_cast<std::uintptr_t>(vector.end);
    const auto capacity = reinterpret_cast<std::uintptr_t>(vector.capacityEnd);
    if (end < begin || capacity < end || (!begin && capacity) || begin % alignof(WebRtcGuestString) ||
        (end - begin) % sizeof(WebRtcGuestString) || (capacity - begin) % sizeof(WebRtcGuestString))
        throw std::invalid_argument("IceServer requires valid string vectors");
    for (const WebRtcGuestString* string = vector.begin; string != vector.end; ++string)
        ValidateString(*string);
}

void ReleaseVector(WebRtcGuestStringVector& vector) {
    if (vector.begin) {
        for (WebRtcGuestString* string = vector.begin; string != vector.end; ++string)
            ReleaseString(*string);
        GuestStringStorage::Deallocate(vector.begin, static_cast<std::size_t>(vector.capacityEnd - vector.begin) * sizeof(WebRtcGuestString));
    }
    InitVector(vector);
}

void CopyVector(WebRtcGuestStringVector& vector, const WebRtcGuestStringVector& other) {
    InitVector(vector);
    if (!other.begin)
        return;
    const auto count = static_cast<std::size_t>(other.end - other.begin);
    if (count == 0)
        return;
    if (count > std::numeric_limits<std::size_t>::max() / sizeof(WebRtcGuestString))
        throw std::bad_alloc();
    auto* strings = static_cast<WebRtcGuestString*>(GuestStringStorage::Allocate(count * sizeof(WebRtcGuestString)));
    vector.begin = strings;
    vector.end = strings;
    vector.capacityEnd = strings + count;
    for (std::size_t index = 0; index < count; ++index) {
        CopyString(strings[index], other.begin[index]);
        vector.end = strings + index + 1;
    }
}

void InitServer(WebRtcIceServer& server) {
    InitString(server.uri);
    InitVector(server.urls);
    InitString(server.username);
    InitString(server.password);
    server.tlsCertPolicy = TlsCertPolicySecure;
    InitString(server.hostname);
    InitVector(server.tlsAlpnProtocols);
    InitVector(server.tlsEllipticCurves);
}

void ValidateServer(const WebRtcIceServer& server) {
    ValidateString(server.uri);
    ValidateVector(server.urls);
    ValidateString(server.username);
    ValidateString(server.password);
    ValidateString(server.hostname);
    ValidateVector(server.tlsAlpnProtocols);
    ValidateVector(server.tlsEllipticCurves);
}

void ReleaseServer(WebRtcIceServer& server) {
    ReleaseVector(server.tlsEllipticCurves);
    ReleaseVector(server.tlsAlpnProtocols);
    ReleaseString(server.hostname);
    ReleaseString(server.password);
    ReleaseString(server.username);
    ReleaseVector(server.urls);
    ReleaseString(server.uri);
}

}

extern "C" {

void APS5_VABI _ZN6webrtc23PeerConnectionInterface9IceServerC1Ev_nid_postfix(WebRtcIceServer* server) {
    if (!server)
        throw std::invalid_argument("IceServer constructor requires an object");
    InitServer(*server);
}

void APS5_VABI _ZN6webrtc23PeerConnectionInterface9IceServerC1ERKS1__nid_postfix(WebRtcIceServer* server, const WebRtcIceServer* other) {
    if (!server || !other || server == other)
        throw std::invalid_argument("IceServer copy constructor requires distinct objects");
    ValidateServer(*other);
    WebRtcIceServer copy;
    InitServer(copy);
    try {
        CopyString(copy.uri, other->uri);
        CopyVector(copy.urls, other->urls);
        CopyString(copy.username, other->username);
        CopyString(copy.password, other->password);
        copy.tlsCertPolicy = other->tlsCertPolicy;
        CopyString(copy.hostname, other->hostname);
        CopyVector(copy.tlsAlpnProtocols, other->tlsAlpnProtocols);
        CopyVector(copy.tlsEllipticCurves, other->tlsEllipticCurves);
    } catch (...) {
        ReleaseServer(copy);
        throw;
    }
    std::memcpy(server, &copy, sizeof(copy));
}

void APS5_VABI _ZN6webrtc23PeerConnectionInterface9IceServerD1Ev_nid_postfix(WebRtcIceServer* server) {
    if (!server)
        throw std::invalid_argument("IceServer destructor requires an object");
    ValidateServer(*server);
    ReleaseServer(*server);
}

}
