#pragma once

#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H

enum SceFontScriptLanguage : uint32_t {
    SCE_FONT_SCRIPT_LANGUAGE_DEFAULT = 0,
    SCE_FONT_SCRIPT_LANGUAGE_JAPANESE = 1,
    SCE_FONT_SCRIPT_LANGUAGE_LATIN = 2,
    SCE_FONT_SCRIPT_LANGUAGE_KOREAN = 3,
    SCE_FONT_SCRIPT_LANGUAGE_CHINESE_TRADITIONAL = 4,
    SCE_FONT_SCRIPT_LANGUAGE_CHINESE_SIMPLIFIED = 5,
    SCE_FONT_SCRIPT_LANGUAGE_ARABIC = 6,
    SCE_FONT_SCRIPT_LANGUAGE_CYRILLIC = 7,

}

struct SceFontLibrary {
    FT_Face face;      

    uint32_t scriptLanguage;
};
