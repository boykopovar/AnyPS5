#pragma once

#include <cstdint>

namespace NKWebKit::WebCoreAbi {

enum class FontCascadeCodePath : std::uint8_t { Auto, Simple, Complex, SimpleWithGlyphOverflow };

static_assert(sizeof(FontCascadeCodePath) == 1);
static_assert(static_cast<std::uint8_t>(FontCascadeCodePath::SimpleWithGlyphOverflow) == 3);

}  // namespace NKWebKit::WebCoreAbi

namespace NKWebKit {

bool AccessibilityEnabled();
bool AccessibilityEnhancedUserInterfaceEnabled();
WebCoreAbi::FontCascadeCodePath CurrentFontCascadeCodePath();

}  // namespace NKWebKit
