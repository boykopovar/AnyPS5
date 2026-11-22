/*
 *  Copyright 2006 The WebRTC Project Authors. All rights reserved.
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
#include "prx/libwebrtc/include/GuestString.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/GuestErrno.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

extern "C" {
[[noreturn]] void abort_nid_postfix(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);
}

namespace {

struct GuestStringView {
    const char* data;
    std::size_t size;
};

template<typename TList> long double ReadLongDouble(TList& args) {
#ifdef _WIN32
    struct SysvVaListTag {
        unsigned gpOffset;
        unsigned fpOffset;
        char* overflowArea;
        char* registerSaveArea;
    };
    auto* tag = reinterpret_cast<SysvVaListTag*>(&args[0]);
    const auto address = (reinterpret_cast<std::uintptr_t>(tag->overflowArea) + 15) & ~std::uintptr_t {15};
    long double value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    tag->overflowArea = reinterpret_cast<char*>(address + 16);
    return value;
#else
    return va_arg(args, long double);
#endif
}

template<typename... TValues> void AppendFormat(std::string& output, const char* format, TValues... values) {
    const int length = snprintf_nid_postfix(nullptr, 0, format, values...);
    if (length <= 0)
        return;
    const std::size_t offset = output.size();
    output.resize(offset + static_cast<std::size_t>(length) + 1);
    snprintf_nid_postfix(output.data() + offset, static_cast<std::size_t>(length) + 1, format, values...);
    output.resize(offset + static_cast<std::size_t>(length));
}

template<typename TList> bool ParseArg(TList& args, const WebRtcCheckArgType*& format, std::string& output) {
    switch (*format) {
    case WebRtcCheckArgType::End:
        return false;
    case WebRtcCheckArgType::Int:
        AppendFormat(output, "%d", va_arg(args, int));
        break;
    case WebRtcCheckArgType::Long:
        AppendFormat(output, "%ld", va_arg(args, std::int64_t));
        break;
    case WebRtcCheckArgType::LongLong:
        AppendFormat(output, "%lld", va_arg(args, std::int64_t));
        break;
    case WebRtcCheckArgType::UInt:
        AppendFormat(output, "%u", va_arg(args, unsigned));
        break;
    case WebRtcCheckArgType::ULong:
        AppendFormat(output, "%lu", va_arg(args, std::uint64_t));
        break;
    case WebRtcCheckArgType::ULongLong:
        AppendFormat(output, "%llu", va_arg(args, std::uint64_t));
        break;
    case WebRtcCheckArgType::Double:
        AppendFormat(output, "%g", va_arg(args, double));
        break;
    case WebRtcCheckArgType::LongDouble:
        AppendFormat(output, "%Lg", ReadLongDouble(args));
        break;
    case WebRtcCheckArgType::CharP:
        output.append(va_arg(args, const char*));
        break;
    case WebRtcCheckArgType::StdString: {
        const auto* string = va_arg(args, const WebRtcGuestString*);
        output.append(WebRtcGuestStringData(*string), string->size);
        break;
    }
    case WebRtcCheckArgType::StringView: {
        const auto* view = va_arg(args, const GuestStringView*);
        output.append(view->data, view->size);
        break;
    }
    case WebRtcCheckArgType::VoidP:
        AppendFormat(output, "%p", va_arg(args, const void*));
        break;
    default:
        output.append("[Invalid CheckArgType]");
        return false;
    }
    ++format;
    return true;
}

template<typename TList> std::string FatalMessage(const char* file, int line, const char* message, const WebRtcCheckArgType* format, TList& args) {
    std::string output;
    AppendFormat(output, "\n\n#\n# Fatal error in: %s, line %d\n# last system error: %u\n# Check failed: %s", file, line, static_cast<unsigned>(*__error_nid_postfix()), message);
    if (*format == WebRtcCheckArgType::CheckOp) {
        ++format;
        std::string left;
        std::string right;
        if (ParseArg(args, format, left) && ParseArg(args, format, right))
            AppendFormat(output, " (%s vs. %s)\n# ", left.c_str(), right.c_str());
    } else {
        output.append("\n# ");
    }
    while (ParseArg(args, format, output))
        ;
    return output;
}

}

extern "C" {

[[noreturn]] void APS5_VABI _ZN3rtc18webrtc_checks_impl8FatalLogEPKciS2_PKNS0_12CheckArgTypeEz_nid_postfix(const char* file, int line, const char* message, const WebRtcCheckArgType* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const std::string output = FatalMessage(file, line, message, format, args);
    __builtin_sysv_va_end(args);
#else
    va_list args;
    va_start(args, format);
    const std::string output = FatalMessage(file, line, message, format, args);
    va_end(args);
#endif
    fflush_nid_postfix(nullptr);
    fwrite_nid_postfix(output.data(), output.size(), 1, &_Stderr_nid_postfix);
    fflush_nid_postfix(&_Stderr_nid_postfix);
    abort_nid_postfix(0, 0, 0, 0, 0, 0);
}

}
