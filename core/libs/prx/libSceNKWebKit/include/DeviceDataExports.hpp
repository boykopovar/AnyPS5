#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace NKWebKit::WebCoreAbi {

struct LegacyOptionalDouble {
    bool hasValue{};
    double value{};

    LegacyOptionalDouble() = default;
    LegacyOptionalDouble(bool engaged, double contents) : hasValue(engaged), value(engaged ? contents : 0.0) {}
    LegacyOptionalDouble(const LegacyOptionalDouble& other) : hasValue(other.hasValue), value(other.hasValue ? other.value : 0.0) {}
    LegacyOptionalDouble(LegacyOptionalDouble&& other) noexcept : LegacyOptionalDouble(other) {}
    LegacyOptionalDouble& operator=(const LegacyOptionalDouble&) = default;
};

static_assert(sizeof(LegacyOptionalDouble) == 16);
static_assert(offsetof(LegacyOptionalDouble, hasValue) == 0);
static_assert(offsetof(LegacyOptionalDouble, value) == 8);
static_assert(!std::is_trivially_copy_constructible_v<LegacyOptionalDouble>);
static_assert(std::is_trivially_destructible_v<LegacyOptionalDouble>);

struct LegacyOptionalBool {
    bool hasValue{};
    bool value{};

    LegacyOptionalBool() = default;
    LegacyOptionalBool(bool engaged, bool contents) : hasValue(engaged), value(engaged && contents) {}
    LegacyOptionalBool(const LegacyOptionalBool& other) : hasValue(other.hasValue), value(other.hasValue && other.value) {}
    LegacyOptionalBool(LegacyOptionalBool&& other) noexcept : LegacyOptionalBool(other) {}
    LegacyOptionalBool& operator=(const LegacyOptionalBool&) = default;
};

static_assert(sizeof(LegacyOptionalBool) == 2);
static_assert(offsetof(LegacyOptionalBool, hasValue) == 0);
static_assert(offsetof(LegacyOptionalBool, value) == 1);
static_assert(!std::is_trivially_copy_constructible_v<LegacyOptionalBool>);

struct DeviceMotionAcceleration;
struct DeviceMotionRotationRate;

template <typename T>
struct DeviceDataRefPtr {
    T* ptr{};
};

static_assert(sizeof(DeviceDataRefPtr<DeviceMotionAcceleration>) == 8);

template <typename T>
struct DeviceDataRef {
    T* ptr{};

    explicit DeviceDataRef(T* object) : ptr(object) {}
    DeviceDataRef(const DeviceDataRef&) = delete;
    DeviceDataRef(DeviceDataRef&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}
    DeviceDataRef& operator=(const DeviceDataRef&) = delete;
    T* leakRef() noexcept { return std::exchange(ptr, nullptr); }
};

static_assert(sizeof(DeviceDataRef<int>) == 8);
static_assert(!std::is_trivially_move_constructible_v<DeviceDataRef<int>>);

struct DeviceMotionData {
    unsigned refCount{1};
    DeviceMotionAcceleration* acceleration{};
    DeviceMotionAcceleration* accelerationIncludingGravity{};
    DeviceMotionRotationRate* rotationRate{};
    LegacyOptionalDouble interval;
};

static_assert(offsetof(DeviceMotionData, acceleration) == 8);
static_assert(offsetof(DeviceMotionData, accelerationIncludingGravity) == 16);
static_assert(offsetof(DeviceMotionData, rotationRate) == 24);
static_assert(offsetof(DeviceMotionData, interval) == 32);
static_assert(sizeof(DeviceMotionData) == 48);

struct DeviceOrientationData {
    unsigned refCount{1};
    LegacyOptionalDouble alpha;
    LegacyOptionalDouble beta;
    LegacyOptionalDouble gamma;
    LegacyOptionalBool absolute;
};

static_assert(offsetof(DeviceOrientationData, alpha) == 8);
static_assert(offsetof(DeviceOrientationData, beta) == 24);
static_assert(offsetof(DeviceOrientationData, gamma) == 40);
static_assert(offsetof(DeviceOrientationData, absolute) == 56);
static_assert(sizeof(DeviceOrientationData) == 64);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

NKWebKit::WebCoreAbi::DeviceDataRef<NKWebKit::WebCoreAbi::DeviceMotionData> APS5_VABI _ZN7WebCore16DeviceMotionData6createEv();
NKWebKit::WebCoreAbi::DeviceDataRef<NKWebKit::WebCoreAbi::DeviceMotionData> APS5_VABI _ZN7WebCore16DeviceMotionData6createEON3WTF6RefPtrINS0_12AccelerationENS1_13DumbPtrTraitsIS3_EEEES7_ONS2_INS0_12RotationRateENS4_IS8_EEEENS1_8OptionalIdEE(
    NKWebKit::WebCoreAbi::DeviceDataRefPtr<NKWebKit::WebCoreAbi::DeviceMotionAcceleration>* acceleration,
    NKWebKit::WebCoreAbi::DeviceDataRefPtr<NKWebKit::WebCoreAbi::DeviceMotionAcceleration>* accelerationIncludingGravity,
    NKWebKit::WebCoreAbi::DeviceDataRefPtr<NKWebKit::WebCoreAbi::DeviceMotionRotationRate>* rotationRate,
    NKWebKit::WebCoreAbi::LegacyOptionalDouble interval);
NKWebKit::WebCoreAbi::DeviceDataRef<NKWebKit::WebCoreAbi::DeviceOrientationData> APS5_VABI _ZN7WebCore21DeviceOrientationData6createEN3WTF8OptionalIdEES3_S3_NS2_IbEE(
    NKWebKit::WebCoreAbi::LegacyOptionalDouble alpha,
    NKWebKit::WebCoreAbi::LegacyOptionalDouble beta,
    NKWebKit::WebCoreAbi::LegacyOptionalDouble gamma,
    NKWebKit::WebCoreAbi::LegacyOptionalBool absolute);

}
