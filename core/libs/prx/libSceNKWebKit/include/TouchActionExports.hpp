#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cstdint>

namespace NKWebKit::WebCoreAbi {

enum class TouchAction : std::uint8_t {
    Auto = 1 << 0,
    None = 1 << 1,
    Manipulation = 1 << 2,
    PanX = 1 << 3,
    PanY = 1 << 4,
    PinchZoom = 1 << 5,
};

static_assert(sizeof(TouchAction) == 1);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamENS_11TouchActionE(NKWebKit::WtfAbi::TextStream& stream, NKWebKit::WebCoreAbi::TouchAction touchAction);

}
