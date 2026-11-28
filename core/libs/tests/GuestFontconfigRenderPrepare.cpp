#include "prx/libfontconfig/libfontconfig.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

static void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "Fontconfig render prepare check failed at line %d\n", line);
    std::abort();
}

#define CHECK(value) Require((value), __LINE__)

static const FcChar8* Text(const char* text) {
    return reinterpret_cast<const FcChar8*>(text);
}

static bool StringAt(const FcPattern* pattern, const char* object, int id, const char* expected) {
    FcChar8* value = nullptr;
    return FcPatternGetString_nid_postfix(pattern, object, id, &value) == FcResultMatch &&
        std::strcmp(reinterpret_cast<const char*>(value), expected) == 0;
}

static bool DoubleAt(const FcPattern* pattern, const char* object, double expected) {
    FcValue value{};
    return FcPatternGet_nid_postfix(pattern, object, 0, &value) == FcResultMatch && value.type == FcTypeDouble && value.u.d == expected;
}

static bool Single(const FcPattern* pattern, const char* object) {
    FcValue value{};
    return FcPatternGet_nid_postfix(pattern, object, 0, &value) == FcResultMatch && FcPatternGet_nid_postfix(pattern, object, 1, &value) == FcResultNoId;
}

static bool Absent(const FcPattern* pattern, const char* object) {
    FcValue value{};
    return FcPatternGet_nid_postfix(pattern, object, 0, &value) == FcResultNoMatch;
}

static void ChosenValues() {
    auto* pattern = FcPatternCreate();
    auto* font = FcPatternCreate();
    CHECK(pattern && font);
    CHECK(FcPatternAddString(pattern, "family", Text("bit stream VERA")));
    CHECK(FcPatternAddString(pattern, "family", Text("Foo")));
    CHECK(FcPatternAddString(font, "family", Text("Bitstream Vera")));
    CHECK(FcPatternAddString(font, "family", Text("Foo")));
    CHECK(FcPatternAddString(pattern, "style", Text("bold")));
    CHECK(FcPatternAddString(font, "style", Text("Regular")));
    CHECK(FcPatternAddString(font, "style", Text("Bold")));
    CHECK(FcPatternAddDouble(pattern, "pixelsize", 16));
    CHECK(FcPatternAddInteger_nid_postfix(font, "pixelsize", 10));
    CHECK(FcPatternAddInteger_nid_postfix(font, "pixelsize", 16));
    CHECK(FcPatternAddInteger_nid_postfix(font, "pixelsize", 20));
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "weight", 200));
    CHECK(FcPatternAddInteger_nid_postfix(font, "weight", 80));
    CHECK(FcPatternAddInteger_nid_postfix(font, "weight", 210));
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "width", 75));
    CHECK(FcPatternAddDouble(font, "width", 100));
    CHECK(FcPatternAddDouble(pattern, "size", 12));
    CHECK(FcPatternAddDouble(font, "size", 10));
    CHECK(FcPatternAddBool_nid_postfix(pattern, "antialias", 1));
    CHECK(FcPatternAddBool_nid_postfix(font, "antialias", 2));
    CHECK(FcPatternAddBool_nid_postfix(pattern, "outline", 1));
    CHECK(FcPatternAddBool_nid_postfix(font, "outline", 0));
    CHECK(FcPatternAddString(pattern, "file", Text("/fonts/*.ttf")));
    CHECK(FcPatternAddString(font, "file", Text("/other/x.otf")));
    CHECK(FcPatternAddString(font, "file", Text("/fonts/a.ttf")));
    CHECK(FcPatternAddString(pattern, "postscriptname", Text("DejaVuSans-Bold")));
    CHECK(FcPatternAddString(font, "postscriptname", Text("Arial")));
    CHECK(FcPatternAddString(font, "postscriptname", Text("DejaVu Sans Bold")));
    CHECK(FcPatternAddString(pattern, "fullname", Text("Ignored")));
    CHECK(FcPatternAddString(font, "fullname", Text("Kept")));
    CHECK(FcPatternAddString(font, "fullname", Text("Second")));
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "index", 3));
    CHECK(FcPatternAddInteger_nid_postfix(font, "index", 0));
    CHECK(FcPatternAddInteger_nid_postfix(font, "index", 1));

    auto* prepared = FcFontRenderPrepare(nullptr, pattern, font);
    CHECK(prepared && prepared != pattern && prepared != font);
    CHECK(Single(prepared, "family") && StringAt(prepared, "family", 0, "Bitstream Vera"));
    CHECK(Single(prepared, "style") && StringAt(prepared, "style", 0, "Bold"));
    int integer = 0;
    CHECK(Single(prepared, "pixelsize") && FcPatternGetInteger_nid_postfix(prepared, "pixelsize", 0, &integer) == FcResultMatch && integer == 16);
    CHECK(Single(prepared, "weight") && DoubleAt(prepared, "weight", 210));
    CHECK(DoubleAt(prepared, "width", 100));
    CHECK(DoubleAt(prepared, "size", 12));
    FcBool flag = 0;
    CHECK(FcPatternGetBool_nid_postfix(prepared, "antialias", 0, &flag) == FcResultMatch && flag == 1);
    CHECK(FcPatternGetBool_nid_postfix(prepared, "outline", 0, &flag) == FcResultMatch && flag == 0);
    CHECK(Single(prepared, "file") && StringAt(prepared, "file", 0, "/fonts/a.ttf"));
    CHECK(Single(prepared, "postscriptname") && StringAt(prepared, "postscriptname", 0, "DejaVu Sans Bold"));
    CHECK(Single(prepared, "fullname") && StringAt(prepared, "fullname", 0, "Kept"));
    CHECK(Single(prepared, "index") && FcPatternGetInteger_nid_postfix(prepared, "index", 0, &integer) == FcResultMatch && integer == 0);
    CHECK(StringAt(font, "family", 1, "Foo") && StringAt(pattern, "family", 1, "Foo"));
    FcPatternDestroy_nid_postfix(prepared);
    FcPatternDestroy_nid_postfix(font);
    FcPatternDestroy_nid_postfix(pattern);
}

static void CopiedObjects() {
    auto* pattern = FcPatternCreate();
    auto* font = FcPatternCreate();
    CHECK(pattern && font);
    CHECK(FcPatternAddString(font, "family", Text("Only Font")));
    CHECK(FcPatternAddString(font, "family", Text("Alias")));
    CHECK(FcPatternAddInteger_nid_postfix(font, "spacing", 100));
    CHECK(FcPatternAddString(font, "custom", Text("font")));
    CHECK(FcPatternAddString(font, "stylelang", Text("en")));
    CHECK(FcPatternAddDouble(pattern, "dpi", 96));
    CHECK(FcPatternAddString(pattern, "custom", Text("pattern")));
    CHECK(FcPatternAddString(pattern, "custom", Text("second")));
    CHECK(FcPatternAddString(pattern, "familylang", Text("de")));
    CHECK(FcPatternAddString(pattern, "fullnamelang", Text("de")));
    CHECK(FcPatternAddString(pattern, "prgname", Text("app")));

    auto* prepared = FcFontRenderPrepare(nullptr, pattern, font);
    CHECK(prepared != nullptr);
    CHECK(StringAt(prepared, "family", 0, "Only Font") && StringAt(prepared, "family", 1, "Alias"));
    int spacing = 0;
    CHECK(FcPatternGetInteger_nid_postfix(prepared, "spacing", 0, &spacing) == FcResultMatch && spacing == 100);
    CHECK(Single(prepared, "custom") && StringAt(prepared, "custom", 0, "font"));
    CHECK(DoubleAt(prepared, "dpi", 96));
    CHECK(StringAt(prepared, "prgname", 0, "app"));
    CHECK(Absent(prepared, "stylelang"));
    CHECK(Absent(prepared, "familylang"));
    CHECK(Absent(prepared, "fullnamelang"));
    FcPatternDestroy_nid_postfix(prepared);

    auto* empty = FcPatternCreate();
    CHECK(empty != nullptr);
    prepared = FcFontRenderPrepare(nullptr, empty, empty);
    CHECK(prepared && prepared != empty);
    CHECK(Absent(prepared, "family"));
    FcPatternDestroy_nid_postfix(prepared);
    FcPatternDestroy_nid_postfix(empty);
    FcPatternDestroy_nid_postfix(font);
    FcPatternDestroy_nid_postfix(pattern);
}

static void NameLanguages() {
    auto* pattern = FcPatternCreate();
    auto* font = FcPatternCreate();
    CHECK(pattern && font);
    CHECK(FcPatternAddString(font, "family", Text("Nimbus Sans")));
    CHECK(FcPatternAddString(font, "family", Text("Nimbus Sans JA")));
    CHECK(FcPatternAddString(font, "family", Text("Nimbus Sans Extra")));
    CHECK(FcPatternAddString(font, "familylang", Text("en")));
    CHECK(FcPatternAddString(font, "familylang", Text("ja")));
    CHECK(FcPatternAddString(pattern, "familylang", Text("ja-JP")));
    CHECK(FcPatternAddString(pattern, "family", Text("Nimbus Sans")));
    CHECK(FcPatternAddString(font, "style", Text("Regular")));
    CHECK(FcPatternAddString(font, "stylelang", Text("en")));
    CHECK(FcPatternAddString(pattern, "style", Text("Bold")));

    auto* prepared = FcFontRenderPrepare(nullptr, pattern, font);
    CHECK(prepared != nullptr);
    CHECK(StringAt(prepared, "family", 0, "Nimbus Sans JA"));
    CHECK(StringAt(prepared, "family", 1, "Nimbus Sans"));
    CHECK(StringAt(prepared, "family", 2, "Nimbus Sans Extra"));
    CHECK(StringAt(prepared, "familylang", 0, "ja"));
    CHECK(StringAt(prepared, "familylang", 1, "en"));
    FcValue value{};
    CHECK(FcPatternGet_nid_postfix(prepared, "familylang", 2, &value) == FcResultNoId);
    CHECK(Single(prepared, "style") && StringAt(prepared, "style", 0, "Regular"));
    CHECK(Single(prepared, "stylelang") && StringAt(prepared, "stylelang", 0, "en"));
    FcPatternDestroy_nid_postfix(prepared);
    FcPatternDestroy_nid_postfix(font);
    FcPatternDestroy_nid_postfix(pattern);
}

static void SetValues() {
    auto* pattern = FcPatternCreate();
    auto* font = FcPatternCreate();
    auto* wanted = FcCharSetCreate();
    auto* first = FcCharSetCreate();
    auto* second = FcCharSetCreate();
    auto* requested = FcLangSetCreate();
    auto* english = FcLangSetCreate();
    auto* chinese = FcLangSetCreate();
    CHECK(pattern && font && wanted && first && second && requested && english && chinese);
    CHECK(FcCharSetAddChar(wanted, 'a') && FcCharSetAddChar(wanted, 'b'));
    CHECK(FcCharSetAddChar(first, 'a'));
    CHECK(FcCharSetAddChar(second, 'a') && FcCharSetAddChar(second, 'b'));
    CHECK(FcPatternAddCharSet(pattern, "charset", wanted));
    CHECK(FcPatternAddCharSet(font, "charset", first));
    CHECK(FcPatternAddCharSet(font, "charset", second));
    CHECK(FcLangSetAdd(requested, Text("zh-tw")));
    CHECK(FcLangSetAdd(english, Text("en")));
    CHECK(FcLangSetAdd(chinese, Text("zh-cn")));
    CHECK(FcPatternAddLangSet(pattern, "lang", requested));
    CHECK(FcPatternAddLangSet(font, "lang", english));
    CHECK(FcPatternAddLangSet(font, "lang", chinese));

    auto* prepared = FcFontRenderPrepare(nullptr, pattern, font);
    CHECK(prepared != nullptr);
    FcCharSet* charset = nullptr;
    CHECK(Single(prepared, "charset") && FcPatternGetCharSet(prepared, "charset", 0, &charset) == FcResultMatch && charset == second);
    FcLangSet* languages = nullptr;
    CHECK(Single(prepared, "lang") && FcPatternGetLangSet(prepared, "lang", 0, &languages) == FcResultMatch);
    CHECK(FcLangSetHasLang(languages, Text("zh-cn")) == FcLangEqual);
    CHECK(FcLangSetHasLang(languages, Text("en")) == FcLangDifferentLang);
    FcPatternDestroy_nid_postfix(prepared);

    CHECK(FcPatternDel_nid_postfix(pattern, "lang"));
    CHECK(FcPatternAddString(pattern, "lang", Text("EN-gb")));
    prepared = FcFontRenderPrepare(nullptr, pattern, font);
    CHECK(prepared != nullptr);
    CHECK(FcPatternGetLangSet(prepared, "lang", 0, &languages) == FcResultMatch);
    CHECK(FcLangSetHasLang(languages, Text("en")) == FcLangEqual);
    FcPatternDestroy_nid_postfix(prepared);

    auto* privateTags = FcLangSetCreate();
    CHECK(privateTags && FcLangSetAdd(privateTags, Text("x-custom")));
    CHECK(FcPatternDel_nid_postfix(pattern, "lang"));
    CHECK(FcPatternAddLangSet(pattern, "lang", privateTags));
    CHECK(FcPatternDel_nid_postfix(font, "lang"));
    CHECK(FcPatternAddLangSet(font, "lang", english));
    CHECK(FcPatternAddString(font, "lang", Text("X-CUSTOM")));
    prepared = FcFontRenderPrepare(nullptr, pattern, font);
    CHECK(prepared != nullptr);
    FcChar8* tag = nullptr;
    CHECK(FcPatternGetString_nid_postfix(prepared, "lang", 0, &tag) == FcResultMatch && std::strcmp(reinterpret_cast<const char*>(tag), "X-CUSTOM") == 0);
    FcPatternDestroy_nid_postfix(prepared);

    FcLangSetDestroy(privateTags);
    FcLangSetDestroy(chinese);
    FcLangSetDestroy(english);
    FcLangSetDestroy(requested);
    FcCharSetDestroy(second);
    FcCharSetDestroy(first);
    FcCharSetDestroy(wanted);
    FcPatternDestroy_nid_postfix(font);
    FcPatternDestroy_nid_postfix(pattern);
}

static void UndefinedScores() {
    auto* pattern = FcPatternCreate();
    auto* font = FcPatternCreate();
    CHECK(pattern && font);
    CHECK(FcPatternAddString(pattern, "postscriptname", Text("")));
    CHECK(FcPatternAddString(font, "postscriptname", Text("")));
    CHECK(FcPatternAddDouble(pattern, "pixelsize", __builtin_nan("")));
    CHECK(FcPatternAddInteger_nid_postfix(font, "pixelsize", 10));
    CHECK(FcPatternAddString(pattern, "style", Text("")));
    CHECK(FcPatternAddString(font, "style", Text("")));

    auto* prepared = FcFontRenderPrepare(nullptr, pattern, font);
    CHECK(prepared != nullptr);
    CHECK(Single(prepared, "postscriptname") && StringAt(prepared, "postscriptname", 0, ""));
    int pixels = 0;
    CHECK(Single(prepared, "pixelsize") && FcPatternGetInteger_nid_postfix(prepared, "pixelsize", 0, &pixels) == FcResultMatch && pixels == 10);
    CHECK(StringAt(prepared, "style", 0, ""));
    FcPatternDestroy_nid_postfix(prepared);

    CHECK(FcPatternAddString(font, "postscriptname", Text("Xyz")));
    prepared = FcFontRenderPrepare(nullptr, pattern, font);
    CHECK(prepared != nullptr);
    CHECK(Single(prepared, "postscriptname") && StringAt(prepared, "postscriptname", 0, "Xyz"));
    FcPatternDestroy_nid_postfix(prepared);
    FcPatternDestroy_nid_postfix(font);
    FcPatternDestroy_nid_postfix(pattern);
}

static bool Throws(FcPattern* pattern, FcPattern* font) {
    try {
        FcFontRenderPrepare(nullptr, pattern, font);
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static void NullArguments() {
    auto* pattern = FcPatternCreate();
    CHECK(pattern != nullptr);
    CHECK(Throws(nullptr, pattern));
    CHECK(Throws(pattern, nullptr));
    FcPatternDestroy_nid_postfix(pattern);
}

int main() {
    ChosenValues();
    CopiedObjects();
    NameLanguages();
    SetValues();
    UndefinedScores();
    NullArguments();
    return 0;
}
