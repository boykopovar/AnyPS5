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
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t CapacityMask = 15;

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() { errno = savedErrno; }
};

void RequireReadable(const void* source, std::size_t bytes, const char* message) {
    if (!bytes) return;
    ErrnoGuard guard;
    if (!source)
        throw std::invalid_argument(message);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
    if (!GuestReadable_nid_no_patch(source, bytes))
        throw std::invalid_argument(message);
}

void CopyIn(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!bytes) return;
    RequireReadable(source, bytes, message);
    ErrnoGuard guard;
    if (!GuestCopyIn_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

class GuestWriteScope {
public:
    GuestWriteScope(void* pointer, std::size_t bytes, const char* message)
        : savedErrno(), guard(pointer ? pointer : nullptr, pointer ? bytes : 0) {
        if (!bytes) return;
        if (!pointer || !guard.Open() || !GuestWritable_nid_no_patch(pointer, bytes))
            throw std::invalid_argument(message);
    }
    GuestWriteScope(const GuestWriteScope&) = delete;
    GuestWriteScope& operator=(const GuestWriteScope&) = delete;

private:
    ErrnoGuard savedErrno;
    GuestArena::HostWrite guard;
};

void RequireWritable(void* pointer, std::size_t bytes, const char* message) {
    GuestWriteScope scope(pointer, bytes, message);
}

void CopyOut(void* destination, const void* source, std::size_t bytes, const char* message) {
    if (!bytes) return;
    GuestWriteScope scope(destination, bytes, message);
    ErrnoGuard guard;
    if (!GuestCopyOut_nid_no_patch(destination, source, bytes))
        throw std::invalid_argument(message);
}

struct AllocatedBuffer {
    char* ptr = nullptr;
    std::size_t bytes = 0;

    ~AllocatedBuffer() {
        if (ptr)
            GuestStringStorage::Deallocate(ptr, bytes);
    }

    char* Release() {
        char* out = ptr;
        ptr = nullptr;
        bytes = 0;
        return out;
    }
};

std::string ReadGuestString(const WebRtcGuestString& source, const char* message) {
    if (source.size > source.capacity)
        throw std::invalid_argument(message);
    if (source.capacity <= WebRtcGuestStringInlineCapacity)
        return std::string(source.buffer, source.size);
    if (!source.pointer && source.size != 0)
        throw std::invalid_argument(message);
    RequireReadable(source.pointer, source.size, message);
    std::string text(source.size, '\0');
    if (source.size != 0)
        CopyIn(text.data(), source.pointer, source.size, message);
    return text;
}

void ValidateOldHeapStorage(char* storage, std::size_t capacity, const char* message) {
    if (capacity > std::numeric_limits<std::size_t>::max() - 1)
        throw std::invalid_argument(message);
    const std::size_t bytes = capacity + 1;
    if (!storage)
        throw std::invalid_argument(message);
    RequireReadable(storage, 1, message);
    if (bytes >= GuestStringStorage::LargeAllocation) {
        const auto addr = reinterpret_cast<std::uintptr_t>(storage);
        if (addr < sizeof(void*) || (addr & (GuestStringStorage::AllocationAlignment - 1)) != 0)
            throw std::invalid_argument(message);
        void* raw = nullptr;
        CopyIn(&raw, reinterpret_cast<const unsigned char*>(storage) - sizeof(void*), sizeof(raw), message);
        const auto rawAddr = reinterpret_cast<std::uintptr_t>(raw);
        if (!raw || addr < rawAddr + sizeof(void*) || addr - rawAddr > GuestStringStorage::AllocationPadding)
            throw std::invalid_argument(message);
    }
}

void InitEmpty(WebRtcGuestString& string) {
    string.reserved = 0;
    std::memset(string.buffer, 0, sizeof(string.buffer));
    string.size = 0;
    string.capacity = WebRtcGuestStringInlineCapacity;
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
    ErrnoGuard exportErrno;
    if (!address || !other)
        throw std::invalid_argument("SocketAddress::operator= requires two addresses");
    if (address == other)
        return address;
    const char* unreadable = "SocketAddress::operator= requires readable addresses";
    const char* unwritable = "SocketAddress::operator= requires a writable address";
    WebRtcSocketAddress sourceCopy{};
    CopyIn(&sourceCopy, other, sizeof(sourceCopy), unreadable);
    const std::string sourceText = ReadGuestString(sourceCopy.hostname, unreadable);

    WebRtcSocketAddress destCopy{};
    CopyIn(&destCopy, address, sizeof(destCopy), unreadable);
    if (destCopy.hostname.size > destCopy.hostname.capacity)
        throw std::invalid_argument(unreadable);
    if (destCopy.hostname.capacity > WebRtcGuestStringInlineCapacity && !destCopy.hostname.pointer)
        throw std::invalid_argument(unreadable);

    RequireWritable(address, sizeof(destCopy), unwritable);
    const std::size_t count = sourceText.size();
    char* oldPointer = nullptr;
    std::size_t oldCapacity = 0;
    AllocatedBuffer newBuffer;

    if (count <= destCopy.hostname.capacity) {
        if (destCopy.hostname.capacity > WebRtcGuestStringInlineCapacity) {
            RequireWritable(destCopy.hostname.pointer, count + 1, unwritable);
            if (count != 0)
                CopyOut(destCopy.hostname.pointer, sourceText.data(), count, unwritable);
            const char terminator = '\0';
            CopyOut(destCopy.hostname.pointer + count, &terminator, 1, unwritable);
        } else {
            if (count != 0)
                std::memcpy(destCopy.hostname.buffer, sourceText.data(), count);
            destCopy.hostname.buffer[count] = '\0';
        }
        destCopy.hostname.size = count;
    } else {
        if (destCopy.hostname.capacity > WebRtcGuestStringInlineCapacity) {
            ValidateOldHeapStorage(destCopy.hostname.pointer, destCopy.hostname.capacity, unreadable);
            oldPointer = destCopy.hostname.pointer;
            oldCapacity = destCopy.hostname.capacity;
        }
        if (count > std::numeric_limits<std::size_t>::max() - CapacityMask - 1)
            throw std::invalid_argument("SocketAddress::operator= hostname length overflow");
        const std::size_t capacity = count | CapacityMask;
        newBuffer.bytes = capacity + 1;
        newBuffer.ptr = static_cast<char*>(GuestStringStorage::Allocate(newBuffer.bytes));
        if (!newBuffer.ptr)
            throw std::invalid_argument("SocketAddress::operator= allocation failed");
        CopyOut(newBuffer.ptr, sourceText.data(), count, unwritable);
        const char terminator = '\0';
        CopyOut(newBuffer.ptr + count, &terminator, 1, unwritable);
        destCopy.hostname.pointer = newBuffer.ptr;
        destCopy.hostname.capacity = capacity;
        destCopy.hostname.size = count;
    }

    CopyFields(destCopy, sourceCopy);
    CopyOut(address, &destCopy, sizeof(destCopy), unwritable);
    newBuffer.Release();
    if (oldPointer)
        GuestStringStorage::Deallocate(oldPointer, oldCapacity + 1);
    return address;
}

void APS5_VABI _ZN3rtc13SocketAddressC1ERKS0__nid_postfix(WebRtcSocketAddress* address, const WebRtcSocketAddress* other) {
    ErrnoGuard exportErrno;
    if (!address || !other)
        throw std::invalid_argument("SocketAddress::SocketAddress requires two addresses");
    const char* unreadable = "SocketAddress::SocketAddress requires a readable address";
    const char* unwritable = "SocketAddress::SocketAddress requires a writable address";
    WebRtcSocketAddress sourceCopy{};
    CopyIn(&sourceCopy, other, sizeof(sourceCopy), unreadable);
    const std::string sourceText = ReadGuestString(sourceCopy.hostname, unreadable);
    RequireWritable(address, sizeof(WebRtcSocketAddress), unwritable);

    WebRtcSocketAddress result{};
    InitEmpty(result.hostname);
    const std::size_t count = sourceText.size();
    AllocatedBuffer newBuffer;
    if (count <= WebRtcGuestStringInlineCapacity) {
        if (count != 0)
            std::memcpy(result.hostname.buffer, sourceText.data(), count);
        result.hostname.buffer[count] = '\0';
        result.hostname.size = count;
    } else {
        if (count > std::numeric_limits<std::size_t>::max() - CapacityMask - 1)
            throw std::invalid_argument("SocketAddress::SocketAddress hostname length overflow");
        const std::size_t capacity = count | CapacityMask;
        newBuffer.bytes = capacity + 1;
        newBuffer.ptr = static_cast<char*>(GuestStringStorage::Allocate(newBuffer.bytes));
        if (!newBuffer.ptr)
            throw std::invalid_argument("SocketAddress::SocketAddress allocation failed");
        CopyOut(newBuffer.ptr, sourceText.data(), count, unwritable);
        const char terminator = '\0';
        CopyOut(newBuffer.ptr + count, &terminator, 1, unwritable);
        result.hostname.pointer = newBuffer.ptr;
        result.hostname.capacity = capacity;
        result.hostname.size = count;
    }

    result.ip = WebRtcIpAddress{};
    CopyFields(result, sourceCopy);
    CopyOut(address, &result, sizeof(result), unwritable);
    newBuffer.Release();
}

}
