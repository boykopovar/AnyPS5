#include "prx/libSceFont/include/FontDriver.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceFontMemoryInit(FontMemory*, void*, std::uint32_t, const FontMemoryInterface*, void*, FontMemoryDestroyFunction, void*);
int APS5_VABI sceFontMemoryTerm(FontMemory*);
int APS5_VABI sceFontCreateLibrary(const FontMemory*, const void*, FontLibrary*);
int APS5_VABI sceFontDestroyLibrary(FontLibrary*);
int APS5_VABI sceFontCreateRenderer(const FontMemory*, const void*, FontRenderer*);
int APS5_VABI sceFontDestroyRenderer(FontRenderer*);
int APS5_VABI sceFontGetPixelResolution(FontLibrary, std::uint32_t*);
int APS5_VABI sceFontAttachDeviceCacheBuffer(FontLibrary, void*, std::uint32_t);
int APS5_VABI sceFontClearDeviceCache(FontLibrary);
int APS5_VABI sceFontDettachDeviceCacheBuffer(FontLibrary, void**, std::uint32_t*);
int APS5_VABI sceFontSupportSystemFonts(FontLibrary);
int APS5_VABI sceFontSupportExternalFonts(FontLibrary, std::uint32_t, std::uint32_t);
int APS5_VABI sceFontOpenFontSet(FontLibrary, std::uint32_t, std::uint32_t, const FontOpenDetail*, FontHandle*);
int APS5_VABI sceFontOpenFontMemory(FontLibrary, const void*, std::uint32_t, const FontOpenDetail*, FontHandle*);
int APS5_VABI sceFontCloseFont(FontHandle);
int APS5_VABI sceFontSetResolutionDpi(FontHandle, std::uint32_t, std::uint32_t);
int APS5_VABI sceFontGetResolutionDpi(FontHandle, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceFontSetScalePixel(FontHandle, float, float);
int APS5_VABI sceFontSetScalePoint(FontHandle, float, float);
int APS5_VABI sceFontBindRenderer(FontHandle, FontRenderer);
int APS5_VABI sceFontUnbindRenderer(FontHandle);
int APS5_VABI sceFontSetupRenderScalePixel(FontHandle, float, float);
int APS5_VABI sceFontSetupRenderScalePoint(FontHandle, float, float);
int APS5_VABI sceFontGetKerning(FontHandle, std::uint32_t, std::uint32_t, FontKerning*);
int APS5_VABI sceFontGetFontGlyphsCount(FontHandle, std::uint32_t*);
int APS5_VABI sceFontGetCharGlyphCode(FontHandle, std::uint32_t, std::uint32_t*);
int APS5_VABI sceFontGetFontResolution(FontHandle, std::uint32_t*, float*);
int APS5_VABI sceFontGetRenderScaledKerning(FontHandle, std::uint32_t, std::uint32_t, FontKerning*);
int APS5_VABI sceFontGenerateCharGlyph(FontHandle, std::uint32_t, const FontGenerateGlyphDetail*, FontGlyph*);
int APS5_VABI sceFontGlyphRenderImage(FontGlyph, FontStyleFrame*, FontRenderer, FontRenderSurface*, float, float, FontGlyphMetrics*, FontRenderOutput*);
int APS5_VABI sceFontGlyphRenderImageHorizontal(FontGlyph, FontStyleFrame*, FontRenderer, FontRenderSurface*, float, float, FontGlyphMetrics*, FontRenderOutput*);
int APS5_VABI sceFontGlyphRenderImageVertical(FontGlyph, FontStyleFrame*, FontRenderer, FontRenderSurface*, float, float, FontGlyphMetrics*, FontRenderOutput*);
int APS5_VABI sceFontGlyphDefineAttribute(FontGlyph, std::uint32_t, std::uint64_t);
int APS5_VABI sceFontDeleteGlyph(const FontMemory*, FontGlyph*);
const void* APS5_VABI sceFontSelectLibraryFt(int);
const void* APS5_VABI sceFontSelectRendererFt(int);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void SetFontDirectory(const std::filesystem::path& directory) {
#ifdef _WIN32
    Require(_putenv_s("ANYPS5_SYSTEM_FONTS", directory.string().c_str()) == 0);
#else
    Require(::setenv("ANYPS5_SYSTEM_FONTS", directory.string().c_str(), 1) == 0);
#endif
}

static int allocations = 0;
static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    ++allocations;
    return std::malloc(size);
}
static void APS5_VABI Release(void*, void* pointer) {
    if (pointer) --allocations;
    std::free(pointer);
}

static std::uint32_t APS5_VABI CoarsePixelResolution() {
    return 16;
}

static void Put16(std::vector<unsigned char>& out, int value) {
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>(value & 0xFF));
}

static std::vector<unsigned char> Words(std::initializer_list<int> values) {
    std::vector<unsigned char> out;
    for (const int value : values) Put16(out, value);
    return out;
}

static std::vector<unsigned char> BuildFont(int glyphCount, std::map<std::string, std::vector<unsigned char>> tables) {
    tables.try_emplace("glyf", Words({0, 0}));
    tables.try_emplace("head", Words({1, 0, 1, 0, 0, 0, 0x5F0F, 0x3CF5, 0, 1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0}));
    tables.try_emplace("hhea", Words({1, 0, 800, -200, 0, 500, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1}));
    auto [hmtx, insertedHmtx] = tables.try_emplace("hmtx", Words({500}));
    (void)insertedHmtx;
    hmtx->second.resize(2 + 2 * static_cast<std::size_t>(glyphCount));
    tables.try_emplace("loca", std::vector<unsigned char>(2 * (static_cast<std::size_t>(glyphCount) + 1), 0));
    tables.try_emplace("maxp", Words({1, 0, glyphCount, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0}));
    const int tableCount = static_cast<int>(tables.size());
    int entrySelector = 0;
    while ((2 << entrySelector) <= tableCount) ++entrySelector;
    std::vector<unsigned char> font = Words({1, 0, tableCount, 16 << entrySelector, entrySelector, 16 * tableCount - (16 << entrySelector)});
    int offset = 12 + 16 * tableCount;
    for (const auto& table : tables) {
        font.insert(font.end(), table.first.begin(), table.first.end());
        for (const int value : {0, 0, offset >> 16, offset & 0xFFFF, 0, static_cast<int>(table.second.size())}) Put16(font, value);
        offset += static_cast<int>((table.second.size() + 3) & ~std::size_t{3});
    }
    for (const auto& table : tables) {
        font.insert(font.end(), table.second.begin(), table.second.end());
        font.resize((font.size() + 3) & ~std::size_t{3});
    }
    return font;
}

static std::vector<unsigned char> RasterGlyphFont() {
    std::vector<unsigned char> glyf;
    const auto put = [&glyf](int value) { glyf.push_back(static_cast<unsigned char>((value >> 8) & 0xFF)); glyf.push_back(static_cast<unsigned char>(value & 0xFF)); };
    put(1);
    put(0);
    put(0);
    put(500);
    put(700);
    put(3);
    put(0);
    glyf.insert(glyf.end(), {0x31, 0x21, 0x01, 0x01});
    put(500);
    put(0);
    put(-500);
    put(700);
    put(0);
    auto maxp = Words({1, 0, 2, 4, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0});
    auto hhea = Words({1, 0, 800, -200, 0, 500, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1});
    auto hmtx = Words({500, 0, 0});
    return BuildFont(2, {
        {"cmap", Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
        {"glyf", std::move(glyf)},
        {"hhea", std::move(hhea)},
        {"hmtx", std::move(hmtx)},
        {"loca", Words({0, 0, 14})},
        {"maxp", std::move(maxp)},
    });
}

static std::vector<unsigned char> EmptyGlyphFont() {
    return BuildFont(1, {});
}

static std::vector<unsigned char> KerningFont() {
    return BuildFont(3, {
        {"cmap", Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
        {"kern", Words({0, 1, 0, 20, 1, 1, 6, 0, 0, 1, 2, -200})},
    });
}

static bool KerningIs(const FontKerning& kerning, float offsetX) {
    return kerning.offsetX == offsetX && kerning.offsetY == 0.0f && kerning.positionX == 0.0f && kerning.positionY == 0.0f;
}

int main() {
    constexpr std::uint32_t SystemFontSet = 0x18070043u;
    const std::filesystem::path fontRoot = std::filesystem::temp_directory_path() / ("anyps5_guest_font-" + std::to_string(std::random_device{}()));
    std::filesystem::remove_all(fontRoot);
    std::filesystem::create_directories(fontRoot);
    SetFontDirectory(fontRoot);
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    Require(sceFontSelectLibraryFt(0) != nullptr && sceFontSelectLibraryFt(1) == nullptr);
    Require(sceFontSelectRendererFt(0) != nullptr && sceFontSelectRendererFt(1) == nullptr);

    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, nullptr, &library) == SCE_FONT_ERROR_INVALID_PARAMETER && library == nullptr);
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK && library != nullptr);

    std::uint32_t subPixelCount = 1;
    Require(sceFontGetPixelResolution(library, &subPixelCount) == SCE_FONT_OK && subPixelCount == 64);
    Require(sceFontGetPixelResolution(library, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontGetPixelResolution(nullptr, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    subPixelCount = 1;
    Require(sceFontGetPixelResolution(nullptr, &subPixelCount) == SCE_FONT_ERROR_INVALID_LIBRARY && subPixelCount == 0);
    FontHandleOpaque notALibrary{};
    subPixelCount = 1;
    Require(sceFontGetPixelResolution(&notALibrary, &subPixelCount) == SCE_FONT_ERROR_INVALID_LIBRARY && subPixelCount == 0);
    Font::SysDriver coarseDriver = *static_cast<const Font::SysDriver*>(sceFontSelectLibraryFt(0));
    coarseDriver.pixel_resolution = CoarsePixelResolution;
    FontLibrary coarseLibrary = nullptr;
    Require(sceFontCreateLibrary(&memory, &coarseDriver, &coarseLibrary) == SCE_FONT_OK && coarseLibrary != nullptr);
    Require(sceFontGetPixelResolution(coarseLibrary, &subPixelCount) == SCE_FONT_OK && subPixelCount == 16);
    Require(sceFontGetPixelResolution(library, &subPixelCount) == SCE_FONT_OK && subPixelCount == 64);
    coarseDriver.pixel_resolution = nullptr;
    Require(sceFontGetPixelResolution(coarseLibrary, &subPixelCount) == SCE_FONT_ERROR_INVALID_LIBRARY && subPixelCount == 0);
    Require(sceFontDestroyLibrary(&coarseLibrary) == SCE_FONT_OK && coarseLibrary == nullptr);

    void* cacheBuffer = nullptr;
    std::uint32_t cacheSize = 0;
    Require(sceFontDettachDeviceCacheBuffer(nullptr, &cacheBuffer, &cacheSize) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(sceFontDettachDeviceCacheBuffer(&notALibrary, &cacheBuffer, &cacheSize) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(sceFontClearDeviceCache(library) == SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER);
    Require(sceFontDettachDeviceCacheBuffer(library, &cacheBuffer, &cacheSize) == SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER);

    std::vector<unsigned char> callerCache(0x2000);
    Require(sceFontAttachDeviceCacheBuffer(library, callerCache.data(), 0x1000) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontAttachDeviceCacheBuffer(library, callerCache.data(), static_cast<std::uint32_t>(callerCache.size())) == SCE_FONT_OK);
    Require(sceFontAttachDeviceCacheBuffer(library, callerCache.data(), static_cast<std::uint32_t>(callerCache.size())) == SCE_FONT_ERROR_ALREADY_ATTACHED);
    Require(sceFontClearDeviceCache(library) == SCE_FONT_OK);
    Require(sceFontDettachDeviceCacheBuffer(library, &cacheBuffer, &cacheSize) == SCE_FONT_OK && cacheBuffer == callerCache.data() && cacheSize == callerCache.size());
    Require(sceFontDettachDeviceCacheBuffer(library, &cacheBuffer, &cacheSize) == SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER);

    Require(sceFontAttachDeviceCacheBuffer(library, nullptr, 0x2000) == SCE_FONT_OK);
    cacheBuffer = nullptr;
    cacheSize = 0;
    Require(sceFontDettachDeviceCacheBuffer(library, &cacheBuffer, &cacheSize) == SCE_FONT_OK && cacheBuffer != nullptr && cacheSize == 0x2000);
    Release(nullptr, cacheBuffer);
    Require(sceFontDettachDeviceCacheBuffer(library, &cacheBuffer, &cacheSize) == SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER);

    Require(sceFontAttachDeviceCacheBuffer(library, nullptr, 0x2000) == SCE_FONT_OK);
    Require(sceFontDettachDeviceCacheBuffer(library, nullptr, nullptr) == SCE_FONT_OK);
    Require(sceFontDettachDeviceCacheBuffer(library, nullptr, nullptr) == SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER);

    FontHandle font = reinterpret_cast<FontHandle>(&memory);
    Require(sceFontOpenFontSet(library, SystemFontSet, 1, nullptr, &font) == SCE_FONT_ERROR_NO_SUPPORT_FUNCTION && font == nullptr);
    Require(sceFontSupportSystemFonts(library) == SCE_FONT_OK);
    Require(sceFontOpenFontSet(library, SystemFontSet, 1, nullptr, &font) == SCE_FONT_ERROR_FONT_OPEN_FAILED && font == nullptr);
    Require(sceFontOpenFontSet(library, 0x12345678u, 1, nullptr, &font) == SCE_FONT_ERROR_NO_SUPPORT_FONTSET);
    Require(sceFontOpenFontSet(library, SystemFontSet, 7, nullptr, &font) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontOpenFontSet(nullptr, SystemFontSet, 1, nullptr, &font) == SCE_FONT_ERROR_INVALID_LIBRARY);
    const unsigned char notAFont[64] = {1, 2, 3, 4};
    Require(sceFontOpenFontMemory(library, notAFont, sizeof(notAFont), nullptr, &font) == SCE_FONT_ERROR_NO_SUPPORT_FUNCTION && font == nullptr);
    Require(sceFontSupportExternalFonts(library, 4, 0x52) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 4, 0x52) == SCE_FONT_ERROR_ALREADY_SPECIFIED);
    Require(sceFontOpenFontMemory(library, nullptr, 0, nullptr, &font) == SCE_FONT_ERROR_INVALID_PARAMETER && font == nullptr);
    Require(sceFontOpenFontMemory(library, notAFont, sizeof(notAFont), nullptr, &font) == SCE_FONT_ERROR_NO_SUPPORT_FORMAT && font == nullptr);

    const std::vector<unsigned char> fontData = EmptyGlyphFont();
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &font) == SCE_FONT_OK && font != nullptr);
    std::uint32_t hDpi = 1;
    std::uint32_t vDpi = 1;
    Require(sceFontGetResolutionDpi(font, &hDpi, &vDpi) == SCE_FONT_OK && hDpi == 72 && vDpi == 72);
    Require(sceFontSetResolutionDpi(font, 96, 144) == SCE_FONT_OK);
    Require(sceFontGetResolutionDpi(font, &hDpi, &vDpi) == SCE_FONT_OK && hDpi == 96 && vDpi == 144);
    hDpi = 1;
    vDpi = 1;
    Require(sceFontGetResolutionDpi(font, &hDpi, nullptr) == SCE_FONT_OK && hDpi == 96);
    Require(sceFontGetResolutionDpi(font, nullptr, &vDpi) == SCE_FONT_OK && vDpi == 144);
    Require(sceFontGetResolutionDpi(font, nullptr, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontSetResolutionDpi(font, 0, 300) == SCE_FONT_OK);
    Require(sceFontGetResolutionDpi(font, &hDpi, &vDpi) == SCE_FONT_OK && hDpi == 72 && vDpi == 300);
    Require(sceFontGetResolutionDpi(nullptr, &hDpi, &vDpi) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && hDpi == 0 && vDpi == 0);
    FontHandleOpaque unopened{};
    hDpi = 1;
    vDpi = 1;
    Require(sceFontGetResolutionDpi(&unopened, &hDpi, &vDpi) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && hDpi == 0 && vDpi == 0);
    std::uint32_t glyphsCount = 0;
    Require(sceFontGetFontGlyphsCount(font, &glyphsCount) == SCE_FONT_OK && glyphsCount == 1);
    Require(sceFontGetFontGlyphsCount(font, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    glyphsCount = 1;
    Require(sceFontGetFontGlyphsCount(nullptr, &glyphsCount) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && glyphsCount == 0);
    glyphsCount = 1;
    Require(sceFontGetFontGlyphsCount(&unopened, &glyphsCount) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && glyphsCount == 0);
    std::uint32_t glyphCode = 1;
    Require(sceFontGetCharGlyphCode(font, 'A', &glyphCode) == SCE_FONT_ERROR_NO_SUPPORT_GLYPH && glyphCode == 0);
    Require(sceFontGetCharGlyphCode(font, 'A', nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    glyphCode = 1;
    Require(sceFontGetCharGlyphCode(nullptr, 'A', &glyphCode) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && glyphCode == 0);
    glyphCode = 1;
    Require(sceFontGetCharGlyphCode(&unopened, 'A', &glyphCode) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && glyphCode == 0);
    std::uint32_t resolution = 0;
    float scalePixel = 0.0f;
    Require(sceFontGetFontResolution(font, &resolution, &scalePixel) == SCE_FONT_OK && resolution == 1000 && scalePixel == 15.625f);
    resolution = 0;
    Require(sceFontGetFontResolution(font, &resolution, nullptr) == SCE_FONT_OK && resolution == 1000);
    scalePixel = 0.0f;
    Require(sceFontGetFontResolution(font, nullptr, &scalePixel) == SCE_FONT_OK && scalePixel == 15.625f);
    Require(sceFontGetFontResolution(font, nullptr, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontGetFontResolution(nullptr, &resolution, &scalePixel) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && resolution == 0 && scalePixel == 0.0f);
    resolution = 1;
    scalePixel = 1.0f;
    Require(sceFontGetFontResolution(&unopened, &resolution, &scalePixel) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && resolution == 0 && scalePixel == 0.0f);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);

    FontRenderer renderer = nullptr;
    Require(sceFontCreateRenderer(&memory, sceFontSelectRendererFt(0), &renderer) == SCE_FONT_OK && renderer != nullptr);

    const std::vector<unsigned char> rasterData = RasterGlyphFont();
    FontHandle rasterFont = nullptr;
    Require(sceFontOpenFontMemory(library, rasterData.data(), static_cast<std::uint32_t>(rasterData.size()), nullptr, &rasterFont) == SCE_FONT_OK && rasterFont != nullptr);
    FontGlyph rasterGlyph = nullptr;
    Require(sceFontGenerateCharGlyph(rasterFont, 'A', nullptr, &rasterGlyph) == SCE_FONT_OK && rasterGlyph != nullptr);
    Require(sceFontSetScalePixel(rasterFont, 32.0f, 32.0f) == SCE_FONT_OK);
    Require(sceFontCloseFont(rasterFont) == SCE_FONT_OK);
    std::vector<std::uint8_t> pixels(64 * 64);
    FontRenderSurface surface{};
    surface.buffer = pixels.data();
    surface.widthByte = 64;
    surface.pixelSizeByte = 1;
    surface.width = 64;
    surface.height = 64;
    surface.sc_x1 = 64;
    surface.sc_y1 = 64;
    FontGlyphMetrics imageMetrics{};
    FontRenderOutput renderOutput{};
    Require(sceFontGlyphRenderImageHorizontal(rasterGlyph, nullptr, renderer, &surface, 10.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_OK);
    Require(renderOutput.UpdateRect.x == 10 && renderOutput.UpdateRect.y == 29 && renderOutput.UpdateRect.w == 8 && renderOutput.UpdateRect.h == 11);
    Require(renderOutput.SurfaceImage.address == pixels.data() + 29 * 64 + 10);
    for (std::uint32_t y = 0; y < 64; ++y) {
        for (std::uint32_t x = 0; x < 64; ++x) {
            const bool inside = x >= 10 && x < 18 && y >= 29 && y < 40;
            Require((pixels[y * 64 + x] != 0) == inside);
        }
    }
    const std::vector<std::uint8_t> expectedHorizontal = pixels;
    const std::uint32_t glyphWidthAtDefaultScale = renderOutput.UpdateRect.w;
    FontStyleFrame scaleFrame{};
    scaleFrame.magic = 0x0F09;
    scaleFrame.flags1 = 0x01;
    scaleFrame.hDpi = 144;
    scaleFrame.vDpi = 144;
    scaleFrame.scalePixelW = 32.0f;
    scaleFrame.scalePixelH = 32.0f;
    std::fill(pixels.begin(), pixels.end(), 0);
    Require(sceFontGlyphRenderImageHorizontal(rasterGlyph, &scaleFrame, renderer, &surface, 10.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_OK);
    Require(renderOutput.UpdateRect.w == 16 && renderOutput.UpdateRect.h == 23 && renderOutput.UpdateRect.w == glyphWidthAtDefaultScale * 2);
    scaleFrame.scaleUnit = 1;
    scaleFrame.scalePixelW = 16.0f;
    scaleFrame.scalePixelH = 16.0f;
    Require(sceFontGlyphRenderImageHorizontal(rasterGlyph, &scaleFrame, renderer, &surface, 10.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_OK);
    Require(renderOutput.UpdateRect.w == 16 && renderOutput.UpdateRect.h == 23);
    std::fill(pixels.begin(), pixels.end(), 0);
    Require(sceFontGlyphRenderImage(rasterGlyph, nullptr, renderer, &surface, 10.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_OK);
    Require(renderOutput.UpdateRect.x == 10 && renderOutput.UpdateRect.y == 29 && renderOutput.UpdateRect.w == 8 && renderOutput.UpdateRect.h == 11);
    Require(pixels == expectedHorizontal);
    std::fill(pixels.begin(), pixels.end(), 0);
    Require(sceFontGlyphRenderImageVertical(rasterGlyph, nullptr, renderer, &surface, 32.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_OK);
    Require(renderOutput.UpdateRect.x == 28 && renderOutput.UpdateRect.y == 42 && renderOutput.UpdateRect.w == 8 && renderOutput.UpdateRect.h == 12);
    Require(std::any_of(pixels.begin(), pixels.end(), [](std::uint8_t value) { return value != 0; }));
    FontHandle pointScaleFont = nullptr;
    Require(sceFontOpenFontMemory(library, rasterData.data(), static_cast<std::uint32_t>(rasterData.size()), nullptr, &pointScaleFont) == SCE_FONT_OK && pointScaleFont != nullptr);
    Require(sceFontSetResolutionDpi(pointScaleFont, 144, 144) == SCE_FONT_OK);
    Require(sceFontSetScalePoint(pointScaleFont, 16.0f, 16.0f) == SCE_FONT_OK);
    FontGlyph pointScaleGlyph = nullptr;
    Require(sceFontGenerateCharGlyph(pointScaleFont, 'A', nullptr, &pointScaleGlyph) == SCE_FONT_OK && pointScaleGlyph != nullptr);
    std::fill(pixels.begin(), pixels.end(), 0);
    Require(sceFontGlyphRenderImageHorizontal(pointScaleGlyph, nullptr, renderer, &surface, 10.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_OK);
    Require(renderOutput.UpdateRect.x == 10 && renderOutput.UpdateRect.y == 17 && renderOutput.UpdateRect.w == 16 && renderOutput.UpdateRect.h == 23);
    Require(sceFontDeleteGlyph(&memory, &pointScaleGlyph) == SCE_FONT_OK && pointScaleGlyph == nullptr);
    Require(sceFontCloseFont(pointScaleFont) == SCE_FONT_OK);
    std::fill(pixels.begin(), pixels.end(), 0);
    surface.sc_x0 = 12;
    surface.sc_x1 = 16;
    surface.sc_y0 = 30;
    surface.sc_y1 = 34;
    Require(sceFontGlyphRenderImageHorizontal(rasterGlyph, nullptr, renderer, &surface, 10.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_OK);
    Require(renderOutput.UpdateRect.x >= 12 && renderOutput.UpdateRect.x + renderOutput.UpdateRect.w <= 16);
    Require(renderOutput.UpdateRect.y >= 30 && renderOutput.UpdateRect.y + renderOutput.UpdateRect.h <= 34);
    Require(std::any_of(pixels.begin(), pixels.end(), [](std::uint8_t value) { return value != 0; }));
    for (std::uint32_t y = 0; y < 64; ++y) {
        for (std::uint32_t x = 0; x < 64; ++x) {
            const bool inside = x >= 12 && x < 16 && y >= 30 && y < 34;
            Require((pixels[y * 64 + x] != 0) == inside);
        }
    }
    surface.sc_x0 = 0;
    surface.sc_x1 = 64;
    surface.sc_y0 = 0;
    surface.sc_y1 = 64;
    imageMetrics = {1.0f, 1.0f, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}};
    renderOutput = {};
    Require(sceFontGlyphRenderImageHorizontal(rasterGlyph, nullptr, nullptr, &surface, 10.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_ERROR_INVALID_RENDERER);
    Require(imageMetrics.width == 0 && renderOutput.UpdateRect.w == 0);
    Require(sceFontGlyphRenderImageHorizontal(rasterGlyph, nullptr, renderer, &surface, 1.0e30f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(imageMetrics.width == 0 && renderOutput.UpdateRect.w == 0);
    scaleFrame.scaleUnit = 0;
    scaleFrame.scalePixelW = std::numeric_limits<float>::max();
    scaleFrame.scalePixelH = 16.0f;
    Require(sceFontGlyphRenderImageHorizontal(rasterGlyph, &scaleFrame, renderer, &surface, 10.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(imageMetrics.width == 0 && renderOutput.UpdateRect.w == 0);
    std::vector<std::uint8_t> rgba(72 * 16, 0xA5);
    FontRenderSurface surface32{};
    surface32.buffer = rgba.data();
    surface32.widthByte = 72;
    surface32.pixelSizeByte = 4;
    surface32.width = 16;
    surface32.height = 16;
    surface32.sc_x1 = 16;
    surface32.sc_y1 = 16;
    Require(sceFontGlyphRenderImageHorizontal(rasterGlyph, nullptr, renderer, &surface32, 2.0f, 14.0f, &imageMetrics, &renderOutput) == SCE_FONT_OK);
    bool rgbaPixelWritten = false;
    for (std::uint32_t y = renderOutput.UpdateRect.y; y < renderOutput.UpdateRect.y + renderOutput.UpdateRect.h; ++y) {
        for (std::uint32_t x = renderOutput.UpdateRect.x; x < renderOutput.UpdateRect.x + renderOutput.UpdateRect.w; ++x) {
            const auto* pixel = rgba.data() + y * 72 + x * 4;
            Require(pixel[0] == pixel[1] && pixel[1] == pixel[2] && pixel[2] == pixel[3]);
            if (pixel[0] != 0xA5) rgbaPixelWritten = true;
        }
    }
    Require(rgbaPixelWritten);
    for (std::uint32_t y = 0; y < 16; ++y) {
        for (std::uint32_t x = 64; x < 72; ++x) Require(rgba[y * 72 + x] == 0xA5);
    }
    Require(renderOutput.SurfaceImage.widthByte == 72 && renderOutput.SurfaceImage.pixelSizeByte == 4);
    std::fill(pixels.begin(), pixels.end(), 0);
    Require(sceFontGlyphRenderImageHorizontal(rasterGlyph, nullptr, renderer, &surface, 1000.0f, 1000.0f, &imageMetrics, &renderOutput) == SCE_FONT_OK);
    Require(renderOutput.UpdateRect.w == 0 && renderOutput.UpdateRect.h == 0 && renderOutput.SurfaceImage.address == nullptr);
    Require(std::all_of(pixels.begin(), pixels.end(), [](std::uint8_t value) { return value == 0; }));
    FontGlyphOpaque notAGeneratedGlyph{};
    Require(sceFontGlyphRenderImageHorizontal(&notAGeneratedGlyph, nullptr, renderer, &surface, 10.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_ERROR_INVALID_GLYPH);
    FontGlyph staleGlyph = rasterGlyph;
    Require(sceFontDeleteGlyph(&memory, &rasterGlyph) == SCE_FONT_OK && rasterGlyph == nullptr);
    Require(sceFontGlyphRenderImageHorizontal(staleGlyph, nullptr, renderer, &surface, 10.0f, 40.0f, &imageMetrics, &renderOutput) == SCE_FONT_ERROR_INVALID_GLYPH);

    const std::vector<unsigned char> kerningData = KerningFont();
    font = nullptr;
    Require(sceFontOpenFontMemory(library, kerningData.data(), static_cast<std::uint32_t>(kerningData.size()), nullptr, &font) == SCE_FONT_OK && font != nullptr);
    Require(sceFontGetFontGlyphsCount(font, &glyphsCount) == SCE_FONT_OK && glyphsCount == 3);
    Require(sceFontGetCharGlyphCode(font, 'A', &glyphCode) == SCE_FONT_OK && glyphCode == 1);
    Require(sceFontGetCharGlyphCode(font, 'V', &glyphCode) == SCE_FONT_OK && glyphCode == 2);
    glyphCode = 1;
    Require(sceFontGetCharGlyphCode(font, 'B', &glyphCode) == SCE_FONT_ERROR_NO_SUPPORT_GLYPH && glyphCode == 0);
    glyphCode = 1;
    Require(sceFontGetCharGlyphCode(font, 0, &glyphCode) == SCE_FONT_ERROR_NO_SUPPORT_CODE && glyphCode == 0);
    Require(sceFontSetResolutionDpi(font, 144, 144) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    FontGlyph glyph = nullptr;
    Require(sceFontGenerateCharGlyph(font, 'A', nullptr, &glyph) == SCE_FONT_OK && glyph != nullptr);
    Require(sceFontGlyphDefineAttribute(glyph, 0x11, 0) == SCE_FONT_OK);
    Require(sceFontGlyphDefineAttribute(nullptr, 0x11, 0) == SCE_FONT_ERROR_INVALID_GLYPH);
    FontGlyphOpaque notAGlyph{};
    Require(sceFontGlyphDefineAttribute(&notAGlyph, 0x11, 0) == SCE_FONT_ERROR_INVALID_GLYPH);
    Require(sceFontDeleteGlyph(&memory, &glyph) == SCE_FONT_OK && glyph == nullptr);
    FontKerning kerning{1.0f, 1.0f, 1.0f, 1.0f};
    Require(sceFontGetKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -20.0f));
    kerning = {1.0f, 1.0f, 1.0f, 1.0f};
    Require(sceFontGetRenderScaledKerning(font, 'A', 'V', &kerning) == SCE_FONT_ERROR_NOT_BOUND_RENDERER && KerningIs(kerning, 0.0f));
    Require(sceFontBindRenderer(font, renderer) == SCE_FONT_OK);
    Require(sceFontGetRenderScaledKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -20.0f));
    Require(sceFontSetupRenderScalePixel(font, 50.0f, 50.0f) == SCE_FONT_OK);
    Require(sceFontGetRenderScaledKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -10.0f));
    Require(sceFontGetKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -20.0f));
    Require(sceFontSetupRenderScalePoint(font, 20.0f, 20.0f) == SCE_FONT_OK);
    Require(sceFontGetRenderScaledKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -8.0f));
    kerning = {1.0f, 1.0f, 1.0f, 1.0f};
    Require(sceFontGetRenderScaledKerning(font, 'V', 'A', &kerning) == SCE_FONT_OK && KerningIs(kerning, 0.0f));
    kerning = {1.0f, 1.0f, 1.0f, 1.0f};
    Require(sceFontGetRenderScaledKerning(font, 'A', 'B', &kerning) == SCE_FONT_OK && KerningIs(kerning, 0.0f));
    Require(sceFontGetRenderScaledKerning(font, 'A', 'V', nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    kerning = {1.0f, 1.0f, 1.0f, 1.0f};
    Require(sceFontGetRenderScaledKerning(nullptr, 'A', 'V', &kerning) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && KerningIs(kerning, 0.0f));
    kerning = {1.0f, 1.0f, 1.0f, 1.0f};
    Require(sceFontGetRenderScaledKerning(&unopened, 'A', 'V', &kerning) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && KerningIs(kerning, 0.0f));
    Require(sceFontUnbindRenderer(font) == SCE_FONT_OK);
    Require(sceFontGetRenderScaledKerning(font, 'A', 'V', &kerning) == SCE_FONT_ERROR_NOT_BOUND_RENDERER);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);

    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK && renderer == nullptr);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_ERROR_INVALID_RENDERER);

    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK && library == nullptr);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_ERROR_INVALID_MEMORY);
    std::filesystem::remove_all(fontRoot);
}
