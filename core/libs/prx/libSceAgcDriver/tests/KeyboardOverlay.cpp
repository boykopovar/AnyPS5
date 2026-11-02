#include "prx/libSceAgcDriver/Execution/include/KeyboardOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceImeDialog/include/PadKeyboard.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::KeyboardMetrics;
using AgcDriver::OverlayColor;
using AgcDriver::OverlayLayout;
using AgcDriver::OverlayRect;
using ImeDialog::PadKeyboardCase;
using ImeDialog::PadKeyboardCurrency;
using ImeDialog::PadKeyboardFamily;

void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint32_t colorOf(OverlayColor color) {
    return static_cast<std::uint32_t>(color);
}

bool contains(const OverlayRect& outer, const OverlayRect& inner) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.width <= outer.x + outer.width && inner.y + inner.height <= outer.y + outer.height;
}

ImeKeySymbol symbolOf(const ImeDialog::PadKeyboardState& state, ImeDialog::PadKeyGlyph glyph) {
    switch (glyph) {
        case ImeDialog::PadKeyGlyph::Shift:
            return state.caseState == PadKeyboardCase::Upper ? ImeKeySymbol::ShiftFilled : state.caseState == PadKeyboardCase::CapsLock ? ImeKeySymbol::CapsLock : ImeKeySymbol::ShiftOutline;
        case ImeDialog::PadKeyGlyph::Backspace: return ImeKeySymbol::Backspace;
        case ImeDialog::PadKeyGlyph::ArrowLeft: return ImeKeySymbol::ArrowLeft;
        case ImeDialog::PadKeyGlyph::ArrowRight: return ImeKeySymbol::ArrowRight;
        case ImeDialog::PadKeyGlyph::ArrowUp: return ImeKeySymbol::ArrowUp;
        case ImeDialog::PadKeyGlyph::ArrowDown: return ImeKeySymbol::ArrowDown;
        case ImeDialog::PadKeyGlyph::None: break;
    }
    return ImeKeySymbol::None;
}

ImeKeyboardView viewOf(const ImeDialog::PadKeyboardState& state, const std::u16string& text, std::size_t caret) {
    ImeKeyboardView view;
    view.text = text;
    view.caret = caret;
    view.rows = ImeDialog::PadKeyboardRows(state);
    view.columns = ImeDialog::PadKeyboardColumns();
    const auto* selected = ImeDialog::PadKeyboardSelected(state);
    for (const auto& key : ImeDialog::PadKeyboardKeys(state)) {
        view.keys.push_back({key.row, key.column, key.columnSpan, key.rowSpan, ImeDialog::PadKeyboardLabel(state, key), symbolOf(state, key.glyph), key.badge != nullptr ? std::u16string(key.badge) : std::u16string(), &key == selected});
    }
    return view;
}

struct Page {
    std::string name;
    ImeDialog::PadKeyboardState state;
};

std::vector<Page> pages() {
    struct Shown {
        const char* name;
        PadKeyboardFamily family;
        PadKeyboardCase caseState;
        std::uint8_t page;
    };
    constexpr std::array<Shown, 8> shown{{
        {"latin lower", PadKeyboardFamily::Latin, PadKeyboardCase::Lower, 0},
        {"latin upper", PadKeyboardFamily::Latin, PadKeyboardCase::Upper, 0},
        {"latin caps lock", PadKeyboardFamily::Latin, PadKeyboardCase::CapsLock, 0},
        {"symbols 1/2", PadKeyboardFamily::Symbols, PadKeyboardCase::Lower, 0},
        {"symbols 2/2", PadKeyboardFamily::Symbols, PadKeyboardCase::Lower, 1},
        {"specials lower", PadKeyboardFamily::Specials, PadKeyboardCase::Lower, 0},
        {"specials upper", PadKeyboardFamily::Specials, PadKeyboardCase::Upper, 0},
        {"specials caps lock", PadKeyboardFamily::Specials, PadKeyboardCase::CapsLock, 0},
    }};
    std::vector<Page> result;
    for (const auto& entry : shown) {
        ImeDialog::PadKeyboardOptions options;
        options.family = entry.family;
        options.caseState = entry.caseState;
        auto state = ImeDialog::PadKeyboardOpen(options);
        state.page = entry.page;
        result.push_back({entry.name, state});
    }
    return result;
}

std::set<char16_t> charactersOf(const ImeKeyboardView& view) {
    std::set<char16_t> characters;
    for (const auto& key : view.keys) {
        characters.insert(key.label.begin(), key.label.end());
        characters.insert(key.badge.begin(), key.badge.end());
    }
    return characters;
}

OverlayRect keyRect(const OverlayLayout& layout, const ImeKeyCell& key) {
    const auto scale = layout.scale;
    const auto top = 2 * KeyboardMetrics::Padding + KeyboardMetrics::FieldHeight;
    const auto x = KeyboardMetrics::Padding + key.column * (KeyboardMetrics::KeyWidth + KeyboardMetrics::Gap);
    const auto y = top + key.row * (KeyboardMetrics::KeyHeight + KeyboardMetrics::Gap);
    const auto w = key.columnSpan * KeyboardMetrics::KeyWidth + (key.columnSpan - 1) * KeyboardMetrics::Gap;
    const auto h = key.rowSpan * KeyboardMetrics::KeyHeight + (key.rowSpan - 1) * KeyboardMetrics::Gap;
    return {layout.bounds.x + x * scale, layout.bounds.y + y * scale, w * scale, h * scale, 0};
}

std::size_t inside(const OverlayLayout& layout, const OverlayRect& area, OverlayColor color) {
    std::size_t count = 0;
    for (const auto& rect : layout.rects) {
        if (rect.color == colorOf(color) && contains(area, rect)) ++count;
    }
    return count;
}

void pageLayoutTests() {
    std::size_t views = 0;
    for (const auto& page : pages()) {
        const auto view = viewOf(page.state, u"Hello", 5);
        for (const VkExtent2D extent : {VkExtent2D{1280, 720}, VkExtent2D{1920, 1080}}) {
            const auto where = page.name + " at " + std::to_string(extent.width) + "x" + std::to_string(extent.height);
            const auto layout = AgcDriver::LayOutKeyboard(view, extent);
            const auto expectedScale = static_cast<std::int32_t>(extent.height / 360);
            require(layout.scale == expectedScale, where + ": scale " + std::to_string(layout.scale) + ", not " + std::to_string(expectedScale) + " screen pixels per glyph pixel");
            const auto& bounds = layout.bounds;
            const auto screenWidth = static_cast<std::int32_t>(extent.width);
            const auto screenHeight = static_cast<std::int32_t>(extent.height);
            require(bounds.width == AgcDriver::KeyboardPanelWidth(view) * layout.scale && bounds.height == AgcDriver::KeyboardPanelHeight(view) * layout.scale, where + ": the panel is not its grid at the layout's scale");
            require(bounds.y >= screenHeight / 2 && bounds.y + bounds.height <= screenHeight, where + ": the panel leaves the bottom half");
            require(std::abs((bounds.y - screenHeight / 2) - (screenHeight - bounds.y - bounds.height)) <= 1, where + ": the panel is not centred in the bottom half");
            require(bounds.x >= 0 && std::abs(bounds.x - (screenWidth - bounds.x - bounds.width)) <= 1, where + ": the panel is not centred across the frame");
            require(layout.rects.front().color == colorOf(OverlayColor::Panel) && layout.rects.front().x == bounds.x && layout.rects.front().width == bounds.width, where + ": the panel is not drawn first");
            for (const auto& rect : layout.rects) require(contains(bounds, rect) && rect.width > 0 && rect.height > 0, where + ": a rectangle leaves the panel");
            std::size_t selected = 0;
            for (const auto& key : view.keys) {
                const auto area = keyRect(layout, key);
                const bool drawsSomething = key.symbol != ImeKeySymbol::None || !key.label.empty();
                if (drawsSomething) require(inside(layout, area, OverlayColor::Text) != 0, where + ": a key draws no label");
                if (!key.badge.empty()) require(inside(layout, area, OverlayColor::Graph) != 0, where + ": a key draws no badge");
                if (!key.selected) continue;
                ++selected;
                const auto border = KeyboardMetrics::Border * layout.scale;
                bool outer = false;
                bool inner = false;
                for (const auto& rect : layout.rects) {
                    outer = outer || (rect.color == colorOf(OverlayColor::Accent) && rect.x == area.x && rect.y == area.y && rect.width == area.width && rect.height == area.height);
                    inner = inner || (rect.color == colorOf(OverlayColor::Key) && rect.x == area.x + border && rect.y == area.y + border && rect.width == area.width - 2 * border && rect.height == area.height - 2 * border);
                }
                require(outer && inner, where + ": the selected key has no accent border " + std::to_string(border) + " pixels wide");
            }
            require(selected == 1, where + ": " + std::to_string(selected) + " keys are selected, not one");
            ++views;
        }
    }
    std::printf("page layouts: %zu pages x 2 resolutions, panel centred in the bottom half, scale 2 at 720p and 3 at 1080p, every label and badge drawn, one bordered key\n", views / 2);
}

void labelTests() {
    std::set<std::u16string> enterLabels;
    std::set<std::u16string> currencies;
    for (std::uint32_t enter = 0; enter < 5; ++enter) {
        ImeDialog::PadKeyboardOptions options;
        options.enterLabel = enter;
        const auto view = viewOf(ImeDialog::PadKeyboardOpen(options), u"", 0);
        for (const auto& key : view.keys) {
            if (key.badge == u"R2") enterLabels.insert(key.label);
        }
        for (const VkExtent2D extent : {VkExtent2D{1280, 720}, VkExtent2D{1920, 1080}}) {
            const auto layout = AgcDriver::LayOutKeyboard(view, extent);
            for (const auto& key : view.keys) {
                if (key.badge == u"R2") require(inside(layout, keyRect(layout, key), OverlayColor::Text) != 0, "the enter key draws no label");
            }
        }
    }
    require(enterLabels == std::set<std::u16string>{u"Done", u"Send", u"Search", u"Go"}, "the enter key does not read Done, Send, Search and Go");
    for (const auto currency : {PadKeyboardCurrency::Euro, PadKeyboardCurrency::Dollar, PadKeyboardCurrency::Pound, PadKeyboardCurrency::Yen, PadKeyboardCurrency::Won}) {
        ImeDialog::PadKeyboardOptions options;
        options.family = PadKeyboardFamily::Symbols;
        options.currency = currency;
        const auto view = viewOf(ImeDialog::PadKeyboardOpen(options), u"", 0);
        for (const auto& key : view.keys) {
            if (key.row == 3 && key.column == 5) currencies.insert(key.label);
        }
        for (const auto character : charactersOf(view)) require(AgcDriver::KeyboardHasGlyph(character), "a currency page has a character without a glyph");
    }
    require(currencies == std::set<std::u16string>{u"€", u"$", u"£", u"¥", u"₩"}, "the currency key does not cover euro, dollar, pound, yen and won");
    std::printf("labels: Done, Send, Search and Go drawn on the enter key; euro, dollar, pound, yen and won on the currency key\n");
}

void coverageTests() {
    const auto table = AgcDriver::KeyboardGlyphTable();
    for (std::size_t index = 1; index < table.size(); ++index) require(table[index - 1].code < table[index].code, "the glyph table is not sorted by code");
    for (char32_t code = 0x20; code < 0x7F; ++code) require(AgcDriver::KeyboardHasGlyph(code), "printable ASCII " + std::to_string(code) + " has no glyph");
    std::set<char16_t> pageCharacters;
    for (const auto& page : pages()) {
        const auto characters = charactersOf(viewOf(page.state, u"", 0));
        pageCharacters.insert(characters.begin(), characters.end());
    }
    std::size_t nonAscii = 0;
    for (const auto character : pageCharacters) {
        require(AgcDriver::KeyboardHasGlyph(character), "page character U+" + std::to_string(static_cast<unsigned>(character)) + " has no glyph");
        if (character > 0x7E) ++nonAscii;
    }
    require(AgcDriver::KeyboardHasGlyph(AgcDriver::KeyboardFallbackCode), "the fallback box has no glyph");
    std::set<std::array<std::uint8_t, KeyboardMetrics::GlyphHeight>> symbols;
    for (const auto symbol : {ImeKeySymbol::ShiftOutline, ImeKeySymbol::ShiftFilled, ImeKeySymbol::CapsLock, ImeKeySymbol::Backspace, ImeKeySymbol::ArrowLeft, ImeKeySymbol::ArrowRight, ImeKeySymbol::ArrowUp, ImeKeySymbol::ArrowDown}) {
        const auto& rows = AgcDriver::KeyboardSymbolGlyph(symbol);
        require(std::any_of(rows.begin(), rows.end(), [](std::uint8_t bits) { return bits != 0; }), "a key symbol glyph is empty");
        symbols.insert(rows);
    }
    require(symbols.size() == 8, "two key symbols share a glyph");
    for (const auto& entry : table) {
        for (const auto bits : entry.rows) require(bits < 0x20, "a glyph is wider than five pixels");
        if (entry.code != u' ') require(std::any_of(entry.rows.begin(), entry.rows.end(), [](std::uint8_t bits) { return bits != 0; }), "a glyph other than space is empty");
    }
    std::printf("glyph coverage: %zu glyphs, all 95 printable ASCII, %zu page characters (%zu outside ASCII), 8 key symbols and the fallback box\n", table.size(), pageCharacters.size(), nonAscii);
}

std::size_t fieldGlyphPixels(const OverlayLayout& layout) {
    const auto scale = layout.scale;
    const OverlayRect field{layout.bounds.x + KeyboardMetrics::Padding * scale, layout.bounds.y + KeyboardMetrics::Padding * scale, (layout.bounds.width / scale - 2 * KeyboardMetrics::Padding) * scale, KeyboardMetrics::FieldHeight * scale, 0};
    std::size_t pixels = 0;
    for (const auto& rect : layout.rects) {
        if (rect.color == colorOf(OverlayColor::Text) && contains(field, rect)) pixels += static_cast<std::size_t>(rect.width / scale);
    }
    return pixels;
}

std::size_t glyphPixels(const AgcDriver::KeyboardGlyphRows& rows) {
    std::size_t pixels = 0;
    for (const auto bits : rows) pixels += static_cast<std::size_t>(std::popcount(static_cast<unsigned>(bits)));
    return pixels;
}

void fallbackTests() {
    ImeDialog::PadKeyboardOptions options;
    auto view = viewOf(ImeDialog::PadKeyboardOpen(options), u"日本語😀", 5);
    view.keys.front().label = u"中";
    const VkExtent2D extent{1920, 1080};
    const auto layout = AgcDriver::LayOutKeyboard(view, extent);
    require(!AgcDriver::KeyboardHasGlyph(0x65E5) && !AgcDriver::KeyboardHasGlyph(0x1F600), "a CJK or emoji character has a glyph of its own");
    const auto box = glyphPixels(AgcDriver::KeyboardGlyph(AgcDriver::KeyboardFallbackCode));
    require(&AgcDriver::KeyboardGlyph(0x65E5) == &AgcDriver::KeyboardGlyph(AgcDriver::KeyboardFallbackCode) && &AgcDriver::KeyboardGlyph(0x1F600) == &AgcDriver::KeyboardGlyph(AgcDriver::KeyboardFallbackCode), "CJK and emoji do not fall back to the box");
    require(fieldGlyphPixels(layout) == 4 * box, "three CJK characters and one surrogate pair do not draw four fallback boxes");
    require(inside(layout, keyRect(layout, view.keys.front()), OverlayColor::Text) != 0, "a CJK key label draws nothing");
    bool threw = false;
    try {
        static_cast<void>(AgcDriver::OverlayGlyph('\x80'));
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "the performance overlay font no longer throws for a character it lacks");
    std::printf("fallback: CJK and an emoji pair drawn as four boxes, a CJK key label drawn, the performance overlay font never consulted\n");
}

void caretTests() {
    ImeDialog::PadKeyboardOptions options;
    const auto state = ImeDialog::PadKeyboardOpen(options);
    const VkExtent2D extent{1280, 720};
    std::u16string text(120, u'a');
    for (const std::size_t caret : {std::size_t{0}, std::size_t{7}, std::size_t{60}, std::size_t{120}}) {
        const auto view = viewOf(state, text, caret);
        const auto layout = AgcDriver::LayOutKeyboard(view, extent);
        const auto scale = layout.scale;
        const OverlayRect field{layout.bounds.x + KeyboardMetrics::Padding * scale, layout.bounds.y + KeyboardMetrics::Padding * scale, (layout.bounds.width / scale - 2 * KeyboardMetrics::Padding) * scale, KeyboardMetrics::FieldHeight * scale, 0};
        const auto carets = inside(layout, field, OverlayColor::Accent);
        require(carets == 1, "the field draws " + std::to_string(carets) + " caret bars at caret " + std::to_string(caret));
        for (const auto& rect : layout.rects) {
            if (rect.color != colorOf(OverlayColor::Accent) || !contains(field, rect)) continue;
            require(rect.width == scale && rect.height == (KeyboardMetrics::FieldHeight - 2) * scale, "the caret is not a bar one glyph pixel wide");
            const auto column = (rect.x + scale - field.x) / scale - KeyboardMetrics::FieldInset;
            if (caret <= 44) require(column == static_cast<std::int32_t>(caret) * KeyboardMetrics::Advance, "the caret is not after character " + std::to_string(caret));
            else require(column == 44 * KeyboardMetrics::Advance, "a caret past the field does not scroll the text to keep it at the right edge");
        }
        require(fieldGlyphPixels(layout) == 44 * glyphPixels(AgcDriver::KeyboardGlyph(u'a')), "the field does not show 44 characters of a long text");
    }
    ImeKeyboardView bad = viewOf(state, u"ab", 3);
    bool threw = false;
    try {
        static_cast<void>(AgcDriver::LayOutKeyboard(bad, extent));
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "a caret past the text is laid out");
    std::printf("caret: one bar per field, after its character, the text scrolled to keep it in the 44-character field\n");
}

double linear(double encoded) {
    return encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
}

double pqNits(double code) {
    constexpr double m1 = 2610.0 / 16384.0;
    constexpr double m2 = 2523.0 / 4096.0 * 128.0;
    constexpr double c1 = 3424.0 / 4096.0;
    constexpr double c2 = 2413.0 / 4096.0 * 32.0;
    constexpr double c3 = 2392.0 / 4096.0 * 32.0;
    const double power = std::pow(code, 1.0 / m2);
    return 10000.0 * std::pow(std::max(power - c1, 0.0) / (c2 - c3 * power), 1.0 / m1);
}

double luminance(const std::array<float, 4>& color, bool pq) {
    const auto channel = [&](std::size_t index) { return pq ? pqNits(color[index]) : linear(color[index]); };
    return pq ? 0.2627 * channel(0) + 0.6780 * channel(1) + 0.0593 * channel(2) : 0.2126 * channel(0) + 0.7152 * channel(1) + 0.0722 * channel(2);
}

double contrast(double a, double b, double floor) {
    return (std::max(a, b) + floor) / (std::min(a, b) + floor);
}

void paletteTests() {
    for (const bool pq : {false, true}) {
        const auto palette = pq ? AgcDriver::OverlayPalette(VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT) : AgcDriver::OverlayPalette(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
        const auto name = std::string(pq ? "PQ" : "sRGB");
        const double floor = pq ? 0.05 * 203.0 : 0.05;
        const auto of = [&](OverlayColor color) { return luminance(palette[colorOf(color)], pq); };
        require(palette[colorOf(OverlayColor::Panel)][3] >= 0.9f && palette[colorOf(OverlayColor::Key)][3] == 1.0f && palette[colorOf(OverlayColor::Accent)][3] == 1.0f && palette[colorOf(OverlayColor::Field)][3] == 1.0f, name + ": the keyboard is not opaque enough to read over the game");
        require(contrast(of(OverlayColor::Text), of(OverlayColor::Key), floor) >= 7.0, name + ": key labels do not reach 7:1 on their keys");
        require(contrast(of(OverlayColor::Text), of(OverlayColor::Field), floor) >= 7.0, name + ": the text does not reach 7:1 on its field");
        require(contrast(of(OverlayColor::Accent), of(OverlayColor::Key), floor) >= 4.5, name + ": the selected border does not reach 4.5:1 on a key");
        require(contrast(of(OverlayColor::Accent), of(OverlayColor::Field), floor) >= 4.5, name + ": the caret does not reach 4.5:1 on the field");
        require(contrast(of(OverlayColor::Graph), of(OverlayColor::Key), floor) >= 4.5, name + ": badges do not reach 4.5:1 on their keys");
        std::printf("%s palette: labels %.1f:1, text %.1f:1, border %.1f:1, caret %.1f:1, badges %.1f:1\n", name.c_str(), contrast(of(OverlayColor::Text), of(OverlayColor::Key), floor), contrast(of(OverlayColor::Text), of(OverlayColor::Field), floor), contrast(of(OverlayColor::Accent), of(OverlayColor::Key), floor), contrast(of(OverlayColor::Accent), of(OverlayColor::Field), floor), contrast(of(OverlayColor::Graph), of(OverlayColor::Key), floor));
    }
}

ModalDialogView dialogOf(const std::u16string& message, std::vector<std::u16string> labels, std::size_t focus) {
    ModalDialogView view;
    view.message = message;
    for (std::size_t index = 0; index < labels.size(); ++index) view.buttons.push_back({labels[index], index == focus});
    return view;
}

std::vector<OverlayRect> rectsOf(const OverlayLayout& layout, OverlayColor color) {
    std::vector<OverlayRect> found;
    for (const auto& rect : layout.rects) {
        if (rect.color == colorOf(color)) found.push_back(rect);
    }
    return found;
}

void dialogWrapTests() {
    using AgcDriver::WrapDialogMessage;
    using Lines = std::vector<std::u32string>;
    require(WrapDialogMessage(u"hello world", 5, 8) == Lines{U"hello", U"world"}, "words are not wrapped at the column limit");
    require(WrapDialogMessage(u"a b c", 5, 8) == Lines{U"a b c"}, "words that fit were split");
    require(WrapDialogMessage(u"abcdefghij", 4, 8) == Lines{U"abcd", U"efgh", U"ij"}, "a long word is not broken at the column limit");
    require(WrapDialogMessage(u"one\ntwo", 20, 8) == Lines{U"one", U"two"}, "a line break is not kept");
    require(WrapDialogMessage(u"one\n\ntwo", 20, 8) == Lines{U"one", U"", U"two"}, "a blank line is not kept");
    require(WrapDialogMessage(u"one\r\ntwo", 20, 8) == Lines{U"one", U"two"}, "a carriage return is drawn");
    require(WrapDialogMessage(u"  spaced   out  ", 20, 8) == Lines{U"spaced out"}, "extra spaces are kept");
    require(WrapDialogMessage(u"", 20, 8) == Lines{U""}, "an empty message has no line");
    require(WrapDialogMessage(u"one\n", 20, 8) == Lines{U"one"}, "a trailing line break adds a line");
    require(WrapDialogMessage(u"a\U0001F600b", 4, 8) == Lines{U"a\U0001F600b"}, "a surrogate pair counts as two characters");
    require(WrapDialogMessage(u"aa bb cc dd ee", 5, 2) == Lines{U"aa bb", U"cc..."}, "an overlong message does not end in dots");
    require(WrapDialogMessage(u"abcde fghij klmno", 5, 2) == Lines{U"abcde", U"fg..."}, "a full last line is not cut to make room for the dots");
    bool threw = false;
    try {
        static_cast<void>(WrapDialogMessage(u"x", 3, 8));
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "a dialog with no room for its text was accepted");
    std::printf("dialog wrap: words, long words, breaks, dots\n");
}

void dialogLayoutTests() {
    const auto message = u"Overwrite the save?\nThis cannot be undone, and the previous data cannot be restored afterwards.";
    for (const VkExtent2D extent : {VkExtent2D{640, 360}, VkExtent2D{1280, 720}, VkExtent2D{1920, 1080}, VkExtent2D{3840, 2160}}) {
        for (const std::size_t count : {std::size_t{1}, std::size_t{2}, std::size_t{3}}) {
            const std::vector<std::u16string> labels{u"Yes", u"No", u"Cancel"};
            const auto view = dialogOf(message, {labels.begin(), labels.begin() + static_cast<std::ptrdiff_t>(count)}, count - 1);
            const auto layout = AgcDriver::LayOutDialog(view, extent);
            const auto name = std::to_string(extent.width) + "x" + std::to_string(extent.height) + " with " + std::to_string(count) + " buttons";
            require(layout.scale == std::max<std::int32_t>(1, static_cast<std::int32_t>(extent.height / KeyboardMetrics::LinesPerScale)), name + ": the dialog scale is not the keyboard's");
            require(contains({0, 0, static_cast<std::int32_t>(extent.width), static_cast<std::int32_t>(extent.height), 0}, layout.bounds), name + ": the dialog leaves the screen");
            require(std::abs(layout.bounds.x + layout.bounds.width / 2 - static_cast<std::int32_t>(extent.width) / 2) <= layout.scale && std::abs(layout.bounds.y + layout.bounds.height / 2 - static_cast<std::int32_t>(extent.height) / 2) <= layout.scale, name + ": the dialog is not centred");
            require(layout.rects.front().color == colorOf(OverlayColor::Panel) && layout.rects.front().x == layout.bounds.x, name + ": the panel is not drawn first");
            for (const auto& rect : layout.rects) require(contains(layout.bounds, rect), name + ": a rectangle leaves the panel");
            const auto accents = rectsOf(layout, OverlayColor::Accent);
            require(rectsOf(layout, OverlayColor::Key).size() == count && accents.size() == 1, name + ": one key per button and one accent border are expected");
            std::vector<OverlayRect> buttons;
            for (const auto& rect : layout.rects) {
                if ((rect.color == colorOf(OverlayColor::Key) || rect.color == colorOf(OverlayColor::Accent)) && rect.height == AgcDriver::DialogMetrics::ButtonHeight * layout.scale) buttons.push_back(rect);
            }
            std::sort(buttons.begin(), buttons.end(), [](const OverlayRect& a, const OverlayRect& b) { return a.x < b.x; });
            require(buttons.size() == count, name + ": the buttons are not all drawn");
            for (std::size_t index = 1; index < buttons.size(); ++index) require(buttons[index].x >= buttons[index - 1].x + buttons[index - 1].width, name + ": the buttons overlap");
            require(buttons.back().color == colorOf(OverlayColor::Accent), name + ": the accent border is not on the focused button");
            require(!rectsOf(layout, OverlayColor::Text).empty(), name + ": no text is drawn");
        }
    }
    std::printf("dialog layout: bounds, centring, buttons, focus\n");

    const VkExtent2D extent{1280, 720};
    const auto first = AgcDriver::LayOutDialog(dialogOf(u"Question", {u"Yes", u"No"}, 0), extent);
    const auto second = AgcDriver::LayOutDialog(dialogOf(u"Question", {u"Yes", u"No"}, 1), extent);
    require(first.bounds.x == second.bounds.x && first.bounds.width == second.bounds.width, "the focus changed the panel size");
    require(rectsOf(first, OverlayColor::Accent).front().x < rectsOf(second, OverlayColor::Accent).front().x, "the accent border does not follow the focus");
    require(rectsOf(AgcDriver::LayOutDialog(dialogOf(u"Question", {u"Yes", u"No"}, 5), extent), OverlayColor::Accent).empty(), "a dialog with no focused button drew an accent border");

    const auto crowded = dialogOf(std::u16string(400, u'w'), {u"OK"}, 0);
    const auto crowdedLayout = AgcDriver::LayOutDialog(crowded, extent);
    require(contains({0, 0, 1280, 720, 0}, crowdedLayout.bounds), "a long message makes the dialog leave the screen");
    require(crowdedLayout.bounds.height <= 720, "a long message makes the dialog taller than the screen");

    const auto small = AgcDriver::LayOutDialog(dialogOf(u"Very long message that needs the whole panel width to be shown at once here", {u"Yes", u"No", u"Cancel"}, 0), VkExtent2D{1280, 720});
    const auto narrow = AgcDriver::LayOutDialog(dialogOf(u"Very long message that needs the whole panel width to be shown at once here", {u"Yes", u"No", u"Cancel"}, 0), VkExtent2D{320, 720});
    require(narrow.scale == 1 && narrow.scale < small.scale, "the dialog does not shrink for a narrow target");

    for (const auto& bad : {dialogOf(u"x", {}, 0), dialogOf(u"x", {u"a", u"b", u"c", u"d"}, 0)}) {
        bool threw = false;
        try {
            static_cast<void>(AgcDriver::LayOutDialog(bad, extent));
        } catch (const std::exception&) {
            threw = true;
        }
        require(threw, "a dialog without one to three buttons was laid out");
    }
    bool threw = false;
    try {
        static_cast<void>(AgcDriver::LayOutDialog(dialogOf(u"x", {u"OK"}, 0), VkExtent2D{0, 0}));
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "a dialog without a destination was laid out");
    std::printf("dialog layout: focus, long text, narrow target, invalid input\n");
}

void systemDialogTests() {
    const VkExtent2D extent{1280, 720};
    require(!AgcDriver::SystemDialogView().has_value() && !AgcDriver::SystemOverlayLayout(extent).has_value(), "an overlay is laid out before one was set");
    const auto dialog = dialogOf(u"Continue?", {u"Yes", u"No"}, 1);
    AgcDriverSetSystemDialog_nid_postfix(&dialog);
    const auto shown = AgcDriver::SystemDialogView();
    require(shown.has_value() && shown->message == u"Continue?" && shown->buttons.size() == 2 && shown->buttons[1].selected, "the shown dialog is not the one set");
    const auto layout = AgcDriver::SystemOverlayLayout(extent);
    const auto expected = AgcDriver::LayOutDialog(dialog, extent);
    require(layout.has_value() && layout->bounds.x == expected.bounds.x && layout->bounds.width == expected.bounds.width && layout->rects.size() == expected.rects.size(), "the system overlay is not the dialog");
    ImeDialog::PadKeyboardOptions options;
    const auto keyboard = viewOf(ImeDialog::PadKeyboardOpen(options), u"ab", 1);
    AgcDriverSetSystemOverlay_nid_postfix(&keyboard);
    const auto both = AgcDriver::SystemOverlayLayout(extent);
    const auto keyboardLayout = AgcDriver::LayOutKeyboard(keyboard, extent);
    require(both.has_value() && both->bounds.width == keyboardLayout.bounds.width && both->rects.size() == keyboardLayout.rects.size(), "the keyboard does not take the overlay from the dialog");
    AgcDriverSetSystemOverlay_nid_postfix(nullptr);
    require(AgcDriver::SystemOverlayLayout(extent).has_value(), "the dialog is gone after the keyboard was cleared");
    AgcDriverSetSystemDialog_nid_postfix(nullptr);
    require(!AgcDriver::SystemDialogView().has_value() && !AgcDriver::SystemOverlayLayout(extent).has_value(), "a cleared dialog is still shown");
    std::printf("system dialog: set, read back, ranked below the keyboard and cleared\n");
}

void systemOverlayTests() {
    require(!AgcDriver::SystemOverlayView().has_value(), "a keyboard is shown before one was set");
    ImeDialog::PadKeyboardOptions options;
    const auto view = viewOf(ImeDialog::PadKeyboardOpen(options), u"ab", 1);
    AgcDriverSetSystemOverlay_nid_postfix(&view);
    const auto shown = AgcDriver::SystemOverlayView();
    require(shown.has_value() && shown->text == u"ab" && shown->caret == 1 && shown->keys.size() == view.keys.size(), "the shown keyboard is not the one set");
    AgcDriverSetSystemOverlay_nid_postfix(nullptr);
    require(!AgcDriver::SystemOverlayView().has_value(), "a cleared keyboard is still shown");
    std::printf("system overlay: set, read back and cleared\n");
}

}

int main() {
    try {
        coverageTests();
        pageLayoutTests();
        labelTests();
        fallbackTests();
        caretTests();
        paletteTests();
        systemOverlayTests();
        dialogWrapTests();
        dialogLayoutTests();
        systemDialogTests();
        std::cout << "keyboard overlay tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
