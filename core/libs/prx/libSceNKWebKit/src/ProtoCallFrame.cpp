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
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>

using NKWebKit::JscAbi::ProtoCallFrame;
using NKWebKit::JscAbi::Register;

extern "C" {

int APS5_VABI _ZNK3JSC14ProtoCallFrame13argumentCountEv(const ProtoCallFrame* self) {
    ProtoCallFrame frame{};
    if (!GuestCopyIn_nid_no_patch(&frame, self, sizeof(frame))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return static_cast<int>(static_cast<std::uint32_t>(frame.argCountAndCodeOriginValue.payload) - 1u);
}

std::uint64_t APS5_VABI _ZN3JSC14ProtoCallFrame8argumentEm(ProtoCallFrame* self, std::size_t argumentIndex) {
    ProtoCallFrame frame{};
    if (!GuestCopyIn_nid_no_patch(&frame, self, sizeof(frame))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    std::uint64_t value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, frame.args + argumentIndex, sizeof(value))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return value;
}

void APS5_VABI _ZN3JSC14ProtoCallFrame11setArgumentEmNS_7JSValueE(ProtoCallFrame* self, std::size_t argumentIndex, std::uint64_t value) {
    ProtoCallFrame frame{};
    if (!GuestCopyIn_nid_no_patch(&frame, self, sizeof(frame))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!GuestCopyOut_nid_no_patch(frame.args + argumentIndex, &value, sizeof(value))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI _ZN3JSC14ProtoCallFrame15clearCurrentVPCEv(ProtoCallFrame* self) {
    const std::int32_t tag = 0;
    auto* destination = reinterpret_cast<std::uint8_t*>(self) + offsetof(ProtoCallFrame, argCountAndCodeOriginValue) + offsetof(Register, tag);
    if (!GuestCopyOut_nid_no_patch(destination, &tag, sizeof(tag))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

ProtoCallFrame* APS5_VABI _ZN3JSC14ProtoCallFrameaSERKS0_(ProtoCallFrame* self, const ProtoCallFrame& other) {
    ProtoCallFrame source{};
    if (!GuestCopyIn_nid_no_patch(&source, &other, sizeof(source))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    ProtoCallFrame target{};
    if (!GuestCopyIn_nid_no_patch(&target, self, sizeof(target))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    target.codeBlockValue = source.codeBlockValue;
    target.calleeValue = source.calleeValue;
    target.argCountAndCodeOriginValue = source.argCountAndCodeOriginValue;
    target.thisArg = source.thisArg;
    target.paddedArgCount = source.paddedArgCount;
    target.args = source.args;
    target.globalObject = source.globalObject;
    if (!GuestCopyOut_nid_no_patch(self, &target, sizeof(target))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    return self;
}

}
