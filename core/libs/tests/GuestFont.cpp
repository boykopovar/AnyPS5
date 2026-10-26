#include "prx/libSceFontFt/include/Api.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontDriver.hpp"
#include "prx/libc/include/General.hpp"
#include "tests/FontFixture.hpp"
#include <array>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void SetFontDirectory(const std::filesystem::path& directory) {
#ifdef _WIN32
    Require(_wputenv_s(L"ANYPS5_SYSTEM_FONTS", directory.wstring().c_str()) == 0);
#else
    Require(::setenv("ANYPS5_SYSTEM_FONTS", directory.c_str(), 1) == 0);
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

static int APS5_VABI WideFontResolution(Font::FontObj*, std::uint32_t* resolution, float* scale) {
    *resolution = 0x12345678;
    *scale = 12.5f;
    return SCE_FONT_OK;
}

using FontFixture::BuildFont;
using FontFixture::Words;

static std::vector<unsigned char> EmptyGlyphFont() {
    return BuildFont(1, {});
}

static std::vector<unsigned char> KerningFont() {
    return BuildFont(3, {
        {"cmap", Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
        {"kern", Words({0, 1, 0, 20, 1, 1, 6, 0, 0, 1, 2, -200})},
    });
}

struct ParsedText {
    const char* text;
    std::size_t at;
    FontHandle font;
};

static std::int32_t APS5_VABI ParseText(FontTextSource* source, void** order, FontTextParseResult* result) {
    auto* parsed = static_cast<ParsedText*>(source->textObject);
    *order = const_cast<char*>(parsed->text + parsed->at);
    if (parsed->text[parsed->at] == 0) {
        result->Terminate.terminateCode = 0;
        return 0;
    }
    result->FontCode.font = parsed->font;
    result->FontCode.code = static_cast<unsigned char>(parsed->text[parsed->at++]);
    return 1;
}

struct ParsedCodes {
    const std::uint32_t* codes;
    std::size_t count;
    std::size_t at;
    FontHandle font;
};

static std::int32_t APS5_VABI ParseCodes(FontTextSource* source, void** order, FontTextParseResult* result) {
    auto* parsed = static_cast<ParsedCodes*>(source->textObject);
    *order = const_cast<std::uint32_t*>(parsed->codes + parsed->at);
    if (parsed->at == parsed->count) {
        result->Terminate.terminateCode = 0;
        return 0;
    }
    result->FontCode.font = parsed->font;
    result->FontCode.code = parsed->codes[parsed->at++];
    return 1;
}

static void CheckTextCodes(const FontMemory* memory, FontHandle font) {
    ParsedText parsed{"AVA", 0, font};
    FontTextSource source{};
    Require(sceFontTextSourceInit(&source, parsed.text, 3, ParseText, &parsed) == SCE_FONT_OK);
    FontString string = nullptr;
    Require(sceFontCreateString(memory, &source, nullptr, &string) == SCE_FONT_OK && string != nullptr);
    std::uint32_t count = 0;
    FontTextCharacter* characters = sceFontStringRefersTextCharacters(string, &count);
    Require(characters != nullptr && count == 3);
    Require(sceFontStringGetWritingForm(string) == 0x12);
    FontString rtlString = nullptr;
    Require(sceFontTextSourceSetWritingForm(&source, 0x11) == SCE_FONT_OK && sceFontCreateString(memory, &source, nullptr, &rtlString) == SCE_FONT_OK);
    Require(sceFontStringGetWritingForm(rtlString) == 0x11);
    Require(sceFontDestroyString(&rtlString) == SCE_FONT_OK);
    Require(sceFontTextSourceSetWritingForm(&source, 0x12) == SCE_FONT_OK && sceFontCreateString(memory, &source, nullptr, &rtlString) == SCE_FONT_OK);
    Require(sceFontStringGetWritingForm(rtlString) == 0x12);
    Require(sceFontDestroyString(&rtlString) == SCE_FONT_OK);
    Require(sceFontTextSourceSetWritingForm(&source, 0x10) == SCE_FONT_OK);

    const std::uint32_t latinCodes[]{'A', 0x00E9, 0x4E2D};
    const std::uint32_t greekCodes[]{0x03B1};
    const std::uint32_t digitsCodes[]{'1', '2', ' '};
    const std::uint32_t hebrewCodes[]{0x05E9, 0x05DC};
    const std::uint32_t arabicCodes[]{0x0627, 0x0644};
    const std::uint32_t syriacCodes[]{0x0710};
    const std::uint32_t thaanaCodes[]{0x0780};
    const std::uint32_t nkoCodes[]{0x07CA};
    const std::uint32_t hebrewLetterCodes[]{0xFB1D};
    const std::uint32_t arabicFormCodes[]{0xFE70};
    const std::uint32_t digitHebrewCodes[]{'1', ' ', 0x05D0};
    const std::uint32_t hebrewLatinCodes[]{0x05D0, 'A'};
    const std::uint32_t latinHebrewCodes[]{'A', 0x05D0};
    const std::uint32_t rlmLatinCodes[]{0x200F, 'A'};
    const std::uint32_t lrmHebrewCodes[]{0x200E, 0x05D0};
    const std::uint32_t embeddingCodes[]{0x202B, 'A'};
    const std::uint32_t overrideCodes[]{0x202E, 'A'};
    const std::uint32_t isolatedHebrewCodes[]{0x2067, 0x05D0, 0x2069, 'A'};
    const std::uint32_t isolatedLatinCodes[]{0x2066, 'A', 0x2069, 0x05D0};
    const std::uint32_t openIsolateCodes[]{0x2067, 0x05D0};
    const struct { const std::uint32_t* codes; std::size_t count; std::int32_t sourceForm; int expected; } forms[]{
        {latinCodes, 3, 0x10, 0x12},
        {greekCodes, 1, 0x10, 0x12},
        {digitsCodes, 3, 0x10, 0x12},
        {nullptr, 0, 0x10, 0x12},
        {hebrewCodes, 2, 0x10, 0x11},
        {arabicCodes, 2, 0x10, 0x11},
        {syriacCodes, 1, 0x10, 0x11},
        {thaanaCodes, 1, 0x10, 0x11},
        {nkoCodes, 1, 0x10, 0x11},
        {hebrewLetterCodes, 1, 0x10, 0x11},
        {arabicFormCodes, 1, 0x10, 0x11},
        {digitHebrewCodes, 3, 0x10, 0x11},
        {hebrewLatinCodes, 2, 0x10, 0x11},
        {latinHebrewCodes, 2, 0x10, 0x12},
        {rlmLatinCodes, 2, 0x10, 0x11},
        {lrmHebrewCodes, 2, 0x10, 0x12},
        {embeddingCodes, 2, 0x10, 0x12},
        {overrideCodes, 2, 0x10, 0x12},
        {isolatedHebrewCodes, 4, 0x10, 0x12},
        {isolatedLatinCodes, 4, 0x10, 0x11},
        {openIsolateCodes, 2, 0x10, 0x12},
        {hebrewCodes, 2, 0x11, 0x11},
        {hebrewCodes, 2, 0x12, 0x12},
        {latinCodes, 3, 0x11, 0x11},
        {latinCodes, 3, 0x12, 0x12},
    };
    for (const auto& form : forms) {
        ParsedCodes parsedCodes{form.codes, form.count, 0, font};
        FontTextSource codeSource{};
        FontString codeString = nullptr;
        Require(sceFontTextSourceInit(&codeSource, parsed.text, 3, ParseCodes, &parsedCodes) == SCE_FONT_OK);
        Require(sceFontTextSourceSetWritingForm(&codeSource, form.sourceForm) == SCE_FONT_OK);
        Require(sceFontCreateString(memory, &codeSource, nullptr, &codeString) == SCE_FONT_OK && codeString != nullptr);
        Require(sceFontStringGetWritingForm(codeString) == form.expected);
        Require(sceFontDestroyString(&codeString) == SCE_FONT_OK);
    }

    FontTextCodes codes;
    std::memset(&codes, 0xa5, sizeof(codes));
    Require(sceFontCharactersRefersTextCodes(characters, nullptr, &codes) == &codes);
    Require(codes.text.order == parsed.text && codes.text.code == 'A' && codes.text.reserved == 0);
    Require(codes.systemUse[5] == characters && codes.systemUse[6] == characters && codes.systemUse[7] == nullptr);
    Require(sceFontTextCodesStepNext(&codes) == &codes && codes.text.code == 'V' && codes.text.order == parsed.text + 1);
    Require(sceFontTextCodesStepNext(&codes) == &codes && codes.text.code == 'A' && codes.text.order == parsed.text + 2);
    Require(sceFontTextCodesStepNext(&codes) == nullptr && codes.text.order == parsed.text + 2);
    Require(sceFontTextCodesStepBack(&codes) == &codes && codes.text.code == 'V' && codes.text.order == parsed.text + 1);
    Require(sceFontTextCodesStepBack(&codes) == &codes && codes.text.code == 'A' && codes.text.order == parsed.text);
    Require(sceFontTextCodesStepBack(&codes) == nullptr && codes.text.order == parsed.text);

    Require(sceFontCharactersRefersTextCodes(characters + 1, characters + 2, &codes) == &codes && codes.text.code == 'V');
    Require(sceFontTextCodesStepNext(&codes) == &codes && codes.text.order == parsed.text + 2);
    Require(sceFontTextCodesStepNext(&codes) == nullptr && codes.text.order == parsed.text + 2);
    Require(sceFontTextCodesStepBack(&codes) == &codes && codes.text.order == parsed.text + 1);
    Require(sceFontTextCodesStepBack(&codes) == nullptr && codes.text.order == parsed.text + 1);

    Require(sceFontCharactersRefersTextCodes(characters + 2, characters + 2, &codes) == &codes && codes.text.order == parsed.text + 2);
    Require(sceFontTextCodesStepNext(&codes) == nullptr && sceFontTextCodesStepBack(&codes) == nullptr);

    characters[1].synthetic = 1;
    Require(sceFontCharactersRefersTextCodes(characters, nullptr, &codes) == &codes);
    Require(sceFontTextCodesStepNext(&codes) == &codes && codes.text.order == parsed.text + 2);
    Require(sceFontTextCodesStepBack(&codes) == &codes && codes.text.order == parsed.text);
    Require(sceFontCharactersRefersTextCodes(characters + 1, nullptr, &codes) == nullptr && codes.text.order == parsed.text);
    characters[1].synthetic = 0;
    characters[1].clusterIndex = 1;
    Require(sceFontCharactersRefersTextCodes(characters + 1, nullptr, &codes) == nullptr && codes.text.order == parsed.text);
    Require(sceFontTextCodesStepNext(&codes) == &codes && codes.text.order == parsed.text + 1);
    characters[1].clusterIndex = 0;

    Require(sceFontCharactersRefersTextCodes(characters, nullptr, nullptr) == nullptr);
    Require(sceFontTextCodesStepNext(nullptr) == nullptr && sceFontTextCodesStepBack(nullptr) == nullptr);
    bool threw = false;
    try {
        sceFontCharactersRefersTextCodes(nullptr, nullptr, &codes);

    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && codes.text.order == parsed.text + 1);
    Require(sceFontDestroyString(&string) == SCE_FONT_OK);
}

static bool KerningIs(const FontKerning& kerning, float offsetX) {
    return kerning.offsetX == offsetX && kerning.offsetY == 0.0f && kerning.positionX == 0.0f && kerning.positionY == 0.0f;
}

static void CheckGlyphScale(FontHandle font, const FontMemory* memory, float expectedW, float expectedH) {
    float width = 0.0f;
    float height = 0.0f;
    Require(sceFontGetScalePixel(font, &width, &height) == SCE_FONT_OK);
    Require(width == expectedW && height == expectedH);
    FontGlyph glyph = nullptr;
    Require(sceFontGenerateCharGlyph(font, 'A', nullptr, &glyph) == SCE_FONT_OK && glyph != nullptr);
    Require(sceFontGlyphGetScalePixel(glyph, &width, &height) == SCE_FONT_OK);
    if (width != expectedW || height != expectedH) {
        std::fprintf(stderr, "Generated glyph scale %.3f x %.3f differs from font scale %.3f x %.3f\n", width, height, expectedW, expectedH);
    }
    Require(width == expectedW && height == expectedH);
    Require(glyph->em_size == static_cast<std::uint16_t>(std::lround(expectedH)));
    Require(sceFontDeleteGlyph(memory, &glyph) == SCE_FONT_OK && glyph == nullptr);
}

static void CheckPointAndDpiScale(FontHandle font, FontLibrary library, const FontMemory* memory, FontRenderer renderer) {
    FontKerning kerning{};
    Require(sceFontSetScalePoint(font, 20.0f, 30.0f) == SCE_FONT_OK);
    CheckGlyphScale(font, memory, 40.0f, 60.0f);
    Require(sceFontGetKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -8.0f));
    Require(sceFontSetResolutionDpi(font, 216, 288) == SCE_FONT_OK);
    CheckGlyphScale(font, memory, 60.0f, 120.0f);
    Require(sceFontGetKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -12.0f));
    Require(sceFontSetScalePoint(font, 50.0f, 40.0f) == SCE_FONT_OK);
    CheckGlyphScale(font, memory, 150.0f, 160.0f);
    Require(sceFontGetKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -30.0f));
    Require(sceFontSetResolutionDpi(font, 0, 0) == SCE_FONT_OK);
    CheckGlyphScale(font, memory, 50.0f, 40.0f);
    Require(sceFontGetKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -10.0f));
    Require(sceFontSetScalePixel(font, 100.0f, 75.0f) == SCE_FONT_OK);
    Require(sceFontSetResolutionDpi(font, 144, 216) == SCE_FONT_OK);
    CheckGlyphScale(font, memory, 100.0f, 75.0f);
    Require(sceFontGetKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -20.0f));
    Require(sceFontSetScalePoint(font, 20.0f, 30.0f) == SCE_FONT_OK);
    CheckGlyphScale(font, memory, 40.0f, 90.0f);
    Require(sceFontGetKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -8.0f));
    FontHandle instance = nullptr;
    Require(sceFontOpenFontInstance(font, nullptr, &instance) == SCE_FONT_OK && instance != nullptr);
    CheckGlyphScale(instance, memory, 40.0f, 90.0f);
    Require(sceFontGetKerning(instance, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -8.0f));
    Require(sceFontSetScalePoint(instance, 30.0f, 40.0f) == SCE_FONT_OK);
    CheckGlyphScale(instance, memory, 60.0f, 120.0f);
    CheckGlyphScale(font, memory, 40.0f, 90.0f);
    Require(sceFontCloseFont(instance) == SCE_FONT_OK);
    Require(sceFontBindRenderer(font, renderer) == SCE_FONT_OK);
    Require(sceFontSetupRenderScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    Require(sceFontGetRenderScaledKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -20.0f));
    Require(sceFontGetKerning(font, 'A', 'V', &kerning) == SCE_FONT_OK && KerningIs(kerning, -8.0f));
    Require(sceFontUnbindRenderer(font) == SCE_FONT_OK);
    const std::vector<unsigned char> data = KerningFont();
    FontHandle fresh = nullptr;
    Require(sceFontOpenFontMemory(library, data.data(), static_cast<std::uint32_t>(data.size()), nullptr, &fresh) == SCE_FONT_OK);
    float width = 0.0f;
    float height = 0.0f;
    Require(sceFontGetScalePixel(fresh, &width, &height) == SCE_FONT_OK);
    CheckGlyphScale(fresh, memory, width, height);
    Require(sceFontCloseFont(fresh) == SCE_FONT_OK);
    Require(sceFontSetResolutionDpi(font, 144, 144) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
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
    Require(sceFontFtSupportBdf(library) == 0 && sceFontFtSupportCid(library) == 0 && sceFontFtSupportOpenType(library) == 0 && sceFontFtSupportOpenTypeOtf(library) == 0 && sceFontFtSupportOpenTypeTtf(library) == 0 && sceFontFtSupportPcf(library) == 0 && sceFontFtSupportPfr(library) == 0 && sceFontFtSupportTrueType(library) == 0 && sceFontFtSupportTrueTypeGx(library) == 0 && sceFontFtSupportType1(library) == 0 && sceFontFtSupportType42(library) == 0 && sceFontFtSupportWinFonts(library) == 0);
    const auto* nativeLibrary = static_cast<const Font::FontLibNative*>(library);
    const auto savedLock = nativeLibrary->lock_word;
    for (auto support : {sceFontFtSupportBdf, sceFontFtSupportCid, sceFontFtSupportOpenType, sceFontFtSupportOpenTypeOtf, sceFontFtSupportOpenTypeTtf, sceFontFtSupportPcf, sceFontFtSupportPfr, sceFontFtSupportTrueType, sceFontFtSupportTrueTypeGx, sceFontFtSupportType1, sceFontFtSupportType42, sceFontFtSupportWinFonts}) {
        Font::FontLibNative invalidLibrary{};
        Require(support(nullptr) == SCE_FONT_ERROR_INVALID_LIBRARY);
        Require(support(&invalidLibrary) == SCE_FONT_ERROR_INVALID_LIBRARY);
        Require(support(library) == SCE_FONT_OK);
        Require(nativeLibrary->lock_word == savedLock);
    }

    Font::FontLibNative foreignLibrary{};
    foreignLibrary.magic = Font::LIBRARY_MAGIC;
    foreignLibrary.lock_word = 5;
    bool foreignDriverRejected = false;
    try {
        static_cast<void>(sceFontFtSupportTrueType(&foreignLibrary));
    } catch (const std::runtime_error&) {
        foreignDriverRejected = true;
    }
    Require(foreignDriverRejected && foreignLibrary.lock_word == 5);

    std::uint32_t subPixelCount = 1;
    Require(sceFontGetPixelResolution(library, &subPixelCount) == SCE_FONT_OK && subPixelCount == 64);
    Require(sceFontGetPixelResolution(library, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontGetPixelResolution(nullptr, nullptr) == SCE_FONT_ERROR_INVALID_LIBRARY);
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
    Require(static_cast<Font::FontLibNative*>(coarseLibrary)->lock_word == 0);
    coarseDriver.pixel_resolution = nullptr;
    Require(sceFontGetPixelResolution(coarseLibrary, &subPixelCount) == SCE_FONT_ERROR_INVALID_LIBRARY && subPixelCount == 0);
    Require(sceFontGetPixelResolution(coarseLibrary, nullptr) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(static_cast<Font::FontLibNative*>(coarseLibrary)->lock_word == 0);
    Require(sceFontDestroyLibrary(&coarseLibrary) == SCE_FONT_OK && coarseLibrary == nullptr);

    void* cacheBuffer = &notALibrary;
    std::uint32_t cacheSize = 0;
    cacheSize = 123;
    Require(sceFontDettachDeviceCacheBuffer(nullptr, &cacheBuffer, &cacheSize) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(cacheBuffer == nullptr && cacheSize == 0);
    Require(sceFontDettachDeviceCacheBuffer(&notALibrary, &cacheBuffer, &cacheSize) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(sceFontClearDeviceCache(library) == SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER);
    Require(sceFontDettachDeviceCacheBuffer(library, &cacheBuffer, &cacheSize) == SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER);

    std::vector<unsigned char> callerCache(0x2000);
    Require(sceFontAttachDeviceCacheBuffer(library, callerCache.data(), 0x1000) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontAttachDeviceCacheBuffer(library, callerCache.data(), static_cast<std::uint32_t>(callerCache.size())) == SCE_FONT_OK);
    Require(sceFontAttachDeviceCacheBuffer(library, callerCache.data(), static_cast<std::uint32_t>(callerCache.size())) == SCE_FONT_ERROR_ALREADY_ATTACHED);
    Require(sceFontClearDeviceCache(library) == SCE_FONT_OK);
    Require(sceFontDettachDeviceCacheBuffer(library, &cacheBuffer, &cacheSize) == SCE_FONT_OK && cacheBuffer == callerCache.data() && cacheSize == callerCache.size());
    for (std::size_t offset = 4; offset < 24; ++offset) Require(callerCache[offset] == 0);
    Require(sceFontDettachDeviceCacheBuffer(library, &cacheBuffer, &cacheSize) == SCE_FONT_ERROR_NOT_ATTACHED_CACHE_BUFFER);

    Require(sceFontAttachDeviceCacheBuffer(library, nullptr, 0x2000) == SCE_FONT_OK);
    cacheBuffer = nullptr;
    cacheSize = 0;
    const int allocationsBeforeDetach = allocations;
    Require(sceFontDettachDeviceCacheBuffer(library, &cacheBuffer, &cacheSize) == SCE_FONT_OK && cacheBuffer == nullptr && cacheSize == 0);
    Require(allocations == allocationsBeforeDetach - 1);
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
    font = reinterpret_cast<FontHandle>(&memory);
    Require(sceFontOpenFontFile(library, "anyps5-missing-font-parent/../font.ttf", 1, nullptr, &font) == SCE_FONT_ERROR_FS_OPEN_FAILED && font == nullptr);
    Require(sceFontOpenFontMemory(library, nullptr, 0, nullptr, &font) == SCE_FONT_ERROR_INVALID_PARAMETER && font == nullptr);
    Require(sceFontOpenFontMemory(library, notAFont, sizeof(notAFont), nullptr, &font) == SCE_FONT_ERROR_NO_SUPPORT_FORMAT && font == nullptr);

    const std::vector<unsigned char> fontData = EmptyGlyphFont();
    const auto fileFont = fontRoot / std::filesystem::path(u8"external-a\u00e7\u00e3o-\u65e5\u672c") / "font.ttf";
    std::filesystem::create_directories(fileFont.parent_path());
    {
        std::ofstream file(fileFont, std::ios::binary);
        file.write(reinterpret_cast<const char*>(fontData.data()), static_cast<std::streamsize>(fontData.size()));
        Require(static_cast<bool>(file));
    }
    const std::u8string fontRootText = fontRoot.u8string();
    AddPathAlias_nid_no_patch("/font-test", reinterpret_cast<const char*>(fontRootText.c_str()));
    const std::u8string fileFontText = u8"/font-test/external-a\u00e7\u00e3o-\u65e5\u672c/font.ttf";
    FontHandle fileHandle = nullptr;
    Require(sceFontOpenFontFile(library, reinterpret_cast<const char*>(fileFontText.c_str()), 1, nullptr, &fileHandle) == SCE_FONT_OK && fileHandle != nullptr);
    Require(sceFontCloseFont(fileHandle) == SCE_FONT_OK);
    RemovePathAlias_nid_no_patch("/font-test");
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &font) == SCE_FONT_OK && font != nullptr);
    int setting = -1;
    Require(sceFontGetScriptLanguage(font, 3, &setting) == SCE_FONT_ERROR_INVALID_PARAMETER && setting == 0);
    Require(sceFontSetScriptLanguage(font, 3, 7) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontSetScriptLanguage(font, 0x100, 0x106) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontSetScriptLanguage(font, 0x600, 0x603) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontSetScriptLanguage(font, 0x3000, 0x3015) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontGetScriptLanguage(font, 0x100, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontSetScriptLanguage(nullptr, 3, 7) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontGetScriptLanguage(nullptr, 3, nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    for (const auto script : {0x100, 0x600, 0x3000}) {
        bool setThrew = false;
        bool getThrew = false;
        try {
            sceFontSetScriptLanguage(font, script, script);

        } catch (const std::logic_error&) {
            setThrew = true;
        }
        try {
            sceFontGetScriptLanguage(font, script, &setting);

        } catch (const std::logic_error&) {
            getThrew = true;
        }
        Require(setThrew && getThrew && setting == 0);
    }
    Require(sceFontSetTypographicDesign(font, 1, 2) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontSetTypographicDesign(font, 17, 1) == SCE_FONT_ERROR_INVALID_PARAMETER);
    for (const auto selector : {-1, 0, 2, 5, 16, 18, 65536, 65540}) {
        Require(sceFontSetTypographicDesign(font, selector, 0) == SCE_FONT_ERROR_INVALID_PARAMETER);
        setting = 123;
        Require(sceFontGetTypographicDesign(font, selector, &setting) == SCE_FONT_ERROR_INVALID_PARAMETER && setting == 0);
    }
    Require(sceFontGetTypographicDesign(nullptr, 1, &setting) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && setting == 0);
    Require(sceFontGetTypographicDesign(nullptr, 1, nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontSetTypographicDesign(nullptr, 1, 2) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontSetTypographicDesign(font, 65537, 0) == SCE_FONT_ERROR_INVALID_PARAMETER);
    for (const auto selector : {1, 17, 65537, 65538, 65539}) {
        Require(sceFontGetTypographicDesign(font, selector, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
        bool getThrew = false;
        try {
            sceFontGetTypographicDesign(font, selector, &setting);

        } catch (const std::logic_error&) {
            getThrew = true;
        }
        Require(getThrew && setting == 0);
        if (selector == 65537) continue;
        for (const auto feature : {0, selector == 1 ? 1 : 32}) {
            bool setThrew = false;
            try {
                sceFontSetTypographicDesign(font, selector, feature);

            } catch (const std::logic_error&) {
                setThrew = true;
            }
            Require(setThrew);
        }
    }
    std::array<std::byte, 116> typographyRecord{};
    auto* nativeFont = reinterpret_cast<Font::FontHandleNative*>(font);
    nativeFont->open_info.fontset_record = typographyRecord.data();
    for (const auto selector : {65537, 65538, 65539}) {
        Require(sceFontSetTypographicDesign(font, selector, 0) == SCE_FONT_ERROR_NO_SUPPORT_TYPOGRAPHY);
        setting = 123;
        Require(sceFontGetTypographicDesign(font, selector, &setting) == SCE_FONT_ERROR_NO_SUPPORT_TYPOGRAPHY && setting == 0);
        Require(sceFontGetTypographicDesign(font, selector, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    }
    const std::uint32_t typographyCount = 1;
    const std::int32_t typographyFontId = 10;
    std::memcpy(typographyRecord.data() + 32, &typographyCount, sizeof(typographyCount));
    std::memcpy(typographyRecord.data() + 36, &typographyFontId, sizeof(typographyFontId));
    Require(sceFontSetTypographicDesign(font, 65537, 0) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontGetTypographicDesign(font, 65538, &setting) == SCE_FONT_ERROR_NO_SUPPORT_TYPOGRAPHY);
    bool typographyQueryThrew = false;
    try {
        sceFontGetTypographicDesign(font, 65537, &setting);

    } catch (const std::logic_error&) {
        typographyQueryThrew = true;
    }
    Require(typographyQueryThrew && setting == 0);
    nativeFont->open_info.fontset_record = nullptr;
    int attribute = -1;
    Require(sceFontGetAttribute(font, 0x11, &attribute) == SCE_FONT_OK && attribute == 0);
    Require(sceFontDefineAttribute(font, 0x11, &attribute) == SCE_FONT_OK && attribute == 0);
    attribute = -1;
    Require(sceFontGetAttribute(font, 0x99, &attribute) == SCE_FONT_OK && attribute == 0x11);
    Require(sceFontDefineAttribute(font, 0, &attribute) == SCE_FONT_OK && attribute == 0x11);
    Require(sceFontDefineAttribute(font, 5, nullptr) == SCE_FONT_OK);
    attribute = -1;
    Require(sceFontGetAttribute(font, 0, &attribute) == SCE_FONT_OK && attribute == 5);
    Require(sceFontGetAttribute(font, 0x11, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    attribute = -1;
    Require(sceFontDefineAttribute(nullptr, 0x11, &attribute) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && attribute == -1);
    Require(sceFontGetAttribute(nullptr, 0x11, &attribute) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && attribute == -1);
    FontHandle otherFont = nullptr;
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &otherFont) == SCE_FONT_OK && otherFont != nullptr);
    attribute = -1;
    Require(sceFontGetAttribute(otherFont, 0, &attribute) == SCE_FONT_OK && attribute == 0);
    Require(sceFontDefineAttribute(otherFont, 7, &attribute) == SCE_FONT_OK && attribute == 0);
    attribute = -1;
    Require(sceFontGetAttribute(otherFont, 0, &attribute) == SCE_FONT_OK && attribute == 7);
    Require(sceFontGetAttribute(font, 0, &attribute) == SCE_FONT_OK && attribute == 5);
    Require(sceFontCloseFont(otherFont) == SCE_FONT_OK);
    auto* resolutionLibrary = static_cast<Font::FontLibNative*>(library);
    const Font::SysDriver* originalDriver = resolutionLibrary->sys_driver;
    Font::SysDriver wideResolutionDriver = *originalDriver;
    wideResolutionDriver.scale = WideFontResolution;
    resolutionLibrary->sys_driver = &wideResolutionDriver;
    std::uint32_t wideResolution = 0;
    float wideScale = 0;
    Require(sceFontGetFontResolution(font, &wideResolution, &wideScale) == SCE_FONT_OK);
    Require(wideResolution == 0x12345678 && wideScale == 12.5f);
    resolutionLibrary->sys_driver = originalDriver;
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

    const std::vector<unsigned char> kerningData = KerningFont();
    font = nullptr;
    Require(sceFontOpenFontMemory(library, kerningData.data(), static_cast<std::uint32_t>(kerningData.size()), nullptr, &font) == SCE_FONT_OK && font != nullptr);
    Require(sceFontGetFontGlyphsCount(font, &glyphsCount) == SCE_FONT_OK && glyphsCount == 3);
    Require(sceFontGetCharGlyphCode(font, 'A', &glyphCode) == SCE_FONT_OK && glyphCode == 0x80000001u);
    Require(sceFontGetCharGlyphCode(font, 'V', &glyphCode) == SCE_FONT_OK && glyphCode == 0x80000002u);
    Require(sceFontGetCharGlyphCode(nullptr, 0, nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontGetFontGlyphsCount(nullptr, nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontGetCharGlyphCode(font, 0, nullptr) == SCE_FONT_ERROR_NO_SUPPORT_CODE);
    Require(sceFontGetCharGlyphCode(font, 0x80000000u, &glyphCode) == SCE_FONT_OK && glyphCode == 0x80000000u);
    Require(sceFontGetCharGlyphCode(font, 0x80000002u, &glyphCode) == SCE_FONT_OK && glyphCode == 0x80000002u);
    Require(sceFontGetCharGlyphCode(font, 0xc0000001u, &glyphCode) == SCE_FONT_OK && glyphCode == 0x80000001u);
    Require(sceFontGetCharGlyphCode(font, 0x80000003u, &glyphCode) == SCE_FONT_ERROR_NO_SUPPORT_GLYPH && glyphCode == 0);
    Require(sceFontGetCharGlyphCode(font, 0x80010001u, &glyphCode) == SCE_FONT_ERROR_NO_SUPPORT_CODE && glyphCode == 0);
    glyphCode = 1;
    Require(sceFontGetCharGlyphCode(font, 'B', &glyphCode) == SCE_FONT_ERROR_NO_SUPPORT_GLYPH && glyphCode == 0);
    glyphCode = 1;
    Require(sceFontGetCharGlyphCode(font, 0, &glyphCode) == SCE_FONT_ERROR_NO_SUPPORT_CODE && glyphCode == 0);
    Require(sceFontSetResolutionDpi(font, 144, 144) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    FontGlyph glyph = nullptr;
    Require(sceFontGenerateCharGlyph(font, 'A', nullptr, &glyph) == SCE_FONT_OK && glyph != nullptr);
    const auto originalFlags = glyph->flags;
    std::int32_t previousAttribute = -1;
    Require(sceFontGlyphDefineAttribute(glyph, 0x11, &previousAttribute) == SCE_FONT_OK);
    Require(previousAttribute == ((originalFlags & 2) ? 0x11 : 0x10));
    Require(glyph->flags == (originalFlags | 2));
    Require(sceFontGlyphDefineAttribute(glyph, 0x10, &previousAttribute) == SCE_FONT_OK && previousAttribute == 0x11);
    Require(glyph->flags == (originalFlags & ~2u));
    Require(sceFontGlyphDefineAttribute(glyph, 0x10, &previousAttribute) == SCE_FONT_OK && previousAttribute == 0x10);
    Require(sceFontGlyphDefineAttribute(glyph, 0x11, nullptr) == SCE_FONT_OK);
    for (std::int32_t attribute = -1; attribute <= 256; ++attribute) {
        if (attribute == 0x10 || attribute == 0x11) continue;
        previousAttribute = -1;
        Require(sceFontGlyphDefineAttribute(glyph, attribute, &previousAttribute) == SCE_FONT_ERROR_INVALID_PARAMETER);
        Require(previousAttribute == 0 && glyph->flags == (originalFlags | 2));
    }
    previousAttribute = -1;
    Require(sceFontGlyphDefineAttribute(nullptr, 0x11, &previousAttribute) == SCE_FONT_ERROR_INVALID_GLYPH && previousAttribute == 0);
    Require(sceFontGlyphDefineAttribute(nullptr, 0x11, nullptr) == SCE_FONT_ERROR_INVALID_GLYPH);
    FontGlyphOpaque notAGlyph{};
    previousAttribute = -1;
    Require(sceFontGlyphDefineAttribute(&notAGlyph, 0x11, &previousAttribute) == SCE_FONT_ERROR_INVALID_GLYPH && previousAttribute == 0);
    int glyphAttribute = -1;
    Require(sceFontGlyphGetAttribute(glyph, 0x11, &glyphAttribute) == SCE_FONT_OK && glyphAttribute == 0x11);
    Require(sceFontGlyphGetAttribute(glyph, 0x11, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontGlyphGetAttribute(nullptr, 0x11, &glyphAttribute) == SCE_FONT_ERROR_INVALID_GLYPH);
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

    CheckPointAndDpiScale(font, library, &memory, renderer);

    ParsedText parsed{"AV", 0, font};
    FontTextSource source{};
    Require(sceFontTextSourceInit(&source, parsed.text, 2, ParseText, &parsed) == SCE_FONT_OK);
    FontString string = nullptr;
    Require(sceFontCreateString(&memory, &source, nullptr, &string) == SCE_FONT_OK && string != nullptr);
    std::uint32_t characterCount = 0;
    FontTextCharacter* characters = sceFontStringRefersTextCharacters(string, &characterCount);
    Require(characters != nullptr && characterCount == 2);
    FontWriting writing{};
    Require(sceFontWritingInit(&writing, string, characters) == SCE_FONT_OK);
    constexpr std::uint64_t guard = 0x5a5a5a5a5a5a5a5aull;
    struct {
        const FontWritingLetterStep* letter;
        std::uint64_t guard;
    } out{reinterpret_cast<const FontWritingLetterStep*>(&memory), guard};
    Require(sceFontWritingRefersRenderStepCharacter(&writing, &out.letter) == nullptr && out.letter == nullptr && out.guard == guard);
    const FontWritingStep* step = sceFontWritingRefersRenderStep(&writing);
    Require(step != nullptr);
    Require(sceFontWritingRefersRenderStepCharacter(&writing, &out.letter) == characters && out.letter != nullptr && out.guard == guard);
    Require(out.letter->x == step->x && out.letter->advanceX == step->advanceX && out.letter->Components.textsIndex == 0 && out.letter->Components.characterTextCount == 1);
    step = sceFontWritingRefersRenderStep(&writing);
    Require(step != nullptr);
    Require(sceFontWritingRefersRenderStepCharacter(&writing, &out.letter) == characters + 1 && out.guard == guard);
    Require(out.letter->x == step->x && out.letter->Components.textsIndex == 1 && out.letter->Components.glyphsIndex == 1);
    Require(sceFontWritingRefersRenderStepCharacter(&writing, nullptr) == characters + 1);
    Require(sceFontDestroyString(&string) == SCE_FONT_OK);
    CheckTextCodes(&memory, font);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);

    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK && renderer == nullptr);
    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_ERROR_INVALID_RENDERER);

    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK && library == nullptr);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_ERROR_INVALID_MEMORY);
    FontTextCharacter character{};
    character.characterCode = 'A';
    int syllableState = -1;
    Require(sceFontCharacterGetSyllableStringState(&character, &syllableState) == SCE_FONT_OK && syllableState == 0);
    syllableState = -1;
    Require(sceFontCharacterGetSyllableStringState(nullptr, &syllableState) == SCE_FONT_ERROR_INVALID_PARAMETER && syllableState == 0);
    Require(sceFontCharacterGetSyllableStringState(&character, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    std::filesystem::remove_all(fontRoot);
}
