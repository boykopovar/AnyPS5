#pragma once

#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H

// تعداد رموز اللغات كما هي معروفة في PS4 SDK
enum SceFontScriptLanguage : uint32_t {
    SCE_FONT_SCRIPT_LANGUAGE_DEFAULT = 0,
    SCE_FONT_SCRIPT_LANGUAGE_JAPANESE = 1,
    SCE_FONT_SCRIPT_LANGUAGE_LATIN = 2,
    SCE_FONT_SCRIPT_LANGUAGE_KOREAN = 3,
    SCE_FONT_SCRIPT_LANGUAGE_CHINESE_TRADITIONAL = 4,
    SCE_FONT_SCRIPT_LANGUAGE_CHINESE_SIMPLIFIED = 5,
    SCE_FONT_SCRIPT_LANGUAGE_ARABIC = 6,
    SCE_FONT_SCRIPT_LANGUAGE_CYRILLIC = 7,
    // ... أضف باقي القيم حسب الحاجة
};

// هيكل مكتبة الخط المستخدم في المشروع
struct SceFontLibrary {
    FT_Face face;          // مؤشر FreeType للخط
    // يمكن إضافة أعضاء أخرى هنا حسب حاجة المشروع
    uint32_t scriptLanguage; // تخزين اللغة المكتشفة
};
