/*
 * Copyright (C) 2017 Caio Lima <ticaiolima@gmail.com>
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
 * Derived from Source/JavaScriptCore/runtime/JSBigInt.cpp@e12a212a531a
 */

#include "prx/libSceNKWebKit/include/JscBigIntExports.hpp"

#include <cstdlib>

extern "C" {

bool APS5_VABI _ZN3JSC8JSBigInt6equalsEPS0_S1_(JSC::JSBigInt* x, JSC::JSBigInt* y) {
    if (x->sign != y->sign || x->length != y->length) return false;
    for (std::uint32_t i = 0; i < x->length; ++i) {
        if (x->data[i] != y->data[i]) return false;
    }
    return true;
}

bool APS5_VABI _ZN3JSC8JSBigInt13equalsToInt32Ei(JSC::JSBigInt* self, std::int32_t value) {
    if (!value) return !self->length;
    return self->length == 1 && self->sign == (value < 0) && self->data[0] == static_cast<std::uint64_t>(std::llabs(value));
}

}
