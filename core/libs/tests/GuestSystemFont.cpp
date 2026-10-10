#include "prx/libSceFont/include/FontTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

extern "C" {
int APS5_VABI sceFontMemoryInit(FontMemory*, void*, std::uint32_t, const FontMemoryInterface*, void*, FontMemoryDestroyFunction, void*);
int APS5_VABI sceFontCreateLibrary(const FontMemory*, const void*, FontLibrary*);
int APS5_VABI sceFontDestroyLibrary(FontLibrary*);
int APS5_VABI sceFontSupportSystemFonts(FontLibrary);
int APS5_VABI sceFontSupportExternalFonts(FontLibrary, std::uint32_t, std::uint32_t);
int APS5_VABI sceFontOpenFontSet(FontLibrary, std::uint32_t, std::uint32_t, const FontOpenDetail*, FontHandle*);
int APS5_VABI sceFontOpenFontMemory(FontLibrary, const void*, std::uint32_t, const FontOpenDetail*, FontHandle*);
int APS5_VABI sceFontOpenFontInstance(FontHandle, FontHandle, FontHandle*);
int APS5_VABI sceFontCloseFont(FontHandle);
int APS5_VABI sceFontSetScalePixel(FontHandle, float, float);
int APS5_VABI sceFontGetCharGlyphMetrics(FontHandle, std::uint32_t, FontGlyphMetrics*);
int APS5_VABI sceFontGetHorizontalLayout(FontHandle, FontHorizontalLayout*);
const void* APS5_VABI sceFontSelectLibraryFt(int);
}

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
    Require(_putenv_s("ANYPS5_SYSTEM_FONTS", directory.string().c_str()) == 0);
#else
    Require(::setenv("ANYPS5_SYSTEM_FONTS", directory.string().c_str(), 1) == 0);
#endif
}

static float AdvanceOf(FontHandle font, std::uint32_t code) {
    FontGlyphMetrics metrics{};
    Require(sceFontGetCharGlyphMetrics(font, code, &metrics) == SCE_FONT_OK);
    return metrics.Horizontal.advance;
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
    const std::filesystem::path fonts = root / "fonts";
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
    Require(sceFontOpenFontSet(library, EuropeanBold, 1, nullptr, &font) == SCE_FONT_ERROR_FONT_OPEN_FAILED && font == nullptr);
    Require(sceFontOpenFontSet(library, 0x18070046u, 1, nullptr, &font) == SCE_FONT_ERROR_NO_SUPPORT_FONTSET && font == nullptr);
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

    for (FontHandle handle : {font, instance, substitute, memoryFont}) Require(sceFontCloseFont(handle) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(allocations == 0);
    std::filesystem::remove_all(root);
}
