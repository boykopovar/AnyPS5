// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/ScriptCallFrame.cpp
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/JavaScriptCore/inspector/ScriptCallFrame.h
/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/InspectorCallStackExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <new>
#include <stdexcept>

namespace {

using NKWebKit::InspectorAbi::ScriptCallFrame;
using NKWebKit::WtfAbi::String;

ScriptCallFrame& Checked(ScriptCallFrame* frame) {
    if (!frame) throw std::invalid_argument("null Inspector::ScriptCallFrame");
    return *frame;
}

const ScriptCallFrame& Checked(const ScriptCallFrame* frame) {
    if (!frame) throw std::invalid_argument("null Inspector::ScriptCallFrame");
    return *frame;
}

void Destroy(ScriptCallFrame& frame) {
    frame.preRedirectURL.~String();
    frame.scriptName.~String();
    frame.functionName.~String();
}

}

namespace NKWebKit::InspectorAbi {

bool IsNative(const ScriptCallFrame& frame) {
    static constexpr unsigned char nativeCode[] = "[native code]";
    return _ZN3WTF5equalEPKNS_10StringImplEPKhj(frame.scriptName.impl, nativeCode, 13);
}

bool FramesEqual(const ScriptCallFrame& a, const ScriptCallFrame& b) {
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.functionName.impl, b.functionName.impl)
        && _ZN3WTF5equalEPKNS_10StringImplES2_(a.scriptName.impl, b.scriptName.impl)
        && _ZN3WTF5equalEPKNS_10StringImplES2_(a.preRedirectURL.impl, b.preRedirectURL.impl)
        && a.lineNumber == b.lineNumber
        && a.column == b.column;
}

}

extern "C" {

void APS5_VABI _ZN9Inspector15ScriptCallFrameC1ERKS0_(ScriptCallFrame* self, const ScriptCallFrame* other) {
    if (!self || !other) throw std::invalid_argument("null Inspector::ScriptCallFrame");
    ::new (static_cast<void*>(self)) ScriptCallFrame(*other);
}

void APS5_VABI _ZN9Inspector15ScriptCallFrameC2ERKS0_(ScriptCallFrame* self, const ScriptCallFrame* other) {
    if (!self || !other) throw std::invalid_argument("null Inspector::ScriptCallFrame");
    ::new (static_cast<void*>(self)) ScriptCallFrame(*other);
}

void APS5_VABI _ZN9Inspector15ScriptCallFrameD1Ev(ScriptCallFrame* self) {
    Destroy(Checked(self));
}

void APS5_VABI _ZN9Inspector15ScriptCallFrameD2Ev(ScriptCallFrame* self) {
    Destroy(Checked(self));
}

ScriptCallFrame& APS5_VABI _ZN9Inspector15ScriptCallFrameaSERKS0_(ScriptCallFrame* self, const ScriptCallFrame* other) {
    auto& frame = Checked(self);
    frame = Checked(other);
    return frame;
}

bool APS5_VABI _ZNK9Inspector15ScriptCallFrame8isNativeEv(const ScriptCallFrame* self) {
    return NKWebKit::InspectorAbi::IsNative(Checked(self));
}

}
