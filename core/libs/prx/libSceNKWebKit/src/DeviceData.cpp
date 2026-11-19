// Derived from Source/WebCore/dom/DeviceMotionData.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
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

// Derived from Source/WebCore/dom/DeviceOrientationData.cpp@73a35b43be7ae44c084885bd3fa4b403e59723c8
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

#include "prx/libSceNKWebKit/include/DeviceDataExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <new>
#include <stdexcept>
#include <string>
#include <utility>

using NKWebKit::WebCoreAbi::DeviceDataRef;
using NKWebKit::WebCoreAbi::DeviceDataRefPtr;
using NKWebKit::WebCoreAbi::DeviceMotionAcceleration;
using NKWebKit::WebCoreAbi::DeviceMotionData;
using NKWebKit::WebCoreAbi::DeviceMotionRotationRate;
using NKWebKit::WebCoreAbi::DeviceOrientationData;
using NKWebKit::WebCoreAbi::LegacyOptionalBool;
using NKWebKit::WebCoreAbi::LegacyOptionalDouble;

namespace {

template <typename T>
T* NewRefCounted() {
    return new (_ZN3WTF10fastMallocEm(sizeof(T))) T{};
}

template <typename T>
T* TakeRefPtr(DeviceDataRefPtr<T>& slot) {
    return std::exchange(slot.ptr, nullptr);
}

}

extern "C" {

DeviceDataRef<DeviceMotionData> APS5_VABI _ZN7WebCore16DeviceMotionData6createEv() {
    return DeviceDataRef<DeviceMotionData>(NewRefCounted<DeviceMotionData>());
}

DeviceDataRef<DeviceMotionData> APS5_VABI _ZN7WebCore16DeviceMotionData6createEON3WTF6RefPtrINS0_12AccelerationENS1_13DumbPtrTraitsIS3_EEEES7_ONS2_INS0_12RotationRateENS4_IS8_EEEENS1_8OptionalIdEE(
    DeviceDataRefPtr<DeviceMotionAcceleration>* acceleration,
    DeviceDataRefPtr<DeviceMotionAcceleration>* accelerationIncludingGravity,
    DeviceDataRefPtr<DeviceMotionRotationRate>* rotationRate,
    LegacyOptionalDouble interval) {
    if (!acceleration || !accelerationIncludingGravity || !rotationRate) throw std::invalid_argument(std::string(__func__) + ": null RefPtr argument");
    auto* data = NewRefCounted<DeviceMotionData>();
    data->acceleration = TakeRefPtr(*acceleration);
    data->accelerationIncludingGravity = TakeRefPtr(*accelerationIncludingGravity);
    data->rotationRate = TakeRefPtr(*rotationRate);
    data->interval = interval;
    return DeviceDataRef<DeviceMotionData>(data);
}

DeviceDataRef<DeviceOrientationData> APS5_VABI _ZN7WebCore21DeviceOrientationData6createEN3WTF8OptionalIdEES3_S3_NS2_IbEE(
    LegacyOptionalDouble alpha,
    LegacyOptionalDouble beta,
    LegacyOptionalDouble gamma,
    LegacyOptionalBool absolute) {
    auto* data = NewRefCounted<DeviceOrientationData>();
    data->alpha = alpha;
    data->beta = beta;
    data->gamma = gamma;
    data->absolute = absolute;
    return DeviceDataRef<DeviceOrientationData>(data);
}

}
