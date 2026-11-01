#include "prx/libfontconfig/libfontconfig.hpp"

#include <array>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    CHECK(FcPatternGetInteger(pattern, "slant", 1, &integer) == FcResultMatch && integer == -3);
    CHECK(FcPatternAddDouble(pattern, "size", 2147483647.75));
    CHECK(FcPatternGetInteger(pattern, "size", 0, &integer) == FcResultMatch && integer == INT_MAX);
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
    CHECK(FcPatternGetInteger(copy, "slant", 1, &integer) == FcResultMatch && integer == -3);
    CHECK(FcPatternGet(copy, "absent", 0, nullptr) == FcResultNoMatch);
    CHECK(FcPatternGet(copy, "slant", 2, nullptr) == FcResultNoId);
    CHECK(FcPatternGetString(copy, "slant", 0, nullptr) == FcResultTypeMismatch);
    FcPatternDestroy(copy);
    FcPatternDestroy(pattern);
    FcPatternDestroy(empty);
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

int main() {
    Languages();
    Patterns();
    Equality();
}
