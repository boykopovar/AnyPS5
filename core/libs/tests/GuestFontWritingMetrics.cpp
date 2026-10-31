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
        std::fprintf(stderr, "Font writing metrics check failed at line %d\n", line);
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

struct ParsedCode {
    std::uint32_t code;
    FontHandle font;
    bool done;
};

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

static std::int32_t APS5_VABI ParseCode(FontTextSource* source, void** order, FontTextParseResult* result) {
    auto* parsed = static_cast<ParsedCode*>(source->textObject);
    *order = parsed;
    if (parsed->done) {
        result->Terminate.terminateCode = 0;
        return 0;
    }
    parsed->done = true;
    result->FontCode.font = parsed->font;
    result->FontCode.code = parsed->code;
    return 1;
}

static bool SameMetrics(const FontWritingMetrics& left, const FontWritingMetrics& right) {
    return left.advanceX == right.advanceX && left.advanceY == right.advanceY && left.Extent.top == right.Extent.top &&
           left.Extent.bottom == right.Extent.bottom && left.Extent.left == right.Extent.left && left.Extent.right == right.Extent.right;
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 1, 0x52) == SCE_FONT_OK);
    const std::vector<unsigned char> fontData = FontFixture::BitmapFont();
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &font) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);

    ParsedText parsed{"AA", 0, font};
    FontTextSource source{};
    Require(sceFontTextSourceInit(&source, parsed.text, 2, ParseText, &parsed) == SCE_FONT_OK);
    FontString string = nullptr;
    Require(sceFontCreateString(&memory, &source, nullptr, &string) == SCE_FONT_OK);
    std::uint32_t count = 0;
    FontTextCharacter* characters = sceFontStringRefersTextCharacters(string, &count);
    Require(characters != nullptr && count == 2);

    FontWriting writing{};
    Require(sceFontWritingInit(&writing, string, characters) == SCE_FONT_OK);
    FontWritingMetrics metrics{1.0f, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
    Require(sceFontWritingGetRenderMetrics(&writing, &metrics) == SCE_FONT_OK && SameMetrics(metrics, {}));
    Require(sceFontWritingGetRenderMetrics(&writing, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    metrics = {1.0f, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
    Require(sceFontWritingGetRenderMetrics(nullptr, &metrics) == SCE_FONT_ERROR_INVALID_PARAMETER && SameMetrics(metrics, {}));
    FontWriting notWriting{};
    metrics = {1.0f, 1.0f, {1.0f, 1.0f, 1.0f, 1.0f}};
    Require(sceFontWritingGetRenderMetrics(&notWriting, &metrics) == SCE_FONT_ERROR_INVALID_WRITING && SameMetrics(metrics, {}));

    Require(sceFontWritingRefersRenderStep(&writing) != nullptr);
    Require(sceFontWritingGetRenderMetrics(&writing, &metrics) == SCE_FONT_OK);
    Require(metrics.advanceX > 0.0f && metrics.Extent.right > metrics.Extent.left);
    const FontWritingMetrics first = metrics;
    Require(sceFontWritingRefersRenderStep(&writing) != nullptr);
    Require(sceFontWritingGetRenderMetrics(&writing, &metrics) == SCE_FONT_OK);
    Require(metrics.advanceX > first.advanceX);
    Require(metrics.Extent.left <= first.Extent.left && metrics.Extent.right >= first.Extent.right);
    Require(metrics.Extent.top <= first.Extent.top && metrics.Extent.bottom >= first.Extent.bottom);
    Require(sceFontWritingRefersRenderStep(&writing) == nullptr);

    Require(sceFontWritingSetMaskInvisible(nullptr, 0) == SCE_FONT_ERROR_INVALID_WRITING);
    Require(sceFontWritingSetMaskInvisible(&notWriting, 0) == SCE_FONT_ERROR_INVALID_WRITING);
    Require(sceFontWritingSetMaskInvisible(&writing, 0) == SCE_FONT_OK);
    Require(sceFontWritingSetMaskInvisible(&writing, 1) == SCE_FONT_OK);
    Require(sceFontWritingSetMaskInvisible(&writing, 2) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontWritingSetMaskInvisible(&writing, -1) == SCE_FONT_ERROR_INVALID_PARAMETER);

    Require(sceFontWritingInit(&writing, string, characters) == SCE_FONT_OK);
    Require(sceFontWritingGetRenderMetrics(&writing, &metrics) == SCE_FONT_OK && SameMetrics(metrics, {}));
    Require(sceFontWritingSetMaskInvisible(&writing, 1) == SCE_FONT_OK);
    Require(sceFontWritingRefersRenderStep(&writing) != nullptr);
    Require(sceFontWritingGetRenderMetrics(&writing, &metrics) == SCE_FONT_OK && SameMetrics(metrics, first));
    Require(sceFontWritingSetMaskInvisible(&writing, 0) == SCE_FONT_OK);

    ParsedCode formatCode{0x200B, font, false};
    FontTextSource formatSource{};
    FontString formatString = nullptr;
    Require(sceFontTextSourceInit(&formatSource, &formatCode, 1, ParseCode, &formatCode) == SCE_FONT_OK);
    Require(sceFontCreateString(&memory, &formatSource, nullptr, &formatString) == SCE_FONT_OK);
    std::uint32_t formatCount = 0;
    FontTextCharacter* formatCharacters = sceFontStringRefersTextCharacters(formatString, &formatCount);
    Require(formatCharacters != nullptr && formatCount == 1);
    FontWriting formatWriting{};
    Require(sceFontWritingInit(&formatWriting, formatString, formatCharacters) == SCE_FONT_OK);
    const FontWritingStep* formatStep = sceFontWritingRefersRenderStep(&formatWriting);
    Require(formatStep != nullptr && formatStep->Profile.invisibleGlyph == 1);
    Require(sceFontWritingInit(&formatWriting, formatString, formatCharacters) == SCE_FONT_OK);
    Require(sceFontWritingSetMaskInvisible(&formatWriting, 1) == SCE_FONT_OK);
    formatStep = sceFontWritingRefersRenderStep(&formatWriting);
    Require(formatStep != nullptr && formatStep->Profile.invisibleGlyph == 0);
    Require(sceFontDestroyString(&formatString) == SCE_FONT_OK);

    Require(sceFontDestroyString(&string) == SCE_FONT_OK);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
}
