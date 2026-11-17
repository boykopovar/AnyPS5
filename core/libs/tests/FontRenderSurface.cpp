#include "prx/libSceFontFt/libSceFontFt.hpp"
#include "prx/libSceFont/libSceFont.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font surface check failed at line %d\n", line);
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

static std::vector<unsigned char> TestGlyphFont(std::uint32_t codepoint, int width) {
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
    PutAll16(glyf, {100, 0, width - 200, -(width - 200) / 2, 0, 700, 0, -700});
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

using Render = decltype(&sceFontRenderCharGlyphImage);

static constexpr std::size_t guardSize = 64;

static std::size_t SurfaceExtent(int width, int height, int stride, int bpp) {
    if (width == 0 || height == 0) return 0;
    return (static_cast<std::uint64_t>(height) - 1) * static_cast<std::uint64_t>(stride)
        + static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(bpp);
}

static void CheckLayout(Render render, FontHandle font, int width, int height, int stride, int bpp, bool clipped, unsigned char fill) {
    const int referenceStride = width * bpp + 7;
    std::vector<unsigned char> reference(std::max<std::size_t>(SurfaceExtent(width, height, referenceStride, bpp), width == 0 || height == 0 ? 8192 : 0) + 2 * guardSize, fill);
    std::vector<unsigned char> pixels(std::max<std::size_t>(SurfaceExtent(width, height, stride, bpp), width == 0 || height == 0 ? 8192 : 0) + 2 * guardSize, fill);
    auto expected = pixels;
    FontRenderSurface surface{};
    FontRenderSurface referenceSurface{};
    sceFontRenderSurfaceInit(&surface, pixels.data() + guardSize, stride, bpp, width, height);
    sceFontRenderSurfaceInit(&referenceSurface, reference.data() + guardSize, referenceStride, bpp, width, height);
    if (clipped) {
        sceFontRenderSurfaceSetScissor(&surface, 8, 8, 24, 24);
        sceFontRenderSurfaceSetScissor(&referenceSurface, 8, 8, 24, 24);
    }
    FontGlyphMetrics referenceMetrics{};
    FontRenderOutput referenceOutput{};
    FontGlyphMetrics metrics{};
    FontRenderOutput output{};
    Require(render(font, 'A', &referenceSurface, 10.0f, 10.0f, &referenceMetrics, &referenceOutput) == SCE_FONT_OK);
    const int rc = render(font, 'A', &surface, 10.0f, 10.0f, &metrics, &output);
    if (rc != SCE_FONT_OK) {
        std::fprintf(stderr, "Font surface render failed: width=%d height=%d stride=%d bpp=%d rc=0x%08X\n",
            width, height, stride, bpp, static_cast<unsigned int>(rc));
    }
    Require(rc == SCE_FONT_OK);
    Require(std::memcmp(&metrics, &referenceMetrics, sizeof(metrics)) == 0);
    Require(std::memcmp(&output.UpdateRect, &referenceOutput.UpdateRect, sizeof(output.UpdateRect)) == 0);
    Require(std::memcmp(&output.ImageMetrics, &referenceOutput.ImageMetrics, sizeof(output.ImageMetrics)) == 0);
    Require(output.SurfaceImage.widthByte == static_cast<std::uint32_t>(stride));
    Require(output.SurfaceImage.pixelSizeByte == bpp);
    if (width > 0 && height == 64) Require(output.UpdateRect.w > 0 && output.UpdateRect.h > 0);
    if (width == 0 || height == 0) {
        Require(output.UpdateRect.w == 0 && output.UpdateRect.h == 0);
        Require(metrics.width > 0.0f && metrics.height > 0.0f);
    } else if (output.UpdateRect.w > 0 && output.UpdateRect.h > 0) {
        Require(output.UpdateRect.x + output.UpdateRect.w <= static_cast<std::uint32_t>(width));
        Require(output.UpdateRect.y + output.UpdateRect.h <= static_cast<std::uint32_t>(height));
        for (std::uint32_t row = output.UpdateRect.y; row < output.UpdateRect.y + output.UpdateRect.h; ++row) {
            for (std::uint32_t column = output.UpdateRect.x; column < output.UpdateRect.x + output.UpdateRect.w; ++column) {
                for (int byte = 0; byte < bpp; ++byte) {
                    const auto destination = guardSize + static_cast<std::size_t>(row) * stride + static_cast<std::size_t>(column) * bpp + byte;
                    const auto source = guardSize + static_cast<std::size_t>(row) * referenceStride + static_cast<std::size_t>(column) * bpp + byte;
                    Require(destination < expected.size() - guardSize);
                    expected[destination] = reference[source];
                }
            }
        }
        const auto address = reinterpret_cast<std::uintptr_t>(pixels.data() + guardSize)
            + static_cast<std::size_t>(output.UpdateRect.y) * stride + static_cast<std::size_t>(output.UpdateRect.x) * bpp;
        Require(reinterpret_cast<std::uintptr_t>(output.SurfaceImage.address) == address);
        Require(std::any_of(expected.begin() + guardSize, expected.end() - guardSize, [fill](unsigned char value) { return value != fill; }));
    }
    if (pixels != expected) {
        std::fprintf(stderr, "Font surface layout differs: width=%d height=%d stride=%d bpp=%d scissor=%d fill=%u\n",
            width, height, stride, bpp, clipped, fill);
    }
    Require(pixels == expected);
    Require(std::all_of(reference.begin(), reference.begin() + guardSize, [fill](unsigned char value) { return value == fill; }));
    Require(std::all_of(reference.end() - guardSize, reference.end(), [fill](unsigned char value) { return value == fill; }));
}

static void CheckCleared(const FontGlyphMetrics& metrics, const FontRenderOutput& output) {
    const FontGlyphMetrics emptyMetrics{};
    const FontRenderOutput emptyOutput{};
    Require(std::memcmp(&metrics, &emptyMetrics, sizeof(metrics)) == 0);
    Require(std::memcmp(&output, &emptyOutput, sizeof(output)) == 0);
}

static void CheckErrors(Render render, FontHandle font) {
    for (int invalid = 0; invalid < 5; ++invalid) {
        std::vector<unsigned char> pixels(8192, 0xA5);
        FontRenderSurface surface{};
        sceFontRenderSurfaceInit(&surface, pixels.data() + guardSize, 16, 1, 80, 64);
        if (invalid == 0) surface.buffer = nullptr;
        if (invalid == 1) surface.height = -1;
        if (invalid == 2) surface.width = -1;
        if (invalid == 3) surface.widthByte = -16;
        if (invalid == 4) surface.pixelSizeByte = 2;
        FontGlyphMetrics metrics;
        FontRenderOutput output;
        std::memset(&metrics, 0x5A, sizeof(metrics));
        std::memset(&output, 0x5A, sizeof(output));
        Require(render(font, 'A', &surface, 10.0f, 10.0f, &metrics, &output) == SCE_FONT_ERROR_NO_SUPPORT_SURFACE);
        CheckCleared(metrics, output);
        Require(std::all_of(pixels.begin(), pixels.end(), [](unsigned char value) { return value == 0xA5; }));
        CheckLayout(render, font, 80, 64, 16, 1, false, 0);
    }
    std::vector<unsigned char> pixels(8192, 0xA5);
    FontRenderSurface surface{};
    sceFontRenderSurfaceInit(&surface, pixels.data() + guardSize, 16, 1, 80, 64);
    FontGlyphMetrics metrics{};
    FontRenderOutput output{};
    Require(render(nullptr, 'A', &surface, 10.0f, 10.0f, &metrics, &output) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(render(font, 0, &surface, 10.0f, 10.0f, &metrics, &output) == SCE_FONT_ERROR_NO_SUPPORT_CODE);
    Require(render(font, 'A', &surface, 10.0f, 10.0f, nullptr, &output) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(std::all_of(pixels.begin(), pixels.end(), [](unsigned char value) { return value == 0xA5; }));
    CheckLayout(render, font, 80, 64, 0, 1, false, 0);
}

static void CheckStyleError(FontLibrary library, FontHandle font) {
    FontStyleFrame frame{};
    Require(sceFontStyleFrameInit(&frame) == SCE_FONT_OK);
    Require(sceFontStyleFrameSetScalePixel(&frame, 40.0f, 40.0f) == SCE_FONT_OK);
    std::vector<unsigned char> pixels(8192, 0xA5);
    int referenceRc = SCE_FONT_OK;
    for (const int stride : {128, 16, 0}) {
        FontRenderSurface surface{};
        sceFontRenderSurfaceInit(&surface, pixels.data() + guardSize, stride, 1, 80, 64);
        Require(sceFontRenderSurfaceSetStyleFrame(&surface, &frame) == SCE_FONT_OK);
        FontGlyphMetrics metrics;
        FontRenderOutput output;
        std::memset(&metrics, 0x5A, sizeof(metrics));
        std::memset(&output, 0x5A, sizeof(output));
        auto* nativeLibrary = static_cast<Font::FontLibNative*>(library);
        const auto* driver = nativeLibrary->sys_driver;
        nativeLibrary->sys_driver = nullptr;
        const int rc = sceFontRenderCharGlyphImage(font, 'A', &surface, 10.0f, 10.0f, &metrics, &output);
        nativeLibrary->sys_driver = driver;
        if (stride == 128) referenceRc = rc;
        Require(referenceRc != SCE_FONT_OK && rc == referenceRc);
        CheckCleared(metrics, output);
        Require(std::all_of(pixels.begin(), pixels.end(), [](unsigned char value) { return value == 0xA5; }));
        CheckLayout(sceFontRenderCharGlyphImage, font, 80, 64, 16, 1, false, 0);
    }
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 1, 0x52) == SCE_FONT_OK);
    const auto bytes = TestGlyphFont('A', 900);
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, bytes.data(), static_cast<std::uint32_t>(bytes.size()), nullptr, &font) == SCE_FONT_OK);
    FontRenderer renderer = nullptr;
    Require(sceFontCreateRenderer(&memory, sceFontSelectRendererFt(0), &renderer) == SCE_FONT_OK);
    Require(sceFontBindRenderer(font, renderer) == SCE_FONT_OK);
    Require(sceFontSetupRenderScalePixel(font, 40.0f, 40.0f) == SCE_FONT_OK);
    CheckStyleError(library, font);
    for (const auto render : {sceFontRenderCharGlyphImage, sceFontRenderCharGlyphImageHorizontal, sceFontRenderCharGlyphImageVertical}) {
        for (const int bpp : {1, 4}) {
            for (const auto dimensions : {std::array<int, 2>{128, 64}, {80, 64}, {80, 1}, {0, 64}, {80, 0}}) {
                const int width = dimensions[0];
                const int height = dimensions[1];
                for (const int stride : {width * bpp, width * bpp + 7, 64, 16, 1, 0, std::max(0, width * bpp - 1), 130}) {
                    for (const bool clipped : {false, true}) {
                        for (const unsigned char fill : {0x00, 0xEE}) {
                            CheckLayout(render, font, width, height, stride, bpp, clipped, fill);
                        }
                    }
                }
            }
        }
        CheckErrors(render, font);
    }
    Require(sceFontUnbindRenderer(font) == SCE_FONT_OK);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    Require(allocations == 0);
}
