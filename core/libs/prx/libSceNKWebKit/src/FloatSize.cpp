/* Derived from Source/WebCore/platform/graphics/FloatSize.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba */
/*
 * Copyright (C) 2003-2023 Apple Inc.  All rights reserved.
 * Copyright (C) 2014 Google Inc.  All rights reserved.
 * Copyright (C) 2005 Nokia.  All rights reserved.
 *               2008 Eric Seidel <eric@webkit.org>
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/FloatSizeExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <stdexcept>

#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <type_traits>

namespace {

static_assert(std::is_trivially_copyable_v<WebCore::FloatSize>);
static_assert(std::is_trivially_copyable_v<WebCore::IntSize>);

bool CanWriteGuestBytes(void* pointer, std::size_t size) {
    if (!pointer) return false;
    const GuestArena::HostWrite guard(pointer, size);
    return guard.Open() && GuestWritable_nid_no_patch(pointer, size);
}

void ConstructFromIntSize(WebCore::FloatSize* self, const WebCore::IntSize& size, const char* function) {
    if (!CanWriteGuestBytes(self, sizeof(*self)))
        throw std::invalid_argument("WebCore::FloatSize(IntSize): invalid guest FloatSize");
    WebCore::IntSize source{};
    if (!GuestCopyIn_nid_no_patch(&source, &size, sizeof(source)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const WebCore::FloatSize result{static_cast<float>(source.m_width), static_cast<float>(source.m_height)};
    if (!GuestCopyOut_nid_no_patch(self, &result, sizeof(result)))
        throw std::invalid_argument("WebCore::FloatSize(IntSize): invalid guest FloatSize");
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore9FloatSizeC1ERKNS_7IntSizeE(WebCore::FloatSize* self, const WebCore::IntSize& size) {
    ConstructFromIntSize(self, size, __func__);
}

void APS5_VABI _ZN7WebCore9FloatSizeC2ERKNS_7IntSizeE(WebCore::FloatSize* self, const WebCore::IntSize& size) {
    ConstructFromIntSize(self, size, __func__);
}

}
