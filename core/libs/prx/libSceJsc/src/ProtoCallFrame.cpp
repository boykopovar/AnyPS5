// Derived from Source/JavaScriptCore/interpreter/ProtoCallFrameInlines.h@c55364d53f971f63a709976f168d932d558b8909
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/JavaScriptCore/interpreter/ProtoCallFrameInlines.h@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2013-2019 Apple Inc. All Rights Reserved.
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
 *
 * Derived from Source/JavaScriptCore/interpreter/ProtoCallFrame.h@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2013, 2016 Apple Inc. All rights reserved.
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
 *
 * Derived from Source/JavaScriptCore/runtime/StackAlignment.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libSceJsc/include/ProtoCallFrameExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

using Jsc::JscAbi::ProtoCallFrame;

namespace {

constexpr unsigned kStackAlignmentRegisters = 16 / sizeof(std::uint64_t);
constexpr unsigned kCallFrameHeaderSizeInRegisters = 5;

template <typename Frame>
Frame& Checked(Frame* self, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + " requires a ProtoCallFrame");
    return *self;
}

unsigned RoundArgumentCountToAlignFrame(unsigned argumentCount) {
    const std::size_t remainderMask = kStackAlignmentRegisters - 1;
    const std::size_t total = argumentCount + kCallFrameHeaderSizeInRegisters;
    return static_cast<unsigned>(((total + remainderMask) & ~remainderMask) - kCallFrameHeaderSizeInRegisters);
}

std::uint64_t Bits(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

} // namespace

extern "C" {

void* APS5_VABI _ZNK3JSC14ProtoCallFrame9codeBlockEv(const ProtoCallFrame* self) {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(Checked(self, __func__).codeBlockValue));
}

bool APS5_VABI _ZN3JSC14ProtoCallFrame14needArityCheckEv(ProtoCallFrame* self) {
    return Checked(self, __func__).hasArityMismatch;
}

void APS5_VABI _ZN3JSC14ProtoCallFrame4initEPNS_9CodeBlockEPNS_14JSGlobalObjectEPNS_8JSObjectENS_7JSValueEiPS7_(
    ProtoCallFrame* self, void* codeBlock, void* globalObject, void* callee, std::uint64_t thisValue,
    int argCountIncludingThis, std::uint64_t* otherArgs) {
    ProtoCallFrame& frame = Checked(self, __func__);
    if (codeBlock) NotImplemented_nid_no_patch(__func__);
    frame.args = otherArgs;
    frame.codeBlockValue = Bits(codeBlock);
    frame.calleeValue = Bits(callee);
    frame.globalObject = Bits(globalObject);
    frame.argCountAndCodeOriginValue.payload = argCountIncludingThis;
    frame.hasArityMismatch = false;
    frame.paddedArgCount = RoundArgumentCountToAlignFrame(static_cast<unsigned>(argCountIncludingThis));
    frame.argCountAndCodeOriginValue.tag = 0;
    frame.thisArg = thisValue;
}

}
