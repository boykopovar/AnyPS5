#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WebCore {

enum ScrollElasticity : std::uint8_t {
    ScrollElasticityAutomatic,
    ScrollElasticityNone,
    ScrollElasticityAllowed
};

enum ScrollbarMode : std::uint8_t {
    ScrollbarAuto,
    ScrollbarAlwaysOff,
    ScrollbarAlwaysOn
};

struct ScrollableAreaParameters {
    ScrollElasticity horizontalScrollElasticity;
    ScrollElasticity verticalScrollElasticity;
    ScrollbarMode horizontalScrollbarMode;
    ScrollbarMode verticalScrollbarMode;
    bool hasEnabledHorizontalScrollbar;
    bool hasEnabledVerticalScrollbar;
    bool horizontalScrollbarHiddenByStyle;
    bool verticalScrollbarHiddenByStyle;
    bool useDarkAppearanceForScrollbars;
};

static_assert(std::is_trivially_copyable_v<ScrollableAreaParameters>);
static_assert(offsetof(ScrollableAreaParameters, horizontalScrollbarMode) == 2);
static_assert(offsetof(ScrollableAreaParameters, hasEnabledHorizontalScrollbar) == 4);
static_assert(offsetof(ScrollableAreaParameters, useDarkAppearanceForScrollbars) == 8);
static_assert(sizeof(ScrollableAreaParameters) == 9);

}  // namespace WebCore

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE(NKWebKit::WtfAbi::TextStream& stream, WebCore::ScrollableAreaParameters parameters);

}
