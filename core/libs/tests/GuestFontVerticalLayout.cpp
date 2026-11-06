#include "prx/libSceFontFt/include/Api.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include "tests/FontFixture.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font vertical layout check failed at line %d\n", line);
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

static void Put16(std::vector<unsigned char>& out, int value) {
    out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
    out.push_back(static_cast<unsigned char>(value & 0xFF));
}

static std::vector<unsigned char> Words(std::initializer_list<int> values) {
    std::vector<unsigned char> out;
    for (const int value : values) Put16(out, value);
    return out;
}

static std::uint32_t ReadU32(const std::vector<unsigned char>& font, std::size_t offset) {
    return (static_cast<std::uint32_t>(font[offset]) << 24) | (static_cast<std::uint32_t>(font[offset + 1]) << 16) |
           (static_cast<std::uint32_t>(font[offset + 2]) << 8) | static_cast<std::uint32_t>(font[offset + 3]);
}

static void WriteI16(std::vector<unsigned char>& font, std::size_t offset, int value) {
    font[offset] = static_cast<unsigned char>((value >> 8) & 0xFF);
    font[offset + 1] = static_cast<unsigned char>(value & 0xFF);
}

static void SetHeadBounds(std::vector<unsigned char>& font, int xMin, int yMin, int xMax, int yMax) {
    const int tableCount = static_cast<int>(font[4] << 8 | font[5]);
    for (int table = 0; table < tableCount; ++table) {
        const std::size_t record = 12 + static_cast<std::size_t>(table) * 16;
        if (std::memcmp(font.data() + record, "head", 4) != 0) continue;
        const std::size_t head = ReadU32(font, record + 8);
        WriteI16(font, head + 36, xMin);
        WriteI16(font, head + 38, yMin);
        WriteI16(font, head + 40, xMax);
        WriteI16(font, head + 42, yMax);
        return;
    }
    std::abort();
}

static std::vector<unsigned char> VerticalFont() {
    auto font = FontFixture::BuildFont(1, {
        {"vhea", Words({1, 0, 800, -200, 0, 500, 1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1})},
        {"vmtx", Words({500, 0})},
    });
    SetHeadBounds(font, -100, -200, 900, 800);
    return font;
}

static void CheckLayout(FontHandle font, FontVerticalLayout& layout) {
    Require(sceFontGetVerticalLayout(font, &layout) == SCE_FONT_OK);
    Require(layout.columnAdvance > 0.0f);
    Require(layout.baselineOffsetX < 0.0f);
    Require(layout.decorationSpan == 0.0f);
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 1, 0x52) == SCE_FONT_OK);
    const std::vector<unsigned char> fontData = VerticalFont();
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &font) == SCE_FONT_OK);

    FontVerticalLayout layout{};
    Require(sceFontGetVerticalLayout(font, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontGetVerticalLayout(nullptr, &layout) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(layout.baselineOffsetX == 0.0f && layout.columnAdvance == 0.0f && layout.decorationSpan == 0.0f);
    layout = {1.0f, 1.0f, 1.0f};
    FontHandleOpaque unopened{};
    Require(sceFontGetVerticalLayout(&unopened, &layout) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(layout.baselineOffsetX == 0.0f && layout.columnAdvance == 0.0f && layout.decorationSpan == 0.0f);

    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    CheckLayout(font, layout);
    const FontVerticalLayout pixelLayout = layout;
    Require(sceFontSetResolutionDpi(font, 144, 144) == SCE_FONT_OK);
    CheckLayout(font, layout);
    Require(std::memcmp(&layout, &pixelLayout, sizeof(pixelLayout)) == 0);

    Require(sceFontSetScalePoint(font, 20.0f, 30.0f) == SCE_FONT_OK);
    CheckLayout(font, layout);
    const FontVerticalLayout pointLayout = layout;
    FontHandle instance = nullptr;
    Require(sceFontOpenFontInstance(font, nullptr, &instance) == SCE_FONT_OK);
    FontVerticalLayout instanceLayout{};
    Require(sceFontGetVerticalLayout(instance, &instanceLayout) == SCE_FONT_OK);
    Require(std::memcmp(&instanceLayout, &pointLayout, sizeof(pointLayout)) == 0);
    Require(sceFontCloseFont(instance) == SCE_FONT_OK);

    Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
}
