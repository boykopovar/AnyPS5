#include "prx/libSceFont/include/FontDriver.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <map>
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
int APS5_VABI sceFontGlyphDefineAttribute(FontGlyph, std::uint32_t, std::uint64_t);
int APS5_VABI sceFontDeleteGlyph(const FontMemory*, FontGlyph*);
const void* APS5_VABI sceFontSelectLibraryFt(int);
const void* APS5_VABI sceFontSelectRendererFt(int);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::uint32_t SystemFontSet = 0x18070043u;

void* APS5_VABI Allocate(void* object, std::uint32_t size) {
    if (object != nullptr) ++*static_cast<int*>(object);
    return std::malloc(size);
}

void APS5_VABI Release(void* object, void* pointer) {
    if (pointer != nullptr && object != nullptr) --*static_cast<int*>(object);
    std::free(pointer);
}

std::uint32_t APS5_VABI CoarsePixelResolution() {
    return 16;
}

class FontMemoryFixture {
public:
    FontMemoryFixture() {
        RequireEqual(sceFontMemoryInit(&memory, nullptr, 0, &iface, &allocations, nullptr, nullptr), SCE_FONT_OK, "initialize font memory");
    }

    ~FontMemoryFixture() {
        sceFontMemoryTerm(&memory);
    }

    FontMemoryFixture(const FontMemoryFixture&) = delete;
    FontMemoryFixture& operator=(const FontMemoryFixture&) = delete;

    int allocations = 0;
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
};

class LibraryFixture : public FontMemoryFixture {
public:
    LibraryFixture() {
        RequireEqual(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library), SCE_FONT_OK, "create the library");
        Require(library != nullptr, "library handle is set");
    }

    ~LibraryFixture() {
        for (auto font : fonts) sceFontCloseFont(font);
        if (renderer != nullptr) sceFontDestroyRenderer(&renderer);
        if (library != nullptr) sceFontDestroyLibrary(&library);
    }

    FontHandle OpenMemory(const std::vector<unsigned char>& data) {
        if (!externalFonts) {
            RequireEqual(sceFontSupportExternalFonts(library, 4, 0x52), SCE_FONT_OK, "support external fonts");
            externalFonts = true;
        }
        FontHandle font = nullptr;
        RequireEqual(sceFontOpenFontMemory(library, data.data(), static_cast<std::uint32_t>(data.size()), nullptr, &font), SCE_FONT_OK,
                     "open the memory font");
        Require(font != nullptr, "font handle is set");
        fonts.push_back(font);
        return font;
    }

    void Close(FontHandle font) {
        std::erase(fonts, font);
        RequireEqual(sceFontCloseFont(font), SCE_FONT_OK, "close the font");
    }

    FontRenderer CreateRenderer() {
        RequireEqual(sceFontCreateRenderer(&memory, sceFontSelectRendererFt(0), &renderer), SCE_FONT_OK, "create the renderer");
        Require(renderer != nullptr, "renderer handle is set");
        return renderer;
    }

    FontLibrary library = nullptr;
    FontRenderer renderer = nullptr;

private:
    bool externalFonts = false;
    std::vector<FontHandle> fonts;
};

void Put16(std::vector<unsigned char>& out, int value) {
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>(value & 0xFF));
}

std::vector<unsigned char> Words(std::initializer_list<int> values) {
    std::vector<unsigned char> out;
    for (const int value : values) Put16(out, value);
    return out;
}

std::vector<unsigned char> BuildFont(int glyphCount, std::map<std::string, std::vector<unsigned char>> tables) {
    tables["glyf"] = Words({0, 0});
    tables["head"] = Words({1, 0, 1, 0, 0, 0, 0x5F0F, 0x3CF5, 0, 1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 2, 0, 0});
    tables["hhea"] = Words({1, 0, 800, -200, 0, 500, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1});
    tables["hmtx"] = Words({500});
    tables["hmtx"].resize(2 + 2 * static_cast<std::size_t>(glyphCount));
    tables["loca"].assign(2 * (static_cast<std::size_t>(glyphCount) + 1), 0);
    tables["maxp"] = Words({1, 0, glyphCount, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0});
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

std::vector<unsigned char> EmptyGlyphFont() {
    return BuildFont(1, {});
}

std::vector<unsigned char> KerningFont() {
    return BuildFont(3, {
        {"cmap", Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
        {"kern", Words({0, 1, 0, 20, 1, 1, 6, 0, 0, 1, 2, -200})},
    });
}

void RequireKerning(const FontKerning& kerning, float offsetX, const std::string& message) {
    RequireEqual(kerning.offsetX, offsetX, message + " offsetX");
    RequireEqual(kerning.offsetY, 0.0f, message + " offsetY");
    RequireEqual(kerning.positionX, 0.0f, message + " positionX");
    RequireEqual(kerning.positionY, 0.0f, message + " positionY");
}

class KerningFontFixture : public LibraryFixture {
public:
    KerningFontFixture() : data(KerningFont()), font(OpenMemory(data)) {
        RequireEqual(sceFontSetResolutionDpi(font, 144, 144), SCE_FONT_OK, "set 144 dpi");
        RequireEqual(sceFontSetScalePixel(font, 100.0f, 100.0f), SCE_FONT_OK, "set the pixel scale");
    }

    const std::vector<unsigned char> data;
    const FontHandle font;
};

const Case selectFt{"SelectFt_VersionZeroOnly_ReturnsDriver", [] {
    Require(sceFontSelectLibraryFt(0) != nullptr, "library driver 0");
    Require(sceFontSelectLibraryFt(1) == nullptr, "library driver 1");
    Require(sceFontSelectRendererFt(0) != nullptr, "renderer driver 0");
    Require(sceFontSelectRendererFt(1) == nullptr, "renderer driver 1");
}};

const Case createLibraryNullDriver{"CreateLibrary_NullDriver_FailsInvalidParameter", [] {
    FontMemoryFixture fixture;
    FontLibrary library = nullptr;
    RequireEqual(sceFontCreateLibrary(&fixture.memory, nullptr, &library), SCE_FONT_ERROR_INVALID_PARAMETER, "null driver");
    Require(library == nullptr, "library stays null");
}};

const Case pixelResolution{"GetPixelResolution_FtLibrary_Returns64", [] {
    const LibraryFixture fixture;
    std::uint32_t subPixelCount = 1;
    RequireEqual(sceFontGetPixelResolution(fixture.library, &subPixelCount), SCE_FONT_OK, "pixel resolution");
    RequireEqual(subPixelCount, 64u, "sub pixel count");
}};

const Case pixelResolutionInvalid{"GetPixelResolution_InvalidArguments_FailAndClearOutput", [] {
    const LibraryFixture fixture;
    RequireEqual(sceFontGetPixelResolution(fixture.library, nullptr), SCE_FONT_ERROR_INVALID_PARAMETER, "null output");
    RequireEqual(sceFontGetPixelResolution(nullptr, nullptr), SCE_FONT_ERROR_INVALID_PARAMETER, "null library and output");
    std::uint32_t subPixelCount = 1;
    RequireEqual(sceFontGetPixelResolution(nullptr, &subPixelCount), SCE_FONT_ERROR_INVALID_LIBRARY, "null library");
    RequireEqual(subPixelCount, 0u, "output cleared for a null library");
    FontHandleOpaque notALibrary{};
    subPixelCount = 1;
    RequireEqual(sceFontGetPixelResolution(&notALibrary, &subPixelCount), SCE_FONT_ERROR_INVALID_LIBRARY, "not a library");
    RequireEqual(subPixelCount, 0u, "output cleared for a non-library");
}};

const Case pixelResolutionDriver{"GetPixelResolution_CustomDriverCallback_IsUsedPerLibrary", [] {
    LibraryFixture fixture;
    Font::SysDriver coarseDriver = *static_cast<const Font::SysDriver*>(sceFontSelectLibraryFt(0));
    coarseDriver.pixel_resolution = CoarsePixelResolution;
    FontLibrary coarseLibrary = nullptr;
    RequireEqual(sceFontCreateLibrary(&fixture.memory, &coarseDriver, &coarseLibrary), SCE_FONT_OK, "create the coarse library");
    Require(coarseLibrary != nullptr, "coarse library handle is set");
    std::uint32_t subPixelCount = 0;
    const int coarseResult = sceFontGetPixelResolution(coarseLibrary, &subPixelCount);
    const auto coarseCount = subPixelCount;
    const int defaultResult = sceFontGetPixelResolution(fixture.library, &subPixelCount);
    const auto defaultCount = subPixelCount;
    coarseDriver.pixel_resolution = nullptr;
    const int missingResult = sceFontGetPixelResolution(coarseLibrary, &subPixelCount);
    const auto missingCount = subPixelCount;
    const int destroyResult = sceFontDestroyLibrary(&coarseLibrary);
    RequireEqual(coarseResult, SCE_FONT_OK, "coarse library resolution");
    RequireEqual(coarseCount, 16u, "coarse sub pixel count");
    RequireEqual(defaultResult, SCE_FONT_OK, "default library resolution");
    RequireEqual(defaultCount, 64u, "default sub pixel count");
    RequireEqual(missingResult, SCE_FONT_ERROR_INVALID_LIBRARY, "driver without callback");
    RequireEqual(missingCount, 0u, "output cleared without callback");
    RequireEqual(destroyResult, SCE_FONT_OK, "destroy the coarse library");
    Require(coarseLibrary == nullptr, "coarse library handle cleared");
}};

const Case cacheNotAttached{"DeviceCache_NotAttached_FailsWithLibraryOrAttachErrors", [] {
    const LibraryFixture fixture;
    void* cacheBuffer = nullptr;
    std::uint32_t cacheSize = 0;
    FontHandleOpaque notALibrary{};
    RequireEqual(sceFontDettachDeviceCacheBuffer(nullptr, &cacheBuffer, &cacheSize), SCE_FONT_ERROR_INVALID_LIBRARY, "null library");
    RequireEqual(sceFontDettachDeviceCacheBuffer(&notALibrary, &cacheBuffer, &cacheSize), SCE_FONT_ERROR_INVALID_LIBRARY, "not a library");
    RequireEqual(sceFontClearDeviceCache(fixture.library), SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER, "clear");
    RequireEqual(sceFontDettachDeviceCacheBuffer(fixture.library, &cacheBuffer, &cacheSize), SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER,
                 "dettach");
}};

const Case cacheCallerBuffer{"DeviceCache_CallerBuffer_AttachesClearsAndDetachesSameBuffer", [] {
    const LibraryFixture fixture;
    std::vector<unsigned char> callerCache(0x2000);
    const auto size = static_cast<std::uint32_t>(callerCache.size());
    RequireEqual(sceFontAttachDeviceCacheBuffer(fixture.library, callerCache.data(), 0x1000), SCE_FONT_ERROR_INVALID_PARAMETER,
                 "too small");
    RequireEqual(sceFontAttachDeviceCacheBuffer(fixture.library, callerCache.data(), size), SCE_FONT_OK, "attach");
    RequireEqual(sceFontAttachDeviceCacheBuffer(fixture.library, callerCache.data(), size), SCE_FONT_ERROR_ALREADY_ATTACHED,
                 "attach twice");
    RequireEqual(sceFontClearDeviceCache(fixture.library), SCE_FONT_OK, "clear");
    void* cacheBuffer = nullptr;
    std::uint32_t cacheSize = 0;
    RequireEqual(sceFontDettachDeviceCacheBuffer(fixture.library, &cacheBuffer, &cacheSize), SCE_FONT_OK, "dettach");
    Require(cacheBuffer == callerCache.data(), "caller buffer returned");
    RequireEqual(cacheSize, size, "caller size returned");
    RequireEqual(sceFontDettachDeviceCacheBuffer(fixture.library, &cacheBuffer, &cacheSize), SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER,
                 "dettach twice");
}};

const Case cacheLibraryBuffer{"DeviceCache_NullBuffer_AllocatesAndHandsBufferToCaller", [] {
    LibraryFixture fixture;
    RequireEqual(sceFontAttachDeviceCacheBuffer(fixture.library, nullptr, 0x2000), SCE_FONT_OK, "attach");
    void* cacheBuffer = nullptr;
    std::uint32_t cacheSize = 0;
    RequireEqual(sceFontDettachDeviceCacheBuffer(fixture.library, &cacheBuffer, &cacheSize), SCE_FONT_OK, "dettach");
    Require(cacheBuffer != nullptr, "allocated buffer returned");
    RequireEqual(cacheSize, 0x2000u, "allocated size returned");
    Release(&fixture.allocations, cacheBuffer);
    RequireEqual(sceFontDettachDeviceCacheBuffer(fixture.library, &cacheBuffer, &cacheSize), SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER,
                 "dettach twice");
}};

const Case cacheNoOutputs{"DeviceCache_DettachWithoutOutputs_Succeeds", [] {
    const LibraryFixture fixture;
    RequireEqual(sceFontAttachDeviceCacheBuffer(fixture.library, nullptr, 0x2000), SCE_FONT_OK, "attach");
    RequireEqual(sceFontDettachDeviceCacheBuffer(fixture.library, nullptr, nullptr), SCE_FONT_OK, "dettach without outputs");
    RequireEqual(sceFontDettachDeviceCacheBuffer(fixture.library, nullptr, nullptr), SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER,
                 "dettach twice");
}};

const Case fontSetUnsupported{"OpenFontSet_WithoutSystemFontSupport_FailsNoSupportFunction", [] {
    LibraryFixture fixture;
    FontHandle font = reinterpret_cast<FontHandle>(&fixture.memory);
    RequireEqual(sceFontOpenFontSet(fixture.library, SystemFontSet, 1, nullptr, &font), SCE_FONT_ERROR_NO_SUPPORT_FUNCTION, "open");
    Require(font == nullptr, "font handle cleared");
}};

const Case fontSetErrors{"OpenFontSet_SystemFontsSupported_ReportsOpenAndArgumentErrors", [] {
    const LibraryFixture fixture;
    RequireEqual(sceFontSupportSystemFonts(fixture.library), SCE_FONT_OK, "support system fonts");
    FontHandleOpaque sentinel{};
    FontHandle font = &sentinel;
    RequireEqual(sceFontOpenFontSet(fixture.library, SystemFontSet, 1, nullptr, &font), SCE_FONT_ERROR_FONT_OPEN_FAILED, "missing font file");
    Require(font == nullptr, "font handle cleared");
    RequireEqual(sceFontOpenFontSet(fixture.library, 0x12345678u, 1, nullptr, &font), SCE_FONT_ERROR_NO_SUPPORT_FONTSET, "unknown set");
    RequireEqual(sceFontOpenFontSet(fixture.library, SystemFontSet, 7, nullptr, &font), SCE_FONT_ERROR_INVALID_PARAMETER, "open mode 7");
    RequireEqual(sceFontOpenFontSet(nullptr, SystemFontSet, 1, nullptr, &font), SCE_FONT_ERROR_INVALID_LIBRARY, "null library");
}};

const Case memoryFontUnsupported{"OpenFontMemory_WithoutExternalSupport_FailsNoSupportFunction", [] {
    const LibraryFixture fixture;
    const unsigned char notAFont[64] = {1, 2, 3, 4};
    FontHandleOpaque sentinel{};
    FontHandle font = &sentinel;
    RequireEqual(sceFontOpenFontMemory(fixture.library, notAFont, sizeof(notAFont), nullptr, &font), SCE_FONT_ERROR_NO_SUPPORT_FUNCTION,
                 "open");
    Require(font == nullptr, "font handle cleared");
}};

const Case externalTwice{"SupportExternalFonts_Twice_FailsAlreadySpecified", [] {
    const LibraryFixture fixture;
    RequireEqual(sceFontSupportExternalFonts(fixture.library, 4, 0x52), SCE_FONT_OK, "first");
    RequireEqual(sceFontSupportExternalFonts(fixture.library, 4, 0x52), SCE_FONT_ERROR_ALREADY_SPECIFIED, "second");
}};

const Case memoryFontInvalid{"OpenFontMemory_NullOrUnknownData_Fails", [] {
    const LibraryFixture fixture;
    RequireEqual(sceFontSupportExternalFonts(fixture.library, 4, 0x52), SCE_FONT_OK, "support external fonts");
    FontHandleOpaque sentinel{};
    FontHandle font = &sentinel;
    RequireEqual(sceFontOpenFontMemory(fixture.library, nullptr, 0, nullptr, &font), SCE_FONT_ERROR_INVALID_PARAMETER, "null data");
    Require(font == nullptr, "font handle cleared for null data");
    const unsigned char notAFont[64] = {1, 2, 3, 4};
    font = &sentinel;
    RequireEqual(sceFontOpenFontMemory(fixture.library, notAFont, sizeof(notAFont), nullptr, &font), SCE_FONT_ERROR_NO_SUPPORT_FORMAT,
                 "unknown format");
    Require(font == nullptr, "font handle cleared for unknown data");
}};

const Case resolutionDpi{"ResolutionDpi_SetAndGet_RoundTripsWithDefaults", [] {
    LibraryFixture fixture;
    const auto data = EmptyGlyphFont();
    const auto font = fixture.OpenMemory(data);
    std::uint32_t hDpi = 1;
    std::uint32_t vDpi = 1;
    RequireEqual(sceFontGetResolutionDpi(font, &hDpi, &vDpi), SCE_FONT_OK, "default dpi");
    RequireEqual(hDpi, 72u, "default horizontal dpi");
    RequireEqual(vDpi, 72u, "default vertical dpi");
    RequireEqual(sceFontSetResolutionDpi(font, 96, 144), SCE_FONT_OK, "set 96x144");
    RequireEqual(sceFontGetResolutionDpi(font, &hDpi, &vDpi), SCE_FONT_OK, "get 96x144");
    RequireEqual(hDpi, 96u, "horizontal dpi");
    RequireEqual(vDpi, 144u, "vertical dpi");
    hDpi = 1;
    vDpi = 1;
    RequireEqual(sceFontGetResolutionDpi(font, &hDpi, nullptr), SCE_FONT_OK, "horizontal only");
    RequireEqual(hDpi, 96u, "horizontal only value");
    RequireEqual(sceFontGetResolutionDpi(font, nullptr, &vDpi), SCE_FONT_OK, "vertical only");
    RequireEqual(vDpi, 144u, "vertical only value");
    RequireEqual(sceFontGetResolutionDpi(font, nullptr, nullptr), SCE_FONT_ERROR_INVALID_PARAMETER, "no outputs");
    RequireEqual(sceFontSetResolutionDpi(font, 0, 300), SCE_FONT_OK, "set 0x300");
    RequireEqual(sceFontGetResolutionDpi(font, &hDpi, &vDpi), SCE_FONT_OK, "get after 0x300");
    RequireEqual(hDpi, 72u, "zero restores the default");
    RequireEqual(vDpi, 300u, "vertical 300");
}};

const Case resolutionDpiInvalid{"GetResolutionDpi_InvalidHandle_FailsAndClearsOutputs", [] {
    std::uint32_t hDpi = 1;
    std::uint32_t vDpi = 1;
    RequireEqual(sceFontGetResolutionDpi(nullptr, &hDpi, &vDpi), SCE_FONT_ERROR_INVALID_FONT_HANDLE, "null handle");
    RequireEqual(hDpi, 0u, "horizontal cleared for null");
    RequireEqual(vDpi, 0u, "vertical cleared for null");
    FontHandleOpaque unopened{};
    hDpi = 1;
    vDpi = 1;
    RequireEqual(sceFontGetResolutionDpi(&unopened, &hDpi, &vDpi), SCE_FONT_ERROR_INVALID_FONT_HANDLE, "unopened handle");
    RequireEqual(hDpi, 0u, "horizontal cleared for unopened");
    RequireEqual(vDpi, 0u, "vertical cleared for unopened");
}};

const Case glyphsCount{"GetFontGlyphsCount_EmptyGlyphFont_ReturnsOneOrFailsForBadArguments", [] {
    LibraryFixture fixture;
    const auto data = EmptyGlyphFont();
    const auto font = fixture.OpenMemory(data);
    std::uint32_t count = 0;
    RequireEqual(sceFontGetFontGlyphsCount(font, &count), SCE_FONT_OK, "count");
    RequireEqual(count, 1u, "one glyph");
    RequireEqual(sceFontGetFontGlyphsCount(font, nullptr), SCE_FONT_ERROR_INVALID_PARAMETER, "null output");
    count = 1;
    RequireEqual(sceFontGetFontGlyphsCount(nullptr, &count), SCE_FONT_ERROR_INVALID_FONT_HANDLE, "null handle");
    RequireEqual(count, 0u, "cleared for null");
    FontHandleOpaque unopened{};
    count = 1;
    RequireEqual(sceFontGetFontGlyphsCount(&unopened, &count), SCE_FONT_ERROR_INVALID_FONT_HANDLE, "unopened handle");
    RequireEqual(count, 0u, "cleared for unopened");
}};

const Case glyphCodeMissing{"GetCharGlyphCode_FontWithoutCmap_FailsAndClearsOutput", [] {
    LibraryFixture fixture;
    const auto data = EmptyGlyphFont();
    const auto font = fixture.OpenMemory(data);
    std::uint32_t glyphCode = 1;
    RequireEqual(sceFontGetCharGlyphCode(font, 'A', &glyphCode), SCE_FONT_ERROR_NO_SUPPORT_GLYPH, "no cmap");
    RequireEqual(glyphCode, 0u, "cleared without cmap");
    RequireEqual(sceFontGetCharGlyphCode(font, 'A', nullptr), SCE_FONT_ERROR_INVALID_PARAMETER, "null output");
    glyphCode = 1;
    RequireEqual(sceFontGetCharGlyphCode(nullptr, 'A', &glyphCode), SCE_FONT_ERROR_INVALID_FONT_HANDLE, "null handle");
    RequireEqual(glyphCode, 0u, "cleared for null");
    FontHandleOpaque unopened{};
    glyphCode = 1;
    RequireEqual(sceFontGetCharGlyphCode(&unopened, 'A', &glyphCode), SCE_FONT_ERROR_INVALID_FONT_HANDLE, "unopened handle");
    RequireEqual(glyphCode, 0u, "cleared for unopened");
}};

const Case fontResolution{"GetFontResolution_EmptyGlyphFont_ReportsUnitsPerEm", [] {
    LibraryFixture fixture;
    const auto data = EmptyGlyphFont();
    const auto font = fixture.OpenMemory(data);
    std::uint32_t resolution = 0;
    float scalePixel = 0.0f;
    RequireEqual(sceFontGetFontResolution(font, &resolution, &scalePixel), SCE_FONT_OK, "both outputs");
    RequireEqual(resolution, 1000u, "units per em");
    RequireEqual(scalePixel, 15.625f, "scale pixel");
    resolution = 0;
    RequireEqual(sceFontGetFontResolution(font, &resolution, nullptr), SCE_FONT_OK, "resolution only");
    RequireEqual(resolution, 1000u, "resolution only value");
    scalePixel = 0.0f;
    RequireEqual(sceFontGetFontResolution(font, nullptr, &scalePixel), SCE_FONT_OK, "scale only");
    RequireEqual(scalePixel, 15.625f, "scale only value");
    RequireEqual(sceFontGetFontResolution(font, nullptr, nullptr), SCE_FONT_ERROR_INVALID_PARAMETER, "no outputs");
}};

const Case fontResolutionInvalid{"GetFontResolution_InvalidHandle_FailsAndClearsOutputs", [] {
    std::uint32_t resolution = 1;
    float scalePixel = 1.0f;
    RequireEqual(sceFontGetFontResolution(nullptr, &resolution, &scalePixel), SCE_FONT_ERROR_INVALID_FONT_HANDLE, "null handle");
    RequireEqual(resolution, 0u, "resolution cleared for null");
    RequireEqual(scalePixel, 0.0f, "scale cleared for null");
    FontHandleOpaque unopened{};
    resolution = 1;
    scalePixel = 1.0f;
    RequireEqual(sceFontGetFontResolution(&unopened, &resolution, &scalePixel), SCE_FONT_ERROR_INVALID_FONT_HANDLE, "unopened handle");
    RequireEqual(resolution, 0u, "resolution cleared for unopened");
    RequireEqual(scalePixel, 0.0f, "scale cleared for unopened");
}};

const Case kerningGlyphCodes{"GetCharGlyphCode_KerningFontCmap_MapsCharactersOrFails", [] {
    KerningFontFixture fixture;
    std::uint32_t count = 0;
    RequireEqual(sceFontGetFontGlyphsCount(fixture.font, &count), SCE_FONT_OK, "count");
    RequireEqual(count, 3u, "three glyphs");
    std::uint32_t glyphCode = 0;
    RequireEqual(sceFontGetCharGlyphCode(fixture.font, 'A', &glyphCode), SCE_FONT_OK, "A");
    RequireEqual(glyphCode, 1u, "A glyph");
    RequireEqual(sceFontGetCharGlyphCode(fixture.font, 'V', &glyphCode), SCE_FONT_OK, "V");
    RequireEqual(glyphCode, 2u, "V glyph");
    glyphCode = 1;
    RequireEqual(sceFontGetCharGlyphCode(fixture.font, 'B', &glyphCode), SCE_FONT_ERROR_NO_SUPPORT_GLYPH, "B");
    RequireEqual(glyphCode, 0u, "B cleared");
    glyphCode = 1;
    RequireEqual(sceFontGetCharGlyphCode(fixture.font, 0, &glyphCode), SCE_FONT_ERROR_NO_SUPPORT_CODE, "code 0");
    RequireEqual(glyphCode, 0u, "code 0 cleared");
}};

const Case generateGlyph{"GenerateCharGlyph_KerningFont_DefinesAttributeAndDeletes", [] {
    KerningFontFixture fixture;
    FontGlyph glyph = nullptr;
    RequireEqual(sceFontGenerateCharGlyph(fixture.font, 'A', nullptr, &glyph), SCE_FONT_OK, "generate");
    Require(glyph != nullptr, "glyph handle is set");
    const int defineResult = sceFontGlyphDefineAttribute(glyph, 0x11, 0);
    const int deleteResult = sceFontDeleteGlyph(&fixture.memory, &glyph);
    RequireEqual(defineResult, SCE_FONT_OK, "define attribute");
    RequireEqual(deleteResult, SCE_FONT_OK, "delete");
    Require(glyph == nullptr, "glyph handle cleared");
    RequireEqual(sceFontGlyphDefineAttribute(nullptr, 0x11, 0), SCE_FONT_ERROR_INVALID_GLYPH, "null glyph");
    FontGlyphOpaque notAGlyph{};
    RequireEqual(sceFontGlyphDefineAttribute(&notAGlyph, 0x11, 0), SCE_FONT_ERROR_INVALID_GLYPH, "not a glyph");
}};

const Case kerning{"GetKerning_KernPair_ScalesToFontPixelSize", [] {
    KerningFontFixture fixture;
    FontKerning value{1.0f, 1.0f, 1.0f, 1.0f};
    RequireEqual(sceFontGetKerning(fixture.font, 'A', 'V', &value), SCE_FONT_OK, "kerning");
    RequireKerning(value, -20.0f, "A V");
}};

const Case scaledKerningUnbound{"GetRenderScaledKerning_NoRenderer_FailsNotBoundAndClears", [] {
    KerningFontFixture fixture;
    FontKerning value{1.0f, 1.0f, 1.0f, 1.0f};
    RequireEqual(sceFontGetRenderScaledKerning(fixture.font, 'A', 'V', &value), SCE_FONT_ERROR_NOT_BOUND_RENDERER, "unbound");
    RequireKerning(value, 0.0f, "unbound");
}};

const Case scaledKerning{"GetRenderScaledKerning_BoundRenderer_FollowsRenderScale", [] {
    KerningFontFixture fixture;
    RequireEqual(sceFontBindRenderer(fixture.font, fixture.CreateRenderer()), SCE_FONT_OK, "bind");
    FontKerning value{};
    RequireEqual(sceFontGetRenderScaledKerning(fixture.font, 'A', 'V', &value), SCE_FONT_OK, "default render scale");
    RequireKerning(value, -20.0f, "default render scale");
    RequireEqual(sceFontSetupRenderScalePixel(fixture.font, 50.0f, 50.0f), SCE_FONT_OK, "render pixel scale 50");
    RequireEqual(sceFontGetRenderScaledKerning(fixture.font, 'A', 'V', &value), SCE_FONT_OK, "render pixel scale");
    RequireKerning(value, -10.0f, "render pixel scale 50");
    RequireEqual(sceFontGetKerning(fixture.font, 'A', 'V', &value), SCE_FONT_OK, "unscaled kerning");
    RequireKerning(value, -20.0f, "unscaled kerning unaffected");
    RequireEqual(sceFontSetupRenderScalePoint(fixture.font, 20.0f, 20.0f), SCE_FONT_OK, "render point scale 20");
    RequireEqual(sceFontGetRenderScaledKerning(fixture.font, 'A', 'V', &value), SCE_FONT_OK, "render point scale");
    RequireKerning(value, -8.0f, "render point scale 20");
    value = {1.0f, 1.0f, 1.0f, 1.0f};
    RequireEqual(sceFontGetRenderScaledKerning(fixture.font, 'V', 'A', &value), SCE_FONT_OK, "V A");
    RequireKerning(value, 0.0f, "V A");
    value = {1.0f, 1.0f, 1.0f, 1.0f};
    RequireEqual(sceFontGetRenderScaledKerning(fixture.font, 'A', 'B', &value), SCE_FONT_OK, "A B");
    RequireKerning(value, 0.0f, "A B");
}};

const Case scaledKerningInvalid{"GetRenderScaledKerning_InvalidArguments_Fail", [] {
    KerningFontFixture fixture;
    RequireEqual(sceFontBindRenderer(fixture.font, fixture.CreateRenderer()), SCE_FONT_OK, "bind");
    RequireEqual(sceFontGetRenderScaledKerning(fixture.font, 'A', 'V', nullptr), SCE_FONT_ERROR_INVALID_PARAMETER, "null output");
    FontKerning value{1.0f, 1.0f, 1.0f, 1.0f};
    RequireEqual(sceFontGetRenderScaledKerning(nullptr, 'A', 'V', &value), SCE_FONT_ERROR_INVALID_FONT_HANDLE, "null handle");
    RequireKerning(value, 0.0f, "null handle");
    FontHandleOpaque unopened{};
    value = {1.0f, 1.0f, 1.0f, 1.0f};
    RequireEqual(sceFontGetRenderScaledKerning(&unopened, 'A', 'V', &value), SCE_FONT_ERROR_INVALID_FONT_HANDLE, "unopened handle");
    RequireKerning(value, 0.0f, "unopened handle");
    RequireEqual(sceFontUnbindRenderer(fixture.font), SCE_FONT_OK, "unbind");
    RequireEqual(sceFontGetRenderScaledKerning(fixture.font, 'A', 'V', &value), SCE_FONT_ERROR_NOT_BOUND_RENDERER, "after unbind");
}};

const Case teardown{"Destroy_AllObjects_ReleasesEveryAllocationAndRejectsSecondDestroy", [] {
    FontMemoryFixture fixture;
    FontLibrary library = nullptr;
    RequireEqual(sceFontCreateLibrary(&fixture.memory, sceFontSelectLibraryFt(0), &library), SCE_FONT_OK, "create the library");
    FontRenderer renderer = nullptr;
    RequireEqual(sceFontCreateRenderer(&fixture.memory, sceFontSelectRendererFt(0), &renderer), SCE_FONT_OK, "create the renderer");
    const auto data = KerningFont();
    FontHandle font = nullptr;
    const int supportResult = sceFontSupportExternalFonts(library, 4, 0x52);
    const int openResult = sceFontOpenFontMemory(library, data.data(), static_cast<std::uint32_t>(data.size()), nullptr, &font);
    const int bindResult = font != nullptr ? sceFontBindRenderer(font, renderer) : -1;
    const int closeResult = font != nullptr ? sceFontCloseFont(font) : -1;
    const int destroyRenderer = sceFontDestroyRenderer(&renderer);
    const int destroyRendererAgain = sceFontDestroyRenderer(&renderer);
    const int destroyLibrary = sceFontDestroyLibrary(&library);
    const int destroyLibraryAgain = sceFontDestroyLibrary(&library);
    RequireEqual(supportResult, SCE_FONT_OK, "support external fonts");
    RequireEqual(openResult, SCE_FONT_OK, "open");
    RequireEqual(bindResult, SCE_FONT_OK, "bind");
    RequireEqual(closeResult, SCE_FONT_OK, "close");
    RequireEqual(destroyRenderer, SCE_FONT_OK, "destroy the renderer");
    Require(renderer == nullptr, "renderer handle cleared");
    RequireEqual(destroyRendererAgain, SCE_FONT_ERROR_INVALID_RENDERER, "destroy the renderer twice");
    RequireEqual(destroyLibrary, SCE_FONT_OK, "destroy the library");
    Require(library == nullptr, "library handle cleared");
    RequireEqual(destroyLibraryAgain, SCE_FONT_ERROR_INVALID_LIBRARY, "destroy the library twice");
    RequireEqual(fixture.allocations, 0, "outstanding allocations");
    RequireEqual(sceFontMemoryTerm(&fixture.memory), SCE_FONT_OK, "terminate memory");
    RequireEqual(sceFontMemoryTerm(&fixture.memory), SCE_FONT_ERROR_INVALID_MEMORY, "terminate memory twice");
}};

} // namespace
