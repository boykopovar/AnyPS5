// Derived from Source/JavaScriptCore/runtime/BytecodeCacheError.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/JavaScriptCore/parser/ParserError.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/JscBytecodeCacheErrorExports.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

using NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError;
using NKWebKit::JscBytecodeCacheErrorAbi::StandardError;
using NKWebKit::JscBytecodeCacheErrorAbi::WriteError;

constexpr std::uint32_t ParserErrorIndex = 0;
constexpr std::uint32_t StandardErrorIndex = 1;
constexpr std::uint32_t WriteErrorIndex = 2;
constexpr std::uint32_t ValuelessIndex = 0xFF;
constexpr std::uint8_t ErrorNone = 0;

BytecodeCacheError& Checked(BytecodeCacheError* error) {
    if (!error) throw std::invalid_argument("null JSC::BytecodeCacheError");
    return *error;
}

const BytecodeCacheError& Checked(const BytecodeCacheError* error) {
    if (!error) throw std::invalid_argument("null JSC::BytecodeCacheError");
    return *error;
}

std::uint32_t Alternative(const BytecodeCacheError& error) {
    std::uint32_t index = error.index & 0xFF;
    if (index > WriteErrorIndex && index != ValuelessIndex) throw std::invalid_argument("invalid JSC::BytecodeCacheError variant index");
    return index;
}

void Destroy(BytecodeCacheError& error, std::uint32_t index) {
    if (index == ParserErrorIndex) NKWebKit::WtfAbi::Release(error.parserError.message);
}

}  // namespace

extern "C" {

bool APS5_VABI _ZNK3JSC18BytecodeCacheError7isValidEv(const BytecodeCacheError* self) {
    const BytecodeCacheError& error = Checked(self);
    switch (Alternative(error)) {
    case ParserErrorIndex:
        return error.parserError.type != ErrorNone;
    case StandardErrorIndex:
    case WriteErrorIndex:
        return true;
    }
    throw std::invalid_argument("valueless JSC::BytecodeCacheError");
}

BytecodeCacheError* APS5_VABI _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(BytecodeCacheError* self, const StandardError* error) {
    BytecodeCacheError& target = Checked(self);
    if (!error) throw std::invalid_argument("null JSC::BytecodeCacheError::StandardError");
    std::uint32_t index = Alternative(target);
    StandardError value = *error;
    if (index != StandardErrorIndex) {
        Destroy(target, index);
        target.index = StandardErrorIndex;
    }
    target.standardError = value;
    return self;
}

BytecodeCacheError* APS5_VABI _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(BytecodeCacheError* self, const WriteError* error) {
    BytecodeCacheError& target = Checked(self);
    if (!error) throw std::invalid_argument("null JSC::BytecodeCacheError::WriteError");
    std::uint32_t index = Alternative(target);
    WriteError value = *error;
    if (index != WriteErrorIndex) {
        Destroy(target, index);
        target.index = WriteErrorIndex;
    }
    target.writeError = value;
    return self;
}

}
