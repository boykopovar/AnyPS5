#include "prx/libSceFontFt/include/Api.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font scissor check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static int allocations = 0;
static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    ++allocations;
    return std::malloc(size);
}
static void APS5_VABI Release(void*, void* pointer) {
    if (pointer) --allocations;
    std::free(pointer);
}

static void Put16(std::vector<unsigned char>& out, std::uint32_t value) {
    out.push_back(static_cast<unsigned char>(value >> 8));
    out.push_back(static_cast<unsigned char>(value));
}

static void Put32(std::vector<unsigned char>& out, std::uint32_t value) {
    Put16(out, value >> 16);
    Put16(out, value & 0xFFFFu);
}

static void PutAll16(std::vector<unsigned char>& out, std::initializer_list<int> values) {
    for (const int value : values) Put16(out, static_cast<std::uint32_t>(static_cast<std::uint16_t>(value)));
}

static std::vector<unsigned char> SquareGlyphFont() {
    constexpr std::uint32_t codepoint = 'A';
    constexpr int width = 900;
    std::vector<unsigned char> head;
    Put32(head, 0x00010000u);
    Put32(head, 0x00010000u);
    Put32(head, 0);
    Put32(head, 0x5F0F3CF5u);
    PutAll16(head, {0x000B, 1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, width, 800, 0, 8, 2, 0, 0});
    std::vector<unsigned char> hhea;
    Put32(hhea, 0x00010000u);
    PutAll16(hhea, {800, -200, 0, width, 0, 0, width, 1, 0, 0, 0, 0, 0, 0, 0, 2});
    std::vector<unsigned char> maxp;
    Put32(maxp, 0x00010000u);
    PutAll16(maxp, {2, 4, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0});
    std::vector<unsigned char> hmtx;
    PutAll16(hmtx, {width, 0, width, 100});
    std::vector<unsigned char> glyf;
    PutAll16(glyf, {1, 100, 0, width - 100, 700, 3, 0});
    glyf.insert(glyf.end(), {1, 1, 1, 1});
    PutAll16(glyf, {100, 0, width - 200, 0, 0, 700, 0, -700});
    std::vector<unsigned char> loca;
    PutAll16(loca, {0, 0, static_cast<int>(glyf.size() / 2)});
    std::vector<unsigned char> cmap;
    PutAll16(cmap, {0, 1, 3, 1});
    Put32(cmap, 12);
    PutAll16(cmap, {4, 32, 0, 4, 4, 1, 0, static_cast<int>(codepoint), 0xFFFF, 0, static_cast<int>(codepoint), 0xFFFF, 1 - static_cast<int>(codepoint), 1, 0, 0});
    const std::vector<std::pair<const char*, const std::vector<unsigned char>*>> tables = {
        {"cmap", &cmap}, {"glyf", &glyf}, {"head", &head}, {"hhea", &hhea}, {"hmtx", &hmtx}, {"loca", &loca}, {"maxp", &maxp}};
    std::vector<unsigned char> font;
    Put32(font, 0x00010000u);
    PutAll16(font, {static_cast<int>(tables.size()), 64, 2, static_cast<int>(tables.size()) * 16 - 64});
    std::uint32_t offset = 12 + static_cast<std::uint32_t>(tables.size()) * 16;
    for (const auto& [tag, data] : tables) {
        font.insert(font.end(), tag, tag + 4);
        Put32(font, 0);
        Put32(font, offset);
        Put32(font, static_cast<std::uint32_t>(data->size()));
        offset += (static_cast<std::uint32_t>(data->size()) + 3u) & ~3u;
    }
    for (const auto& [tag, data] : tables) {
        font.insert(font.end(), data->begin(), data->end());
        font.resize((font.size() + 3u) & ~std::size_t{3});
    }
    return font;
}

static void CheckScissor(int width, int height, int x0, int y0, int w, int h, int expectedX0, int expectedY0, int expectedX1, int expectedY1) {
    FontRenderSurface surface{};
    sceFontRenderSurfaceInit(&surface, nullptr, 0, 1, width, height);
    sceFontRenderSurfaceSetScissor(&surface, x0, y0, w, h);
    Require(surface.sc_x0 == expectedX0 && surface.sc_y0 == expectedY0);
    Require(surface.sc_x1 == expectedX1 && surface.sc_y1 == expectedY1);
}

static void CheckScissorArithmetic() {
    CheckScissor(100, 80, 10, 20, 30, 40, 10, 20, 40, 60);
    CheckScissor(100, 80, -10, -5, 30, 20, 0, 0, 20, 15);
    CheckScissor(100, 80, -30, -20, 30, 20, 0, 0, 0, 0);
    CheckScissor(100, 80, 90, 70, 30, 20, 90, 70, 100, 80);
    CheckScissor(100, 80, 100, 80, 10, 10, 100, 80, 100, 80);
    CheckScissor(100, 80, 101, 81, 10, 10, 100, 80, 100, 80);
    CheckScissor(100, 80, 0, 0, 0, 0, 0, 0, 0, 0);
    CheckScissor(100, 80, 50, 40, 0, 0, 50, 40, 50, 40);
    CheckScissor(100, 80, 50, 40, 1000, 1000, 50, 40, 100, 80);
    CheckScissor(100, 80, -1000, -1000, 1000, 1000, 0, 0, 0, 0);
    CheckScissor(100, 80, -5, -5, 10, 10, 0, 0, 5, 5);
    CheckScissor(1, 1, 0, 0, 2, 2, 0, 0, 1, 1);
    CheckScissor(0, 80, 10, 20, 30, 40, 0, 20, 0, 60);
    CheckScissor(100, 0, 10, 20, 30, 40, 10, 0, 40, 0);

    FontRenderSurface surface{};
    sceFontRenderSurfaceInit(&surface, nullptr, 0, 1, 0, 80);
    surface.sc_x0 = 7;
    surface.sc_x1 = 9;
    sceFontRenderSurfaceSetScissor(&surface, 10, 20, 30, 40);
    Require(surface.sc_x0 == 7 && surface.sc_x1 == 9);
    Require(surface.sc_y0 == 20 && surface.sc_y1 == 60);

    sceFontRenderSurfaceInit(&surface, nullptr, 0, 1, 100, 0);
    surface.sc_y0 = 3;
    surface.sc_y1 = 5;
    sceFontRenderSurfaceSetScissor(&surface, 10, 20, 30, 40);
    Require(surface.sc_x0 == 10 && surface.sc_x1 == 40);
    Require(surface.sc_y0 == 3 && surface.sc_y1 == 5);
}

struct RenderedGlyph {
    std::vector<std::uint8_t> pixels = std::vector<std::uint8_t>(128 * 128);
    FontGlyphMetrics metrics{};
    FontRenderOutput output{};
};

static RenderedGlyph RenderGlyph(FontHandle font, int x, int y, bool scissor, int x0 = 0, int y0 = 0, int w = 0, int h = 0) {
    RenderedGlyph rendered;
    FontRenderSurface surface{};
    sceFontRenderSurfaceInit(&surface, rendered.pixels.data(), 128, 1, 128, 128);
    if (scissor) sceFontRenderSurfaceSetScissor(&surface, x0, y0, w, h);
    Require(sceFontRenderCharGlyphImageHorizontal(font, 'A', &surface, static_cast<float>(x), static_cast<float>(y), &rendered.metrics, &rendered.output) == SCE_FONT_OK);
    return rendered;
}

static void CheckRenderedClipping(FontHandle font) {
    const RenderedGlyph reference = RenderGlyph(font, 10, 100, false);
    const auto& rect = reference.output.UpdateRect;
    Require(rect.w > 4 && rect.h > 4);
    Require(rect.x + rect.w <= 128 && rect.y + rect.h <= 128);
    int lit = 0;
    for (std::size_t index = 0; index < reference.pixels.size(); ++index) {
        const int x = static_cast<int>(index % 128);
        const int y = static_cast<int>(index / 128);
        const bool inside = x >= static_cast<int>(rect.x) && x < static_cast<int>(rect.x + rect.w) && y >= static_cast<int>(rect.y) && y < static_cast<int>(rect.y + rect.h);
        if (!inside) Require(reference.pixels[index] == 0);
        if (reference.pixels[index] != 0) ++lit;
    }
    Require(lit > 0);

    const int insetX = static_cast<int>(rect.x) + 1;
    const int insetY = static_cast<int>(rect.y) + 1;
    const int insetW = static_cast<int>(rect.w) - 2;
    const int insetH = static_cast<int>(rect.h) - 2;
    const RenderedGlyph clipped = RenderGlyph(font, 10, 100, true, insetX, insetY, insetW, insetH);
    Require(clipped.output.UpdateRect.x == static_cast<std::uint32_t>(insetX) && clipped.output.UpdateRect.y == static_cast<std::uint32_t>(insetY));
    Require(clipped.output.UpdateRect.w == static_cast<std::uint32_t>(insetW) && clipped.output.UpdateRect.h == static_cast<std::uint32_t>(insetH));
    for (std::size_t index = 0; index < clipped.pixels.size(); ++index) {
        const int x = static_cast<int>(index % 128);
        const int y = static_cast<int>(index / 128);
        const bool inside = x >= insetX && x < insetX + insetW && y >= insetY && y < insetY + insetH;
        Require(clipped.pixels[index] == (inside ? reference.pixels[index] : 0));
    }

    const int largerX = std::max(0, static_cast<int>(rect.x) - 5);
    const int largerY = std::max(0, static_cast<int>(rect.y) - 5);
    const int largerW = std::min(128 - largerX, static_cast<int>(rect.w) + 10);
    const int largerH = std::min(128 - largerY, static_cast<int>(rect.h) + 10);
    const RenderedGlyph larger = RenderGlyph(font, 10, 100, true, largerX, largerY, largerW, largerH);
    Require(std::memcmp(&larger.output.UpdateRect, &rect, sizeof(rect)) == 0);
    Require(larger.pixels == reference.pixels);

    const int negativeX = static_cast<int>(rect.x) - 10;
    const int negativeY = static_cast<int>(rect.y) - 10;
    const RenderedGlyph negative = RenderGlyph(font, 10, 100, true, negativeX, negativeY, static_cast<int>(rect.w) + 20, static_cast<int>(rect.h) + 20);
    Require(std::memcmp(&negative.output.UpdateRect, &rect, sizeof(rect)) == 0);
    Require(negative.pixels == reference.pixels);

    const RenderedGlyph empty = RenderGlyph(font, 10, 100, true, static_cast<int>(rect.x), static_cast<int>(rect.y), 0, 0);
    Require(empty.output.UpdateRect.w == 0 && empty.output.UpdateRect.h == 0);
    Require(std::all_of(empty.pixels.begin(), empty.pixels.end(), [](std::uint8_t value) { return value == 0; }));
    Require(empty.metrics.width > 0.0f && empty.metrics.Horizontal.advance > 0.0f);

    const RenderedGlyph outside = RenderGlyph(font, 10, 100, true, 0, 0, 1, 1);
    Require(outside.output.UpdateRect.w == 0 && outside.output.UpdateRect.h == 0);
    Require(std::all_of(outside.pixels.begin(), outside.pixels.end(), [](std::uint8_t value) { return value == 0; }));
    Require(outside.metrics.width > 0.0f && outside.metrics.Horizontal.advance > 0.0f);
}

int main() {
    CheckScissorArithmetic();

    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 1, 0x52) == SCE_FONT_OK);
    const std::vector<unsigned char> fontData = SquareGlyphFont();
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &font) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    FontRenderer renderer = nullptr;
    Require(sceFontCreateRenderer(&memory, sceFontSelectRendererFt(0), &renderer) == SCE_FONT_OK);
    Require(sceFontBindRenderer(font, renderer) == SCE_FONT_OK);
    Require(sceFontSetupRenderScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    CheckRenderedClipping(font);
    Require(sceFontUnbindRenderer(font) == SCE_FONT_OK);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
}
