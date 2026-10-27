#include "prx/libSceFontFt/include/Api.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "System font check failed at line %d\n", line);
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

static std::vector<unsigned char> SquareGlyphFont(std::uint32_t codepoint, int width) {
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

static void WriteFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file));
}

static void SetFontDirectory(const std::filesystem::path& directory) {
#ifdef _WIN32
    Require(_wputenv_s(L"ANYPS5_SYSTEM_FONTS", directory.wstring().c_str()) == 0);
#else
    Require(::setenv("ANYPS5_SYSTEM_FONTS", directory.c_str(), 1) == 0);
#endif
}

template <typename TRun>
static std::string CaptureStderr(const std::filesystem::path& path, TRun run) {
    std::fflush(stderr);
#ifdef _WIN32
    const int saved = _dup(2);
#else
    const int saved = dup(2);
#endif
    std::FILE* file = std::fopen(path.string().c_str(), "wb");
    Require(saved >= 0 && file != nullptr);
#ifdef _WIN32
    Require(_dup2(_fileno(file), 2) == 0);
#else
    Require(dup2(fileno(file), 2) == 2);
#endif
    run();
    std::fflush(stderr);
#ifdef _WIN32
    Require(_dup2(saved, 2) == 0);
    _close(saved);
#else
    Require(dup2(saved, 2) == 2);
    close(saved);
#endif
    std::fclose(file);
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

static std::size_t Occurrences(const std::string& text, const std::string& pattern) {
    std::size_t count = 0;
    for (auto at = text.find(pattern); at != std::string::npos; at = text.find(pattern, at + pattern.size())) ++count;
    return count;
}

static float AdvanceOf(FontHandle font, std::uint32_t code) {
    FontGlyphMetrics metrics{};
    Require(sceFontGetCharGlyphMetrics(font, code, &metrics) == SCE_FONT_OK);
    return metrics.Horizontal.advance;
}

static float SetAdvanceOf(FontLibrary library, std::uint32_t fontSetType, std::uint32_t code) {
    FontHandle set = nullptr;
    Require(sceFontOpenFontSet(library, fontSetType, 1, nullptr, &set) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(set, 100.0f, 100.0f) == SCE_FONT_OK);
    const float advance = AdvanceOf(set, code);
    Require(sceFontCloseFont(set) == SCE_FONT_OK);
    return advance;
}

static int RenderedPixels(FontHandle font, std::uint32_t code, float& advance) {
    std::vector<std::uint8_t> pixels(128 * 128);
    FontRenderSurface surface{};
    sceFontRenderSurfaceInit(&surface, pixels.data(), 128, 1, 128, 128);

    FontGlyphMetrics metrics{};
    FontRenderOutput result{};
    const int rc = sceFontRenderCharGlyphImageHorizontal(font, code, &surface, 10.0f, 100.0f, &metrics, &result);
    advance = metrics.Horizontal.advance;
    if (rc != SCE_FONT_OK) return rc;
    int lit = 0;
    for (const std::uint8_t pixel : pixels) lit += pixel != 0 ? 1 : 0;
    return lit;
}

struct GlyphImage {
    int rc = SCE_FONT_OK;
    std::vector<std::uint8_t> pixels = std::vector<std::uint8_t>(128 * 128);
    FontGlyphMetrics metrics{};
    FontRenderOutput result{};
};

template<typename TRender> static GlyphImage RenderInto(TRender render) {
    GlyphImage image;
    FontRenderSurface surface{};
    sceFontRenderSurfaceInit(&surface, image.pixels.data(), 128, 1, 128, 128);
    image.rc = render(&surface, &image.metrics, &image.result);
    return image;
}

static bool SurfaceOffset(const GlyphImage& image, std::uintptr_t& offset) {
    const auto& surface = image.result.SurfaceImage;
    const auto& rect = image.result.UpdateRect;
    const auto base = reinterpret_cast<std::uintptr_t>(image.pixels.data());
    const auto address = reinterpret_cast<std::uintptr_t>(surface.address);
    const std::uintptr_t span = rect.w == 0 || rect.h == 0 ? 0 : static_cast<std::uintptr_t>(rect.h - 1) * surface.widthByte + static_cast<std::uintptr_t>(rect.w) * surface.pixelSizeByte;
    if (address < base || address - base > image.pixels.size() || span > image.pixels.size() - (address - base)) return false;
    offset = address - base;
    return true;
}

static bool SameImage(const GlyphImage& left, const GlyphImage& right) {
    int lit = 0;
    for (const std::uint8_t pixel : left.pixels) lit += pixel != 0 ? 1 : 0;
    const auto& a = left.result;
    const auto& b = right.result;
    std::uintptr_t leftOffset = 0;
    std::uintptr_t rightOffset = 0;
    const bool sameSurface = SurfaceOffset(left, leftOffset) && SurfaceOffset(right, rightOffset) && leftOffset == rightOffset && a.SurfaceImage.widthByte == b.SurfaceImage.widthByte && a.SurfaceImage.pixelSizeByte == b.SurfaceImage.pixelSizeByte && a.SurfaceImage.pixelFormat == b.SurfaceImage.pixelFormat;
    const bool sameOutput = a.stage == b.stage && sameSurface && std::memcmp(&a.UpdateRect, &b.UpdateRect, sizeof(a.UpdateRect)) == 0 && std::memcmp(&a.ImageMetrics, &b.ImageMetrics, sizeof(a.ImageMetrics)) == 0;
    return left.rc == SCE_FONT_OK && right.rc == SCE_FONT_OK && lit > 0 && left.pixels == right.pixels && std::memcmp(&left.metrics, &right.metrics, sizeof(left.metrics)) == 0 && sameOutput;
}

static bool Cleared(const GlyphImage& image, int rc) {
    const FontGlyphMetrics noMetrics{};
    const FontRenderOutput noResult{};
    return image.rc == rc && std::memcmp(&image.metrics, &noMetrics, sizeof(noMetrics)) == 0 && std::memcmp(&image.result, &noResult, sizeof(noResult)) == 0;
}

static void CheckGlyphImages(FontHandle font, FontRenderer renderer) {
    Require(sceFontBindRenderer(font, renderer) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(font, 40.0f, 40.0f) == SCE_FONT_OK);
    Require(sceFontSetupRenderScalePixel(font, 40.0f, 40.0f) == SCE_FONT_OK);
    FontGlyph glyph = nullptr;
    Require(sceFontGenerateCharGlyph(font, 'A', nullptr, &glyph) == SCE_FONT_OK && glyph != nullptr);

    const auto horizontal = RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontRenderCharGlyphImageHorizontal(font, 'A', surface, 10.0f, 100.0f, metrics, result); });
    Require(SameImage(horizontal, RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontGlyphRenderImageHorizontal(glyph, nullptr, renderer, surface, 10.0f, 100.0f, metrics, result); })));
    const auto written = RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontRenderCharGlyphImage(font, 'A', surface, 10.0f, 20.0f, metrics, result); });
    Require(SameImage(written, RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontGlyphRenderImage(glyph, nullptr, renderer, surface, 10.0f, 20.0f, metrics, result); })));

    Require(sceFontSetupRenderScalePixel(font, 20.0f, 20.0f) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(font, 20.0f, 20.0f) == SCE_FONT_OK);
    const auto smaller = RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontRenderCharGlyphImageHorizontal(font, 'A', surface, 10.0f, 100.0f, metrics, result); });
    Require(smaller.rc == SCE_FONT_OK && smaller.pixels != horizontal.pixels);
    Require(SameImage(horizontal, RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontGlyphRenderImageHorizontal(glyph, nullptr, renderer, surface, 10.0f, 100.0f, metrics, result); })));
    FontStyleFrame frame{};
    Require(sceFontStyleFrameInit(&frame) == SCE_FONT_OK && sceFontStyleFrameSetScalePixel(&frame, 20.0f, 20.0f) == SCE_FONT_OK);
    Require(SameImage(smaller, RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontGlyphRenderImageHorizontal(glyph, &frame, renderer, surface, 10.0f, 100.0f, metrics, result); })));
    Require(SameImage(smaller, RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) {
        Require(sceFontRenderSurfaceSetStyleFrame(surface, &frame) == SCE_FONT_OK);
        return sceFontGlyphRenderImageHorizontal(glyph, nullptr, renderer, surface, 10.0f, 100.0f, metrics, result);
    })));
    FontStyleFrame larger{};
    Require(sceFontStyleFrameInit(&larger) == SCE_FONT_OK && sceFontStyleFrameSetScalePixel(&larger, 30.0f, 30.0f) == SCE_FONT_OK);
    const auto surfaceLarger = RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) {
        Require(sceFontRenderSurfaceSetStyleFrame(surface, &larger) == SCE_FONT_OK);
        return sceFontGlyphRenderImageHorizontal(glyph, nullptr, renderer, surface, 10.0f, 100.0f, metrics, result);
    });
    Require(surfaceLarger.rc == SCE_FONT_OK && surfaceLarger.pixels != smaller.pixels && surfaceLarger.pixels != horizontal.pixels);
    Require(SameImage(surfaceLarger, RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) {
        Require(sceFontRenderSurfaceSetStyleFrame(surface, &larger) == SCE_FONT_OK);
        return sceFontGlyphRenderImageHorizontal(glyph, &frame, renderer, surface, 10.0f, 100.0f, metrics, result);
    })));
    Require(SameImage(smaller, RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) {
        Require(sceFontRenderSurfaceSetStyleFrame(surface, &frame) == SCE_FONT_OK);
        return sceFontGlyphRenderImageHorizontal(glyph, &larger, renderer, surface, 10.0f, 100.0f, metrics, result);
    })));
    FontStyleFrame notAFrame{};
    FontStyleFrame initialized{};
    Require(sceFontStyleFrameInit(&initialized) == SCE_FONT_OK);
    for (const FontStyleFrame* unscaled : {&notAFrame, &initialized}) {
        Require(SameImage(horizontal, RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontGlyphRenderImageHorizontal(glyph, unscaled, renderer, surface, 10.0f, 100.0f, metrics, result); })));
        Require(SameImage(written, RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontGlyphRenderImage(glyph, unscaled, renderer, surface, 10.0f, 20.0f, metrics, result); })));
    }

    Require(sceFontUnbindRenderer(font) == SCE_FONT_OK);
    Require(RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontRenderCharGlyphImageHorizontal(font, 'A', surface, 10.0f, 100.0f, metrics, result); }).rc == SCE_FONT_ERROR_NOT_BOUND_RENDERER);
    Require(SameImage(horizontal, RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { return sceFontGlyphRenderImageHorizontal(glyph, nullptr, renderer, surface, 10.0f, 100.0f, metrics, result); })));

    const auto filled = [](FontGlyphMetrics* metrics, FontRenderOutput* result) {
        std::memset(metrics, 0xA5, sizeof(*metrics));
        std::memset(result, 0xA5, sizeof(*result));
    };
    FontGlyphOpaque notAGlyph{};
    for (const FontGlyph wrong : {static_cast<FontGlyph>(nullptr), &notAGlyph}) {
        for (const auto render : {sceFontGlyphRenderImage, sceFontGlyphRenderImageHorizontal, sceFontGlyphRenderImageVertical}) {
            Require(Cleared(RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { filled(metrics, result); return render(wrong, nullptr, renderer, surface, 10.0f, 100.0f, metrics, result); }), SCE_FONT_ERROR_INVALID_GLYPH));
        }
    }
    for (const auto render : {sceFontGlyphRenderImage, sceFontGlyphRenderImageHorizontal, sceFontGlyphRenderImageVertical}) {
        Require(Cleared(RenderInto([&](FontRenderSurface*, FontGlyphMetrics* metrics, FontRenderOutput* result) { filled(metrics, result); return render(glyph, nullptr, renderer, nullptr, 10.0f, 100.0f, metrics, result); }), SCE_FONT_ERROR_INVALID_PARAMETER));
        Require(Cleared(RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics*, FontRenderOutput* result) { std::memset(result, 0xA5, sizeof(*result)); return render(glyph, nullptr, renderer, surface, 10.0f, 100.0f, nullptr, result); }), SCE_FONT_ERROR_INVALID_PARAMETER));
        Require(Cleared(RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput*) { std::memset(metrics, 0xA5, sizeof(*metrics)); return render(glyph, nullptr, renderer, surface, 10.0f, 100.0f, metrics, nullptr); }), SCE_FONT_ERROR_INVALID_PARAMETER));
        Require(Cleared(RenderInto([&](FontRenderSurface* surface, FontGlyphMetrics* metrics, FontRenderOutput* result) { filled(metrics, result); return render(glyph, nullptr, nullptr, surface, 10.0f, 100.0f, metrics, result); }), SCE_FONT_ERROR_INVALID_RENDERER));
    }
    Require(sceFontDeleteGlyph(nullptr, &glyph) == SCE_FONT_OK && glyph == nullptr);
}

int main() {
    constexpr std::uint32_t EuropeanLight = 0x180700C3u;
    constexpr std::uint32_t EuropeanBold = 0x180700C7u;
    constexpr std::uint32_t EuropeanItalic = 0x18170044u;
    constexpr std::uint32_t ThaiMedium = 0x18071055u;
    constexpr std::uint32_t VietnameseBold = 0x18070057u;
    constexpr std::uint32_t JapaneseJg2Light = 0x1A0835D3u;
    constexpr std::uint32_t ChineseGb = 0x180CB0D4u;
    const std::filesystem::path root = std::filesystem::temp_directory_path() / ("anyps5_guest_system_font-" + std::to_string(std::random_device{}()));
    std::filesystem::remove_all(root);
    const std::filesystem::path empty = root / "empty";
    const std::filesystem::path fonts = root / std::filesystem::path(u8"fonts-a\u00e7\u00e3o-\u65e5\u672c");
    std::filesystem::create_directories(empty);
    std::filesystem::create_directories(fonts);
    WriteFile(fonts / "SST-Bold.otf", SquareGlyphFont('A', 700));
    WriteFile(fonts / "NotoSans-Bold.ttf", SquareGlyphFont('A', 300));
    WriteFile(fonts / "NotoSans-Light.ttf", SquareGlyphFont('A', 500));
    WriteFile(fonts / "SST-Italic.otf", SquareGlyphFont('A', 520));
    WriteFile(fonts / "NotoSansThai-Medium.ttf", SquareGlyphFont(0x0E01, 540));
    WriteFile(fonts / "SSTVietnamese-Bold.otf", SquareGlyphFont(0x1EA0, 560));
    WriteFile(fonts / "NotoSansCJK-Light.ttc", SquareGlyphFont(0x3042, 580));

    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportSystemFonts(library) == SCE_FONT_OK);

    FontHandle font = nullptr;
    SetFontDirectory(empty);
    const std::string warnings = CaptureStderr(root / "stderr.txt", [&] {
        Require(sceFontOpenFontSet(library, EuropeanBold, 1, nullptr, &font) == SCE_FONT_ERROR_FONT_OPEN_FAILED && font == nullptr);
        Require(sceFontOpenFontSet(library, EuropeanLight, 1, nullptr, &font) == SCE_FONT_ERROR_FONT_OPEN_FAILED && font == nullptr);
        Require(sceFontOpenFontSet(library, 0x18070046u, 1, nullptr, &font) == SCE_FONT_ERROR_NO_SUPPORT_FONTSET && font == nullptr);
    });
    Require(Occurrences(warnings, "[libSceFont] no file for system font set") == 1);
    Require(Occurrences(warnings, "0x180700C7 in " + empty.string()) == 1 && Occurrences(warnings, "ANYPS5_SYSTEM_FONTS") == 1);
    SetFontDirectory(root / "missing");
    bool threw = false;
    try {
        sceFontOpenFontSet(library, EuropeanBold, 1, nullptr, &font);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    SetFontDirectory(fonts);
    const int beforeOpen = allocations;
    Require(sceFontOpenFontSet(library, EuropeanBold, 1, nullptr, &font) == SCE_FONT_OK && font != nullptr);
    Require(allocations - beforeOpen == 3);
    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    Require(AdvanceOf(font, 'A') == 70.0f);
    FontGlyphMetrics metrics{};
    Require(sceFontGetCharGlyphMetrics(font, 'A', &metrics) == SCE_FONT_OK);
    Require(metrics.width == 50.0f && metrics.height == 70.0f && metrics.Horizontal.bearingX == 10.0f && metrics.Horizontal.bearingY == 70.0f);
    Require(metrics.Vertical.bearingX == -25.0f && metrics.Vertical.bearingY == 15.0f && metrics.Vertical.advance == 100.0f);
    Require(sceFontGetCharGlyphMetrics(font, 'B', &metrics) == SCE_FONT_ERROR_NO_SUPPORT_GLYPH);
    FontHorizontalLayout layout{};
    Require(sceFontGetHorizontalLayout(font, &layout) == SCE_FONT_OK && layout.baselineOffset > 0.0f && layout.lineAdvance >= layout.baselineOffset);

    FontHandle again = nullptr;
    Require(sceFontOpenFontSet(library, EuropeanBold, 2, nullptr, &again) == SCE_FONT_OK && again != font);
    FontHandle instance = nullptr;
    Require(sceFontOpenFontInstance(font, nullptr, &instance) == SCE_FONT_OK && instance != nullptr);
    Require(sceFontGetHorizontalLayout(instance, &layout) == SCE_FONT_OK && layout.baselineOffset > 0.0f);

    const std::vector<std::pair<std::uint32_t, std::pair<std::uint32_t, float>>> expected = {
        {EuropeanLight, {'A', 50.0f}}, {EuropeanItalic, {'A', 52.0f}}, {ThaiMedium, {0x0E01, 54.0f}},
        {VietnameseBold, {0x1EA0, 56.0f}}, {JapaneseJg2Light, {0x3042, 58.0f}}};
    for (const auto& [type, glyph] : expected) {
        FontHandle set = nullptr;
        Require(sceFontOpenFontSet(library, type, 1, nullptr, &set) == SCE_FONT_OK);
        Require(sceFontSetScalePixel(set, 100.0f, 100.0f) == SCE_FONT_OK);
        Require(AdvanceOf(set, glyph.first) == glyph.second);
        Require(sceFontCloseFont(set) == SCE_FONT_OK);
    }
    Require(sceFontCloseFont(again) == SCE_FONT_OK);
    FontHandle missing = nullptr;
    Require(sceFontOpenFontSet(library, ChineseGb, 1, nullptr, &missing) == SCE_FONT_ERROR_FONT_OPEN_FAILED && missing == nullptr);

    const auto substituteFonts = root / "substitute-fonts";
    std::filesystem::create_directories(substituteFonts);
    WriteFile(substituteFonts / "NotoSans-Bold.ttf", SquareGlyphFont('A', 300));
    SetFontDirectory(substituteFonts);
    FontHandle substitute = nullptr;
    Require(sceFontOpenFontSet(library, EuropeanBold, 3, nullptr, &substitute) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(substitute, 100.0f, 100.0f) == SCE_FONT_OK);
    Require(AdvanceOf(substitute, 'A') == 30.0f);
    Require(AdvanceOf(font, 'A') == 70.0f);

    Require(sceFontSupportExternalFonts(library, 2, 0x52) == SCE_FONT_OK);
    const std::vector<unsigned char> external = SquareGlyphFont('A', 900);
    FontHandle memoryFont = nullptr;
    Require(sceFontOpenFontMemory(library, external.data(), static_cast<std::uint32_t>(external.size()), nullptr, &memoryFont) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(memoryFont, 100.0f, 100.0f) == SCE_FONT_OK);
    Require(AdvanceOf(memoryFont, 'A') == 90.0f);
    Require(sceFontGetHorizontalLayout(memoryFont, &layout) == SCE_FONT_OK);

    constexpr std::uint32_t EuropeanJpAr = 0x180704C4u;
    constexpr std::uint32_t EuropeanJpCjk = 0x1807A444u;
    constexpr std::uint32_t EuropeanGbCjk = 0x1807AC44u;
    constexpr std::uint32_t European = 0x18070044u;
    const auto fallbackFonts = root / "fallback-fonts";
    std::filesystem::create_directories(fallbackFonts);
    WriteFile(fallbackFonts / "SST-Roman.otf", SquareGlyphFont('A', 600));
    WriteFile(fallbackFonts / "SSTJpPro-Regular.otf", SquareGlyphFont(0x4E00, 630));
    WriteFile(fallbackFonts / "DFHEI5-SONY.ttf", SquareGlyphFont(0x4E00, 640));
    WriteFile(fallbackFonts / "SCEPS4Yoongd-Medium.otf", SquareGlyphFont(0xAC00, 650));
    WriteFile(fallbackFonts / "NotoSansArabic-Regular.ttf", SquareGlyphFont(0x0627, 660));
    SetFontDirectory(fallbackFonts);
    Require(SetAdvanceOf(library, EuropeanJpAr, 'A') == 60.0f);
    Require(SetAdvanceOf(library, EuropeanJpAr, 0x4E00) == 63.0f);
    Require(SetAdvanceOf(library, EuropeanJpAr, 0x0627) == 66.0f);
    Require(SetAdvanceOf(library, EuropeanJpCjk, 0x4E00) == 63.0f);
    Require(SetAdvanceOf(library, EuropeanJpCjk, 0xAC00) == 65.0f);
    Require(SetAdvanceOf(library, EuropeanGbCjk, 0x4E00) == 64.0f);
    WriteFile(fallbackFonts / "NotoSans-Regular.ttf", SquareGlyphFont(0x1EA0, 670));
    Require(SetAdvanceOf(library, 0x18070454u, 0x1EA0) == 67.0f);
    FontHandle plain = nullptr;
    Require(sceFontOpenFontSet(library, European, 1, nullptr, &plain) == SCE_FONT_OK);
    Require(sceFontGetCharGlyphMetrics(plain, 0x4E00, &metrics) == SCE_FONT_ERROR_NO_SUPPORT_GLYPH);
    Require(sceFontCloseFont(plain) == SCE_FONT_OK);

    FontHandle mixed = nullptr;
    Require(sceFontOpenFontSet(library, EuropeanJpAr, 1, nullptr, &mixed) == SCE_FONT_OK);
    FontHandle mixedInstance = nullptr;
    Require(sceFontOpenFontInstance(mixed, nullptr, &mixedInstance) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(mixedInstance, 100.0f, 100.0f) == SCE_FONT_OK);
    Require(AdvanceOf(mixedInstance, 0x0627) == 66.0f);
    FontKerning kerning{1.0f, 1.0f, 1.0f, 1.0f};
    Require(sceFontGetKerning(mixed, 'A', 0x4E00, &kerning) == SCE_FONT_OK && kerning.offsetX == 0.0f);
    FontRenderer renderer = nullptr;
    Require(sceFontCreateRenderer(&memory, sceFontSelectRendererFt(0), &renderer) == SCE_FONT_OK);
    CheckGlyphImages(memoryFont, renderer);
    Require(sceFontBindRenderer(mixed, renderer) == SCE_FONT_OK);
    Require(sceFontSetupRenderScalePixel(mixed, 100.0f, 100.0f) == SCE_FONT_OK);
    float advance = 0.0f;
    Require(RenderedPixels(mixed, 'A', advance) > 0 && advance == 60.0f);
    Require(RenderedPixels(mixed, 0x4E00, advance) > 0 && advance == 63.0f);
    Require(RenderedPixels(mixed, 0x0627, advance) > 0 && advance == 66.0f);
    Require(RenderedPixels(mixed, 0xAC00, advance) == SCE_FONT_ERROR_NO_SUPPORT_GLYPH);
    std::vector<std::uint8_t> untouched(128 * 128);
    FontRenderSurface measuring{};
    sceFontRenderSurfaceInit(&measuring, untouched.data(), 0, 1, 0, 31);

    FontGlyphMetrics measured{};
    FontRenderOutput measuredResult{};
    Require(sceFontRenderCharGlyphImageHorizontal(mixed, 'A', &measuring, 10.0f, 100.0f, &measured, &measuredResult) == SCE_FONT_OK);
    Require(measured.Horizontal.advance == 60.0f && measured.width > 0.0f && measuredResult.ImageMetrics.advance > 0.0f);
    Require(measuredResult.UpdateRect.w == 0 && measuredResult.UpdateRect.h == 0);
    for (const std::uint8_t pixel : untouched) Require(pixel == 0);
    sceFontRenderSurfaceInit(&measuring, untouched.data(), 128, 1, 128, 0);

    Require(sceFontRenderCharGlyphImageHorizontal(mixed, 'A', &measuring, 10.0f, 100.0f, &measured, &measuredResult) == SCE_FONT_OK && measured.Horizontal.advance == 60.0f);
    sceFontRenderSurfaceInit(&measuring, untouched.data(), 128, 1, 128, 128);

    measuring.width = -1;
    Require(sceFontRenderCharGlyphImageHorizontal(mixed, 'A', &measuring, 10.0f, 100.0f, &measured, &measuredResult) == SCE_FONT_ERROR_NO_SUPPORT_SURFACE);
    sceFontRenderSurfaceInit(&measuring, untouched.data(), 0, 1, 128, 128);

    Require(sceFontRenderCharGlyphImageHorizontal(mixed, 'A', &measuring, 10.0f, 100.0f, &measured, &measuredResult) == SCE_FONT_ERROR_NO_SUPPORT_SURFACE);
    sceFontRenderSurfaceInit(&measuring, nullptr, 0, 1, 0, 31);

    Require(sceFontRenderCharGlyphImageHorizontal(mixed, 'A', &measuring, 10.0f, 100.0f, &measured, &measuredResult) == SCE_FONT_ERROR_NO_SUPPORT_SURFACE);
    Require(sceFontUnbindRenderer(mixed) == SCE_FONT_OK);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK);

    const auto hangulFonts = root / "hangul-fonts";
    std::filesystem::create_directories(hangulFonts);
    for (const auto* name : {"SST-Light.otf", "SST-Roman.otf", "SST-Bold.otf", "SSTJpPro-Regular.otf"}) {
        WriteFile(hangulFonts / name, SquareGlyphFont('A', 600));
    }
    WriteFile(hangulFonts / "SCEPS4Yoongd-Light.otf", SquareGlyphFont(0xAC00, 400));
    WriteFile(hangulFonts / "SCEPS4Yoongd-Medium.otf", SquareGlyphFont(0xAC00, 650));
    WriteFile(hangulFonts / "SCEPS4Yoongd-Bold.otf", SquareGlyphFont(0xAC00, 800));
    SetFontDirectory(hangulFonts);
    Require(SetAdvanceOf(library, 0x1A072543u, 0xAC00) == 40.0f);
    Require(SetAdvanceOf(library, 0x1A082543u, 0xAC00) == 40.0f);
    Require(SetAdvanceOf(library, 0x18072444u, 0xAC00) == 65.0f);
    Require(SetAdvanceOf(library, 0x18072447u, 0xAC00) == 80.0f);

    for (FontHandle handle : {font, instance, substitute, memoryFont, mixed, mixedInstance}) Require(sceFontCloseFont(handle) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(allocations == 0);
    std::filesystem::remove_all(root);
}
