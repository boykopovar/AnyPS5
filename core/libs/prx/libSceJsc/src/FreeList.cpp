// Derived from Source/JavaScriptCore/heap/FreeList.cpp@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "prx/libSceJsc/include/FreeListExports.hpp"

#include <stdexcept>

namespace {

Jsc::HeapAbi::FreeList& Checked(Jsc::HeapAbi::FreeList* self) {
    if (!self) throw std::invalid_argument("null JSC::FreeList");
    return *self;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3JSC8FreeList14initializeBumpEPcj(Jsc::HeapAbi::FreeList* self, char* payloadEnd, unsigned remaining) {
    auto& list = Checked(self);
    list.scrambledHead = 0;
    list.secret = 0;
    list.payloadEnd = payloadEnd;
    list.remaining = remaining;
    list.originalSize = remaining;
}

void APS5_VABI _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(Jsc::HeapAbi::FreeList* self, Jsc::HeapAbi::FreeCell* head, std::uintptr_t secret, unsigned bytes) {
    auto& list = Checked(self);
    list.scrambledHead = reinterpret_cast<std::uintptr_t>(head) ^ secret;
    list.secret = secret;
    list.payloadEnd = nullptr;
    list.remaining = 0;
    list.originalSize = bytes;
}

}
