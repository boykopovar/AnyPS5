#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_KEYBOARDOVERLAY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_KEYBOARDOVERLAY_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceIme/include/ImeKeyboardView.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct KeyboardMetrics {
    static constexpr std::int32_t GlyphWidth = 5;
    static constexpr std::int32_t GlyphHeight = 9;
    static constexpr std::int32_t Advance = 6;
    static constexpr std::int32_t KeyWidth = 26;
    static constexpr std::int32_t KeyHeight = 21;
    static constexpr std::int32_t Gap = 1;
    static constexpr std::int32_t Padding = 4;
    static constexpr std::int32_t FieldHeight = 13;
    static constexpr std::int32_t FieldInset = 2;
    static constexpr std::int32_t Border = 1;
    static constexpr std::int32_t LineGap = 1;
    static constexpr std::int32_t LinesPerScale = 360;
};

struct DialogMetrics {
    static constexpr std::int32_t MaxColumns = 36;
    static constexpr std::int32_t MaxLines = 8;
    static constexpr std::int32_t LineHeight = KeyboardMetrics::GlyphHeight + KeyboardMetrics::LineGap;
    static constexpr std::int32_t Padding = 6;
    static constexpr std::int32_t MessageGap = 8;
    static constexpr std::int32_t ButtonHeight = 17;
    static constexpr std::int32_t ButtonInset = 6;
    static constexpr std::int32_t ButtonGap = 4;
    static constexpr std::int32_t ButtonMinWidth = 36;
    static constexpr std::int32_t ButtonMaxColumns = 16;
};

using KeyboardGlyphRows = std::array<std::uint8_t, KeyboardMetrics::GlyphHeight>;

struct KeyboardGlyphEntry {
    char16_t code;
    KeyboardGlyphRows rows;
};

inline constexpr char32_t KeyboardFallbackCode = 0xFFFD;

std::span<const KeyboardGlyphEntry> KeyboardGlyphTable();
bool KeyboardHasGlyph(char32_t code);
const KeyboardGlyphRows& KeyboardGlyph(char32_t code);
const KeyboardGlyphRows& KeyboardSymbolGlyph(ImeKeySymbol symbol);
std::int32_t KeyboardPanelWidth(const ImeKeyboardView& view);
std::int32_t KeyboardPanelHeight(const ImeKeyboardView& view);
std::int32_t KeyboardScale(VkExtent2D extent, const ImeKeyboardView& view);
OverlayLayout LayOutKeyboard(const ImeKeyboardView& view, VkExtent2D extent);
std::optional<ImeKeyboardView> SystemOverlayView();
std::vector<std::u32string> WrapDialogMessage(const std::u16string& message, std::size_t columns, std::size_t maxLines);
std::int32_t DialogScale(VkExtent2D extent, const ModalDialogView& view);
OverlayLayout LayOutDialog(const ModalDialogView& view, VkExtent2D extent);
std::optional<ModalDialogView> SystemDialogView();
std::optional<OverlayLayout> SystemOverlayLayout(VkExtent2D extent);

}

#endif
