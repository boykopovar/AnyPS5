#include "prx/libSceNKWebKit/include/SerializedCryptoKeyWrapExports.hpp"

#include <cstddef>

// Derived from Source/WebCore/crypto/openssl/SerializedCryptoKeyWrapOpenSSL.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
 */

namespace {

using ByteVector = NKWebKit::WtfAbi::Vector<unsigned char>;

static_assert(sizeof(ByteVector) == 16);
static_assert(offsetof(ByteVector, buffer) == 0);
static_assert(offsetof(ByteVector, capacity) == 8);
static_assert(offsetof(ByteVector, size) == 12);

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCore23wrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_(const ByteVector&, const ByteVector& key, ByteVector& result) {
    result = ByteVector(key);
    return true;
}

bool APS5_VABI _ZN7WebCore25unwrapSerializedCryptoKeyERKN3WTF6VectorIhLm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEES6_RS4_(const ByteVector&, const ByteVector& wrappedKey, ByteVector& key) {
    key = ByteVector(wrappedKey);
    return true;
}

}
