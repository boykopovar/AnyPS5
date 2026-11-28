#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

namespace NKWebKit::WebCoreAbi {

struct LegacyOptionalDouble {
    bool engaged;
    double value;
};

struct LegacyOptionalBool {
    bool engaged;
    bool value;
};

struct LegacyDeviceMotionVector {
    unsigned refCount;
    LegacyOptionalDouble x;
    LegacyOptionalDouble y;
    LegacyOptionalDouble z;

    void deref() {
        if (!--refCount) _ZN3WTF8fastFreeEPv(this);
    }
};

struct LegacyDeviceOrientationData {
    unsigned refCount;
    LegacyOptionalDouble alpha;
    LegacyOptionalDouble beta;
    LegacyOptionalDouble gamma;
    LegacyOptionalBool absolute;

    void deref() {
        if (!--refCount) _ZN3WTF8fastFreeEPv(this);
    }
};

struct LegacyDeviceMotionData {
    unsigned refCount;
    LegacyDeviceMotionVector* acceleration;
    LegacyDeviceMotionVector* accelerationIncludingGravity;
    LegacyDeviceMotionVector* rotationRate;
    LegacyOptionalDouble interval;

    void deref() {
        if (--refCount) return;
        if (acceleration) acceleration->deref();
        if (accelerationIncludingGravity) accelerationIncludingGravity->deref();
        if (rotationRate) rotationRate->deref();
        _ZN3WTF8fastFreeEPv(this);
    }
};

template<typename T>
struct LegacyDeviceRef {
    T* ptr;

    explicit LegacyDeviceRef(T* adopted) : ptr(adopted) {}
    LegacyDeviceRef(const LegacyDeviceRef&) = delete;
    LegacyDeviceRef(LegacyDeviceRef&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}
    LegacyDeviceRef& operator=(const LegacyDeviceRef&) = delete;
    LegacyDeviceRef& operator=(LegacyDeviceRef&&) = delete;
    ~LegacyDeviceRef() {
        if (ptr) ptr->deref();
    }
};

static_assert(sizeof(LegacyOptionalDouble) == 16);
static_assert(offsetof(LegacyOptionalDouble, value) == 8);
static_assert(sizeof(LegacyOptionalBool) == 2);
static_assert(offsetof(LegacyOptionalBool, value) == 1);
static_assert(sizeof(LegacyDeviceMotionVector) == 56);
static_assert(offsetof(LegacyDeviceMotionVector, x) == 8);
static_assert(offsetof(LegacyDeviceMotionVector, y) == 24);
static_assert(offsetof(LegacyDeviceMotionVector, z) == 40);
static_assert(sizeof(LegacyDeviceOrientationData) == 64);
static_assert(offsetof(LegacyDeviceOrientationData, alpha) == 8);
static_assert(offsetof(LegacyDeviceOrientationData, beta) == 24);
static_assert(offsetof(LegacyDeviceOrientationData, gamma) == 40);
static_assert(offsetof(LegacyDeviceOrientationData, absolute) == 56);
static_assert(sizeof(LegacyDeviceMotionData) == 48);
static_assert(offsetof(LegacyDeviceMotionData, acceleration) == 8);
static_assert(offsetof(LegacyDeviceMotionData, accelerationIncludingGravity) == 16);
static_assert(offsetof(LegacyDeviceMotionData, rotationRate) == 24);
static_assert(offsetof(LegacyDeviceMotionData, interval) == 32);
static_assert(sizeof(LegacyDeviceRef<LegacyDeviceMotionData>) == 8);
static_assert(!std::is_trivially_copyable_v<LegacyDeviceRef<LegacyDeviceMotionData>>);

}  // namespace NKWebKit::WebCoreAbi
