// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/dom/DeviceOrientationData.cpp
/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/dom/DeviceMotionData.cpp
/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/DeviceMotionDataExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::LegacyDeviceMotionData;
using NKWebKit::WebCoreAbi::LegacyDeviceMotionVector;
using NKWebKit::WebCoreAbi::LegacyDeviceOrientationData;
using NKWebKit::WebCoreAbi::LegacyDeviceRef;
using NKWebKit::WebCoreAbi::LegacyOptionalBool;
using NKWebKit::WebCoreAbi::LegacyOptionalDouble;

template<typename T>
const T& Checked(const T* optional) {
    if (!optional) throw std::invalid_argument("null WTF::Optional");
    return *optional;
}

template<typename T>
T Copy(const T& optional) {
    T copy{};
    if (optional.engaged) {
        copy.engaged = true;
        copy.value = optional.value;
    }
    return copy;
}

template<typename T>
T* Allocate() {
    auto* object = static_cast<T*>(_ZN3WTF10fastMallocEm(sizeof(T)));
    object->refCount = 1;
    return object;
}

}  // namespace

extern "C" {

LegacyDeviceRef<LegacyDeviceOrientationData> APS5_VABI _ZN7WebCore21DeviceOrientationData6createEN3WTF8OptionalIdEES3_S3_NS2_IbEE(
    const LegacyOptionalDouble* alpha, const LegacyOptionalDouble* beta, const LegacyOptionalDouble* gamma, const LegacyOptionalBool* absolute) {
    const LegacyOptionalDouble alphaValue = Copy(Checked(alpha));
    const LegacyOptionalDouble betaValue = Copy(Checked(beta));
    const LegacyOptionalDouble gammaValue = Copy(Checked(gamma));
    const LegacyOptionalBool absoluteValue = Copy(Checked(absolute));
    auto* data = Allocate<LegacyDeviceOrientationData>();
    data->alpha = alphaValue;
    data->beta = betaValue;
    data->gamma = gammaValue;
    data->absolute = absoluteValue;
    return LegacyDeviceRef<LegacyDeviceOrientationData>(data);
}

LegacyDeviceRef<LegacyDeviceMotionData> APS5_VABI _ZN7WebCore16DeviceMotionData6createEON3WTF6RefPtrINS0_12AccelerationENS1_13DumbPtrTraitsIS3_EEEES7_ONS2_INS0_12RotationRateENS4_IS8_EEEENS1_8OptionalIdEE(
    LegacyDeviceMotionVector** acceleration, LegacyDeviceMotionVector** accelerationIncludingGravity, LegacyDeviceMotionVector** rotationRate,
    const LegacyOptionalDouble* interval) {
    if (!acceleration || !accelerationIncludingGravity || !rotationRate) throw std::invalid_argument("null WTF::RefPtr");
    const LegacyOptionalDouble intervalValue = Copy(Checked(interval));
    auto* data = Allocate<LegacyDeviceMotionData>();
    data->acceleration = std::exchange(*acceleration, nullptr);
    data->accelerationIncludingGravity = std::exchange(*accelerationIncludingGravity, nullptr);
    data->rotationRate = std::exchange(*rotationRate, nullptr);
    data->interval = intervalValue;
    return LegacyDeviceRef<LegacyDeviceMotionData>(data);
}

}
