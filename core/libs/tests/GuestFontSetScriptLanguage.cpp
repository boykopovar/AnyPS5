#include "prx/libSceFont/include/FontInternal.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceFontSetScriptLanguage(FontHandle fontHandle, std::int32_t fontScript, std::int32_t fontLanguage);
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

    Require(sceFontSetScriptLanguage(font, 0x0000, 0x0000) == SCE_FONT_OK);
    Require(sceFontSetScriptLanguage(font, 0x0100, 0x0100) == SCE_FONT_OK);
    Require(sceFontSetScriptLanguage(font, 0x0100, 0x0102) == SCE_FONT_OK);
    Require(sceFontSetScriptLanguage(font, 0x0600, 0x0601) == SCE_FONT_OK);
    Require(sceFontSetScriptLanguage(font, 0x3000, 0x3011) == SCE_FONT_OK);
    Require(sceFontSetScriptLanguage(font, 0x3000, 0x3041) == SCE_FONT_OK);
    Require(state.scriptLanguages.size() == 4);
    Require(state.scriptLanguages.at(0x0000) == 0x0000);
    Require(state.scriptLanguages.at(0x0100) == 0x0102);
    Require(state.scriptLanguages.at(0x0600) == 0x0601);
    Require(state.scriptLanguages.at(0x3000) == 0x3041);

    Require(Throws([&] { sceFontSetScriptLanguage(font, 0x0200, 0x0200); }));
    Require(Throws([&] { sceFontSetScriptLanguage(font, 0x0100, 0x0106); }));
    Require(Throws([&] { sceFontSetScriptLanguage(font, 0x0100, 0x3011); }));
    Require(Throws([&] { sceFontSetScriptLanguage(font, 0x0000, 0x0100); }));
    Require(Throws([&] { sceFontSetScriptLanguage(font, 0x0100, 0x0000); }));
    Require(state.scriptLanguages.size() == 4);

    Require(sceFontSetScriptLanguage(nullptr, 0x0100, 0x0100) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Font::FontHandleNative withoutMagic{};
    Require(sceFontSetScriptLanguage(reinterpret_cast<FontHandle>(&withoutMagic), 0x0100, 0x0100) ==
            SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(state.scriptLanguages.size() == 4);

    Font::RemoveState(font);
    return 0;
}