#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using NKWebKit::WebCoreAbi::LegacyDeviceMotionData;
using NKWebKit::WebCoreAbi::LegacyDeviceMotionVector;
using NKWebKit::WebCoreAbi::LegacyDeviceOrientationData;
using NKWebKit::WebCoreAbi::LegacyDeviceRef;
using NKWebKit::WebCoreAbi::LegacyOptionalBool;
using NKWebKit::WebCoreAbi::LegacyOptionalDouble;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "device motion data check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static LegacyOptionalDouble Some(double value) {
    return {true, value};
}

static LegacyOptionalDouble None(double leftover = 0) {
    return {false, leftover};
}

static bool Is(const LegacyOptionalDouble& optional, double value) {
    return optional.engaged && std::memcmp(&optional.value, &value, sizeof(value)) == 0;
}

static LegacyDeviceMotionVector* Vector(double x) {
    auto* vector = static_cast<LegacyDeviceMotionVector*>(_ZN3WTF10fastMallocEm(sizeof(LegacyDeviceMotionVector)));
    *vector = LegacyDeviceMotionVector{1, Some(x), None(), None()};
    return vector;
}

static void TestOrientation() {
    constexpr auto create = &_ZN7WebCore21DeviceOrientationData6createEN3WTF8OptionalIdEES3_S3_NS2_IbEE;
    {
        const LegacyOptionalDouble alpha = Some(1.5);
        const LegacyOptionalDouble beta = Some(-0.0);
        const LegacyOptionalDouble gamma = Some(360.25);
        const LegacyOptionalBool absolute{true, true};
        const LegacyDeviceRef<LegacyDeviceOrientationData> data = create(&alpha, &beta, &gamma, &absolute);
        Require(data.ptr != nullptr && data.ptr->refCount == 1);
        Require(Is(data.ptr->alpha, 1.5) && Is(data.ptr->beta, -0.0) && Is(data.ptr->gamma, 360.25));
        Require(data.ptr->absolute.engaged && data.ptr->absolute.value);
        const LegacyDeviceRef<LegacyDeviceOrientationData> other = create(&alpha, &beta, &gamma, &absolute);
        Require(other.ptr != data.ptr);
    }
    {
        const LegacyOptionalDouble alpha = None(7);
        const LegacyOptionalDouble beta = Some(2);
        const LegacyOptionalDouble gamma = None(9);
        const LegacyOptionalBool absolute{false, true};
        const LegacyDeviceRef<LegacyDeviceOrientationData> data = create(&alpha, &beta, &gamma, &absolute);
        Require(!data.ptr->alpha.engaged && data.ptr->alpha.value == 0 && Is(data.ptr->beta, 2) && !data.ptr->gamma.engaged && data.ptr->gamma.value == 0);
        Require(!data.ptr->absolute.engaged && !data.ptr->absolute.value);
    }
    {
        const LegacyOptionalDouble alpha = Some(1);
        const LegacyOptionalDouble beta = None();
        const LegacyOptionalDouble gamma = Some(3);
        const LegacyOptionalBool absolute{true, false};
        const LegacyDeviceRef<LegacyDeviceOrientationData> data = create(&alpha, &beta, &gamma, &absolute);
        Require(Is(data.ptr->alpha, 1) && !data.ptr->beta.engaged && Is(data.ptr->gamma, 3));
        Require(data.ptr->absolute.engaged && !data.ptr->absolute.value);
        ++data.ptr->refCount;
        data.ptr->deref();
        Require(data.ptr->refCount == 1);
    }
    const LegacyOptionalDouble value = Some(1);
    const LegacyOptionalBool flag{true, true};
    Require(Throws([&] { create(nullptr, &value, &value, &flag); }));
    Require(Throws([&] { create(&value, nullptr, &value, &flag); }));
    Require(Throws([&] { create(&value, &value, nullptr, &flag); }));
    Require(Throws([&] { create(&value, &value, &value, nullptr); }));
}

static void TestMotion() {
    constexpr auto create = &_ZN7WebCore16DeviceMotionData6createEON3WTF6RefPtrINS0_12AccelerationENS1_13DumbPtrTraitsIS3_EEEES7_ONS2_INS0_12RotationRateENS4_IS8_EEEENS1_8OptionalIdEE;
    {
        LegacyDeviceMotionVector* acceleration = Vector(1);
        LegacyDeviceMotionVector* gravity = Vector(2);
        LegacyDeviceMotionVector* rotation = Vector(3);
        LegacyDeviceMotionVector* const first = acceleration;
        LegacyDeviceMotionVector* const second = gravity;
        LegacyDeviceMotionVector* const third = rotation;
        ++first->refCount;
        const LegacyOptionalDouble interval = Some(16.5);
        {
            const LegacyDeviceRef<LegacyDeviceMotionData> data = create(&acceleration, &gravity, &rotation, &interval);
            Require(acceleration == nullptr && gravity == nullptr && rotation == nullptr);
            Require(data.ptr->refCount == 1 && data.ptr->acceleration == first && data.ptr->accelerationIncludingGravity == second && data.ptr->rotationRate == third);
            Require(first->refCount == 2 && second->refCount == 1 && third->refCount == 1 && Is(data.ptr->interval, 16.5));
        }
        Require(first->refCount == 1 && Is(first->x, 1));
        first->deref();
    }
    {
        LegacyDeviceMotionVector* none = nullptr;
        LegacyDeviceMotionVector* rotation = Vector(3);
        const LegacyOptionalDouble interval = None(5);
        const LegacyDeviceRef<LegacyDeviceMotionData> data = create(&none, &none, &rotation, &interval);
        Require(data.ptr->acceleration == nullptr && data.ptr->accelerationIncludingGravity == nullptr && data.ptr->rotationRate != nullptr);
        Require(!data.ptr->interval.engaged && data.ptr->interval.value == 0 && rotation == nullptr);
    }
    {
        LegacyDeviceMotionVector* shared = Vector(4);
        LegacyDeviceMotionVector* const kept = shared;
        LegacyDeviceMotionVector* rotation = nullptr;
        const LegacyOptionalDouble interval = Some(1);
        const LegacyDeviceRef<LegacyDeviceMotionData> data = create(&shared, &shared, &rotation, &interval);
        Require(data.ptr->acceleration == kept && data.ptr->accelerationIncludingGravity == nullptr && shared == nullptr);
    }
    LegacyDeviceMotionVector* vector = nullptr;
    const LegacyOptionalDouble interval = Some(1);
    Require(Throws([&] { create(nullptr, &vector, &vector, &interval); }));
    Require(Throws([&] { create(&vector, nullptr, &vector, &interval); }));
    Require(Throws([&] { create(&vector, &vector, nullptr, &interval); }));
    LegacyDeviceMotionVector* kept = Vector(5);
    LegacyDeviceMotionVector* const original = kept;
    Require(Throws([&] { create(&kept, &vector, &vector, nullptr); }));
    Require(kept == original && kept->refCount == 1);
    kept->deref();
}

int main() {
    TestOrientation();
    TestMotion();
    std::puts("libSceNKWebKit device motion data tests passed");
    return 0;
}
