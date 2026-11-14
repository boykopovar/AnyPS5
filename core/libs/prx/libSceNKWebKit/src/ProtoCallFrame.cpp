// Derived from neonmodder123/PS5-WebKit-11.x/Source/JavaScriptCore/interpreter/ProtoCallFrame.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2013-2023 Apple Inc. All Rights Reserved.
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

#include "prx/libSceNKWebKit/include/ProtoCallFrameExports.hpp"

#include <cstddef>
#include <cstdint>

using NKWebKit::JscAbi::ProtoCallFrame;

extern "C" {

int APS5_VABI _ZNK3JSC14ProtoCallFrame13argumentCountEv(const ProtoCallFrame* self) {
    return static_cast<int>(static_cast<std::uint32_t>(self->argCountAndCodeOriginValue.payload) - 1u);
}

std::uint64_t APS5_VABI _ZN3JSC14ProtoCallFrame8argumentEm(ProtoCallFrame* self, std::size_t argumentIndex) {
    return self->args[argumentIndex];
}

void APS5_VABI _ZN3JSC14ProtoCallFrame11setArgumentEmNS_7JSValueE(ProtoCallFrame* self, std::size_t argumentIndex, std::uint64_t value) {
    self->args[argumentIndex] = value;
}

void APS5_VABI _ZN3JSC14ProtoCallFrame15clearCurrentVPCEv(ProtoCallFrame* self) {
    self->argCountAndCodeOriginValue.tag = 0;
}

ProtoCallFrame& APS5_VABI _ZN3JSC14ProtoCallFrameaSERKS0_(ProtoCallFrame* self, const ProtoCallFrame& other) {
    self->codeBlockValue = other.codeBlockValue;
    self->calleeValue = other.calleeValue;
    self->argCountAndCodeOriginValue = other.argCountAndCodeOriginValue;
    self->thisArg = other.thisArg;
    self->paddedArgCount = other.paddedArgCount;
    self->args = other.args;
    self->globalObject = other.globalObject;
    return *self;
}

}
