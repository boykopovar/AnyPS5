// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/animation/TimingFunction.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
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
#include "prx/libSceNKWebKit/include/TimingFunctionDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

namespace {

using NKWebKit::WtfAbi::TextStream;

WebCore::TimingFunction::Type TimingFunctionType(const WebCore::TimingFunction& timingFunction) {
    using Function = WebCore::TimingFunction::Type(APS5_VABI*)(const WebCore::TimingFunction*);
    return reinterpret_cast<Function>(const_cast<void*>(timingFunction.vtable[WebCore::TimingFunction::TypeSlot]))(&timingFunction);
}

const char* StepPositionName(WebCore::StepsTimingFunction::StepPosition position) {
    using enum WebCore::StepsTimingFunction::StepPosition;
    switch (position) {
    case JumpStart:
        return "jump-start";
    case JumpEnd:
        return "jump-end";
    case JumpNone:
        return "jump-none";
    case JumpBoth:
        return "jump-both";
    case Start:
        return "start";
    case End:
        return "end";
    }
    return nullptr;
}

void WriteSteps(TextStream& stream, const WebCore::StepsTimingFunction& function) {
    _ZN3WTF10TextStreamlsEPKc(&stream, "steps(");
    _ZN3WTF10TextStreamlsEi(&stream, function.steps);
    if (function.hasStepPosition) {
        _ZN3WTF10TextStreamlsEPKc(&stream, ", ");
        if (const char* name = StepPositionName(function.stepPosition)) _ZN3WTF10TextStreamlsEPKc(&stream, name);
    }
    _ZN3WTF10TextStreamlsEPKc(&stream, ")");
}

void WriteCubicBezier(TextStream& stream, const WebCore::CubicBezierTimingFunction& function) {
    _ZN3WTF10TextStreamlsEPKc(&stream, "cubic-bezier(");
    _ZN3WTF10TextStreamlsEd(&stream, function.x1);
    _ZN3WTF10TextStreamlsEPKc(&stream, ", ");
    _ZN3WTF10TextStreamlsEd(&stream, function.y1);
    _ZN3WTF10TextStreamlsEPKc(&stream, ", ");
    _ZN3WTF10TextStreamlsEd(&stream, function.x2);
    _ZN3WTF10TextStreamlsEPKc(&stream, ", ");
    _ZN3WTF10TextStreamlsEd(&stream, function.y2);
    _ZN3WTF10TextStreamlsEPKc(&stream, ")");
}

void WriteSpring(TextStream& stream, const WebCore::SpringTimingFunction& function) {
    _ZN3WTF10TextStreamlsEPKc(&stream, "spring(");
    _ZN3WTF10TextStreamlsEd(&stream, function.mass);
    _ZN3WTF10TextStreamlsEPKc(&stream, " ");
    _ZN3WTF10TextStreamlsEd(&stream, function.stiffness);
    _ZN3WTF10TextStreamlsEPKc(&stream, " ");
    _ZN3WTF10TextStreamlsEd(&stream, function.damping);
    _ZN3WTF10TextStreamlsEPKc(&stream, " ");
    _ZN3WTF10TextStreamlsEd(&stream, function.initialVelocity);
    _ZN3WTF10TextStreamlsEPKc(&stream, ")");
}

}  // namespace

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_14TimingFunctionE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::TimingFunction& timingFunction) {
    switch (TimingFunctionType(timingFunction)) {
    case WebCore::TimingFunction::Type::LinearFunction:
        _ZN3WTF10TextStreamlsEPKc(&stream, "linear");
        break;
    case WebCore::TimingFunction::Type::CubicBezierFunction:
        WriteCubicBezier(stream, reinterpret_cast<const WebCore::CubicBezierTimingFunction&>(timingFunction));
        break;
    case WebCore::TimingFunction::Type::StepsFunction:
        WriteSteps(stream, reinterpret_cast<const WebCore::StepsTimingFunction&>(timingFunction));
        break;
    case WebCore::TimingFunction::Type::SpringFunction:
        WriteSpring(stream, reinterpret_cast<const WebCore::SpringTimingFunction&>(timingFunction));
        break;
    }
    return stream;
}

}
