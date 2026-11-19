#include "prx/libSceNKWebKit/include/DeviceDataExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::DeviceDataRef;
using NKWebKit::WebCoreAbi::DeviceDataRefPtr;
using NKWebKit::WebCoreAbi::DeviceMotionAcceleration;
using NKWebKit::WebCoreAbi::DeviceMotionData;
using NKWebKit::WebCoreAbi::DeviceMotionRotationRate;
using NKWebKit::WebCoreAbi::DeviceOrientationData;
using NKWebKit::WebCoreAbi::LegacyOptionalBool;
using NKWebKit::WebCoreAbi::LegacyOptionalDouble;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "DeviceData check failed at line %u\n", location.line());
        std::abort();
    }
}

template <typename T>
T* Adopt(DeviceDataRef<T>&& ref) {
    T* object = ref.leakRef();
    Check(object != nullptr);
    Check(reinterpret_cast<std::uintptr_t>(object) % alignof(T) == 0);
    return object;
}

template <typename T>
void Release(T* object) {
    _ZN3WTF8fastFreeEPv(object);
}

template <typename T>
T* Fake(std::uintptr_t address) {
    return reinterpret_cast<T*>(address);
}

void EmptyMotion() {
    auto* data = Adopt(_ZN7WebCore16DeviceMotionData6createEv());
    Check(data->refCount == 1);
    Check(data->acceleration == nullptr);
    Check(data->accelerationIncludingGravity == nullptr);
    Check(data->rotationRate == nullptr);
    Check(!data->interval.hasValue);
    Release(data);
}

void MotionTakesReferences() {
    DeviceDataRefPtr<DeviceMotionAcceleration> acceleration{Fake<DeviceMotionAcceleration>(0x1000)};
    DeviceDataRefPtr<DeviceMotionAcceleration> gravity{Fake<DeviceMotionAcceleration>(0x2000)};
    DeviceDataRefPtr<DeviceMotionRotationRate> rotation{Fake<DeviceMotionRotationRate>(0x3000)};
    auto* data = Adopt(_ZN7WebCore16DeviceMotionData6createEON3WTF6RefPtrINS0_12AccelerationENS1_13DumbPtrTraitsIS3_EEEES7_ONS2_INS0_12RotationRateENS4_IS8_EEEENS1_8OptionalIdEE(
        &acceleration, &gravity, &rotation, LegacyOptionalDouble(true, 0.016)));
    Check(data->refCount == 1);
    Check(data->acceleration == Fake<DeviceMotionAcceleration>(0x1000));
    Check(data->accelerationIncludingGravity == Fake<DeviceMotionAcceleration>(0x2000));
    Check(data->rotationRate == Fake<DeviceMotionRotationRate>(0x3000));
    Check(data->interval.hasValue);
    Check(data->interval.value == 0.016);
    Check(acceleration.ptr == nullptr);
    Check(gravity.ptr == nullptr);
    Check(rotation.ptr == nullptr);
    Release(data);
}

void MotionKeepsNullMembers() {
    DeviceDataRefPtr<DeviceMotionAcceleration> acceleration{};
    DeviceDataRefPtr<DeviceMotionAcceleration> gravity{Fake<DeviceMotionAcceleration>(0x2000)};
    DeviceDataRefPtr<DeviceMotionRotationRate> rotation{};
    auto* data = Adopt(_ZN7WebCore16DeviceMotionData6createEON3WTF6RefPtrINS0_12AccelerationENS1_13DumbPtrTraitsIS3_EEEES7_ONS2_INS0_12RotationRateENS4_IS8_EEEENS1_8OptionalIdEE(
        &acceleration, &gravity, &rotation, LegacyOptionalDouble()));
    Check(data->acceleration == nullptr);
    Check(data->accelerationIncludingGravity == Fake<DeviceMotionAcceleration>(0x2000));
    Check(data->rotationRate == nullptr);
    Check(!data->interval.hasValue);
    Check(gravity.ptr == nullptr);
    Release(data);
}

void MotionRejectsNullArguments() {
    DeviceDataRefPtr<DeviceMotionAcceleration> acceleration{Fake<DeviceMotionAcceleration>(0x1000)};
    DeviceDataRefPtr<DeviceMotionRotationRate> rotation{};
    bool threw = false;
    try {
        _ZN7WebCore16DeviceMotionData6createEON3WTF6RefPtrINS0_12AccelerationENS1_13DumbPtrTraitsIS3_EEEES7_ONS2_INS0_12RotationRateENS4_IS8_EEEENS1_8OptionalIdEE(
            &acceleration, nullptr, &rotation, LegacyOptionalDouble());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Check(threw);
    Check(acceleration.ptr == Fake<DeviceMotionAcceleration>(0x1000));
}

void OrientationStoresValues() {
    auto* data = Adopt(_ZN7WebCore21DeviceOrientationData6createEN3WTF8OptionalIdEES3_S3_NS2_IbEE(
        LegacyOptionalDouble(true, 90.5), LegacyOptionalDouble(true, -45.25), LegacyOptionalDouble(true, 0.0), LegacyOptionalBool(true, true)));
    Check(data->refCount == 1);
    Check(data->alpha.hasValue && data->alpha.value == 90.5);
    Check(data->beta.hasValue && data->beta.value == -45.25);
    Check(data->gamma.hasValue && data->gamma.value == 0.0);
    Check(data->absolute.hasValue && data->absolute.value);
    Release(data);
}

void OrientationKeepsMissingValues() {
    auto* data = Adopt(_ZN7WebCore21DeviceOrientationData6createEN3WTF8OptionalIdEES3_S3_NS2_IbEE(
        LegacyOptionalDouble(), LegacyOptionalDouble(true, 12.0), LegacyOptionalDouble(), LegacyOptionalBool(true, false)));
    Check(!data->alpha.hasValue);
    Check(data->beta.hasValue && data->beta.value == 12.0);
    Check(!data->gamma.hasValue);
    Check(data->absolute.hasValue && !data->absolute.value);
    Release(data);

    data = Adopt(_ZN7WebCore21DeviceOrientationData6createEN3WTF8OptionalIdEES3_S3_NS2_IbEE(
        LegacyOptionalDouble(), LegacyOptionalDouble(), LegacyOptionalDouble(), LegacyOptionalBool()));
    Check(!data->alpha.hasValue && !data->beta.hasValue && !data->gamma.hasValue);
    Check(!data->absolute.hasValue);
    Release(data);
}

}

int main() {
    EmptyMotion();
    MotionTakesReferences();
    MotionKeepsNullMembers();
    MotionRejectsNullArguments();
    OrientationStoresValues();
    OrientationKeepsMissingValues();
    std::puts("DeviceData tests passed");
    return 0;
}
