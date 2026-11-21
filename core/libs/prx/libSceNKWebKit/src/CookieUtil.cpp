/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <new>

extern "C" NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore10CookieUtil17defaultPathForURLERKN3WTF3URLE(const NKWebKit::WtfAbi::URL* url) {
    using namespace NKWebKit::WtfAbi;
    auto view = _ZNK3WTF3URL4pathEv(url);
    String path = view.is8Bit
        ? _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(view.data), view.length)
        : _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(view.data), view.length);
    auto character = [&](unsigned index) -> char16_t {
        return view.is8Bit ? static_cast<const unsigned char*>(path.impl->data)[index]
            : static_cast<const char16_t*>(path.impl->data)[index];
    };
    auto slash = [] {
        static constexpr unsigned char characters[] = "/";
        return String(new (_ZN3WTF10fastMallocEm(sizeof(StringImpl))) StringImpl{2, 1, characters, 4});
    };
    if (!view.length || character(0) != '/') return slash();
    unsigned lastSlashPosition = view.length - 1;
    while (character(lastSlashPosition) != '/') --lastSlashPosition;
    if (!lastSlashPosition) return slash();
    return _ZN3WTF10StringImpl9substringEjj(path.impl, 0, lastSlashPosition);
}
