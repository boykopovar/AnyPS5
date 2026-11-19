// Derived from WebKit/WebKit/Source/WebCore/platform/sql/SQLiteFileSystem.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/PalCryptoDigestExports.hpp"
#include "prx/libSceNKWebKit/include/SQLiteFileSystemExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <stdexcept>

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore16SQLiteFileSystem22computeHashForFileNameERKN3WTF6StringE(const NKWebKit::WtfAbi::String* fileName) {
    if (!fileName) throw std::invalid_argument("SQLiteFileSystem::computeHashForFileName: null fileName");
    auto digest = _ZN3PAL12CryptoDigest6createENS0_9AlgorithmE(PAL::CryptoDigest::Algorithm::SHA_256);
    if (!digest) throw std::runtime_error("SQLiteFileSystem::computeHashForFileName: SHA-256 digest unavailable");
    const auto utf8 = _ZNK3WTF6String4utf8ENS_14ConversionModeE(fileName, 0);
    const char* bytes = utf8.buffer ? reinterpret_cast<const char*>(utf8.buffer + 1) : nullptr;
    const std::size_t length = utf8.buffer ? utf8.buffer->length : 0;
    _ZN3PAL12CryptoDigest8addBytesEPKvm(digest.get(), bytes, length);
    const auto hash = _ZN3PAL12CryptoDigest11computeHashEv(digest.get());
    static constexpr char hexDigits[] = "0123456789ABCDEF";
    char* output = nullptr;
    const auto hex = _ZN3WTF7CString16newUninitializedEmRPc(std::size_t{hash.size} * 2, output);
    for (unsigned i = 0; i < hash.size; ++i) {
        output[2 * i] = hexDigits[hash.data()[i] >> 4];
        output[2 * i + 1] = hexDigits[hash.data()[i] & 0xF];
    }
    return _ZN3WTF6String8fromUTF8ERKNS_7CStringE(hex);
}
