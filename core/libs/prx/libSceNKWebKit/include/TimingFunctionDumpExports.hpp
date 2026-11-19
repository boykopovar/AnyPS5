#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace WebCore {

struct TimingFunction {
    enum class Type : int {
        LinearFunction,
        CubicBezierFunction,
        StepsFunction,
        SpringFunction
    };

    static constexpr std::size_t TypeSlot = 3;

    void* const* vtable;
    std::uint32_t refCount;
};

struct CubicBezierTimingFunction {
    void* const* vtable;
    std::uint32_t refCount;
    double x1;
    double y1;
    double x2;
    double y2;
    std::uint8_t timingFunctionPreset;
};

struct StepsTimingFunction {
    enum class StepPosition : std::uint8_t {
        JumpStart,
        JumpEnd,
        JumpNone,
        JumpBoth,
        Start,
        End
    };

    void* const* vtable;
    std::uint32_t refCount;
    std::int32_t steps;
    StepPosition stepPosition;
    bool hasStepPosition;
};

struct SpringTimingFunction {
    void* const* vtable;
    std::uint32_t refCount;
    double mass;
    double stiffness;
    double damping;
    double initialVelocity;
};

static_assert(sizeof(TimingFunction) == 16);
static_assert(offsetof(TimingFunction, refCount) == 8);
static_assert(sizeof(CubicBezierTimingFunction) == 56);
static_assert(offsetof(CubicBezierTimingFunction, x1) == 16);
static_assert(offsetof(CubicBezierTimingFunction, timingFunctionPreset) == 48);
static_assert(sizeof(StepsTimingFunction) == 24);
static_assert(offsetof(StepsTimingFunction, steps) == 12);
static_assert(offsetof(StepsTimingFunction, stepPosition) == 16);
static_assert(offsetof(StepsTimingFunction, hasStepPosition) == 17);
static_assert(sizeof(SpringTimingFunction) == 48);
static_assert(offsetof(SpringTimingFunction, mass) == 16);

}  // namespace WebCore

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_14TimingFunctionE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::TimingFunction& timingFunction);

}
