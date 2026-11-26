// Derived from Source/WTF/wtf/SHA1.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 2011, 2014-2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from Source/WTF/wtf/persistence/PersistentCoders.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

/*
 * Copyright (C) 2010, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from Source/WTF/wtf/persistence/PersistentEncoder.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/WtfSha1Exports.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>

namespace {

using Jsc::WtfAbi::PersistenceAllocator;
using Jsc::WtfAbi::PersistenceEncoder;
using Jsc::WtfAbi::SHA1;

std::atomic<const PersistenceAllocator*> registeredPersistenceAllocator{nullptr};

constexpr std::size_t BlockSize = 64;
constexpr std::size_t LengthOffset = 56;

std::uint32_t F(int t, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    if (t < 20) return (b & c) | ((~b) & d);
    if (t < 40) return b ^ c ^ d;
    if (t < 60) return (b & c) | (b & d) | (c & d);
    return b ^ c ^ d;
}

std::uint32_t K(int t) {
    if (t < 20) return 0x5a827999;
    if (t < 40) return 0x6ed9eba1;
    if (t < 60) return 0x8f1bbcdc;
    return 0xca62c1d6;
}

std::uint32_t RotateLeft(int n, std::uint32_t x) {
    return (x << n) | (x >> (32 - n));
}

void ProcessBlock(SHA1& sha1) {
    std::uint32_t w[80] = {0};
    for (int t = 0; t < 16; ++t) {
        const std::uint8_t* bytes = sha1.buffer + t * 4;
        w[t] = (std::uint32_t{bytes[0]} << 24) | (std::uint32_t{bytes[1]} << 16) | (std::uint32_t{bytes[2]} << 8) | bytes[3];
    }
    for (int t = 16; t < 80; ++t)
        w[t] = RotateLeft(1, w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16]);

    std::uint32_t a = sha1.hash[0];
    std::uint32_t b = sha1.hash[1];
    std::uint32_t c = sha1.hash[2];
    std::uint32_t d = sha1.hash[3];
    std::uint32_t e = sha1.hash[4];

    for (int t = 0; t < 80; ++t) {
        std::uint32_t temp = RotateLeft(5, a) + F(t, b, c, d) + e + w[t] + K(t);
        e = d;
        d = c;
        c = RotateLeft(30, b);
        b = a;
        a = temp;
    }

    sha1.hash[0] += a;
    sha1.hash[1] += b;
    sha1.hash[2] += c;
    sha1.hash[3] += d;
    sha1.hash[4] += e;

    sha1.cursor = 0;
}

void Finalize(SHA1& sha1) {
    sha1.buffer[sha1.cursor++] = 0x80;
    if (sha1.cursor > LengthOffset) {
        while (sha1.cursor < BlockSize)
            sha1.buffer[sha1.cursor++] = 0x00;
        ProcessBlock(sha1);
    }

    for (std::size_t i = sha1.cursor; i < LengthOffset; ++i)
        sha1.buffer[i] = 0x00;

    std::uint64_t bits = sha1.totalBytes * 8;
    for (int i = 0; i < 8; ++i) {
        sha1.buffer[LengthOffset + (7 - i)] = bits & 0xFF;
        bits >>= 8;
    }
    sha1.cursor = BlockSize;
    ProcessBlock(sha1);
}

void AddBytes(SHA1& sha1, const std::uint8_t* input, std::size_t length) {
    while (length--) {
        sha1.buffer[sha1.cursor++] = *input++;
        ++sha1.totalBytes;
        if (sha1.cursor == BlockSize) ProcessBlock(sha1);
    }
}

void Reset(SHA1& sha1) {
    sha1.cursor = 0;
    sha1.totalBytes = 0;
    sha1.hash[0] = 0x67452301;
    sha1.hash[1] = 0xefcdab89;
    sha1.hash[2] = 0x98badcfe;
    sha1.hash[3] = 0x10325476;
    sha1.hash[4] = 0xc3d2e1f0;
    std::memset(sha1.buffer, 0, sizeof(sha1.buffer));
}

void EnsureEncoderCapacity(PersistenceEncoder& encoder, std::size_t additional) {
    if (encoder.size > std::numeric_limits<unsigned>::max() - additional)
        throw std::length_error("WTF::Persistence::Coder<SHA1::Digest>::encode: buffer size overflow");
    const std::size_t required = static_cast<std::size_t>(encoder.size) + additional;
    if (required <= encoder.capacity) return;
    const auto* allocator = registeredPersistenceAllocator.load(std::memory_order_acquire);
    if (!allocator || !allocator->allocate || !allocator->deallocate)
        throw std::runtime_error("WTF::Persistence::Coder<SHA1::Digest>::encode: buffer growth requires libSceNKWebKit fastMalloc");
    const std::size_t maxUnsigned = std::numeric_limits<unsigned>::max();
    const std::size_t doubled = static_cast<std::size_t>(encoder.capacity) * 2;
    const std::size_t nextCapacity = std::min(maxUnsigned, std::max(doubled, required));
    auto* nextBuffer = static_cast<std::uint8_t*>(allocator->allocate(nextCapacity));
    if (!nextBuffer) throw std::bad_alloc();
    if (encoder.size) std::memcpy(nextBuffer, encoder.buffer, encoder.size);
    if (encoder.buffer != encoder.inlineBuffer) allocator->deallocate(encoder.buffer);
    encoder.buffer = nextBuffer;
    encoder.capacity = static_cast<unsigned>(nextCapacity);
}

constexpr unsigned BoolSalt = 3;
constexpr unsigned UInt32Salt = 11;
constexpr unsigned DataSalt = 101;
constexpr std::uint32_t NullStringLength = std::numeric_limits<std::uint32_t>::max();
constexpr std::uint32_t MaxDecodedLength = std::numeric_limits<std::int32_t>::max();

void CheckDecoder(const void* result, const Jsc::WtfAbi::PersistenceDecoder* decoder, const std::string& name) {
    if (!result || !decoder) throw std::invalid_argument(name + " requires return storage and a decoder");
    if (decoder->sha1.cursor >= BlockSize) throw std::invalid_argument(name + ": buffer cursor out of range");
    if (decoder->bufferPosition > decoder->bufferEnd) throw std::invalid_argument(name + ": position past the buffer end");
}

std::size_t Remaining(const Jsc::WtfAbi::PersistenceDecoder& decoder) {
    return static_cast<std::size_t>(decoder.bufferEnd - decoder.bufferPosition);
}

bool DecodeNumber(Jsc::WtfAbi::PersistenceDecoder& decoder, unsigned salt, void* value, std::size_t size) {
    if (Remaining(decoder) < size) return false;
    std::memcpy(value, decoder.bufferPosition, size);
    decoder.bufferPosition += size;
    AddBytes(decoder.sha1, reinterpret_cast<const std::uint8_t*>(&salt), sizeof(salt));
    AddBytes(decoder.sha1, static_cast<const std::uint8_t*>(value), size);
    return true;
}

void DecodeData(Jsc::WtfAbi::PersistenceDecoder& decoder, std::uint8_t* data, std::size_t size) {
    std::memcpy(data, decoder.bufferPosition, size);
    decoder.bufferPosition += size;
    AddBytes(decoder.sha1, reinterpret_cast<const std::uint8_t*>(&DataSalt), sizeof(DataSalt));
    AddBytes(decoder.sha1, data, size);
}

void* FastMalloc(std::size_t size, const std::string& name) {
    const auto* allocator = registeredPersistenceAllocator.load(std::memory_order_acquire);
    if (!allocator || !allocator->allocate || !allocator->deallocate)
        throw std::runtime_error(name + ": the string requires libSceNKWebKit fastMalloc");
    void* storage = allocator->allocate(size);
    if (!storage) throw std::bad_alloc();
    return storage;
}

}

extern "C" {

void APS5_VABI _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(SHA1* self, Jsc::WtfAbi::SHA1Digest* digest) {
    if (!self || !digest) throw std::invalid_argument("WTF::SHA1::computeHash requires an object and a digest");
    if (self->cursor >= BlockSize) throw std::invalid_argument("WTF::SHA1::computeHash: buffer cursor out of range");
    Finalize(*self);

    for (std::size_t i = 0; i < 5; ++i) {
        std::uint32_t hashValue = self->hash[i];
        for (int j = 0; j < 4; ++j) {
            (*digest)[4 * i + (3 - j)] = hashValue & 0xFF;
            hashValue >>= 8;
        }
    }

    Reset(*self);
}

Jsc::WtfAbi::OptionalSHA1Digest APS5_VABI _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(Jsc::WtfAbi::PersistenceDecoder* decoder) {
    if (!decoder) throw std::invalid_argument("WTF::Persistence::Coder<SHA1::Digest>::decode requires a decoder");
    if (decoder->sha1.cursor >= BlockSize) throw std::invalid_argument("WTF::Persistence::Coder<SHA1::Digest>::decode: buffer cursor out of range");
    if (decoder->bufferPosition > decoder->bufferEnd) throw std::invalid_argument("WTF::Persistence::Coder<SHA1::Digest>::decode: position past the buffer end");
    Jsc::WtfAbi::OptionalSHA1Digest result{};
    if (static_cast<std::size_t>(decoder->bufferEnd - decoder->bufferPosition) < result.value.size()) return result;
    std::memcpy(result.value.data(), decoder->bufferPosition, result.value.size());
    decoder->bufferPosition += result.value.size();
    const unsigned typeSalt = 101;
    AddBytes(decoder->sha1, reinterpret_cast<const std::uint8_t*>(&typeSalt), sizeof(typeSalt));
    AddBytes(decoder->sha1, result.value.data(), result.value.size());
    result.hasValue = true;
    return result;
}

void APS5_VABI _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(
    PersistenceEncoder* encoder, const Jsc::WtfAbi::SHA1Digest* digest) {
    if (!encoder || !digest)
        throw std::invalid_argument("WTF::Persistence::Coder<SHA1::Digest>::encode requires an encoder and a digest");
    if (encoder->sha1.cursor >= BlockSize)
        throw std::invalid_argument("WTF::Persistence::Coder<SHA1::Digest>::encode: buffer cursor out of range");
    if (!encoder->buffer || encoder->size > encoder->capacity)
        throw std::invalid_argument("WTF::Persistence::Coder<SHA1::Digest>::encode: invalid encoder buffer state");
    const Jsc::WtfAbi::SHA1Digest value = *digest;
    EnsureEncoderCapacity(*encoder, value.size());
    const unsigned typeSalt = 101;
    AddBytes(encoder->sha1, reinterpret_cast<const std::uint8_t*>(&typeSalt), sizeof(typeSalt));
    AddBytes(encoder->sha1, value.data(), value.size());
    std::memcpy(encoder->buffer + encoder->size, value.data(), value.size());
    encoder->size += static_cast<unsigned>(value.size());
}

Jsc::WtfAbi::OptionalCString* APS5_VABI _ZN3WTF11Persistence5CoderINS_7CStringEE6decodeERNS0_7DecoderE(
    Jsc::WtfAbi::OptionalCString* result, Jsc::WtfAbi::PersistenceDecoder* decoder) {
    const std::string name = "WTF::Persistence::Coder<CString>::decode";
    CheckDecoder(result, decoder, name);
    *result = {};
    std::uint32_t length = 0;
    if (!DecodeNumber(*decoder, UInt32Salt, &length, sizeof(length))) return result;
    if (length == NullStringLength) {
        result->hasValue = true;
        return result;
    }
    if (Remaining(*decoder) < length) return result;
    if (length > MaxDecodedLength) throw std::length_error(name + ": length above INT32_MAX");
    using Jsc::WtfAbi::CStringBuffer;
    auto* buffer = new (FastMalloc(sizeof(CStringBuffer) + length + 1, name)) CStringBuffer{1u, length};
    auto* characters = reinterpret_cast<std::uint8_t*>(buffer + 1);
    DecodeData(*decoder, characters, length);
    characters[length] = 0;
    result->value.buffer = buffer;
    result->hasValue = true;
    return result;
}

Jsc::WtfAbi::OptionalString* APS5_VABI _ZN3WTF11Persistence5CoderINS_6StringEE6decodeERNS0_7DecoderE(
    Jsc::WtfAbi::OptionalString* result, Jsc::WtfAbi::PersistenceDecoder* decoder) {
    const std::string name = "WTF::Persistence::Coder<String>::decode";
    CheckDecoder(result, decoder, name);
    *result = {};
    std::uint32_t length = 0;
    if (!DecodeNumber(*decoder, UInt32Salt, &length, sizeof(length))) return result;
    if (length == NullStringLength) {
        result->hasValue = true;
        return result;
    }
    std::uint8_t is8Bit = 0;
    if (!DecodeNumber(*decoder, BoolSalt, &is8Bit, sizeof(is8Bit))) return result;
    if (is8Bit > 1) throw std::invalid_argument(name + ": the 8-bit flag byte is neither 0 nor 1");
    const std::size_t byteLength = static_cast<std::size_t>(length) * (is8Bit ? 1 : 2);
    if (Remaining(*decoder) < byteLength) return result;
    if (!length) NotImplemented_nid_no_patch(__func__);
    if (length > MaxDecodedLength) throw std::length_error(name + ": length above INT32_MAX");
    using Jsc::WtfAbi::StringImpl;
    constexpr auto charactersOffset = offsetof(StringImpl, hashAndFlags) + sizeof(StringImpl::hashAndFlags);
    const std::size_t allocationSize = std::max<std::size_t>(sizeof(StringImpl), charactersOffset + byteLength);
    void* storage = FastMalloc(allocationSize, name);
    auto* characters = static_cast<std::uint8_t*>(storage) + charactersOffset;
    auto* string = new (storage) StringImpl{2u, length, characters, is8Bit ? Jsc::WtfAbi::StringImplFlag8Bit : 0u};
    DecodeData(*decoder, characters, byteLength);
    result->value = string;
    result->hasValue = true;
    return result;
}

void RegisterPersistenceAllocator_nid_no_patch(const PersistenceAllocator* allocator) {
    registeredPersistenceAllocator.store(allocator, std::memory_order_release);
}

const PersistenceAllocator* RegisteredPersistenceAllocator_nid_no_patch() {
    return registeredPersistenceAllocator.load(std::memory_order_acquire);
}

}

namespace Jsc::WtfAbi {

const PersistenceAllocator* RegisteredFastMalloc() {
    return registeredPersistenceAllocator.load(std::memory_order_acquire);
}

} // namespace Jsc::WtfAbi
