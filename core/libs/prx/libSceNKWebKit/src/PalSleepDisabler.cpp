// Derived from Source/WebCore/PAL/pal/system/SleepDisabler.cpp@e12a212a531aeddc11259e8515671cf5188e038d
// Derived from Source/WebCore/platform/SleepDisablerClient.cpp@e12a212a531aeddc11259e8515671cf5188e038d
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Derived from Source/WebCore/PAL/pal/system/SleepDisabler.cpp@e12a212a531aeddc11259e8515671cf5188e038d
 */
/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/WebCore/platform/SleepDisablerClient.cpp@e12a212a531aeddc11259e8515671cf5188e038d
 */

#include "prx/libSceNKWebKit/include/PalSleepDisablerExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <stdexcept>

namespace {

using NKWebKit::PalAbi::SleepDisabler;
using NKWebKit::PalAbi::SleepDisablerType;
using NKWebKit::PalAbi::SleepDisablerVtable;

void APS5_VABI CompleteDestructor(void*) {
}

void APS5_VABI DeletingDestructor(void* self) {
    _ZN3PAL13SleepDisablerD0Ev(static_cast<SleepDisabler*>(self));
}

const SleepDisablerVtable Vtable{CompleteDestructor, DeletingDestructor};

void Construct(SleepDisabler* self, SleepDisablerType type, const char* function) {
    if (!self) throw std::invalid_argument(function);
    self->vptr = &Vtable;
    self->type = type;
}

void* client = nullptr;

}  // namespace

extern "C" {

void APS5_VABI _ZN3PAL13SleepDisablerC1EPKcNS0_4TypeE(SleepDisabler* self, const char*, SleepDisablerType type) {
    Construct(self, type, "PAL::SleepDisabler::SleepDisabler requires an object");
}

void APS5_VABI _ZN3PAL13SleepDisablerC2EPKcNS0_4TypeE(SleepDisabler* self, const char*, SleepDisablerType type) {
    Construct(self, type, "PAL::SleepDisabler::SleepDisabler requires an object");
}

void APS5_VABI _ZN3PAL13SleepDisablerD0Ev(SleepDisabler* self) {
    if (!self) throw std::invalid_argument("PAL::SleepDisabler deleting destructor requires an object");
    _ZdlPv_nid_postfix(self);
}

void** APS5_VABI _ZN7WebCore19sleepDisablerClientEv() {
    return &client;
}

}
