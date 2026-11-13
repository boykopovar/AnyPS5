#include "prx/libfontconfig/libfontconfig.hpp"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <limits>

static void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "Fontconfig object check failed at line %d\n", line);
    std::abort();
}

#define CHECK(value) Require((value), __LINE__)

static const FcChar8* Text(const char* text) {
    return reinterpret_cast<const FcChar8*>(text);
}

static void Languages() {
    FcLangSetDestroy(nullptr);
    auto* first = FcLangSetCreate();
    auto* second = FcLangSetCreate();
    CHECK(first && second);
    CHECK(FcLangSetHasLang(first, Text("en")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("EN-us")) == 1);
    CHECK(FcLangSetAdd(first, Text("en-US")) == 1);
    CHECK(FcLangSetHasLang(first, Text("en-US")) == FcLangEqual);
    CHECK(FcLangSetHasLang(first, Text("en")) == FcLangDifferentTerritory);
    CHECK(FcLangSetHasLang(first, Text("en-GB")) == FcLangDifferentTerritory);
    CHECK(FcLangSetHasLang(first, Text("eng")) == FcLangDifferentLang);
    CHECK(FcLangSetHasLang(second, Text("en-US")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("EN")) == 1);
    CHECK(FcLangSetHasLang(first, Text("en")) == FcLangEqual);
    CHECK(FcLangSetHasLang(first, Text("en_US")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("zh-cn")) == 1);
    CHECK(FcLangSetHasLang(first, Text("zh-tw")) == FcLangDifferentTerritory);
    CHECK(FcLangSetAdd(first, Text("x-private-long-tag")) == 1);
    CHECK(FcLangSetHasLang(first, Text("X-PRIVATE-LONG-TAG")) == FcLangEqual);
    CHECK(FcLangSetHasLang(first, Text("x-another")) == FcLangDifferentTerritory);
    CHECK(FcLangSetAdd(first, Text("und")) == 1);
    CHECK(FcLangSetHasLang(first, Text("und")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(first, Text("und-Latn")) == 1);
    CHECK(FcLangSetHasLang(first, Text("und-latn")) == FcLangEqual);
    CHECK(FcLangSetAdd(first, Text("")) == 1);
    CHECK(FcLangSetHasLang(first, Text("")) == FcLangEqual);
    FcLangSetDestroy(first);
    CHECK(FcLangSetAdd(second, Text("fr")) == 1);
    CHECK(FcLangSetHasLang(second, Text("FR-ca")) == FcLangDifferentTerritory);
    FcLangSetDestroy(second);
}

static void Patterns() {
    FcPatternDestroy(nullptr);
    CHECK(FcPatternDuplicate(nullptr) == nullptr);
    auto* pattern = FcPatternCreate();
    auto* empty = FcPatternCreate();
    CHECK(pattern && empty);
    CHECK(FcPatternEqual(pattern, pattern) == 1);
    CHECK(FcPatternEqual(pattern, empty) == 0);
    CHECK(FcPatternEqual(nullptr, nullptr) == 1);
    FcValue value{};
    value.type = FcTypeRange;
    value.u.i = 123;
    CHECK(FcPatternGet_nid_postfix(nullptr, "family", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet_nid_postfix(pattern, "missing", 0, &value) == FcResultNoMatch);
    CHECK(value.type == FcTypeRange && value.u.i == 123);
    char object[] = "family";
    FcChar8 original[] = "Example";
    CHECK(FcPatternAddString(pattern, object, original));
    object[0] = 'X';
    original[0] = 'Y';
    FcChar8* string = nullptr;
    CHECK(FcPatternGetString(pattern, "family", 0, &string) == FcResultMatch);
    CHECK(std::strcmp(reinterpret_cast<const char*>(string), "Example") == 0);
    const auto* savedString = string;
    CHECK(FcPatternAddString(pattern, "family", Text("Second")));
    CHECK(std::strcmp(reinterpret_cast<const char*>(savedString), "Example") == 0);
    CHECK(FcPatternGetString(pattern, "family", 1, &string) == FcResultMatch);
    CHECK(std::strcmp(reinterpret_cast<const char*>(string), "Second") == 0);
    CHECK(FcPatternGet_nid_postfix(pattern, "family", -1, &value) == FcResultNoId);
    CHECK(FcPatternGet_nid_postfix(pattern, "family", INT_MAX, &value) == FcResultNoId);
    CHECK(value.type == FcTypeRange && value.u.i == 123);
    int integer = 0x12345678;
    CHECK(FcPatternGetInteger(pattern, "family", 0, &integer) == FcResultTypeMismatch);
    CHECK(integer == 0x12345678);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "family", 4) == 0);
    CHECK(FcPatternAddString(pattern, "family", nullptr) == 0);
    CHECK(FcPatternAddMatrix(pattern, "matrix", nullptr) == 0);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "slant", INT_MIN));
    CHECK(FcPatternAddDouble(pattern, "slant", -3.75));
    CHECK(FcPatternGetInteger(pattern, "slant", 0, &integer) == FcResultMatch && integer == INT_MIN);
    CHECK(FcPatternGetInteger(pattern, "slant", 1, &integer) == FcResultTypeMismatch);
    CHECK(integer == INT_MIN);
    CHECK(FcPatternAddDouble(pattern, "size", 2147483647.75));
    CHECK(FcPatternGetInteger(pattern, "size", 0, &integer) == FcResultTypeMismatch);
    CHECK(integer == INT_MIN);
    CHECK(FcPatternAddBool_nid_postfix(pattern, "antialias", 7));
    CHECK(FcPatternGetBool(pattern, "antialias", 0, &integer) == FcResultMatch && integer == 7);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "antialias", 1) == 0);
    CHECK(FcPatternAddString(pattern, "lang", Text("en")));
    CHECK(FcPatternAddBool_nid_postfix(pattern, "lang", 1) == 0);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "custom", 42));
    CHECK(FcPatternAddString(pattern, "custom", Text("mixed")));
    CHECK(FcPatternGet_nid_postfix(pattern, "custom", 0, &value) == FcResultMatch && value.type == FcTypeInteger && value.u.i == 42);
    CHECK(FcPatternGet_nid_postfix(pattern, "custom", 1, &value) == FcResultMatch && value.type == FcTypeString);
    CHECK(FcPatternAddString(pattern, "Family", Text("case-sensitive object name")));
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "Family", 5));
    FcMatrix input{1, 2, 3, 4};
    CHECK(FcPatternAddMatrix(pattern, "matrix", &input));
    input.xx = 99;
    FcMatrix* matrix = nullptr;
    CHECK(FcPatternGetMatrix(pattern, "matrix", 0, &matrix) == FcResultMatch);
    CHECK(matrix->xx == 1 && matrix->xy == 2 && matrix->yx == 3 && matrix->yy == 4);
    CHECK(FcPatternAddMatrix(pattern, "matrix", matrix));
    CHECK(matrix->xx == 1);
    auto* copy = FcPatternDuplicate(pattern);
    CHECK(copy && copy != pattern && FcPatternEqual(pattern, copy));
    FcChar8* copyString = nullptr;
    CHECK(FcPatternGetString(copy, "family", 0, &copyString) == FcResultMatch);
    CHECK(copyString != savedString);
    CHECK(FcPatternGetMatrix(copy, "matrix", 0, &matrix) == FcResultMatch);
    matrix->xx = 9;
    CHECK(!FcPatternEqual(pattern, copy));
    matrix->xx = 1;
    CHECK(FcPatternEqual(pattern, copy));
    CHECK(!FcPatternRemove(pattern, "family", -1));
    CHECK(!FcPatternRemove(pattern, "family", 2));
    CHECK(FcPatternRemove(pattern, "family", 0));
    CHECK(FcPatternGetString(pattern, "family", 0, &string) == FcResultMatch);
    CHECK(std::strcmp(reinterpret_cast<const char*>(string), "Second") == 0);
    CHECK(FcPatternRemove(pattern, "family", 0));
    CHECK(FcPatternGet_nid_postfix(pattern, "family", 0, &value) == FcResultNoMatch);
    CHECK(!FcPatternDel_nid_postfix(pattern, "family"));
    CHECK(FcPatternDel_nid_postfix(pattern, "custom"));
    CHECK(!FcPatternDel_nid_postfix(pattern, "custom"));
    FcPatternReference(copy);
    FcPatternDestroy(copy);
    CHECK(FcPatternGetInteger(copy, "slant", 0, &integer) == FcResultMatch && integer == INT_MIN);
    CHECK(FcPatternGet_nid_postfix(copy, "absent", 0, nullptr) == FcResultNoMatch);
    CHECK(FcPatternGet_nid_postfix(copy, "slant", 2, nullptr) == FcResultNoId);
    CHECK(FcPatternGetString(copy, "slant", 0, nullptr) == FcResultTypeMismatch);
    FcPatternDestroy(copy);
    FcPatternDestroy(pattern);
    FcPatternDestroy(empty);
}

static void IntegerGetters() {
    auto* pattern = FcPatternCreate();
    CHECK(pattern);
    const int integers[] = {INT_MIN, -1, 0, 1, INT_MAX};
    for (int i = 0; i < static_cast<int>(std::size(integers)); ++i) {
        CHECK(FcPatternAddInteger_nid_postfix(pattern, "integer", integers[i]));
        int output = 0x12345678;
        CHECK(FcPatternGetInteger(pattern, "integer", i, &output) == FcResultMatch);
        CHECK(output == integers[i]);
    }
    const double doubles[] = {
        0.0, -0.0, 42.0, -3.75, 2147483647.75, -2147483649.0,
        std::numeric_limits<double>::max(), std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()
    };
    for (int i = 0; i < static_cast<int>(std::size(doubles)); ++i) {
        CHECK(FcPatternAddDouble(pattern, "double", doubles[i]));
        FcValue value{};
        CHECK(FcPatternGet_nid_postfix(pattern, "double", i, &value) == FcResultMatch);
        CHECK(value.type == FcTypeDouble);
        int output = 0x12345678;
        CHECK(FcPatternGetInteger(pattern, "double", i, &output) == FcResultTypeMismatch);
        CHECK(output == 0x12345678);
    }
    int output = 0x12345678;
    CHECK(FcPatternGetInteger(pattern, "missing", 0, &output) == FcResultNoMatch);
    CHECK(output == 0x12345678);
    CHECK(FcPatternGetInteger(pattern, "double", -1, &output) == FcResultNoId);
    CHECK(output == 0x12345678);
    CHECK(FcPatternGetInteger(pattern, "double", static_cast<int>(std::size(doubles)), &output) == FcResultNoId);
    CHECK(output == 0x12345678);
    FcPatternDestroy(pattern);
}

static void Equality() {
    const struct { const char* first; const char* second; bool equal; } pairs[] = {
        {"Example", "eXAMPLE", true},
        {"Stra\xc3\x9f" "e", "STRASSE", true},
        {"\xce\xa3\xcf\x82\xcf\x83", "\xcf\x83\xcf\x83\xcf\x83", true},
        {"\xe2\x84\xaa", "k", true},
        {"\xc4\xb0", "i\xcc\x87", true},
        {"\xef\xac\x83", "ffi", true},
        {"\xc4\xb1", "i", false},
        {"\xe1\xb2\x88", "", true},
        {"\xe1\xb2\x88", "\xea\x99\x8b", false},
        {"\xc1\x81", "a", true},
        {"\xff" "A", "\xff" "a", true},
        {"\xc3", "\xc3", true},
        {"a b", "ab", false}
    };
    for (const auto& pair : pairs) {
        auto* first = FcPatternCreate();
        auto* second = FcPatternCreate();
        CHECK(first && second);
        CHECK(FcPatternAddString(first, "family", Text(pair.first)));
        CHECK(FcPatternAddString(second, "family", Text(pair.second)));
        CHECK((FcPatternEqual(first, second) != 0) == pair.equal);
        CHECK((FcPatternEqual(second, first) != 0) == pair.equal);
        FcPatternDestroy(first);
        FcPatternDestroy(second);
    }
    auto* first = FcPatternCreate();
    auto* second = FcPatternCreate();
    CHECK(first && second);
    CHECK(FcPatternAddInteger_nid_postfix(first, "custom", 42));
    CHECK(FcPatternAddDouble(second, "custom", 42));
    CHECK(FcPatternEqual(first, second));
    CHECK(FcPatternAddDouble(first, "nan", std::numeric_limits<double>::quiet_NaN()));
    CHECK(FcPatternAddDouble(second, "nan", std::numeric_limits<double>::quiet_NaN()));
    CHECK(!FcPatternEqual(first, second));
    CHECK(FcPatternEqual(first, first));
    FcPatternDestroy(first);
    FcPatternDestroy(second);
}

static void FontSets() {
    auto* pattern = FcPatternCreate();
    auto* set = FcFontSetCreate();
    CHECK(pattern && set);
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "weight", 80));
    CHECK(FcFontSetAdd(set, pattern));
    CHECK(set->nfont == 1);
    CHECK(set->fonts[0] != pattern);
    CHECK(FcPatternEqual(set->fonts[0], pattern));
    CHECK(FcPatternDel_nid_postfix(pattern, "weight"));
    CHECK(FcPatternAddInteger_nid_postfix(pattern, "weight", 200));
    CHECK(FcFontSetAdd(set, pattern));
    CHECK(set->nfont == 2);
    CHECK(set->fonts[1] != pattern && set->fonts[1] != set->fonts[0]);
    FcPatternDestroy(pattern);
    int weight = 0;
    CHECK(FcPatternGetInteger(set->fonts[0], "weight", 0, &weight) == FcResultMatch);
    CHECK(weight == 80);
    CHECK(FcPatternGetInteger(set->fonts[1], "weight", 0, &weight) == FcResultMatch);
    CHECK(weight == 200);
    FcFontSetDestroy(set);
}

static void FreeTypeFaceGetter() {
    int sentinel = 0;
    void* face = &sentinel;
    CHECK(FcPatternGetFTFace(nullptr, "ftface", 0, &face) == FcResultNoMatch && face == &sentinel);
    auto* pattern = FcPatternCreate();
    CHECK(pattern);
    CHECK(FcPatternGetFTFace(pattern, "ftface", 0, &face) == FcResultNoMatch && face == &sentinel);
    CHECK(FcPatternAddString(pattern, "family", Text("Sans")) == 1);
    CHECK(FcPatternAddInteger(pattern, "weight", 80) == 1);
    CHECK(FcPatternGetFTFace(pattern, "family", 0, &face) == FcResultTypeMismatch && face == &sentinel);
    CHECK(FcPatternGetFTFace(pattern, "weight", 0, &face) == FcResultTypeMismatch && face == &sentinel);
    CHECK(FcPatternGetFTFace(pattern, "family", 0, nullptr) == FcResultTypeMismatch);
    CHECK(FcPatternGetFTFace(pattern, "family", 1, &face) == FcResultNoId && face == &sentinel);
    CHECK(FcPatternGetFTFace(pattern, "family", -1, &face) == FcResultNoId && face == &sentinel);
    FcPatternDestroy(pattern);
}

static void CharSetValues() {
    auto* pattern = FcPatternCreate();
    auto* other = FcPatternCreate();
    auto* first = FcCharSetCreate();
    auto* second = FcCharSetCreate();
    CHECK(pattern && other && first && second);
    CHECK(FcCharSetAddChar(first, 'A'));
    CHECK(FcCharSetAddChar(second, 'A'));
    CHECK(!FcPatternAddCharSet(pattern, "family", first));
    CHECK(!FcPatternAddCharSet(pattern, "charset", nullptr));
    CHECK(FcPatternAddCharSet(pattern, "charset", first));
    CHECK(FcPatternAddCharSet(other, "charset", second));
    CHECK(FcPatternEqual(pattern, other));

    auto* borrowed = second;
    CHECK(FcPatternGetCharSet(pattern, "absent", 0, &borrowed) == FcResultNoMatch && borrowed == second);
    CHECK(FcPatternGetCharSet(pattern, "charset", -1, &borrowed) == FcResultNoId && borrowed == second);
    CHECK(FcPatternGetCharSet(pattern, "charset", INT_MAX, &borrowed) == FcResultNoId && borrowed == second);
    CHECK(FcPatternAddString(pattern, "family", Text("example")));
    CHECK(FcPatternGetCharSet(pattern, "family", 0, &borrowed) == FcResultTypeMismatch && borrowed == second);
    CHECK(FcPatternDel_nid_postfix(pattern, "family"));
    CHECK(FcPatternGetCharSet(pattern, "charset", 0, &borrowed) == FcResultMatch && borrowed == first);
    FcValue value{};
    CHECK(FcPatternGet_nid_postfix(pattern, "charset", 0, &value) == FcResultMatch && value.type == FcTypeCharSet && value.u.c == borrowed);
    FcCharSetDestroy(first);
    CHECK(FcCharSetAddChar(borrowed, 0x1f680));
    CHECK(!FcPatternEqual(pattern, other));
    CHECK(FcCharSetSubtractCount(borrowed, second) == 1);

    auto* copy = FcPatternDuplicate(pattern);
    auto* select = FcObjectSetBuild("charset", "charset", static_cast<const char*>(nullptr));
    CHECK(copy && select && FcPatternEqual(pattern, copy));
    auto* filtered = FcPatternFilter(pattern, select);
    FcObjectSetDestroy(select);
    CHECK(filtered);
    CHECK(FcPatternRemove(pattern, "charset", 0));
    FcPatternDestroy(pattern);
    FcPatternDestroy(copy);
    CHECK(FcPatternRemove(filtered, "charset", 0));
    auto* retained = second;
    CHECK(FcPatternGetCharSet(filtered, "charset", 0, &retained) == FcResultMatch && retained == borrowed);
    CHECK(FcCharSetSubtractCount(retained, second) == 1);
    CHECK(FcCharSetAddChar(second, 0x1f680));
    CHECK(FcPatternEqual(filtered, other));
    FcCharSetDestroy(second);
    FcPatternDestroy(other);
    FcPatternDestroy(filtered);
}

static void LangSetValues() {
    auto* pattern = FcPatternCreate();
    auto* languages = FcLangSetCreate();
    CHECK(pattern && languages);
    CHECK(FcLangSetAdd(languages, Text("en")));
    CHECK(FcLangSetAdd(languages, Text("x-private-font")));
    CHECK(!FcPatternAddLangSet(pattern, "family", languages));
    CHECK(!FcPatternAddLangSet(pattern, "lang", nullptr));
    CHECK(FcPatternAddString(pattern, "lang", Text("en")));
    CHECK(FcPatternAddLangSet(pattern, "lang", languages));
    auto* borrowed = languages;
    CHECK(FcPatternGetLangSet(pattern, "absent", 0, &borrowed) == FcResultNoMatch && borrowed == languages);
    CHECK(FcPatternGetLangSet(pattern, "lang", 0, &borrowed) == FcResultTypeMismatch && borrowed == languages);
    CHECK(FcPatternGetLangSet(pattern, "lang", 2, &borrowed) == FcResultNoId && borrowed == languages);
    CHECK(FcPatternGetLangSet(pattern, "lang", 1, &borrowed) == FcResultMatch && borrowed != languages);
    FcValue value{};
    CHECK(FcPatternGet_nid_postfix(pattern, "lang", 1, &value) == FcResultMatch && value.type == FcTypeLangSet && value.u.l == borrowed);
    CHECK(FcLangSetAdd(languages, Text("fr")));
    CHECK(FcLangSetHasLang(borrowed, Text("fr")) == FcLangDifferentLang);
    FcLangSetDestroy(languages);
    CHECK(FcLangSetHasLang(borrowed, Text("en")) == FcLangEqual);
    CHECK(FcLangSetHasLang(borrowed, Text("x-private-font")) == FcLangEqual);

    auto* copy = FcPatternFilter(pattern, nullptr);
    auto* select = FcObjectSetBuild("lang", "lang", static_cast<const char*>(nullptr));
    CHECK(copy && copy != pattern && select && FcPatternEqual(pattern, copy));
    auto* filtered = FcPatternFilter(pattern, select);
    FcObjectSetDestroy(select);
    CHECK(filtered);
    FcLangSet* copyLanguages = nullptr;
    FcLangSet* selectedFirst = nullptr;
    FcLangSet* selectedSecond = nullptr;
    CHECK(FcPatternGetLangSet(copy, "lang", 1, &copyLanguages) == FcResultMatch && copyLanguages != borrowed);
    CHECK(FcPatternGetLangSet(filtered, "lang", 1, &selectedFirst) == FcResultMatch && selectedFirst != borrowed);
    CHECK(FcPatternGetLangSet(filtered, "lang", 3, &selectedSecond) == FcResultMatch && selectedSecond != selectedFirst);
    CHECK(FcLangSetAdd(borrowed, Text("de")));
    CHECK(!FcPatternEqual(pattern, copy));
    CHECK(FcLangSetHasLang(copyLanguages, Text("de")) == FcLangDifferentLang);
    CHECK(FcLangSetAdd(copyLanguages, Text("de")));
    CHECK(FcPatternEqual(pattern, copy));
    CHECK(FcLangSetAdd(selectedFirst, Text("it")));
    CHECK(FcLangSetHasLang(selectedSecond, Text("it")) == FcLangDifferentLang);
    CHECK(FcPatternDel_nid_postfix(pattern, "lang"));
    FcPatternDestroy(pattern);
    FcPatternDestroy(copy);
    CHECK(FcLangSetHasLang(selectedFirst, Text("it")) == FcLangEqual);
    CHECK(FcLangSetHasLang(selectedSecond, Text("x-private-font")) == FcLangEqual);
    FcPatternDestroy(filtered);
}

static void PatternFiltering() {
    auto* source = FcPatternCreate();
    CHECK(source);
    CHECK(FcPatternAddString(source, "family", Text("First")));
    CHECK(FcPatternAddString(source, "family", Text("Second")));
    CHECK(FcPatternAddInteger_nid_postfix(source, "weight", 180));
    CHECK(FcPatternAddDouble(source, "size", 12.5));
    auto* select = FcObjectSetBuild("family", "missing", "family", "size", static_cast<const char*>(nullptr));
    auto* selectNone = FcObjectSetBuild(nullptr);
    CHECK(select && selectNone);
    auto* filtered = FcPatternFilter(source, select);
    auto* empty = FcPatternFilter(source, selectNone);
    FcObjectSetDestroy(select);
    FcObjectSetDestroy(selectNone);
    CHECK(filtered && empty && !FcPatternEqual(source, filtered));
    FcPatternDestroy(source);
    FcValue value{};
    CHECK(FcPatternGet_nid_postfix(empty, "family", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet_nid_postfix(filtered, "missing", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet_nid_postfix(filtered, "weight", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet_nid_postfix(filtered, "size", 0, &value) == FcResultMatch && value.type == FcTypeDouble && value.u.d == 12.5);
    const char* expected[] = {"First", "Second", "First", "Second"};
    for (int index = 0; index < static_cast<int>(std::size(expected)); ++index) {
        FcChar8* text = nullptr;
        CHECK(FcPatternGetString(filtered, "family", index, &text) == FcResultMatch);
        CHECK(std::strcmp(reinterpret_cast<const char*>(text), expected[index]) == 0);
    }
    CHECK(FcPatternGet_nid_postfix(filtered, "family", 4, &value) == FcResultNoId);
    FcPatternDestroy(empty);
    FcPatternDestroy(filtered);
}

int main() {
    CharSetValues();
    LangSetValues();
    PatternFiltering();
    Languages();
    Patterns();
    IntegerGetters();
    Equality();
    FontSets();
    FreeTypeFaceGetter();
}
