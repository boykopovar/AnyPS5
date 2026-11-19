#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <cstddef>

namespace JSC {

class Uint8ClampedArray;

}  // namespace JSC

namespace WebCore {

struct ImageData {
    unsigned m_refCount{1};
    IntSize m_size;
    JSC::Uint8ClampedArray* m_data{nullptr};
    int m_colorSpace{0};
};

static_assert(offsetof(ImageData, m_size) == 4);
static_assert(offsetof(ImageData, m_data) == 16);
static_assert(sizeof(ImageData) == 32);

}  // namespace WebCore

extern "C" {

NKWebKit::WtfAbi::TextStream& APS5_VABI _ZN7WebCorelsERN3WTF10TextStreamERKNS_9ImageDataE(NKWebKit::WtfAbi::TextStream& stream, const WebCore::ImageData& imageData);

}
