// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WebCore/platform/graphics/filters/FilterOperations.cpp
/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/FilterOperationExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

namespace {

using NKWebKit::FilterAbi::FilterOperations;
using NKWebKit::WtfAbi::TextStream;

template<typename T>
T& Checked(T* pointer) {
    if (!pointer) throw std::invalid_argument("null WebCore filter operation");
    return *pointer;
}

}  // namespace

extern "C" {

TextStream* APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_16FilterOperationsE(TextStream* stream, const FilterOperations* operations) {
    auto& ts = Checked(stream);
    auto& filters = Checked(operations);
    for (unsigned i = 0; i < filters.size; ++i) {
        auto* filter = filters.buffer[i];
        if (filter)
            _ZN7WebCorelsERN3WTF10TextStreamERKNS_15FilterOperationE(&ts, filter);
        else
            _ZN3WTF10TextStreamlsEPKc(&ts, "(null)");
        if (i + 1 < filters.size) _ZN3WTF10TextStreamlsEPKc(&ts, " ");
    }
    return &ts;
}

}
