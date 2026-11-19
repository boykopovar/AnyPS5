#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <array>

namespace WebCore {

template <typename T>
struct RectEdges {
    std::array<T, 4> m_sides{};
};

static_assert(sizeof(RectEdges<float>) == 16);

}  // namespace WebCore

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::RectEdges<float>& edges);

}
