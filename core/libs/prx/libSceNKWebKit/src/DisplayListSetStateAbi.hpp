#pragma once

#include "prx/libSceNKWebKit/src/DisplayListItemAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::DisplayListSetStateAbi {

struct State {
    void* strokeGradient;
    void* strokePattern;
    void* fillGradient;
    void* fillPattern;
    WebCore::FloatSize shadowOffset;
    float strokeThickness;
    float shadowBlur;
    std::uint8_t textDrawingMode;
    unsigned char padding1[7];
    WebCore::Color strokeColor;
    WebCore::Color fillColor;
    WebCore::Color shadowColor;
    std::uint8_t strokeStyle;
    std::uint8_t fillRule;
    unsigned char padding2[2];
    float alpha;
    std::uint8_t compositeOperator;
    std::uint8_t blendMode;
    std::uint8_t imageInterpolationQuality;
    std::uint8_t flags;
};

struct StateChange {
    State state;
    std::uint32_t changeFlags;
    unsigned char padding[4];
};

struct SetState {
    DisplayListAbi::Item item;
    StateChange stateChange;
};

static_assert(std::is_standard_layout_v<State>);
static_assert(sizeof(State) == 96);
static_assert(alignof(State) == 8);
static_assert(offsetof(State, shadowOffset) == 32);
static_assert(offsetof(State, strokeThickness) == 40);
static_assert(offsetof(State, strokeColor) == 56);
static_assert(offsetof(State, fillColor) == 64);
static_assert(offsetof(State, shadowColor) == 72);
static_assert(offsetof(State, alpha) == 84);
static_assert(offsetof(State, compositeOperator) == 88);

static_assert(std::is_standard_layout_v<StateChange>);
static_assert(sizeof(StateChange) == 104);
static_assert(alignof(StateChange) == 8);
static_assert(offsetof(StateChange, state) == 0);
static_assert(offsetof(StateChange, changeFlags) == 96);

static_assert(std::is_standard_layout_v<SetState>);
static_assert(sizeof(SetState) == 120);
static_assert(alignof(SetState) == 8);
static_assert(offsetof(SetState, item) == 0);
static_assert(offsetof(SetState, stateChange) == 16);

}
