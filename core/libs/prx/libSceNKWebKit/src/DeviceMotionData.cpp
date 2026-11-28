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
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>
#include <type_traits>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::LegacyDeviceMotionData;
using NKWebKit::WebCoreAbi::LegacyDeviceMotionVector;
using NKWebKit::WebCoreAbi::LegacyDeviceOrientationData;
using NKWebKit::WebCoreAbi::LegacyDeviceRef;
using NKWebKit::WebCoreAbi::LegacyOptionalBool;
using NKWebKit::WebCoreAbi::LegacyOptionalDouble;

static_assert(std::is_trivially_copyable_v<LegacyOptionalDouble>);
static_assert(std::is_trivially_copyable_v<LegacyOptionalBool>);
static_assert(std::is_trivially_copyable_v<LegacyDeviceMotionVector*>);

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template<typename T>
void RequireWritableGuestValue(T* pointer, const char* function) {
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, sizeof(T));
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, sizeof(T));
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename T>
bool TryPublishGuestValue(T* pointer, const T& value, const T& previous) {
    if (GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T))) return true;
    static_cast<void>(GuestCopyOut_nid_no_patch(pointer, &previous, sizeof(T)));
    return false;
}

template<typename T>
T ReadOptional(const T* optional, const char* function) {
    if (!optional) throw std::invalid_argument("null WTF::Optional");
    return ReadGuestValue(optional, function);
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
    const LegacyOptionalDouble alphaValue = Copy(ReadOptional(alpha, __func__));
    const LegacyOptionalDouble betaValue = Copy(ReadOptional(beta, __func__));
    const LegacyOptionalDouble gammaValue = Copy(ReadOptional(gamma, __func__));
    const LegacyOptionalBool absoluteValue = Copy(ReadOptional(absolute, __func__));
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
    const LegacyOptionalDouble intervalValue = Copy(ReadOptional(interval, __func__));
    auto* const previousAcceleration = ReadGuestValue(acceleration, __func__);
    auto* const previousGravity = ReadGuestValue(accelerationIncludingGravity, __func__);
    auto* const previousRotation = ReadGuestValue(rotationRate, __func__);
    RequireWritableGuestValue(acceleration, __func__);
    RequireWritableGuestValue(accelerationIncludingGravity, __func__);
    RequireWritableGuestValue(rotationRate, __func__);
    LegacyDeviceMotionVector* const takenAcceleration = previousAcceleration;
    LegacyDeviceMotionVector* const takenGravity =
        accelerationIncludingGravity == acceleration ? nullptr : previousGravity;
    LegacyDeviceMotionVector* const takenRotation =
        (rotationRate == acceleration || rotationRate == accelerationIncludingGravity) ? nullptr : previousRotation;
    auto* data = Allocate<LegacyDeviceMotionData>();
    LegacyDeviceMotionVector* const nullVector = nullptr;
    if (!TryPublishGuestValue(acceleration, nullVector, previousAcceleration)) {
        _ZN3WTF8fastFreeEPv(data);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if (accelerationIncludingGravity != acceleration &&
        !TryPublishGuestValue(accelerationIncludingGravity, nullVector, previousGravity)) {
        static_cast<void>(GuestCopyOut_nid_no_patch(acceleration, &previousAcceleration, sizeof(previousAcceleration)));
        _ZN3WTF8fastFreeEPv(data);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if (rotationRate != acceleration && rotationRate != accelerationIncludingGravity &&
        !TryPublishGuestValue(rotationRate, nullVector, previousRotation)) {
        if (accelerationIncludingGravity != acceleration)
            static_cast<void>(GuestCopyOut_nid_no_patch(accelerationIncludingGravity, &previousGravity, sizeof(previousGravity)));
        static_cast<void>(GuestCopyOut_nid_no_patch(acceleration, &previousAcceleration, sizeof(previousAcceleration)));
        _ZN3WTF8fastFreeEPv(data);
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    data->acceleration = takenAcceleration;
    data->accelerationIncludingGravity = takenGravity;
    data->rotationRate = takenRotation;
    data->interval = intervalValue;
    return LegacyDeviceRef<LegacyDeviceMotionData>(data);
}

}
