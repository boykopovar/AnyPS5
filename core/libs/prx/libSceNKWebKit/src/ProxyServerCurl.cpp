// Derived from Source/WebCore/platform/network/curl/ProxyServerCurl.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2010 Brent Fulgham <bfulgham@webkit.org>. All rights reserved.
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

#include "prx/libSceNKWebKit/include/ProxyServerExports.hpp"

#include <stdexcept>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <type_traits>

namespace {

using NKWebKit::ProxyAbi::ProxyServerVector;

static_assert(std::is_trivially_copyable_v<ProxyServerVector>);

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const GuestArena::HostWrite guard(pointer, size);
    if (!guard.Open() || !GuestWritable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

}  // namespace

extern "C" {

NKWebKit::ProxyAbi::ProxyServerVector* APS5_VABI _ZN7WebCore18proxyServersForURLERKN3WTF3URLE(NKWebKit::ProxyAbi::ProxyServerVector* result, const NKWebKit::WtfAbi::URL*) {
    if (!result) throw std::invalid_argument("WebCore::proxyServersForURL: null result");
    RequireWritableGuestBytes(result, sizeof(*result), __func__);
    const ProxyServerVector empty{nullptr, 0, 0};
    if (!GuestCopyOut_nid_no_patch(result, &empty, sizeof(empty)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return result;
}

}
