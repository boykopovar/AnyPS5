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
    CHECK(FcPatternGet(nullptr, "family", 0, &value) == FcResultNoMatch);
    CHECK(FcPatternGet(pattern, "missing", 0, &value) == FcResultNoMatch);
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
    CHECK(FcPatternGet(pattern, "family", -1, &value) == FcResultNoId);
    CHECK(FcPatternGet(pattern, "family", INT_MAX, &value) == FcResultNoId);
    CHECK(value.type == FcTypeRange && value.u.i == 123);
    int integer = 0x12345678;
    CHECK(FcPatternGetInteger(pattern, "family", 0, &integer) == FcResultTypeMismatch);
    CHECK(integer == 0x12345678);
    CHECK(FcPatternAddInteger(pattern, "family", 4) == 0);
    CHECK(FcPatternAddString(pattern, "family", nullptr) == 0);
    CHECK(FcPatternAddMatrix(pattern, "matrix", nullptr) == 0);
    CHECK(FcPatternAddInteger(pattern, "slant", INT_MIN));
    CHECK(FcPatternAddDouble(pattern, "slant", -3.75));
    CHECK(FcPatternGetInteger(pattern, "slant", 0, &integer) == FcResultMatch && integer == INT_MIN);
    CHECK(FcPatternGetInteger(pattern, "slant", 1, &integer) == FcResultTypeMismatch);
    CHECK(integer == INT_MIN);
    CHECK(FcPatternAddDouble(pattern, "size", 2147483647.75));
    CHECK(FcPatternGetInteger(pattern, "size", 0, &integer) == FcResultTypeMismatch);
    CHECK(integer == INT_MIN);
    CHECK(FcPatternAddBool(pattern, "antialias", 7));
    CHECK(FcPatternGetBool(pattern, "antialias", 0, &integer) == FcResultMatch && integer == 7);
    CHECK(FcPatternAddInteger(pattern, "antialias", 1) == 0);
    CHECK(FcPatternAddString(pattern, "lang", Text("en")));
    CHECK(FcPatternAddBool(pattern, "lang", 1) == 0);
    CHECK(FcPatternAddInteger(pattern, "custom", 42));
    CHECK(FcPatternAddString(pattern, "custom", Text("mixed")));
    CHECK(FcPatternGet(pattern, "custom", 0, &value) == FcResultMatch && value.type == FcTypeInteger && value.u.i == 42);
    CHECK(FcPatternGet(pattern, "custom", 1, &value) == FcResultMatch && value.type == FcTypeString);
    CHECK(FcPatternAddString(pattern, "Family", Text("case-sensitive object name")));
    CHECK(FcPatternAddInteger(pattern, "Family", 5));
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
    CHECK(FcPatternGet(pattern, "family", 0, &value) == FcResultNoMatch);
    CHECK(!FcPatternDel(pattern, "family"));
    CHECK(FcPatternDel(pattern, "custom"));
    CHECK(!FcPatternDel(pattern, "custom"));
    FcPatternReference(copy);
    FcPatternDestroy(copy);
    CHECK(FcPatternGetInteger(copy, "slant", 0, &integer) == FcResultMatch && integer == INT_MIN);
    CHECK(FcPatternGet(copy, "absent", 0, nullptr) == FcResultNoMatch);
    CHECK(FcPatternGet(copy, "slant", 2, nullptr) == FcResultNoId);
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
        CHECK(FcPatternAddInteger(pattern, "integer", integers[i]));
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
        CHECK(FcPatternGet(pattern, "double", i, &value) == FcResultMatch);
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
    CHECK(FcPatternAddInteger(first, "custom", 42));
    CHECK(FcPatternAddDouble(second, "custom", 42));
    CHECK(FcPatternEqual(first, second));
    CHECK(FcPatternAddDouble(first, "nan", std::numeric_limits<double>::quiet_NaN()));
    CHECK(FcPatternAddDouble(second, "nan", std::numeric_limits<double>::quiet_NaN()));
    CHECK(!FcPatternEqual(first, second));
    CHECK(FcPatternEqual(first, first));
    FcPatternDestroy(first);
    FcPatternDestroy(second);
}

static int Count(const FcPattern* pattern, const char* object) {
    FcValue value{};
    int count = 0;
    while (FcPatternGet(pattern, object, count, &value) == FcResultMatch) ++count;
    return count;
}

static bool HasInteger(const FcPattern* pattern, const char* object, int expected) {
    int value = 0;
    return Count(pattern, object) == 1 && FcPatternGetInteger(pattern, object, 0, &value) == FcResultMatch
        && value == expected;
}

static bool HasBool(const FcPattern* pattern, const char* object, FcBool expected) {
    FcBool value = 2;
    return Count(pattern, object) == 1 && FcPatternGetBool(pattern, object, 0, &value) == FcResultMatch
        && value == expected;
}

static bool HasDouble(const FcPattern* pattern, const char* object, double expected) {
    FcValue value{};
    return Count(pattern, object) == 1 && FcPatternGet(pattern, object, 0, &value) == FcResultMatch
        && value.type == FcTypeDouble && value.u.d == expected;
}

static bool HasString(const FcPattern* pattern, const char* object, int id, const char* expected) {
    FcChar8* value = nullptr;
    return FcPatternGetString(pattern, object, id, &value) == FcResultMatch
        && std::strcmp(reinterpret_cast<const char*>(value), expected) == 0;
}

static void DefaultSubstitute() {
    auto* pattern = FcPatternCreate();
    CHECK(pattern);
    FcDefaultSubstitute(pattern);
    CHECK(HasInteger(pattern, "weight", 80));
    CHECK(HasInteger(pattern, "slant", 0));
    CHECK(HasInteger(pattern, "width", 100));
    CHECK(HasBool(pattern, "hinting", 1));
    CHECK(HasBool(pattern, "verticallayout", 0));
    CHECK(HasBool(pattern, "autohint", 0));
    CHECK(HasBool(pattern, "globaladvance", 1));
    CHECK(HasBool(pattern, "embeddedbitmap", 1));
    CHECK(HasBool(pattern, "decorative", 0));
    CHECK(HasBool(pattern, "symbol", 0));
    CHECK(HasBool(pattern, "variable", 0));
    CHECK(HasDouble(pattern, "size", 12.0));
    CHECK(HasDouble(pattern, "scale", 1.0));
    CHECK(HasDouble(pattern, "dpi", 75.0));
    CHECK(HasDouble(pattern, "pixelsize", 12.0 * 1.0 * (75.0 / 72.0)));
    CHECK(HasInteger(pattern, "fontversion", 0x7fffffff));
    CHECK(HasInteger(pattern, "hintstyle", 3));
    CHECK(Count(pattern, "namelang") == 1 && HasString(pattern, "namelang", 0, "en"));
    for (const char* object : {"familylang", "stylelang", "fullnamelang"}) {
        CHECK(Count(pattern, object) == 2);
        CHECK(HasString(pattern, object, 0, "en") && HasString(pattern, object, 1, "en-us"));
    }
    CHECK(HasInteger(pattern, "order", 0));
    CHECK(Count(pattern, "prgname") == 0);
    CHECK(Count(pattern, "desktop") == 0);
    auto* again = FcPatternDuplicate(pattern);
    CHECK(again);
    FcDefaultSubstitute(again);
    CHECK(FcPatternEqual(pattern, again));
    FcPatternDestroy(again);
    FcPatternDestroy(pattern);

    pattern = FcPatternCreate();
    CHECK(pattern);
    CHECK(FcPatternAddInteger(pattern, "weight", 200));
    CHECK(FcPatternAddBool(pattern, "hinting", 0));
    CHECK(FcPatternAddInteger(pattern, "size", 20));
    CHECK(FcPatternAddDouble(pattern, "scale", 2.0));
    CHECK(FcPatternAddDouble(pattern, "dpi", 144.0));
    CHECK(FcPatternAddString(pattern, "namelang", Text("ja")));
    CHECK(FcPatternAddString(pattern, "stylelang", Text("de")));
    CHECK(FcPatternAddBool(pattern, "variable", 1));
    CHECK(FcPatternAddInteger(pattern, "order", 7));
    FcDefaultSubstitute(pattern);
    CHECK(HasBool(pattern, "variable", 1));
    CHECK(HasInteger(pattern, "order", 7));
    CHECK(HasInteger(pattern, "weight", 200));
    CHECK(HasBool(pattern, "hinting", 0));
    CHECK(HasDouble(pattern, "size", 20.0));
    CHECK(HasDouble(pattern, "scale", 2.0));
    CHECK(HasDouble(pattern, "dpi", 144.0));
    CHECK(HasDouble(pattern, "pixelsize", 80.0));
    CHECK(Count(pattern, "familylang") == 2);
    CHECK(HasString(pattern, "familylang", 0, "ja") && HasString(pattern, "familylang", 1, "en-us"));
    CHECK(Count(pattern, "stylelang") == 1 && HasString(pattern, "stylelang", 0, "de"));
    FcPatternDestroy(pattern);

    pattern = FcPatternCreate();
    CHECK(pattern);
    CHECK(FcPatternAddDouble(pattern, "size", 50.0));
    CHECK(FcPatternAddDouble(pattern, "pixelsize", 30.0));
    CHECK(FcPatternAddDouble(pattern, "dpi", 96.0));
    FcDefaultSubstitute(pattern);
    CHECK(HasDouble(pattern, "pixelsize", 30.0));
    CHECK(HasDouble(pattern, "size", 30.0 / 96.0 * 72.0 / 1.0));
    CHECK(HasDouble(pattern, "dpi", 96.0));
    CHECK(Count(pattern, "scale") == 0);
    FcPatternDestroy(pattern);
}

static void FontSets() {
    auto* pattern = FcPatternCreate();
    auto* set = FcFontSetCreate();
    CHECK(pattern && set);
    CHECK(FcPatternAddInteger(pattern, "weight", 80));
    CHECK(FcFontSetAdd(set, pattern));
    CHECK(set->nfont == 1);
    CHECK(set->fonts[0] != pattern);
    CHECK(FcPatternEqual(set->fonts[0], pattern));
    CHECK(FcPatternDel(pattern, "weight"));
    CHECK(FcPatternAddInteger(pattern, "weight", 200));
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

int main() {
    Languages();
    Patterns();
    IntegerGetters();
    Equality();
    FontSets();
    DefaultSubstitute();
}
