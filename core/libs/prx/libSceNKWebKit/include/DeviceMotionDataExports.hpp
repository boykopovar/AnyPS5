#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/DeviceMotionAbi.hpp"

extern "C" {

NKWebKit::WebCoreAbi::LegacyDeviceRef<NKWebKit::WebCoreAbi::LegacyDeviceOrientationData> APS5_VABI _ZN7WebCore21DeviceOrientationData6createEN3WTF8OptionalIdEES3_S3_NS2_IbEE(
    const NKWebKit::WebCoreAbi::LegacyOptionalDouble* alpha, const NKWebKit::WebCoreAbi::LegacyOptionalDouble* beta,
    const NKWebKit::WebCoreAbi::LegacyOptionalDouble* gamma, const NKWebKit::WebCoreAbi::LegacyOptionalBool* absolute);
NKWebKit::WebCoreAbi::LegacyDeviceRef<NKWebKit::WebCoreAbi::LegacyDeviceMotionData> APS5_VABI _ZN7WebCore16DeviceMotionData6createEON3WTF6RefPtrINS0_12AccelerationENS1_13DumbPtrTraitsIS3_EEEES7_ONS2_INS0_12RotationRateENS4_IS8_EEEENS1_8OptionalIdEE(
    NKWebKit::WebCoreAbi::LegacyDeviceMotionVector** acceleration, NKWebKit::WebCoreAbi::LegacyDeviceMotionVector** accelerationIncludingGravity,
    NKWebKit::WebCoreAbi::LegacyDeviceMotionVector** rotationRate, const NKWebKit::WebCoreAbi::LegacyOptionalDouble* interval);

}
