// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/bmalloc/bmalloc/FreeList.cpp
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/bmalloc/bmalloc/FreeList.h
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

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <stdexcept>

namespace {

bmalloc::FreeList& Checked(bmalloc::FreeList* self) {
    if (!self) throw std::invalid_argument("bmalloc::FreeList: null self");
    return *self;
}

}

extern "C" void APS5_VABI _ZN7bmalloc8FreeListC1Ev(bmalloc::FreeList* self) {
    Checked(self) = bmalloc::FreeList{};
}

extern "C" void APS5_VABI _ZN7bmalloc8FreeList5clearEv(bmalloc::FreeList* self) {
    Checked(self) = bmalloc::FreeList{};
}

extern "C" void APS5_VABI _ZN7bmalloc8FreeList14initializeListEPNS_8FreeCellEmj(bmalloc::FreeList* self, bmalloc::FreeCell* head, std::uintptr_t secret, unsigned bytes) {
    auto& list = Checked(self);
    list.m_scrambledHead = reinterpret_cast<std::uintptr_t>(head) ^ secret;
    list.m_secret = secret;
    list.m_payloadEnd = nullptr;
    list.m_remaining = 0;
    list.m_originalSize = bytes;
}

extern "C" void APS5_VABI _ZN7bmalloc8FreeList14initializeBumpEPcj(bmalloc::FreeList* self, char* payloadEnd, unsigned remaining) {
    auto& list = Checked(self);
    list.m_scrambledHead = 0;
    list.m_secret = 0;
    list.m_payloadEnd = payloadEnd;
    list.m_remaining = remaining;
    list.m_originalSize = remaining;
}
