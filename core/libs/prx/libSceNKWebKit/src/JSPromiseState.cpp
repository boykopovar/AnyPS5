// Derived from Source/JavaScriptCore/runtime/JSPromise.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/JSPromiseStateExports.hpp"

#include <bit>
#include <cerrno>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::JscAbi::JSPromise;
using NKWebKit::JscAbi::JSPromiseIsHandledFlag;
using NKWebKit::JscAbi::JSPromiseStateMask;
using NKWebKit::JscAbi::JSValueDoubleEncodeOffset;
using NKWebKit::JscAbi::JSValueNumberTag;
using NKWebKit::JscAbi::JSValueUndefined;

JSPromise ReadPromise(const JSPromise* promise, const char* function) {
    if (!promise) throw std::invalid_argument(std::string(function) + ": null promise");
    JSPromise value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, promise, sizeof(JSPromise));
    errno = savedErrno;
    if (!ok) throw std::invalid_argument(std::string(function) + ": unreadable promise");
    return value;
}

std::int64_t TruncateToInt64(double number) {
    if (number >= -9223372036854775808.0 && number < 9223372036854775808.0) return static_cast<std::int64_t>(number);
    return std::numeric_limits<std::int64_t>::min();
}

std::uint32_t AsUInt32AsAnyInt(std::uint64_t value) {
    const bool isInt32 = (value & JSValueNumberTag) == JSValueNumberTag;
    if (isInt32 && static_cast<std::int32_t>(value) >= 0) return static_cast<std::uint32_t>(value);
    return static_cast<std::uint32_t>(TruncateToInt64(std::bit_cast<double>(value - JSValueDoubleEncodeOffset)));
}

std::uint32_t Flags(const JSPromise* promise, const char* function) {
    return AsUInt32AsAnyInt(ReadPromise(promise, function).flags);
}

}

extern "C" {

std::uint32_t APS5_VABI _ZNK3JSC9JSPromise6statusERNS_2VME(const JSPromise* self, void*) {
    return Flags(self, __func__) & JSPromiseStateMask;
}

std::uint64_t APS5_VABI _ZNK3JSC9JSPromise6resultERNS_2VME(const JSPromise* self, void*) {
    const JSPromise promise = ReadPromise(self, __func__);
    if ((AsUInt32AsAnyInt(promise.flags) & JSPromiseStateMask) == 0) return JSValueUndefined;
    return promise.reactionsOrResult;
}

bool APS5_VABI _ZNK3JSC9JSPromise9isHandledERNS_2VME(const JSPromise* self, void*) {
    return (Flags(self, __func__) & JSPromiseIsHandledFlag) != 0;
}

}
