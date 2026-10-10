#include "Font.h"
#include <stdexcept>
#include <string>


static bool FontSupportsScript(FT_Face face, SceFontScriptLanguage script) {
    if (!face) {
        return false;
    }


    switch (script) {
        case SCE_FONT_SCRIPT_LANGUAGE_JAPANESE:

            return FT_Get_Char_Index(face, 0x3042) != 0 && 
                   FT_Get_Char_Index(face, 0x30A2) != 0;
                   
        case SCE_FONT_SCRIPT_LANGUAGE_KOREAN:

            return FT_Get_Char_Index(face, 0xAC00) != 0;
            
        case SCE_FONT_SCRIPT_LANGUAGE_CHINESE_SIMPLIFIED:

            return FT_Get_Char_Index(face, 0x4F60) != 0 && 
                   FT_Get_Char_Index(face, 0x597D) != 0;
                   
        case SCE_FONT_SCRIPT_LANGUAGE_CHINESE_TRADITIONAL:

            return FT_Get_Char_Index(face, 0x4F60) != 0 && 
                   FT_Get_Char_Index(face, 0x597D) != 0;
                   
        case SCE_FONT_SCRIPT_LANGUAGE_ARABIC:

            return FT_Get_Char_Index(face, 0x0627) != 0 && 
                   FT_Get_Char_Index(face, 0x0628) != 0;
                   
        case SCE_FONT_SCRIPT_LANGUAGE_CYRILLIC:

            return FT_Get_Char_Index(face, 0x0410) != 0 && 
                   FT_Get_Char_Index(face, 0x0411) != 0;
                   
        case SCE_FONT_SCRIPT_LANGUAGE_LATIN:
        case SCE_FONT_SCRIPT_LANGUAGE_DEFAULT:
        default:

            return FT_Get_Char_Index(face, 0x0041) != 0 && 
                   FT_Get_Char_Index(face, 0x0061) != 0;
    }
}

int APS5_VABI sceFontGetScriptLanguage(SceFontLibrary* lib, uint32_t* language) {

    if (!lib || !lib->face || !language) {
        throw std::runtime_error("sceFontGetScriptLanguage: invalid argument");
    }

    if (lib->scriptLanguage != 0) {
        *language = lib->scriptLanguage;
        return 0;
    }
    const SceFontScriptLanguage priorityLanguages[] = {
        SCE_FONT_SCRIPT_LANGUAGE_JAPANESE,
        SCE_FONT_SCRIPT_LANGUAGE_KOREAN,
        SCE_FONT_SCRIPT_LANGUAGE_CHINESE_SIMPLIFIED,
        SCE_FONT_SCRIPT_LANGUAGE_CHINESE_TRADITIONAL,
        SCE_FONT_SCRIPT_LANGUAGE_ARABIC,
        SCE_FONT_SCRIPT_LANGUAGE_CYRILLIC,
        SCE_FONT_SCRIPT_LANGUAGE_LATIN
    };

    for (const auto& script : priorityLanguages) {
        if (FontSupportsScript(lib->face, script)) {
            lib->scriptLanguage = script;
            *language = script;
            return 0;
        }
    }

    lib->scriptLanguage = SCE_FONT_SCRIPT_LANGUAGE_DEFAULT;
    *language = SCE_FONT_SCRIPT_LANGUAGE_DEFAULT;
    return 0;
}
