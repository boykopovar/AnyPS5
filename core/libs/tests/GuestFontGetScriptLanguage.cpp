#include "prx/libSceFont/include/FontInternal.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceFontGetScriptLanguage(FontHandle fontHandle, std::int32_t fontScript, std::int32_t* fontLanguage);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font script language check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename F>
static bool Throws(F f) {
    try {
        f();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    Font::FontHandleNative native{};
    native.magic = Font::HANDLE_MAGIC;
    const FontHandle font = reinterpret_cast<FontHandle>(&native);
    Font::FontState& state = Font::ResetState(font);
    state.scriptLanguages[0x0000] = 0x0000;
    state.scriptLanguages[0x0100] = 0x0102;
    state.scriptLanguages[0x3000] = 0x3041;

    std::int32_t language = -1;
    Require(sceFontGetScriptLanguage(font, 0x0100, &language) == SCE_FONT_OK && language == 0x0102);
    Require(sceFontGetScriptLanguage(font, 0x0000, &language) == SCE_FONT_OK && language == 0x0000);
    Require(sceFontGetScriptLanguage(font, 0x3000, &language) == SCE_FONT_OK && language == 0x3041);

    language = -1;
    Require(sceFontGetScriptLanguage(font, 0x0600, &language) == SCE_FONT_ERROR_UNSET_PARAMETER && language == 0);

    Require(Throws([&] { sceFontGetScriptLanguage(font, 0x0200, &language); }));
    Require(Throws([&] { sceFontGetScriptLanguage(font, 0x0106, &language); }));

    Require(sceFontGetScriptLanguage(font, 0x0100, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);

    language = -1;
    Require(sceFontGetScriptLanguage(nullptr, 0x0100, &language) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && language == 0);
    Font::FontHandleNative withoutMagic{};
    language = -1;
    Require(sceFontGetScriptLanguage(reinterpret_cast<FontHandle>(&withoutMagic), 0x0100, &language) ==
            SCE_FONT_ERROR_INVALID_FONT_HANDLE && language == 0);

    Font::RemoveState(font);
    return 0;
}