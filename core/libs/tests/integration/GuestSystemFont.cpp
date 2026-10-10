#include "prx/libSceFont/include/FontTypes.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
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

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::uint32_t EuropeanLight = 0x180700C3u;
constexpr std::uint32_t EuropeanBold = 0x180700C7u;
constexpr std::uint32_t EuropeanItalic = 0x18170044u;
constexpr std::uint32_t ThaiMedium = 0x18071055u;
constexpr std::uint32_t VietnameseBold = 0x18070057u;
constexpr std::uint32_t JapaneseJg2Light = 0x1A0835D3u;
constexpr std::uint32_t ChineseGb = 0x180CB0D4u;
constexpr const char* fontDirectoryVariable = "ANYPS5_SYSTEM_FONTS";

void* APS5_VABI Allocate(void* object, std::uint32_t size) {
    if (object != nullptr) ++*static_cast<int*>(object);
    return std::malloc(size);
}

void APS5_VABI Release(void* object, void* pointer) {
    if (pointer != nullptr && object != nullptr) --*static_cast<int*>(object);
    std::free(pointer);
}

void Put16(std::vector<unsigned char>& out, std::uint32_t value) {
    out.push_back(static_cast<unsigned char>(value >> 8));
    out.push_back(static_cast<unsigned char>(value));
}

void Put32(std::vector<unsigned char>& out, std::uint32_t value) {
    Put16(out, value >> 16);
    Put16(out, value & 0xFFFFu);
}

void PutAll16(std::vector<unsigned char>& out, std::initializer_list<int> values) {
    for (const int value : values) Put16(out, static_cast<std::uint32_t>(static_cast<std::uint16_t>(value)));
}

std::vector<unsigned char> SquareGlyphFont(std::uint32_t codepoint, int width) {
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
    PutAll16(cmap, {4, 32, 0, 4, 4, 1, 0, static_cast<int>(codepoint), 0xFFFF, 0, static_cast<int>(codepoint), 0xFFFF,
                    1 - static_cast<int>(codepoint), 1, 0, 0});
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

void WriteFile(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file), "write " + path.string());
}

void SetEnvironment(const char* name, const std::optional<std::string>& value) {
#ifdef _WIN32
    _putenv_s(name, value ? value->c_str() : "");
#else
    if (value) ::setenv(name, value->c_str(), 1);
    else ::unsetenv(name);
#endif
}

class SystemFontFixture {
public:
    SystemFontFixture() {
        if (const char* previous = std::getenv(fontDirectoryVariable)) previousDirectory = previous;
        std::filesystem::create_directories(Empty());
        std::filesystem::create_directories(Fonts());
        WriteFile(Fonts() / "SST-Bold.otf", SquareGlyphFont('A', 700));
        WriteFile(Fonts() / "NotoSans-Bold.ttf", SquareGlyphFont('A', 300));
        WriteFile(Fonts() / "NotoSans-Light.ttf", SquareGlyphFont('A', 500));
        WriteFile(Fonts() / "SST-Italic.otf", SquareGlyphFont('A', 520));
        WriteFile(Fonts() / "NotoSansThai-Medium.ttf", SquareGlyphFont(0x0E01, 540));
        WriteFile(Fonts() / "SSTVietnamese-Bold.otf", SquareGlyphFont(0x1EA0, 560));
        WriteFile(Fonts() / "NotoSansCJK-Light.ttc", SquareGlyphFont(0x3042, 580));
        RequireEqual(sceFontMemoryInit(&memory, nullptr, 0, &iface, &allocations, nullptr, nullptr), SCE_FONT_OK, "initialize font memory");
        RequireEqual(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library), SCE_FONT_OK, "create the library");
        RequireEqual(sceFontSupportSystemFonts(library), SCE_FONT_OK, "support system fonts");
    }

    ~SystemFontFixture() {
        for (auto font : fonts) sceFontCloseFont(font);
        if (library != nullptr) sceFontDestroyLibrary(&library);
        SetEnvironment(fontDirectoryVariable, previousDirectory);
    }

    SystemFontFixture(const SystemFontFixture&) = delete;
    SystemFontFixture& operator=(const SystemFontFixture&) = delete;

    std::filesystem::path Empty() const {
        return root.Path() / "empty";
    }

    std::filesystem::path Fonts() const {
        return root.Path() / "fonts";
    }

    void UseDirectory(const std::filesystem::path& directory) const {
        SetEnvironment(fontDirectoryVariable, directory.string());
    }

    FontHandle Track(FontHandle font) {
        fonts.push_back(font);
        return font;
    }

    FontHandle OpenSet(std::uint32_t type, std::uint32_t mode) {
        FontHandle font = nullptr;
        RequireEqual(sceFontOpenFontSet(library, type, mode, nullptr, &font), SCE_FONT_OK, "open font set " + std::to_string(type));
        Require(font != nullptr, "font handle is set");
        return Track(font);
    }

    void Close(FontHandle font) {
        std::erase(fonts, font);
        RequireEqual(sceFontCloseFont(font), SCE_FONT_OK, "close the font");
    }

    const Testing::TemporaryDirectory root;
    int allocations = 0;
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    FontLibrary library = nullptr;

private:
    std::optional<std::string> previousDirectory;
    std::vector<FontHandle> fonts;
};

float AdvanceOf(FontHandle font, std::uint32_t code) {
    RequireEqual(sceFontSetScalePixel(font, 100.0f, 100.0f), SCE_FONT_OK, "set the pixel scale");
    FontGlyphMetrics metrics{};
    RequireEqual(sceFontGetCharGlyphMetrics(font, code, &metrics), SCE_FONT_OK, "glyph metrics of " + std::to_string(code));
    return metrics.Horizontal.advance;
}

const Case emptyDirectory{"OpenFontSet_EmptyFontDirectory_FailsOpenOrUnsupported", [] {
    SystemFontFixture fixture;
    fixture.UseDirectory(fixture.Empty());
    FontHandle font = nullptr;
    RequireEqual(sceFontOpenFontSet(fixture.library, EuropeanBold, 1, nullptr, &font), SCE_FONT_ERROR_FONT_OPEN_FAILED, "bold set");
    Require(font == nullptr, "no handle for a missing file");
    RequireEqual(sceFontOpenFontSet(fixture.library, 0x18070046u, 1, nullptr, &font), SCE_FONT_ERROR_NO_SUPPORT_FONTSET, "unknown set");
    Require(font == nullptr, "no handle for an unknown set");
}};

const Case missingDirectory{"OpenFontSet_MissingFontDirectory_Throws", [] {
    SystemFontFixture fixture;
    fixture.UseDirectory(fixture.root.Path() / "missing");
    FontHandle font = nullptr;
    Testing::RequireThrows<std::runtime_error>([&] { sceFontOpenFontSet(fixture.library, EuropeanBold, 1, nullptr, &font); },
                                               "missing directory");
}};

const Case openBold{"OpenFontSet_EuropeanBold_LoadsSstBoldWithThreeAllocations", [] {
    SystemFontFixture fixture;
    fixture.UseDirectory(fixture.Fonts());
    const int beforeOpen = fixture.allocations;
    const auto font = fixture.OpenSet(EuropeanBold, 1);
    RequireEqual(fixture.allocations - beforeOpen, 3, "allocations for one open");
    RequireEqual(AdvanceOf(font, 'A'), 70.0f, "SST-Bold advance");
    FontGlyphMetrics metrics{};
    RequireEqual(sceFontGetCharGlyphMetrics(font, 'B', &metrics), SCE_FONT_ERROR_NO_SUPPORT_GLYPH, "missing glyph");
    FontHorizontalLayout layout{};
    RequireEqual(sceFontGetHorizontalLayout(font, &layout), SCE_FONT_OK, "layout");
    Require(layout.baselineOffset > 0.0f, "positive baseline");
    Require(layout.lineAdvance >= layout.baselineOffset, "line advance covers the baseline");
}};

const Case openTwiceAndInstance{"OpenFontSet_AgainOrInstance_ReturnsIndependentHandles", [] {
    SystemFontFixture fixture;
    fixture.UseDirectory(fixture.Fonts());
    const auto font = fixture.OpenSet(EuropeanBold, 1);
    const auto again = fixture.OpenSet(EuropeanBold, 2);
    Require(again != font, "second open returns a new handle");
    FontHandle instance = nullptr;
    RequireEqual(sceFontOpenFontInstance(font, nullptr, &instance), SCE_FONT_OK, "open instance");
    Require(instance != nullptr, "instance handle is set");
    fixture.Track(instance);
    FontHorizontalLayout layout{};
    RequireEqual(sceFontGetHorizontalLayout(instance, &layout), SCE_FONT_OK, "instance layout");
    Require(layout.baselineOffset > 0.0f, "instance baseline");
}};

const Case styleFiles{"OpenFontSet_EachStyle_LoadsMatchingFile", [] {
    SystemFontFixture fixture;
    fixture.UseDirectory(fixture.Fonts());
    const std::vector<std::pair<std::uint32_t, std::pair<std::uint32_t, float>>> expected = {
        {EuropeanLight, {'A', 50.0f}}, {EuropeanItalic, {'A', 52.0f}}, {ThaiMedium, {0x0E01, 54.0f}},
        {VietnameseBold, {0x1EA0, 56.0f}}, {JapaneseJg2Light, {0x3042, 58.0f}}};
    for (const auto& [type, glyph] : expected) {
        const auto set = fixture.OpenSet(type, 1);
        RequireEqual(AdvanceOf(set, glyph.first), glyph.second, "advance of set " + std::to_string(type));
        fixture.Close(set);
    }
}};

const Case missingFile{"OpenFontSet_FileAbsentFromDirectory_FailsOpen", [] {
    SystemFontFixture fixture;
    fixture.UseDirectory(fixture.Fonts());
    FontHandle missing = nullptr;
    RequireEqual(sceFontOpenFontSet(fixture.library, ChineseGb, 1, nullptr, &missing), SCE_FONT_ERROR_FONT_OPEN_FAILED, "Chinese set");
    Require(missing == nullptr, "no handle");
}};

const Case substitute{"OpenFontSet_PrimaryFileMissing_FallsBackToSubstituteWithoutAffectingOpenFonts", [] {
    SystemFontFixture fixture;
    fixture.UseDirectory(fixture.Fonts());
    const auto font = fixture.OpenSet(EuropeanBold, 1);
    const auto substituteFonts = fixture.root.Path() / "substitute-fonts";
    std::filesystem::create_directories(substituteFonts);
    WriteFile(substituteFonts / "NotoSans-Bold.ttf", SquareGlyphFont('A', 300));
    fixture.UseDirectory(substituteFonts);
    const auto replacement = fixture.OpenSet(EuropeanBold, 3);
    RequireEqual(AdvanceOf(replacement, 'A'), 30.0f, "substitute advance");
    RequireEqual(AdvanceOf(font, 'A'), 70.0f, "already open font keeps its file");
}};

const Case memoryFont{"OpenFontMemory_WithSystemFontsSupported_LoadsExternalFont", [] {
    SystemFontFixture fixture;
    RequireEqual(sceFontSupportExternalFonts(fixture.library, 2, 0x52), SCE_FONT_OK, "support external fonts");
    const std::vector<unsigned char> external = SquareGlyphFont('A', 900);
    FontHandle font = nullptr;
    RequireEqual(sceFontOpenFontMemory(fixture.library, external.data(), static_cast<std::uint32_t>(external.size()), nullptr, &font),
                 SCE_FONT_OK, "open");
    fixture.Track(font);
    RequireEqual(AdvanceOf(font, 'A'), 90.0f, "memory font advance");
    FontHorizontalLayout layout{};
    RequireEqual(sceFontGetHorizontalLayout(font, &layout), SCE_FONT_OK, "layout");
}};

const Case releaseAll{"DestroyLibrary_AfterClosingEverything_ReleasesAllAllocations", [] {
    SystemFontFixture fixture;
    fixture.UseDirectory(fixture.Fonts());
    const auto font = fixture.OpenSet(EuropeanBold, 1);
    FontHandle instance = nullptr;
    RequireEqual(sceFontOpenFontInstance(font, nullptr, &instance), SCE_FONT_OK, "open instance");
    fixture.Track(instance);
    RequireEqual(sceFontSupportExternalFonts(fixture.library, 2, 0x52), SCE_FONT_OK, "support external fonts");
    const std::vector<unsigned char> external = SquareGlyphFont('A', 900);
    FontHandle memoryFont = nullptr;
    RequireEqual(sceFontOpenFontMemory(fixture.library, external.data(), static_cast<std::uint32_t>(external.size()), nullptr,
                                       &memoryFont), SCE_FONT_OK, "open memory font");
    fixture.Track(memoryFont);
    for (FontHandle handle : {font, instance, memoryFont}) fixture.Close(handle);
    RequireEqual(sceFontDestroyLibrary(&fixture.library), SCE_FONT_OK, "destroy the library");
    RequireEqual(fixture.allocations, 0, "outstanding allocations");
}};

} // namespace
